/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#include <eosio/producer_heartbeat_plugin/producer_heartbeat_plugin.hpp>
#include <eosio/chain/types.hpp>
#include <boost/asio/steady_timer.hpp>
#include <fc/variant_object.hpp>
#include <fc/io/json.hpp>
#include <eosio/chain/abi_serializer.hpp>
#include <boost/thread/mutex.hpp>

#include <algorithm> 
#include <cctype>
#include <locale>



namespace eosio {
   using namespace eosio::chain;
   template<typename I>
   std::string itoh(I n, size_t hlen = sizeof(I)<<1) {
      static const char* digits = "0123456789abcdef";
      std::string r(hlen, '0');
      for(size_t i = 0, j = (hlen - 1) * 4 ; i < hlen; ++i, j -= 4)
         r[i] = digits[(n>>j) & 0x0f];
      return r;
   }
   static appbase::abstract_plugin& _template_plugin = app().register_plugin<producer_heartbeat_plugin>();

// https://stackoverflow.com/questions/216823/whats-the-best-way-to-trim-stdstring
// trim from start (in place)
static inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
        return !std::isspace(ch);
    }));
}

// trim from end (in place)
static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(std::string &s) {
    ltrim(s);
    rtrim(s);
}

// trim (copying)
static inline std::string trim_copy(std::string s) {
    trim(s);
    return s;
}

class producer_heartbeat_plugin_impl {
   public:
      unique_ptr<boost::asio::steady_timer> timer;
      unique_ptr<boost::asio::steady_timer> retry_timer;
      boost::asio::steady_timer::duration timer_period;
      boost::asio::steady_timer::duration retry_timer_period;
      fc::optional<boost::signals2::scoped_connection> accepted_block_conn;
      int interval;
      int retry_max = 0;
      uint8_t curr_retry = 0;
      account_name heartbeat_contract = "";
      account_name heartbeat_blacklist_contract = "";
      std::string heartbeat_permission = "";
      std::string oncall = "telegram:nobody";
      account_name producer_name;
      std::string actor_blacklist_hash = "";
      uint32_t actor_blacklist_count = 0;
      uint64_t state_db_size = 0;
      uint64_t total_memory = 0;
      std::string cpu_info = "";
      std::string virtualization_type = "";
      fc::crypto::private_key _heartbeat_private_key;
      chain::public_key_type _heartbeat_public_key;
      boost::mutex mtx;
   	mutable_variant_object latencies;
   	map<std::string,std::pair<int64_t, uint32_t>> latencies_sum_count;

