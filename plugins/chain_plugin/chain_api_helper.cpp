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

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include <fc/io/fstream.hpp>
#include <fc/io/json.hpp>
#include <fc/variant.hpp>

using namespace eosio;
using namespace eosio::chain;
using namespace eosio::chain::config;
using namespace eosio::chain::plugin_interface;
using vm_type = wasm_interface::vm_type;
using fc::flat_map;

namespace eosio {
namespace chain_apis {

std::vector<fc::variant> read_only::get_table_rows_by_primary_to_json( const name& code,
                                                                       const uint64_t& scope,
                                                                       const name& table,
                                                                       const abi_serializer& abis,
                                                                       const std::size_t max_size ) const {
   std::vector<fc::variant> result;

   const auto& d = db.db();

   const auto* t_id = d.find<chain::table_id_object, chain::by_code_scope_table>(boost::make_tuple(code, scope, table));
   if( t_id != nullptr ) {
      const auto &idx = d.get_index<key_value_index, by_scope_primary>();
      result.reserve(max_size);

      decltype(t_id->id) next_tid(t_id->id._id + 1);
      const auto lower = idx.lower_bound(boost::make_tuple(t_id->id));
      const auto upper = idx.lower_bound(boost::make_tuple(next_tid));

      std::size_t added = 0;
      vector<char> data;
      data.reserve(4096);
      for( auto itr = lower; itr != upper; ++itr ) {
         if(added >= max_size) {
            break;
         }
         copy_inline_row(*itr, data);
         result.push_back(abis.binary_to_variant(abis.get_table_type(table), data, abi_serializer_max_time, shorten_abi_errors));
         added++;
      }
   }

   return result;
}


} // namespace chain_apis
} // namespace eosio