#include <eosio/chain_plugin/chain_plugin.hpp>
#include <eosio/chain/fork_database.hpp>
#include <eosio/chain/block_log.hpp>
#include <eosio/chain/exceptions.hpp>
#include <eosio/chain/authorization_manager.hpp>
#include <eosio/chain/txfee_manager.hpp>
#include <eosio/chain/producer_object.hpp>
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

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include <fc/io/fstream.hpp>
#include <fc/io/json.hpp>
#include <fc/variant.hpp>

namespace eosio {
namespace chain_apis {

// get_vote_rewards get voter 's reward by vote in eosforce
read_only::get_vote_rewards_result read_only::get_vote_rewards( const read_only::get_vote_rewards_params& p )const {
   read_only::get_vote_rewards_result result;

   ilog( "get_vote_rewards ${acc} from ${bp} in ${num}, ${json}", 
         ("acc", p.voter)("bp", p.bp_name)("json", p.json)("num", p.block_num) );

   return result;
}

} // namespace chain_apis
} // namespace eosio