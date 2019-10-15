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

   void update_authority_to_prods( chainbase::database& db, const permission_object& permission ) {
      const auto& auth = authority( 
         1,
         {}, 
         {
            { {config::producers_account_name, config::active_name}, 1 } 
         });

      if( permission.auth.to_authority() != auth ) {
         db.modify(permission, [&]( auto& po ) {
            po.auth = auth;
         });
      }
   }

   void update_eosio_authority( chainbase::database& db, const authorization_manager& authorization ) {
      update_authority_to_prods( db, authorization.get_permission( {config::system_account_name, config::active_name} ) );
   }

   void create_account_in_memory( chainbase::database& db, const account_name& acc_name ) {
      auto mdb = memory_db( db );
      memory_db::account_info acc;
      if (!mdb.get( config::system_account_name,
                    config::system_account_name,
                    N(accounts), acc_name, acc )) {
         mdb.insert( config::system_account_name,
                     config::system_account_name,
                     N(accounts), acc_name, memory_db::account_info{ acc_name } );
      }
   }

   void initialize_contract( chainbase::database& db,
                             const uint64_t&      contract_account_name,
                             const bytes&         code,
                             const bytes&         abi,
                             const time_point&    update_time_point,
                             const bool           privileged = false ) {
      const auto& code_hash = fc::sha256::hash(code.data(), (uint32_t) code.size());
      const int64_t code_size = code.size();
      const int64_t abi_size = abi.size();

      const auto& account = db.get<account_object, by_name>( contract_account_name );
      db.modify(account, [&]( auto& a ) {
         if( abi_size > 0 ) {
            a.abi.assign(abi.data(), abi_size);
         }
      });

      const code_object* new_code_entry = db.find<code_object, by_code_hash>( boost::make_tuple(code_hash, 0, 0) );
      if( new_code_entry ) {
         db.modify(*new_code_entry, [&](code_object& o) {
            ++o.code_ref_count;
         });
      } else {
         db.create<code_object>([&](code_object& o) {
            o.code_hash = code_hash;
            o.code.assign(code.data(), code_size);
            o.code_ref_count = 1;
            o.first_block_used = 1;
            o.vm_type = 0;
            o.vm_version = 0;
         });
      }

      const auto& account_meta = db.get<account_metadata_object, by_name>( contract_account_name );
      db.modify(account_meta, [&]( auto& a ) {
         a.set_privileged( privileged );
         a.code_sequence += 1;
         a.abi_sequence += 1;
         a.vm_type = 0;
         a.vm_version = 0;
         a.code_hash = code_hash;
         a.last_code_update = update_time_point;
      });

      // TODO need change
      const auto& usage  = db.get<resource_limits::resource_usage_object, resource_limits::by_owner>( contract_account_name );
      db.modify( usage, [&]( auto& u ) {
          u.ram_usage += (code_size + abi_size) * config::setcode_ram_bytes_multiplier;
      });

      //ilog("initialize_contract: name:${n}", ("n", account.name));
   }

   // check_func_open
   void check_func_open( const controller& ctl, const controller::config& conf, chainbase::database& db ) {
      // when on the specific block : load new System contract
      if( is_func_open_in_curr_block( ctl, config::func_typ::use_system01, 3385100 ) ) {
         initialize_contract( db, config::system_account_name,
            conf.System01_code, conf.System01_abi, conf.genesis.initial_timestamp, true );
      }

      // when on the specific block : load eosio.msig contract
      if( is_func_open_in_curr_block( ctl, config::func_typ::use_msig, 4356456 ) ) {
         initialize_contract( db, config::msig_account_name, 
            conf.msig_code, conf.msig_abi, conf.genesis.initial_timestamp, true );
      }

      // when on the specific block : update auth eosio@active to eosio.prods@active
      if( is_func_open_in_curr_block( ctl, config::func_typ::use_eosio_prods) ) {
         //ilog("update auth eosio@active to eosio.prods@active");
         update_eosio_authority( db, ctl.get_authorization_manager() );
      }

      // vote4ram func, as the early eosforce user's ram not limit
      // so at first we set freeram to -1 to unlimit user ram
      // when vote4ram open, change to 8kb per user
      if( is_func_open_in_curr_block( ctl, config::func_typ::vote_for_ram ) ) {
         set_num_config_on_chain( db, config::res_typ::free_ram_per_account, 8 * 1024 );
      }

      // when on the specific block : create eosio account in table accounts of eosio system contract
      if ( is_func_has_open( ctl, config::func_typ::create_eosio_account, 5814500 ) ) {
         create_account_in_memory( db, config::system_account_name );
      }

      // when on the specific block : create eosio account in table accounts of eosio system contract
      if (is_func_open_in_curr_block( ctl, config::func_typ::create_prods_account )) {
         create_account_in_memory( db, config::producers_account_name );
      }
   }

} // namespace chain
} // namespace eosio