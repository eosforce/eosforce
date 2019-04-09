/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#include <eosio/bus_plugin/bus_plugin.hpp>
#include <eosio/chain/eosio_contract.hpp>
#include <eosio/chain/config.hpp>
#include <eosio/chain/exceptions.hpp>
#include <eosio/chain/transaction.hpp>
#include <eosio/chain/types.hpp>

#include <fc/io/json.hpp>
#include <fc/utf8.hpp>
#include <fc/variant.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/chrono.hpp>
#include <boost/signals2/connection.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>

#include <queue>
#include <eosio/chain/genesis_state.hpp>
#include <grpcpp/grpcpp.h>
#include <relay_commit.grpc.pb.h>

namespace fc { class variant; }

namespace eosio {

using chain::account_name;
using chain::action_name;
using chain::block_id_type;
using chain::permission_name;
using chain::transaction;
using chain::signed_transaction;
using chain::signed_block;
using chain::transaction_id_type;
using chain::packed_transaction;

using grpc::Status;
using grpc::Channel;
using grpc::ClientContext;

using force_relay_commit::relay_commit;
using force_relay_commit::RelayCommitRequest;
using force_relay_commit::RelayBlock;
using force_relay_commit::RelayAction;
using force_relay_commit::RelayPermission_level;
using force_relay_commit::RelayCommitReply;


static appbase::abstract_plugin& _bus_plugin = app().register_plugin<bus_plugin>();

class grpc_stub {
public:
   grpc_stub( std::shared_ptr<Channel> channel ) : relay_stub_(relay_commit::NewStub(channel)) {}

   ~grpc_stub() = default;

   void put_relay_commit_req(RelayBlock* block, const vector<RelayAction>& action);

private:
   std::unique_ptr<relay_commit::Stub> relay_stub_;
};

class bus_plugin_impl {
public:
   bus_plugin_impl() = default;
   ~bus_plugin_impl();

   std::string client_address = std::string("");
   std::string bus_transfer_account = std::string("");

   void init();

   boost::thread client_thread;

   void consume_blocks();

   void insert_default_abi();

   fc::optional<boost::signals2::scoped_connection> accepted_block_connection;
   fc::optional<boost::signals2::scoped_connection> irreversible_block_connection;
   fc::optional<boost::signals2::scoped_connection> accepted_transaction_connection;
   fc::optional<boost::signals2::scoped_connection> applied_transaction_connection;

   void accepted_block(const chain::block_state_ptr&);

   void applied_irreversible_block(const chain::block_state_ptr&);

   void accepted_transaction(const chain::transaction_metadata_ptr&);

   void applied_transaction(const chain::transaction_trace_ptr&);

   void process_accepted_transaction(const chain::transaction_metadata_ptr&);

   //void _process_accepted_transaction(const chain::transaction_metadata_ptr&);
   void process_applied_transaction(const chain::transaction_trace_ptr&);

   //void _process_applied_transaction(const chain::transaction_trace_ptr&);
   void process_accepted_block(const chain::block_state_ptr&);

   //void _process_accepted_block( const chain::block_state_ptr& );
   void process_irreversible_block(const chain::block_state_ptr&);

   void _process_irreversible_block(const chain::block_state_ptr&);

   template<typename Queue, typename Entry> void queue(Queue& queue, const Entry& e);

   optional <abi_serializer> get_abi_serializer(account_name n);

   template<typename T> fc::variant to_variant_with_abi(const T& obj);

   void purge_abi_cache();

   fc::microseconds abi_serializer_max_time;
   size_t abi_cache_size = 0;
   struct by_account;
   struct by_last_access;

   struct abi_cache {
      account_name account;
      fc::time_point last_accessed;
      fc::optional<abi_serializer> serializer;
   };

   typedef boost::multi_index_container <abi_cache, indexed_by<ordered_unique < tag < by_account>, member<abi_cache, account_name, &abi_cache::account>>,
   ordered_non_unique <tag<by_last_access>, member<abi_cache, fc::time_point, &abi_cache::last_accessed>>
   >
   >
   abi_cache_index_t;

   abi_cache_index_t abi_cache_index;

   std::deque <chain::transaction_metadata_ptr> transaction_metadata_queue;
   std::deque <chain::transaction_metadata_ptr> transaction_metadata_process_queue;
   std::deque <chain::transaction_trace_ptr> transaction_trace_queue;
   std::deque <chain::transaction_trace_ptr> transaction_trace_process_queue;
   std::deque <chain::block_state_ptr> block_state_queue;
   std::deque <chain::block_state_ptr> block_state_process_queue;
   std::deque <chain::block_state_ptr> irreversible_block_state_queue;
   std::deque <chain::block_state_ptr> irreversible_block_state_process_queue;

