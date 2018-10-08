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

        //contract add set code tmp imp
        fee_map[N(setabi)]  = asset(50 * 10000); // 50 EOS
        fee_map[N(setfee)]  = asset(1000);       // 0.1 EOS
        fee_map[N(setcode)] = asset(50 * 10000); // 50 EOS

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

   asset txfee_manager::get_required_fee( const chainbase::database& db, const transaction& trx)const
   {
      auto fee = asset(0);

      for (const auto& act : trx.actions ) {
         auto it = fee_map.find(act.name);
         if(it != fee_map.end()) {
            // fee in fee_map for system contract
            fee += it->second;
            continue;
         }

         // try to found fee for action in db
         try {
            auto key = boost::make_tuple(act.account, act.name);
            const auto &fee_in_db = db.get<action_fee_object, by_action_name>(key);

            ilog("get fee from db ${fee} ${act} ${msg}",
                  ("fee", fee_in_db.fee)("act", fee_in_db.account)("msg", fee_in_db.message_type));

            fee += fee_in_db.fee;
            continue;
         } catch (fc::exception &exp){
            elog("catch exp ${e}", ("e", exp.what()));
         } catch (...){
         }

         // no fee found throw err
         EOS_ASSERT(false, action_validate_exception,
               "action ${acc} ${act} name not include in feemap or db", ("acc", act.account)("act", act.name));
      }

      return fee;
   }

} } /// namespace eosio::chain