      void on_accepted_block(const block_state_ptr& block_state){
        if(producer_name == block_state->block->producer)
            return;
        boost::mutex::scoped_lock lock(mtx, boost::try_to_lock);
        if (lock){
           uint32_t latency = (fc::time_point::now() - block_state->block->timestamp).count()/1000;
           auto producer_name_str = block_state->block->producer.to_string();
           auto sumcount = latencies_sum_count.find(producer_name_str);
           if(sumcount == latencies_sum_count.end()){
               latencies_sum_count[producer_name_str] = std::pair<int64_t, uint32_t>(1, latency);
               latencies(producer_name_str, latency);
           }
           else{
              auto oldpair = sumcount->second;
              auto pair = std::pair<int64_t, uint32_t>(oldpair.first + 1, oldpair.second + latency);
              latencies_sum_count[producer_name_str] = pair;
              latencies(producer_name_str, pair.second / pair.first);
           }
        }
      }
      mutable_variant_object collect_metadata(controller& cc){
         boost::mutex::scoped_lock lock(mtx);
         // get latencies table & clear table
         auto latencies_to_use = latencies;
         latencies = mutable_variant_object();
         latencies_sum_count.clear();
         lock.unlock();

         return mutable_variant_object()
               ("hb_version", "1.5.01")
               ("version", itoh(static_cast<uint32_t>(app().version())))
               ("version_string", app().version_string())
               ("abl_hash", actor_blacklist_hash)
               ("abl_cnt", actor_blacklist_count)
               ("interval", interval)
               ("cpu", cpu_info)
               ("oncall", oncall)
               ("vtype", virtualization_type)
               ("memory", total_memory)
               ("db_size", state_db_size)
               ("latencies", latencies_to_use)
               ("head",  cc.fork_db_head_block_num());
      }
      void send_heartbeat_transaction(int retry = 0){
            auto& plugin = app().get_plugin<chain_plugin>();
            
            auto chainid = plugin.get_chain_id();
            auto abi_serializer_max_time = plugin.get_abi_serializer_max_time();
            // auto rw_api = plugin.get_read_write_api();

            controller& cc = plugin.chain();
            auto* account_obj = cc.db().find<account_object, by_name>(heartbeat_contract);
            if(account_obj == nullptr)
               return;
            abi_def abi;
            if (!abi_serializer::to_abi(account_obj->abi, abi)) 
               return;
            if(!producer_name)
               return;
            abi_serializer eosio_serializer(abi, abi_serializer_max_time);
            signed_transaction trx;
            action act;
            act.account = heartbeat_contract;
            act.name = N(heartbeat);
            act.authorization = vector<permission_level>{{producer_name,heartbeat_permission}};

            auto action_type = eosio_serializer.get_action_type( name{"heartbeat"} );

            act.data = eosio_serializer.variant_to_binary(action_type,
               mutable_variant_object()
                  ("bpname", producer_name)
                  ("timestamp", fc::time_point::now()), 
               abi_serializer_max_time);
            trx.actions.push_back(act);
            
            trx.expiration = cc.head_block_time() + fc::seconds(30);
            trx.set_reference_block(cc.head_block_id());
            trx.sign(_heartbeat_private_key, chainid);
            curr_retry = retry;
            plugin.accept_transaction( packed_transaction(trx),[=](const fc::static_variant<fc::exception_ptr, transaction_trace_ptr>& result){
                   if (result.contains<fc::exception_ptr>()) {
                     if(curr_retry < retry_max - 1){
                        elog("heartbeat failed - retry: ${err}", ("err", result.get<fc::exception_ptr>()->to_detail_string()));
                        retry_timer->expires_from_now(retry_timer_period);
                        retry_timer->async_wait( [this](boost::system::error_code ec) {
                           send_heartbeat_transaction(curr_retry + 1);
                        });
                     }
                     else
                        elog("heartbeat failed: ${err}", ("err", result.get<fc::exception_ptr>()->to_detail_string()));
                  } else {
                     dlog("heartbeat success");
                  }
            });
      }
      void start_timer( ) {
         timer->expires_from_now(timer_period);
         timer->async_wait( [this](boost::system::error_code ec) {
               start_timer();
               if(!ec) {
                  try{
                     send_heartbeat_transaction();
                  }
                  FC_LOG_AND_DROP();
                  
               }
               else {
                  elog( "Error from heartbeat timer: ${m}",( "m", ec.message()));
               }
            });
      }      
};

producer_heartbeat_plugin::producer_heartbeat_plugin():my(new producer_heartbeat_plugin_impl()){
   my->timer.reset(new boost::asio::steady_timer( app().get_io_service()));
   my->retry_timer.reset(new boost::asio::steady_timer( app().get_io_service()));
   my->latencies = mutable_variant_object();
}
producer_heartbeat_plugin::~producer_heartbeat_plugin(){}

void producer_heartbeat_plugin::set_program_options(options_description&, options_description& cfg) {
   cfg.add_options()
         ("heartbeat-period", bpo::value<int>()->default_value(1500),
          "Heartbeat transaction period in seconds")
         ("heartbeat-retry-max", bpo::value<int>()->default_value(3),
          "Heartbeat max retries")
         ("heartbeat-retry-delay-seconds", bpo::value<int>()->default_value(10),
          "Heartbeat retry delay")          
         ("heartbeat-signature-provider", bpo::value<string>()->default_value("HEARTBEAT_PUB_KEY=KEY:HEARTBEAT_PRIVATE_KEY"),
          "Heartbeat key provider")
         ("heartbeat-contract", bpo::value<string>()->default_value("eosheartbeat"),
          "Heartbeat Contract")
         ("heartbeat-permission", bpo::value<string>()->default_value("heartbeat"),
          "Heartbeat permission name")    
         ("heartbeat-user", bpo::value<string>()->default_value("eosforce"),
          "Heartbeat user") 
         ;
}

#define LOAD_VALUE_SET(options, name, container, type) \
if( options.count(name) ) { \
   const std::vector<std::string>& ops = options[name].as<std::vector<std::string>>(); \
   std::copy(ops.begin(), ops.end(), std::inserter(container, container.end())); \
}