   std::atomic_bool done{false};
   std::atomic_bool startup{true};
   boost::mutex mtx;
   boost::condition_variable condition;
   size_t max_queue_size = 512;
   int queue_sleep_time = 0;
private:
   std::unique_ptr <grpc_stub> _grpc_stub;

};

void grpc_stub::put_relay_commit_req(RelayBlock* block, const vector<RelayAction>& actions) {
   try {
      RelayCommitRequest request;
      request.set_allocated_block(block); // no need delete block
      for( const auto& act : actions ) {
         auto actiontemp = request.add_action();
         *actiontemp = act;
      }

      RelayCommitReply reply;
      ClientContext context;
      Status status = relay_stub_->rpc_sendaction(&context, request, &reply);
      if(!status.ok()) {
         EOS_THROW( chain::plugin_exception,
                    "PutBlockRequest error error_code:${error_code},error_message:${message}",
                    ("error_code", (int)(status.error_code()))("message", status.error_message()))
      }
   } catch(std::exception& e) {
      throw;
   } catch(...) {
      throw;
   }

   return;
}

template<typename Queue, typename Entry> void bus_plugin_impl::queue(Queue& queue, const Entry& e) {
   boost::mutex::scoped_lock lock(mtx);
   auto queue_size = queue.size();
   if(queue_size > max_queue_size) {
      lock.unlock();
      condition.notify_one();
      queue_sleep_time += 10;
      if(queue_sleep_time > 1000)
         wlog("queue size: ${q}", ("q", queue_size));
      boost::this_thread::sleep_for(boost::chrono::milliseconds(queue_sleep_time));
      lock.lock();
   } else {
      queue_sleep_time -= 10;
      if(queue_sleep_time < 0) queue_sleep_time = 0;
   }
   queue.emplace_back(e);
   lock.unlock();
   condition.notify_one();
}

void bus_plugin_impl::accepted_transaction(const chain::transaction_metadata_ptr& t) {
   try {
      queue(transaction_metadata_queue, t);
   } catch(fc::exception& e) {
      elog("FC Exception while accepted_transaction ${e}", ("e", e.to_string()));
   } catch(std::exception& e) {
      elog("STD Exception while accepted_transaction ${e}", ("e", e.what()));
   } catch(...) {
      elog("Unknown exception while accepted_transaction");
   }
}

void bus_plugin_impl::applied_transaction(const chain::transaction_trace_ptr& t) {
   try {
      // always queue since account information always gathered
      queue(transaction_trace_queue, t);
   } catch(fc::exception& e) {
      elog("FC Exception while applied_transaction ${e}", ("e", e.to_string()));
   } catch(std::exception& e) {
      elog("STD Exception while applied_transaction ${e}", ("e", e.what()));
   } catch(...) {
      elog("Unknown exception while applied_transaction");
   }
}

void bus_plugin_impl::applied_irreversible_block(const chain::block_state_ptr& bs) {
   try {
      ilog("applied_irreversible_block ${num}", ("num", bs->block_num));
      queue(irreversible_block_state_queue, bs);
   } catch(fc::exception& e) {
      elog("FC Exception while applied_irreversible_block ${e}", ("e", e.to_string()));
   } catch(std::exception& e) {
      elog("STD Exception while applied_irreversible_block ${e}", ("e", e.what()));
   } catch(...) {
      elog("Unknown exception while applied_irreversible_block");
   }
}

void bus_plugin_impl::accepted_block(const chain::block_state_ptr& bs) {
   try {
      queue(block_state_queue, bs);
   } catch(fc::exception& e) {
      elog("FC Exception while accepted_block ${e}", ("e", e.to_string()));
   } catch(std::exception& e) {
      elog("STD Exception while accepted_block ${e}", ("e", e.what()));
   } catch(...) {
      elog("Unknown exception while accepted_block");
   }
}

void bus_plugin_impl::process_accepted_transaction(const chain::transaction_metadata_ptr& t) {
   try {
      const auto& trx = t->trx;
      auto json = fc::json::to_string(trx);
      std::string action = std::string("process_accepted_transaction");
      //  auto reply = _grpc_stub->PutRequest(action,json);
   } catch(fc::exception& e) {
      elog("FC Exception while processing accepted transaction metadata: ${e}", ("e", e.to_detail_string()));
   } catch(std::exception& e) {
      elog("STD Exception while processing accepted tranasction metadata: ${e}", ("e", e.what()));
   } catch(...) {
      elog("Unknown exception while processing accepted transaction metadata");
   }
}

void bus_plugin_impl::process_applied_transaction(const chain::transaction_trace_ptr& t) {
   try {
      // always call since we need to capture setabi on accounts even if not storing transaction traces
      // auto json = fc::json::to_string(t.trace).c_str();
      // std::string action = std::string("process_applied_transaction");
      // auto reply = _grpc_stub->PutRequest(action,json);
   } catch(fc::exception& e) {
      elog("FC Exception while processing applied transaction trace: ${e}", ("e", e.to_detail_string()));
   } catch(std::exception& e) {
      elog("STD Exception while processing applied transaction trace: ${e}", ("e", e.what()));
   } catch(...) {
      elog("Unknown exception while processing applied transaction trace");
   }
}

void bus_plugin_impl::process_irreversible_block(const chain::block_state_ptr& bs) {
   try {
      _process_irreversible_block(bs);
   } catch(fc::exception& e) {
      elog("FC Exception while processing irreversible block: ${e}", ("e", e.to_detail_string()));
   } catch(std::exception& e) {
      elog("STD Exception while processing irreversible block: ${e}", ("e", e.what()));
   } catch(...) {
      elog("Unknown exception while processing irreversible block");
   }
}

void bus_plugin_impl::_process_irreversible_block(const chain::block_state_ptr& bs) {
   const auto block_num = bs->block->block_num();
   bool transactions_in_block = false;

   RelayBlock* block = new RelayBlock;
   block->set_producer(bs->block->producer.to_string());
   block->set_id(bs->block->id().data(), bs->block->id().data_size());
   block->set_previous(bs->block->previous.data(), bs->block->previous.data_size());
   block->set_confirmed(bs->block->confirmed);
   block->set_transaction_mroot(bs->block->transaction_mroot.data(), bs->block->transaction_mroot.data_size());
   block->set_action_mroot(bs->block->action_mroot.data(), bs->block->action_mroot.data_size());
   block->set_mroot(bs->block->digest().data(), bs->block->digest().data_size());

   vector<RelayAction> relay_actions;
   relay_actions.reserve(bs->block->transactions.size() * 3); // most trx has 1-2 actions
   for(const auto& receipt : bs->block->transactions) {
      if(receipt.trx.contains<packed_transaction>()) {
         const auto& pt = receipt.trx.get<packed_transaction>();
         // get id via get_raw_transaction() as packed_transaction.id() mutates internal transaction state
         const auto& raw = pt.get_raw_transaction();
         const auto& trx = fc::raw::unpack<transaction>(raw);

         for(const auto& act: trx.actions) {
            RelayAction tempaction;
            tempaction.set_account(act.account.to_string());
            tempaction.set_action_name(act.name.to_string());
            std::string tempdata;
            tempdata.assign(act.data.begin(), act.data.end());
            tempaction.set_data(tempdata);

            for(const auto& auth : act.authorization) {
               RelayPermission_level* tempauth = tempaction.add_authorization();
               tempauth->set_actor(auth.actor.to_string());
               tempauth->set_permission(auth.permission.to_string());
            }
            relay_actions.push_back(tempaction);
         }
      }
   }

   const auto max_req_times = 3;

   for(auto i = 0; i < max_req_times; i++) {
      try {
         _grpc_stub->put_relay_commit_req(block, relay_actions);
      } catch(fc::exception& e) {
         elog("req block ${num} err by ${w}", ("num", bs->block_num)("w", e.what()));
      } catch(std::exception& e) {
         elog("req block ${num} err by std exp ${w}", ("num", bs->block_num)("w", e.what()));
      }
      ilog("send block req ${num} ${id}", ("num", bs->block_num)("id", bs->id));
      return;
   }

   elog("req block err too much times!");
   // TODO process service err
}

void bus_plugin_impl::process_accepted_block(const chain::block_state_ptr& bs) {
   try {
      //_process_accepted_block( bs );
   } catch(fc::exception& e) {
      elog("FC Exception while processing accepted block trace ${e}", ("e", e.to_string()));
   } catch(std::exception& e) {
      elog("STD Exception while processing accepted block trace ${e}", ("e", e.what()));
   } catch(...) {
      elog("Unknown exception while processing accepted block trace");
   }
}

void bus_plugin_impl::consume_blocks() {
   try {
      //tobe modify
     // insert_default_abi();
      while(true) {
         boost::mutex::scoped_lock lock(mtx);
         while(transaction_metadata_queue.empty() && transaction_trace_queue.empty() && block_state_queue.empty() && irreversible_block_state_queue.empty() && !done) {
            condition.wait(lock);
         }

         // capture for processing
         size_t transaction_metadata_size = transaction_metadata_queue.size();
         if(transaction_metadata_size > 0) {
            transaction_metadata_process_queue = move(transaction_metadata_queue);
            transaction_metadata_queue.clear();
         }
         size_t transaction_trace_size = transaction_trace_queue.size();
         if(transaction_trace_size > 0) {
            transaction_trace_process_queue = move(transaction_trace_queue);
            transaction_trace_queue.clear();
         }
         size_t block_state_size = block_state_queue.size();
         if(block_state_size > 0) {
            block_state_process_queue = move(block_state_queue);
            block_state_queue.clear();
         }
         size_t irreversible_block_size = irreversible_block_state_queue.size();
         if(irreversible_block_size > 0) {
            irreversible_block_state_process_queue = move(irreversible_block_state_queue);
            irreversible_block_state_queue.clear();
         }

         lock.unlock();

         if(done) {
            ilog("draining queue, size: ${q}",
                 ("q", transaction_metadata_size + transaction_trace_size + block_state_size + irreversible_block_size));
         }

         // process applied transactions
         while(!transaction_trace_process_queue.empty()) {
            const auto& t = transaction_trace_process_queue.front();
            process_applied_transaction(t);
            transaction_trace_process_queue.pop_front();
         }

         //process accepted transactions
         while(!transaction_metadata_process_queue.empty()) {
            const auto& t = transaction_metadata_process_queue.front();
            process_accepted_transaction(t);
            transaction_metadata_process_queue.pop_front();
         }

         // process blocks
         while(!block_state_process_queue.empty()) {
            const auto& bs = block_state_process_queue.front();
            process_accepted_block(bs);
            block_state_process_queue.pop_front();
         }

         // process irreversible blocks
         while(!irreversible_block_state_process_queue.empty()) {
            const auto& bs = irreversible_block_state_process_queue.front();
            process_irreversible_block(bs);
            irreversible_block_state_process_queue.pop_front();
         }

         if(transaction_metadata_size == 0 && transaction_trace_size == 0 && block_state_size == 0 && irreversible_block_size == 0 && done) {
            break;
         }
      }
      ilog("grpc_client consume thread shutdown gracefully");
   } catch(fc::exception& e) {
      elog("FC Exception while consuming block ${e}", ("e", e.to_string()));
   } catch(std::exception& e) {
      elog("STD Exception while consuming block ${e}", ("e", e.what()));
   } catch(...) {
      elog("Unknown exception while consuming block");
   }
}

void bus_plugin_impl::purge_abi_cache() {
   if(abi_cache_index.size() < abi_cache_size) return;

   // remove the oldest (smallest) last accessed
   auto& idx = abi_cache_index.get<by_last_access>();
   auto itr = idx.begin();
   if(itr != idx.end()) {
      idx.erase(itr);
   }
}

optional <abi_serializer> bus_plugin_impl::get_abi_serializer(account_name n) {
   if(n.good()) {
      try {

         auto itr = abi_cache_index.find(n);
         if(itr != abi_cache_index.end()) {
            abi_cache_index.modify(itr, [](auto& entry) {
               entry.last_accessed = fc::time_point::now();
            });

            return itr->serializer;
         }

      }
      FC_CAPTURE_AND_LOG((n))
   }
   return optional<abi_serializer>();
}

template<typename T> fc::variant bus_plugin_impl::to_variant_with_abi(const T& obj) {
   fc::variant pretty_output;
   abi_serializer::to_variant(obj, pretty_output, [&](account_name n) { return get_abi_serializer(n); },
                              abi_serializer_max_time);
   return pretty_output;
}

void bus_plugin_impl::insert_default_abi() {
   //eosio.token
   {
      auto abiPath = app().config_dir() / "eosio.token" += ".abi";
      FC_ASSERT(fc::exists(abiPath), "no abi file found ");
      auto abijson = fc::json::from_file(abiPath).as<abi_def>();
      auto abi = fc::raw::pack(abijson);
      abi_def abi_def = fc::raw::unpack<chain::abi_def>(abi);
      // const string json_str = fc::json::to_string( abi_def );
      purge_abi_cache(); // make room if necessary
      abi_cache entry;
      entry.account = N(force.token);
      entry.last_accessed = fc::time_point::now();
      abi_serializer abis;
      abis.set_abi(abi_def, abi_serializer_max_time);
      entry.serializer.emplace(std::move(abis));
      abi_cache_index.insert(entry);
   }

   //eosio
   {
      auto abiPath = app().config_dir() / "System02" += ".abi";
      FC_ASSERT(fc::exists(abiPath), "no abi file found ");
      auto abijson = fc::json::from_file(abiPath).as<abi_def>();
      auto abi = fc::raw::pack(abijson);
      abi_def abi_def = fc::raw::unpack<chain::abi_def>(abi);
      // const string json_str = fc::json::to_string( abi_def );
      purge_abi_cache(); // make room if necessary
      abi_cache entry;
      entry.account = N(eosio);
      entry.last_accessed = fc::time_point::now();
      abi_serializer abis;
      abis.set_abi(abi_def, abi_serializer_max_time);
      entry.serializer.emplace(std::move(abis));
      abi_cache_index.insert(entry);
   }


}

void bus_plugin_impl::init() {
   try {
      _grpc_stub.reset(new grpc_stub(grpc::CreateChannel(client_address, grpc::InsecureChannelCredentials())));
      //_grpc_stub->PutRequest(std::string("init"),std::string("init--json"));
      client_thread = boost::thread([this] { consume_blocks(); });
   } catch(...) {
      elog("grpc_client unknown exception, init failed, line ${line_nun}", ("line_num", __LINE__));
   }
   startup = false;
}


bus_plugin_impl::~bus_plugin_impl() {
   if(!startup) {
      try {
         ilog("grpc shutdown in process please be patient this can take a few minutes");
         done = true;
         condition.notify_one();
         client_thread.join();
      } catch(std::exception& e) {
         elog("Exception on mongo_db_plugin shutdown of consume thread: ${e}", ("e", e.what()));
      }
   }
}
////////////
// bus_plugin
////////////

bus_plugin::bus_plugin() : my(new bus_plugin_impl) {
}

bus_plugin::~bus_plugin() {
}

void bus_plugin::set_program_options(options_description& cli, options_description& cfg) {
   cfg.add_options()("grpc-client-address", bpo::value<std::string>(),
                     "grpc-client-address string.grcp server bind ip and port. Example:127.0.0.1:21005")(
         "grpc-abi-cache-size", bpo::value<uint32_t>()->default_value(2048),
         "The maximum size of the abi cache for serializing data.")("bus_transfer_account", bpo::value<std::string>(),
                                                                    "the account name for Map tokens to the relay chain");
}

void bus_plugin::plugin_initialize(const variables_map& options) {
   try {
      if(options.count("grpc-client-address")) {
         my->client_address = options.at("grpc-client-address").as<std::string>();
         //b_need_start = true;

         if(options.count("grpc-abi-cache-size")) {
            my->abi_cache_size = options.at("grpc-abi-cache-size").as<uint32_t>();
            EOS_ASSERT(my->abi_cache_size > 0, chain::plugin_config_exception, "mongodb-abi-cache-size > 0 required");
         }

         if(options.count("bus_transfer_account")) {
            my->bus_transfer_account = options.at("bus_transfer_account").as<std::string>();
         }
         my->abi_serializer_max_time = app().get_plugin<chain_plugin>().get_abi_serializer_max_time();
         // hook up to signals on controller
         chain_plugin* chain_plug = app().find_plugin<chain_plugin>();
         EOS_ASSERT(chain_plug, chain::missing_chain_plugin_exception, "");
         auto& chain = chain_plug->chain();
         //my->chain_id.emplace( chain.get_chain_id());

         my->accepted_block_connection.emplace(chain.accepted_block.connect([&](const chain::block_state_ptr& bs) {
            my->accepted_block(bs);
         }));
         my->irreversible_block_connection.emplace(
               chain.irreversible_block.connect([&](const chain::block_state_ptr& bs) {
                  my->applied_irreversible_block(bs);
               }));
         my->accepted_transaction_connection.emplace(
               chain.accepted_transaction.connect([&](const chain::transaction_metadata_ptr& t) {
                  my->accepted_transaction(t);
               }));
         my->applied_transaction_connection.emplace(
               chain.applied_transaction.connect([&](const chain::transaction_trace_ptr& t) {
                  my->applied_transaction(t);
               }));

         my->init();
      }


   }
   FC_LOG_AND_RETHROW()
}

void bus_plugin::plugin_startup() {
}

void bus_plugin::plugin_shutdown() {
   my.reset();
}


} // namespace eosio
