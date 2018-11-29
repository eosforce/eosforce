/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#include <eosio/chain/exceptions.hpp>
#include <eosio/chain/txfee_manager.hpp>
#include <eosio/chain/controller.hpp>

namespace eosio { namespace chain {

   txfee_manager::txfee_manager(){
      init_native_fee(config::system_account_name, N(newaccount), asset(1000));
      init_native_fee(config::system_account_name, N(updateauth), asset(1000));
      init_native_fee(config::system_account_name, N(deleteauth), asset(1000));

      init_native_fee(config::system_account_name, N(transfer),     asset(100));
      init_native_fee(config::system_account_name, N(vote),         asset(500));
      init_native_fee(config::system_account_name, N(unfreeze),     asset(100));
      init_native_fee(config::system_account_name, N(vote4ram),     asset(500));
      init_native_fee(config::system_account_name, N(unfreezeram),  asset(100));
      init_native_fee(config::system_account_name, N(claim),        asset(300));
      init_native_fee(config::system_account_name, N(updatebp),     asset(100*10000));
      init_native_fee(config::system_account_name, N(setemergency), asset(10*10000));

      init_native_fee(config::token_account_name, N(transfer), asset(100));
      init_native_fee(config::token_account_name, N(issue),    asset(100));
      init_native_fee(config::token_account_name, N(create),   asset(10*10000));

      init_native_fee(config::system_account_name, N(setabi),  asset(1000));
      init_native_fee(config::system_account_name, N(setfee),  asset(1000));
      init_native_fee(config::system_account_name, N(setcode), asset(1000));

      init_native_fee(config::system_account_name, N(setconifg), asset(100));

      init_native_fee(config::system_account_name, N(canceldelay), asset(5000));
      init_native_fee(config::system_account_name, N(linkauth),    asset(5000));
      init_native_fee(config::system_account_name, N(unlinkauth),  asset(5000));

      init_native_fee(config::msig_account_name, N(propose),   asset(15000));
      init_native_fee(config::msig_account_name, N(approve),   asset(10000));
      init_native_fee(config::msig_account_name, N(unapprove), asset(10000));
      init_native_fee(config::msig_account_name, N(cancel),    asset(10000));
      init_native_fee(config::msig_account_name, N(exec),      asset(10000));
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

   /*
    * About Fee
    *
    * fee come from three mode:
    *  - native, set in cpp
    *  - set by eosio
    *  - set by user
    *
    * and res limit can set a value or zero,
    * all of this will make diff mode to calc res limit,
    * support 1.0 EOS is for `C` cpu, `N` net and `R` ram,
    * and cost fee by `f` EOS and extfee by `F` EOS
    *
    * then can give:
    *  - native and no setfee : use native fee and unlimit res use
    *  - eosio set fee {f, (c,n,r)}
    *      (cpu_limit, net_limit, ram_limit) == (c + F*C, n + F*N, r + F*R)
    *  - eosio set fee {f, (0,0,0)}
    *      (cpu_limit, net_limit, ram_limit) == ((f+F)*C, (f+F)*N, (f+F)*R)
    *  - user set fee {f, (0,0,0)}, user cannot set fee by c>0||n>0||r>0
    *      (cpu_limit, net_limit, ram_limit) == ((f+F)*C, (f+F)*N, (f+F)*R)
    *
    *  so it can be check by:
    *  if no setfee
    *       if no native -> err
    *       if native -> use native and unlimit res use
    *  else
    *       if res limit is (0,0,0) -> limit res by ((f+F)*C, (f+F)*N, (f+F)*R)
    *       if res limit is (c,n,r) -> (c + F*C, n + F*N, r + F*R)
    *
    *  at the same time, eosio can set res limit > (0,0,0) and user cannot
    *
    */


   asset txfee_manager::get_required_fee( const controller& ctl, const transaction& trx)const
   {
      auto fee = asset(0);

      for (const auto& act : trx.actions ) {
         fee += get_required_fee(ctl, act);
      }

      return fee;
   }

   asset txfee_manager::get_required_fee( const controller& ctl, const action& act)const{
      const auto &db = ctl.db();
      const auto block_num = ctl.head_block_num();

      // keep consensus for main net, some action in main net exec action
      // like newaccount in diff account
      {
         if ((act.name == N(newaccount)) &&
             ((act.account == N(eosio.bios))
              || (act.account == N(eosio.token))
             )) {
            const auto native_fee = get_native_fee(block_num, config::system_account_name, act.name);
            if (native_fee != asset(0)) {
               return native_fee;
            }
         }

         if ((act.name == N(transfer)) &&
             (   (act.account == N(victor))
                 || (act.account == N(eosvictor))
             )) {
            const auto native_fee = get_native_fee(block_num, config::system_account_name, act.name);
            if (native_fee != asset(0)) {
               return native_fee;
            }
         }
      }

      // first check if changed fee
      try{
         const auto fee_in_db = db.find<action_fee_object, by_action_name>(
               boost::make_tuple(act.account, act.name));
         if(    ( fee_in_db != nullptr )
                && ( fee_in_db->fee != asset(0) ) ){
            return fee_in_db->fee;
         }
      } catch (fc::exception &exp){
         elog("catch exp ${e}", ("e", exp.what()));
      } catch (...){
         elog("catch unknown exp in get_required_fee");
      }

      const auto native_fee = get_native_fee(block_num, act.account, act.name);
      if (native_fee != asset(0)) {
         return native_fee;
      }

      // no fee found throw err
      EOS_ASSERT(false, action_validate_exception,
                 "action ${acc} ${act} name not include in feemap or db",
                 ("acc", act.account)("act", act.name));
   }

} } /// namespace eosio::chain