template <class Container, class Function>
auto apply (const Container &cont, Function fun) {
    std::vector< typename
            std::result_of<Function(const typename Container::value_type&)>::type> ret;
    ret.reserve(cont.size());
    for (const auto &v : cont) {
       ret.push_back(fun(v));
    }
    return ret;
}
std::string get_cpuinfo() {
    std::string token;
    std::ifstream file("/proc/cpuinfo");
    while(file >> token) {
        if(token == "model") {
            std::string mem;
            if(!(file >> mem))
               return 0;
            if(mem == "name"){
               if(!(file >> mem))
                  return 0;
               std::getline(file,mem);
               return mem;
            }
        }
        // ignore rest of the line
        file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
    return ""; // nothing found
}

std::string get_cpu_type(){
   std::string token;
   std::ifstream file("/sys/devices/virtual/dmi/id/product_name");
   if(!file)
      return "unknown";
   file >> token;
   
   if(token == "HVM" || token == "Droplet"){
      return "HVM";
   }
   if(token == "VMware"){
      return "VMware";
   }
   return "Bare-metal";
}
unsigned long get_mem_total() {
    std::string token;
    std::ifstream file("/proc/meminfo");
    while(file >> token) {
        if(token == "MemTotal:") {
            unsigned long mem;
            if(file >> mem) {
                return mem;
            } else {
                return 0;       
            }
        }
        // ignore rest of the line
        file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
    return 0; // nothing found
}

template <typename T>
void remove_duplicates(std::vector<T>& vec)
{
  std::sort(vec.begin(), vec.end());
  vec.erase(std::unique(vec.begin(), vec.end()), vec.end());
}

void producer_heartbeat_plugin::plugin_initialize(const variables_map& options) {
   try {
      my->total_memory = get_mem_total();
      my->cpu_info = trim_copy(get_cpuinfo());
      my->virtualization_type = get_cpu_type();
      if( options.count( "heartbeat-period" )) {
         my->interval = options.at( "heartbeat-period" ).as<int>();
         my->timer_period = std::chrono::seconds( my->interval );
      }
      if( options.count( "heartbeat-retry-delay-seconds" )) {
         my->retry_timer_period = std::chrono::seconds( options.at( "heartbeat-retry-delay-seconds" ).as<int>() );
      }
      if( options.count( "heartbeat-retry-max" )) {
         my->retry_max = options.at( "heartbeat-retry-max" ).as<int>();
      }
      if( options.count( "heartbeat-contract" )) {
         my->heartbeat_contract = options.at( "heartbeat-contract" ).as<string>();
      }
      if( options.count( "heartbeat-permission" )) {
         my->heartbeat_permission = options.at( "heartbeat-permission" ).as<string>();
      }
      if(options.count("heartbeat-user")){
         const std::string ops = options["heartbeat-user"].as<std::string>();
         my->producer_name = ops;
      }
      if(options.count("chain-state-db-size-mb")){
         my->state_db_size = options["chain-state-db-size-mb"].as<uint64_t>();
      }
      if( options.count("heartbeat-signature-provider") ) {
            auto key_spec_pair = options["heartbeat-signature-provider"].as<std::string>();
            
            try {
               auto delim = key_spec_pair.find("=");
               EOS_ASSERT(delim != std::string::npos, eosio::chain::plugin_config_exception, "Missing \"=\" in the key spec pair");
               auto pub_key_str = key_spec_pair.substr(0, delim);
               auto spec_str = key_spec_pair.substr(delim + 1);
   
               auto spec_delim = spec_str.find(":");
               EOS_ASSERT(spec_delim != std::string::npos, eosio::chain::plugin_config_exception, "Missing \":\" in the key spec pair");
               auto spec_type_str = spec_str.substr(0, spec_delim);
               auto spec_data = spec_str.substr(spec_delim + 1);
   
               auto pubkey = public_key_type(pub_key_str);
               
               
               if (spec_type_str == "KEY") {
                  ilog("Loaded heartbeat key");
                  my->_heartbeat_private_key = fc::crypto::private_key(spec_data);
                  my->_heartbeat_public_key = pubkey;   
               } else if (spec_type_str == "KEOSD") {
                  elog("KEOSD heartbeat key not supported");
                  // not supported
               }
   
            } catch (...) {
               elog("Malformed signature provider: \"${val}\", ignoring!", ("val", key_spec_pair));
            }
         }
         
   }
   FC_LOG_AND_RETHROW()
}

void producer_heartbeat_plugin::plugin_startup() {
   ilog("producer heartbeat plugin:  plugin_startup() begin");
   try{
      auto& chain = app().find_plugin<chain_plugin>()->chain();
      my->accepted_block_conn.emplace(chain.accepted_block.connect(
         [&](const block_state_ptr& b_state) {
            my->on_accepted_block(b_state);
      }));
      my->send_heartbeat_transaction();
   }
   FC_LOG_AND_DROP();
   my->start_timer();
}

      



void producer_heartbeat_plugin::plugin_shutdown() {
   my->accepted_block_conn.reset();
}
   
}
