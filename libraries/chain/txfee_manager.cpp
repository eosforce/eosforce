/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#include <eosio/chain/exceptions.hpp>
#include <eosio/chain/txfee_manager.hpp>
#include <eosio/chain/controller.hpp>

namespace eosio { namespace chain {

   txfee_manager::txfee_manager(){
      const auto eosio_acc = N(eosio);
      const auto token_acc = N(eosio.token);

      init_native_fee(eosio_acc, N(newaccount), asset(1000));
      init_native_fee(eosio_acc, N(updateauth), asset(1000));
      init_native_fee(eosio_acc, N(deleteauth), asset(1000));

      init_native_fee(eosio_acc, N(transfer),     asset(100));
      init_native_fee(eosio_acc, N(vote),         asset(500));
      init_native_fee(eosio_acc, N(unfreeze),     asset(100));
      init_native_fee(eosio_acc, N(claim),        asset(300));
      init_native_fee(eosio_acc, N(updatebp),     asset(100*10000));
      init_native_fee(eosio_acc, N(setemergency), asset(10*10000));

      init_native_fee(token_acc, N(transfer), asset(100));
      init_native_fee(token_acc, N(issue),    asset(100));
      init_native_fee(token_acc, N(create),   asset(10*10000));

      init_native_fee(eosio_acc, N(setabi),  asset(1000));
      init_native_fee(eosio_acc, N(setfee),  asset(1000));
      init_native_fee(eosio_acc, N(setcode), asset(1000));

      //fee_map[N(setabi)]  = asset(50 * 10000); // 50 EOS
      //fee_map[N(setfee)]  = asset(1000);       // 0.1 EOS
      //fee_map[N(setcode)] = asset(50 * 10000); // 50 EOS

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

   asset txfee_manager::get_required_fee( const controller& ctl, const transaction& trx)const
   {
      const auto &db = ctl.db();
      auto fee = asset(0);
      const auto block_num = ctl.head_block_num();

      for (const auto& act : trx.actions ) {
         // keep consensus for test net
         // Just for test net, will delete before to main net
         if (act.account == N(diceonlineon)) {
            {
               const auto native_fee = get_native_fee(block_num, N(eosio), act.name);
               if (native_fee != asset(0)) {
                  fee += native_fee;
                  continue;
               }
            }

            {
               const auto native_fee = get_native_fee(block_num, N(eosio.token), act.name);
               if (native_fee != asset(0)) {
                  fee += native_fee;
                  continue;
               }
            }
         }

         // keep consensus for main net, some action in main net exec action
         // like newaccount in diff account
         {
            if ((act.name == N(newaccount)) &&
                ((act.account == N(eosio.bios))
                 || (act.account == N(eosio.token))
                )) {
               const auto native_fee = get_native_fee(block_num, N(eosio), act.name);
               if (native_fee != asset(0)) {
                  fee += native_fee;
                  continue;
               }
            }

            if ((act.name == N(transfer)) &&
                (   (act.account == N(victor))
                 || (act.account == N(eosvictor))
                )) {
               const auto native_fee = get_native_fee(block_num, N(eosio), act.name);
               if (native_fee != asset(0)) {
                  fee += native_fee;
                  continue;
               }
            }
         }

         // first check if changed fee
         try{
            const auto fee_in_db = db.find<action_fee_object, by_action_name>(
                  boost::make_tuple(act.account, act.name));
            if(    ( fee_in_db != nullptr )
                && ( fee_in_db->fee != asset(0) ) ){
               fee += fee_in_db->fee;
               continue;
            }
         } catch (fc::exception &exp){
            elog("catch exp ${e}", ("e", exp.what()));
         } catch (...){
            elog("catch unknown exp in get_required_fee");
         }

         const auto native_fee = get_native_fee(block_num, act.account, act.name);
         if (native_fee != asset(0)) {
            fee += native_fee;
            continue;
         }

         // no fee found throw err
         EOS_ASSERT(false, action_validate_exception,
               "action ${acc} ${act} name not include in feemap or db",
               ("acc", act.account)("act", act.name));
      }

      return fee;
   }

} } /// namespace eosio::chain
