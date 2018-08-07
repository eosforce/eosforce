/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#include <eosio/chain/exceptions.hpp>
#include <eosio/chain/txfee_manager.hpp>

namespace eosio { namespace chain {

   txfee_manager::txfee_manager(){
         //eosio.bios
        fee_map[N(newaccount)]      = asset(1000);
        fee_map[N(updateauth)]      = asset(1000);
        fee_map[N(deleteauth)]      = asset(1000);

        //System
        fee_map[N(transfer)]        = asset(100);
        fee_map[N(vote)]            = asset(500);
        fee_map[N(unfreeze)]        = asset(100);
        fee_map[N(claim)]           = asset(300);
        fee_map[N(updatebp)]        = asset(100*10000);
        fee_map[N(setemergency)]    = asset(10*10000);

        //eosio.token
        fee_map[N(issue)]           = asset(100);
        fee_map[N(create)]          = asset(10*10000);

        //eosio.msig
        // fee_map[N(propose)]         = asset(1000);
        // fee_map[N(approve)]         = asset(1000);
        // fee_map[N(unapprove)]       = asset(1000);
        // fee_map[N(cancel)]          = asset(1000);
        // fee_map[N(exec)]            = asset(1000);
   }

   bool txfee_manager::check_transaction( const transaction& trx)const
   {
      for( const auto& act : trx.actions ) {
          for (const auto & perm : act.authorization) {
            if (perm.actor != trx.actions[0].authorization[0].actor) {
                return false;
            }
          }
      }
      return true;
   }

   asset txfee_manager::get_required_fee( const transaction& trx)const
   {
      auto fee = asset(0);
      for (const auto& act : trx.actions ) {
        auto it = fee_map.find(act.name);
        EOS_ASSERT(it != fee_map.end(), action_validate_exception, "action name not include in feemap");
        fee += it->second;
      }

      return fee;
   }

} } /// namespace eosio::chain
