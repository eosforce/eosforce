/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#include <eosio/chain/config_on_chain.hpp>
#include <eosio/heartbeat_plugin/heartbeat_plugin.hpp>
#include <eosio/chain/producer_object.hpp>
#include <eosio/chain/plugin_interface.hpp>
#include <eosio/chain/global_property_object.hpp>
#include <eosio/chain/transaction_object.hpp>
#include <eosio/chain/transaction.hpp>
#include <eosio/chain/authorization_manager.hpp>


#include <fc/io/json.hpp>
#include <fc/smart_ref_impl.hpp>
#include <fc/scoped_exit.hpp>

#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <iostream>
#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/range/adaptor/map.hpp>
#include <boost/function_output_iterator.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/signals2/connection.hpp>

namespace bmi = boost::multi_index;
using bmi::indexed_by;
using bmi::ordered_non_unique;
using bmi::member;
using bmi::tag;
using bmi::hashed_unique;

using boost::multi_index_container;

using std::string;
using std::vector;
using std::deque;
using boost::signals2::scoped_connection;

// HACK TO EXPOSE LOGGER MAP

namespace fc {
   extern std::unordered_map<std::string,logger>& get_logger_map();
}

const fc::string logger_name("heartbeat_plugin");
fc::logger heartbeat_log;

const fc::string trx_trace_logger_name("transaction_tracing");
fc::logger heartbeat_trx_trace_log;

namespace eosio {

static appbase::abstract_plugin& _heartbeat_plugin = app().register_plugin<heartbeat_plugin>();

using namespace eosio::chain;
using namespace eosio::chain::plugin_interface;

class heartbeat_plugin_impl : public std::enable_shared_from_this<heartbeat_plugin_impl> {
   public:
      heartbeat_plugin_impl(boost::asio::io_service& io)
      :_timer(io)
      {
      }

      boost::program_options::variables_map _options;
      std::set<chain::account_name>                             _producers;
      std::map<chain::public_key_type, chain::private_key_type> _keys;
      std::map<chain::account_name, chain::account_name>        _bp_mappings;// producer_name -> account_name
      boost::asio::deadline_timer                               _timer;


      time_point _last_signed_block_time;
      time_point _start_time = fc::time_point::now();

      heartbeat_plugin* _self = nullptr;

      void schedule_heartbeat_loop();
      action get_action( account_name code, action_name acttype, vector<permission_level> auths,
                                   const variant_object& data ) const;
      chain::signed_transaction get_heartbeat_transaction();
      void heartbeat();


