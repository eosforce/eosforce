/**
 *  Some config data in chain state db
 *  @copyright defined in eosforce/LICENSE.txt
 */

#include <eosio/chain/config_on_chain.hpp>
#include <chainbase/chainbase.hpp>
#include <eosio/chain/contract_types.hpp>
#include <eosio/chain/apply_context.hpp>

namespace eosio { namespace chain {

// get_num_config_on_chain return -1 if no found
int64_t get_num_config_on_chain( const chainbase::database& db, const name& typ ) {
   const auto cfg_itr = db.find<config_data_object, by_name>(typ);
   if( cfg_itr == nullptr ) {
      //ilog("no found num cfg ${n}", ( "n", typ ));
      return -1;
   }
   return cfg_itr->num;
}

void set_num_config_on_chain( chainbase::database& db, const name& typ, const int64_t num ) {
   if( num == -1 ) {
      EOS_THROW(action_validate_exception,
                "cannot set num cfg -1 for ${n}", ( "n", typ ));
   }

   auto itr = db.find<config_data_object, by_name>(typ);
   if( itr == nullptr ) {
      //ilog("set num config ${t} to ${v}", ( "n", typ )("v", num));
      db.create<config_data_object>([&]( auto& c ) {
         c.typ = typ;
         c.num = num;
      });
   } else {
      db.modify<config_data_object>(*itr, [&]( auto& c ) {
         c.num = num;
      });
   }
}

void set_config_on_chain( chainbase::database& db, const setconfig &cfg ) {
   auto itr = db.find<config_data_object, by_name>(cfg.typ);
   if( itr == nullptr ) {
      ilog("set num config ${n} to ${v}", ( "n", cfg.typ )("v", cfg));
      db.create<config_data_object>([&]( auto& c ) {
         c.typ = cfg.typ;
         c.num = cfg.num;
         c.key = cfg.key;
         c.fee = cfg.fee;
      });
   } else {
      db.modify<config_data_object>(*itr, [&]( auto& c ) {
         c.num = cfg.num;
         c.key = cfg.key;
         c.fee = cfg.fee;
      });
   }
}

bool is_func_has_open( const apply_context& context, const name &func_typ ) {
      const auto head_num = static_cast<int64_t>( context.control.head_block_num() );
      const auto open_num = get_num_config_on_chain( context.control.db(), eosio::chain::name{func_typ} );
      if( open_num < 0 || head_num < 0 ) {
         // no cfg
         return false;
      }

      //idump((head_num)(open_num)(eosio::chain::accout_name{func_typ}));

      return head_num >= open_num;
}


} }  /// eosio::chain