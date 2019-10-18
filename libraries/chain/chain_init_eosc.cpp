#include <eosio/chain/controller.hpp>
#include <eosio/chain/transaction_context.hpp>

#include <eosio/chain/exceptions.hpp>

#include <eosio/chain/account_object.hpp>
#include <eosio/chain/memory_db.hpp>
#include <eosio/chain/code_object.hpp>
#include <eosio/chain/block_summary_object.hpp>
#include <eosio/chain/eosio_contract.hpp>
#include <eosio/chain/global_property_object.hpp>
#include <eosio/chain/protocol_state_object.hpp>
#include <eosio/chain/contract_table_objects.hpp>
#include <eosio/chain/generated_transaction_object.hpp>
#include <eosio/chain/transaction_object.hpp>
#include <eosio/chain/reversible_block_object.hpp>

#include <eosio/chain/resource_limits.hpp>
#include <eosio/chain/resource_limits_private.hpp>

#include <eosio/chain/authorization_manager.hpp>
#include <eosio/chain/txfee_manager.hpp>
#include <eosio/chain/config_on_chain.hpp>
#include <eosio/chain/config.hpp>
#include <eosio/chain/chain_snapshot.hpp>
#include <eosio/chain/thread_utils.hpp>

#include <chainbase/chainbase.hpp>
#include <fc/io/json.hpp>
#include <fc/variant_object.hpp>

#include <eosio/chain/eosio_contract.hpp>
#include <set>

namespace eosio {
namespace chain {

   void initialize_contract( chainbase::database& db,
                             const uint64_t&      contract_account_name,
                             const bytes&         code,
                             const bytes&         abi,
                             const time_point&    update_time_point,
                             const bool           privileged );

   // initialize_producer init bios bps in initial_producer_list
   void initialize_producer( chainbase::database& db, const controller::config& conf ) {
      auto mdb = memory_db( db );
      for( const auto& producer : conf.genesis.initial_producer_list ) {
         // store bp data in bp table
         mdb.insert( config::system_account_name, config::system_account_name, N(bps),
                     producer.name,
                     memory_db::bp_info {
                           producer.name,
                           producer.bpkey,
                           producer.commission_rate,
                           producer.url
                     } );
      }
   }

   // initialize_chain_emergency init chain emergency stat
   void initialize_chain_emergency( chainbase::database& db ) {
      memory_db( db ).insert(
            config::system_account_name, config::system_account_name, N(chainstatus),
            config::system_account_name,
            memory_db::chain_status{N(chainstatus), false});
   }

   // initialize_account init account from genesis;
   // inactive account freeze(lock) asset by inactive_freeze_percent;
   void initialize_account( controller& ctl, const controller::config& conf, chainbase::database& db ) {
      std::set<account_name> active_acc_set;
      for( const auto &account : conf.active_initial_account_list ) {
         active_acc_set.insert( account.name );
      }

      const auto acc_name_a = N(a);
      auto mdb = memory_db( db );
      for( const auto &account : conf.genesis.initial_account_list ) {
         const auto &public_key = account.key;
         auto acc_name = account.name;
         if (acc_name == acc_name_a) {
            const auto pk_str = std::string(public_key);
            const auto name_r = pk_str.substr(pk_str.size() - 12, 12);
            acc_name = string_to_name(format_name(name_r).c_str());
         }

         // init asset
         eosio::chain::asset amount;
         if( active_acc_set.find(account.name) == active_acc_set.end() ) {
            //issue eoslock token to this account
            uint64_t eoslock_amount = account.asset.get_amount() * conf.inactive_freeze_percent / 100;
            mdb.insert( config::eoslock_account_name,
                        config::eoslock_account_name,
                        N(accounts), acc_name,
                        memory_db::eoslock_account{
                           acc_name,
                           eosio::chain::asset(eoslock_amount, symbol(4, "EOSLOCK"))});

            //inactive account freeze(lock) asset
            amount = account.asset - eosio::chain::asset(eoslock_amount);
         } else {
            //active account
            amount = account.asset;
         }

         // initialize_account_to_table
         mdb.insert(
                 config::system_account_name, config::system_account_name, N(accounts), acc_name,
                 memory_db::account_info{acc_name, amount});

         //ilog("gen genesis account info ${acc}, ${ass}", ("acc", acc_name)("ass", amount));

         const authority auth(public_key);
         ctl.create_native_account(acc_name, auth, auth, false);
      }
   }

   // initialize_eos_stats init stats for eos token
   void initialize_eos_stats( chainbase::database& db ) {
      const auto& sym = symbol(CORE_SYMBOL).to_symbol_code();
      memory_db( db ).insert( config::token_account_name, sym, N(stat),
                              config::token_account_name,
                              memory_db::currency_stats{
                                   asset(10000000),
                                   asset(100000000000),
                                   config::token_account_name} );
   }

   void initialize_eosc_chain( controller& ctl, const controller::config& conf, chainbase::database& db ) {
      authority system_auth( conf.genesis.initial_key );

      ctl.create_native_account( config::token_account_name, system_auth, system_auth, false );
      ctl.create_native_account( config::eoslock_account_name, system_auth, system_auth, false );

      initialize_contract( db, config::system_account_name, conf.System_code, conf.System_abi, conf.genesis.initial_timestamp, true );
      initialize_contract( db, config::token_account_name, conf.token_code, conf.token_abi, conf.genesis.initial_timestamp, false );
      initialize_eos_stats( db );
      initialize_contract( db, config::eoslock_account_name, conf.lock_code, conf.lock_abi, conf.genesis.initial_timestamp, false );

      initialize_account( ctl, conf, db );
      initialize_producer( db, conf );
      initialize_chain_emergency( db );

      // vote4ram func, as the early eosforce user's ram not limit
      // so at first we set freeram to -1 to unlimit user ram
      set_num_config_on_chain( db, config::res_typ::free_ram_per_account, -1 );
   }

   // format_name format name from genesis
   const std::string format_name( const std::string& name ) {
      std::stringstream ss;
      for( int i = 0; i < 12; i++ ) {
         const auto n = name[i];
         if( n >= 'A' && n <= 'Z' ) {
            ss << static_cast<char>( n + 32 );
         } else if(( n >= 'a' && n <= 'z' ) || ( n >= '1' && n <= '5' )) {
            ss << static_cast<char>( n );
         } else if( n >= '6' && n <= '9' ) {
            ss << static_cast<char>( n - 5 );
         } else {
            // unknown char no process
         }
      }

      const auto res = ss.str();

      if( res.size() < 12 ) {
         EOS_ASSERT(false, name_type_exception, "initialize format name failed");
      }
      return res;
   }

} // namespace chain
} // namespace eosio