      /*
       * HACK ALERT
       * Boost timers can be in a state where a handler has not yet executed but is not abortable.
       * As this method needs to mutate state handlers depend on for proper functioning to maintain
       * invariants for other code (namely accepting incoming transactions in a nearly full block)
       * the handlers capture a corelation ID at the time they are set.  When they are executed
       * they must check that correlation_id against the global ordinal.  If it does not match that
       * implies that this method has been called with the handler in the state where it should be
       * cancelled but wasn't able to be.
       */
      uint32_t _timer_corelation_id = 0;

     
};

heartbeat_plugin::heartbeat_plugin() 
    : my(new heartbeat_plugin_impl(app().get_io_service())){
      my->_self = this;
    }

heartbeat_plugin::~heartbeat_plugin() {}

void heartbeat_plugin::set_program_options(
   boost::program_options::options_description& command_line_options,
   boost::program_options::options_description& config_file_options)
{
   boost::program_options::options_description heartbeat_options;

   heartbeat_options.add_options()
         //("bp-mapping", boost::program_options::value<vector<string>>()->composing()->multitoken()->default_value({std::string("") + "=KEY:" + ""}, std::string("") + "=KEY:" + std::string("")),
         ("bp-mapping", boost::program_options::value<vector<string>>(),
          "Key=Value pairs in the form <producer_name>=<account_name>\n"
          "Where:\n"
          "   <producer_name> \tis a string form of producer_name\n\n"
          "   <account_name> \tis a string in the form <provider-type>:<data>\n\n"
          "   <provider-type> \tis KEY, or KEOSD\n\n"
          "   KEY:<data>      \tis a string form of a valid EOSIO account_name which maps to the provided producer_name\n\n")
         ;
   config_file_options.add(heartbeat_options);
}

template<typename T>
T dejsonify(const string& s) {
   return fc::json::from_string(s).as<T>();
}

#define LOAD_VALUE_SET(options, name, container) \
if( options.count(name) ) { \
   const std::vector<std::string>& ops = options[name].as<std::vector<std::string>>(); \
   std::copy(ops.begin(), ops.end(), std::inserter(container, container.end())); \
}

void heartbeat_plugin::plugin_initialize(const boost::program_options::variables_map& options)
{ try {
   my->_options = &options;
   LOAD_VALUE_SET(options, "producer-name", my->_producers)

   if( options.count("private-key") )
   {
      const std::vector<std::string> key_id_to_wif_pair_strings = options["private-key"].as<std::vector<std::string>>();
      for (const std::string& key_id_to_wif_pair_string : key_id_to_wif_pair_strings)
      {
         try {
            auto key_id_to_wif_pair = dejsonify<std::pair<public_key_type, private_key_type>>(key_id_to_wif_pair_string);
            my->_keys[key_id_to_wif_pair.first] = chain::private_key_type(key_id_to_wif_pair.second);
         } catch ( fc::exception& e ) {
            elog("Malformed private key pair");
         }
      }
   }

   if( options.count("signature-provider") ) {
      const std::vector<std::string> key_spec_pairs = options["signature-provider"].as<std::vector<std::string>>();
      for (const auto& key_spec_pair : key_spec_pairs) {
         try {
            auto delim = key_spec_pair.find("=");
            EOS_ASSERT(delim != std::string::npos, plugin_config_exception, "Missing \"=\" in the key spec pair");
            auto pub_key_str = key_spec_pair.substr(0, delim);
            auto spec_str = key_spec_pair.substr(delim + 1);

            auto spec_delim = spec_str.find(":");
            EOS_ASSERT(spec_delim != std::string::npos, plugin_config_exception, "Missing \":\" in the key spec pair");
            auto spec_type_str = spec_str.substr(0, spec_delim);
            auto spec_data = spec_str.substr(spec_delim + 1);

            auto pubkey = public_key_type(pub_key_str);

            if (spec_type_str == "KEY") {
               my->_keys[pubkey] = private_key_type(spec_data);
            } else if (spec_type_str == "KEOSD") {
               my->_keys[pubkey] = private_key_type(spec_data);
            }
         } catch (...) {
            elog("Malformed signature provider: \"${val}\", ignoring!", ("val", key_spec_pair));
         }
      }
   }
   
   if( options.count("bp-mapping") ) {
      const std::vector<std::string> key_spec_pairs = options["bp-mapping"].as<std::vector<std::string>>();
      for (const auto& key_spec_pair : key_spec_pairs) {
         try {
            auto delim = key_spec_pair.find("=");
            EOS_ASSERT(delim != std::string::npos, plugin_config_exception, "Missing \"=\" in the key spec pair");
            auto pub_key_str = key_spec_pair.substr(0, delim);
            auto spec_str = key_spec_pair.substr(delim + 1);

            auto spec_delim = spec_str.find(":");
            EOS_ASSERT(spec_delim != std::string::npos, plugin_config_exception, "Missing \":\" in the key spec pair");
            auto spec_type_str = spec_str.substr(0, spec_delim);
            auto spec_data = spec_str.substr(spec_delim + 1);

            //auto pubkey = public_key_type(pub_key_str);

            if (spec_type_str == "KEY") {
               my->_bp_mappings[pub_key_str] = spec_data;
            } else if (spec_type_str == "KEOSD") {
               my->_bp_mappings[pub_key_str] = spec_data;
            }
         } catch (...) {
            elog("Malformed signature provider: \"${val}\", ignoring!", ("val", key_spec_pair));
         }
      }
   }
   
} FC_LOG_AND_RETHROW() 
}

void heartbeat_plugin::plugin_startup()
{ try {
   auto& logger_map = fc::get_logger_map();
   if(logger_map.find(logger_name) != logger_map.end()) {
      heartbeat_log = logger_map[logger_name];
   }

   if( logger_map.find(trx_trace_logger_name) != logger_map.end()) {
      heartbeat_trx_trace_log = logger_map[trx_trace_logger_name];
   }

   ilog("heartbeat plugin:  plugin_startup() begin");


   my->schedule_heartbeat_loop();

   ilog("heartbeat plugin:  plugin_startup() end");
} FC_CAPTURE_AND_RETHROW() }

void heartbeat_plugin::plugin_shutdown() {
   try {
      my->_timer.cancel();
   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
   }
}

void heartbeat_plugin_impl::schedule_heartbeat_loop() {
   chain::controller& chain = app().get_plugin<chain_plugin>().chain();
   _timer.cancel();
   std::weak_ptr<heartbeat_plugin_impl> weak_this = shared_from_this();

   heartbeat();

   // ship this block off no later than its deadline
   //auto deadline = 3;
   auto deadline = get_num_config_on_chain( chain.db(), config::heartbeat_typ::hb_intval, 600 );
   _timer.expires_from_now( boost::posix_time::seconds( deadline ));

   _timer.async_wait([&chain,weak_this,cid=++_timer_corelation_id](const boost::system::error_code& ec) {
      auto self = weak_this.lock();
      if (self && ec != boost::asio::error::operation_aborted && cid == self->_timer_corelation_id) {
         // pending_block_state expected, but can't assert inside async_wait
         self->schedule_heartbeat_loop();
         //fc_dlog(heartbeat_log, "Producing Block #${num} returned: ${res}", ("num", block_num)("res", res));
      }
   });
}

action heartbeat_plugin_impl::get_action( account_name code, action_name acttype, vector<permission_level> auths,
                                   const variant_object& data ) const 
{ 
try {
   chain::controller& chain = app().get_plugin<chain_plugin>().chain();
   const auto& acnt = chain.get_account(code);
   auto abi = acnt.get_abi();
   const fc::microseconds abi_serializer_max_time{1000*1000};
   chain::abi_serializer abis(abi, abi_serializer_max_time);

   string action_type_name = abis.get_action_type(acttype);
   FC_ASSERT( action_type_name != string(), "unknown action type ${a}", ("a",acttype) );

   action act;
   act.account = code;
   act.name = acttype;
   act.authorization = auths;
   act.data = abis.variant_to_binary(action_type_name, data, abi_serializer_max_time);
   return act;
} FC_CAPTURE_AND_RETHROW() 
}

signed_transaction heartbeat_plugin_impl::get_heartbeat_transaction()
{
   FC_ASSERT( !_keys.empty(), "no signing keys!" );
   FC_ASSERT( !_bp_mappings.empty(), "no mapping producer_name!" );
   FC_ASSERT( _bp_mappings.size() == 1, "only one mapping producer_name allowed!" );
   chain::controller& chain = app().get_plugin<chain_plugin>().chain();

   auto it = _bp_mappings.cbegin();
   signed_transaction trx;
   trx.actions.emplace_back( get_action( config::system_account_name, N(heartbeat), vector<permission_level>{{it->second, config::active_name}}, 
                 fc::mutable_variant_object()
                  ("bpname", it->first)
                  ("timestamp", fc::time_point::now()) ) );
   trx.set_reference_block(chain.head_block_id());
   trx.expiration = chain.head_block_time() + fc::microseconds(10'999'999); // Round up to nearest second to avoid appearing expired
   trx.fee = asset(100);
   
   flat_set<public_key_type> pub_keys;
   pub_keys.reserve(_keys.size());
   for( const auto& key: _keys) {
      pub_keys.insert(key.first);
   }
   auto pub_keys2 = chain.get_authorization_manager().get_required_keys( trx, pub_keys, fc::seconds( trx.delay_sec ));
   FC_ASSERT( !pub_keys2.empty(), "no matching signing keys!" );
   for( const auto& key2: pub_keys2) {
      trx.sign( _keys[key2], chain.get_chain_id() );
   }

   return trx;
}

void heartbeat_plugin_impl::heartbeat() {
   chain::controller& chain = app().get_plugin<chain_plugin>().chain();

   try {
      /*auto hb_trx = std::make_shared<transaction_metadata>(
            get_heartbeat_transaction());
      
      auto hbtrace = chain.push_transaction(hb_trx, fc::time_point::maximum(),
                                       config::default_min_transaction_cpu_usage);*/
      next_function<chain_apis::read_write::push_transaction_results> next = {};
      auto hb_trx = std::make_shared<transaction_metadata>(get_heartbeat_transaction());
      app().get_method<incoming::methods::transaction_async>()(hb_trx, true, 
        [this, next](const fc::static_variant<fc::exception_ptr, transaction_trace_ptr>& result) -> void{ });
   } catch( const fc::exception& e ) {
      //EOS_ASSERT(false, transaction_exception, "heartbeart transaction failed, exception: ${e}", ( "e", e ));
      elog("heartbeart transaction failed, exception: ${e}", ( "e", e ));
   } catch( ... ) {
      /*EOS_ASSERT(false, transaction_exception,
                 "heartbeart transaction failed, but could have nott enough asset to pay for transaction fee");*/
      elog("heartbeart transaction failed, but could have nott enough asset to pay for transaction fee");
   }


   /*ilog("Produced block ${id}... #${n} @ ${t} signed by ${p} [trxs: ${count}, lib: ${lib}, confirmed: ${confs}]",
        ("p",new_bs->header.producer)("id",fc::variant(new_bs->id).as_string().substr(0,16))
        ("n",new_bs->block_num)("t",new_bs->header.timestamp)
        ("count",new_bs->block->transactions.size())("lib",chain.last_irreversible_block_num())("confs", new_bs->header.confirmed));*/

}

} // namespace eosio
