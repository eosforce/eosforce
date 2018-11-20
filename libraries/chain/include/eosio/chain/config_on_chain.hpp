/**
 *  Some config data in chain state db
 *  @copyright defined in eosforce/LICENSE.txt
 */

#ifndef EOSIO_CONFIG_ON_CHAIN_HPP
#define EOSIO_CONFIG_ON_CHAIN_HPP

#pragma once

#include <eosio/chain/types.hpp>
#include <eosio/chain/asset.hpp>
#include <eosio/chain/multi_index_includes.hpp>

namespace eosio { namespace chain {


// some spec type for fast
class config_data_object : public chainbase::object<config_data_object_type, config_data_object> {
   OBJECT_CTOR(config_data_object);

   id_type id;
   name typ;
   int64_t num = 0;
   name key; // TBD By FanYang will use for cfg future
   asset fee; // TBD By FanYang will use for cfg future
};

struct by_name;
using config_data_object_index = chainbase::shared_multi_index_container<
      config_data_object,
      indexed_by<
            ordered_unique< tag<by_id>,
                  BOOST_MULTI_INDEX_MEMBER(config_data_object, config_data_object::id_type, id)
            >,
            ordered_unique< tag<by_name>,
                  BOOST_MULTI_INDEX_MEMBER(config_data_object, name, typ)
            >
      >
>;

// get_num_config_on_chain return -1 if no found
int64_t get_num_config_on_chain( const chainbase::database& db, const name& typ );

// set_num_config_on_chain if is -1 err
void set_num_config_on_chain( chainbase::database& db, const name& typ, const int64_t num );

} } /// namespace eosio::chain

FC_REFLECT(eosio::chain::config_data_object, (id)(typ)(num)(key)(fee))
CHAINBASE_SET_INDEX_TYPE(eosio::chain::config_data_object, eosio::chain::config_data_object_index)


#endif //EOSIO_CONFIG_ON_CHAIN_HPP
