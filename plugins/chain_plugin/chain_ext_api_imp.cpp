#include <eosio/chain_plugin/chain_plugin.hpp>
#include <eosio/chain/fork_database.hpp>
#include <eosio/chain/block_log.hpp>
#include <eosio/chain/exceptions.hpp>
#include <eosio/chain/authorization_manager.hpp>
#include <eosio/chain/txfee_manager.hpp>
#include <eosio/chain/config.hpp>
#include <eosio/chain/wasm_interface.hpp>
#include <eosio/chain/resource_limits.hpp>
#include <eosio/chain/reversible_block_object.hpp>
#include <eosio/chain/controller.hpp>
#include <eosio/chain/generated_transaction_object.hpp>
#include <eosio/chain/snapshot.hpp>

#include <eosio/chain/eosio_contract.hpp>
#include <eosio/chain/config_on_chain.hpp>
#include <eosio/chain/memory_db.hpp>
#include <eosio/chain/types.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include <fc/io/fstream.hpp>
#include <fc/io/json.hpp>
#include <fc/variant.hpp>

using eosio::chain::int128_t;

namespace eosio {
namespace chain_apis {

const auto sys_account = chain::config::system_account_name;

// get_vote_rewards get voter 's reward by vote in eosforce
read_only::get_vote_rewards_result read_only::get_vote_rewards( const read_only::get_vote_rewards_params& p )const {
   //ilog( "get_vote_rewards ${acc} from ${bp}", ("acc", p.voter)("bp", p.bp_name));

   const auto curr_block_num = db.head_block_num();

   // 1. Need BP total voteage and reward_pool info
   chain::memory_db::bp_info bp_data;
   EOS_ASSERT( get_table_row_by_primary_key( sys_account, sys_account, N(bps), p.bp_name, bp_data ), 
               chain::contract_table_query_exception,
               "cannot find bp info by name ${n}", ("n", p.bp_name) );

   //ilog( "bp data: ${data}", ("data", bp_data) );

   const auto bp_total_assetage = 
        ( static_cast<int128_t>(bp_data.total_staked)
          * static_cast<int128_t>(curr_block_num - bp_data.voteage_update_height) )
      + bp_data.total_voteage;

   //ilog( "bp data: ${data} on ${n}", ("data", bp_total_assetage)("n", curr_block_num) );

   uint64_t voter_total_assetage = 0;

   // 2. Need calc voter current vote voteage
   chain::memory_db::vote_info curr_vote_data;
   const auto is_has_curr_vote = get_table_row_by_primary_key(
         sys_account, p.voter, N(votes), p.bp_name, curr_vote_data );
   if( is_has_curr_vote ) {
      //ilog( "get current vote data : ${d}", ("d", curr_vote_data) );
      const auto curr_vote_assetage = 
         (   static_cast<int128_t>(curr_vote_data.staked.get_amount() / curr_vote_data.staked.precision()) 
           * static_cast<int128_t>(curr_block_num - curr_vote_data.voteage_update_height) )
         + curr_vote_data.voteage;
      
      //ilog( "get current vote assetage : ${d}", ("d", curr_vote_assetage) );
      voter_total_assetage += curr_vote_assetage;
   }

   // 3. Need calc the sum of voter 's fix-time vote voteage
   walk_table_by_seckey<chain::memory_db::votefix_info>( 
      sys_account, p.voter, N(fixvotes), p.bp_name, 
      [&]( unsigned int c, const chain::memory_db::votefix_info& v ) -> bool{
         //ilog("walk fix ${n} : ${data}", ("n", c)("data", v));
         const auto fix_votepower_age =
            (   static_cast<int128_t>(v.votepower_age.staked.get_amount() / v.votepower_age.staked.precision())
              * static_cast<int128_t>(curr_block_num - v.votepower_age.update_height) )
            + v.votepower_age.age;
         voter_total_assetage += fix_votepower_age;
         return false; // no break
   } );

   // 4. Make reward to result
   const auto amount_voteage = static_cast<int128_t>( bp_data.rewards_pool.get_amount() ) 
                             * voter_total_assetage;
   const auto& reward = asset{
      bp_total_assetage > 0
            ? static_cast<int64_t>( amount_voteage / bp_total_assetage )
            : 0
   };

   return {
      reward,
      voter_total_assetage,
      curr_block_num,
      {}
   };
}

} // namespace chain_apis
} // namespace eosio