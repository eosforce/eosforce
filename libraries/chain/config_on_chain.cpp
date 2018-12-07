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
int64_t get_num_config_on_chain( const chainbase::database& db, const name& typ, const int64_t default_value/* = -1*/ ) {
   const auto cfg_itr = db.find<config_data_object, by_name>(typ);
   if( cfg_itr == nullptr ) {
      //ilog("no found num cfg ${n}", ( "n", typ ));
      return default_value;
   }
   return cfg_itr->num;
}

void set_num_config_on_chain( chainbase::database& db, const name& typ, const int64_t num ) {
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

bool is_func_has_open( const controller& ctl, const name &func_typ ) {
      const auto head_num = static_cast<int64_t>( ctl.head_block_num() );
      const auto open_num = get_num_config_on_chain( ctl.db(), func_typ );
      if( open_num < 0 || head_num < 0 ) {
         // no cfg
         return false;
      }

      return head_num >= open_num;
}

// is_func_open_in_curr_block if a func is open in curr block
bool is_func_open_in_curr_block( const controller& ctl, const name &func_typ, const int64_t default_open_block /* =0 */  ) {
   const auto head_num = static_cast<int64_t>( ctl.head_block_num() );
   const auto open_num = get_num_config_on_chain( ctl.db(), func_typ );
   if( open_num < 0 ) {
      // no cfg
      return (default_open_block > 0) && (head_num == default_open_block); // if head_num < 0 , default_open_block != head_num
   }
   if( head_num < 0 ){
      return false;
   }
   return head_num == open_num;
}

} }  /// eosio::chain