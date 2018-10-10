/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once

#include <eosio/chain/types.hpp>
#include <eosio/chain/asset.hpp>

#include "multi_index_includes.hpp"

namespace eosio { namespace chain {

   class controller;
   struct transaction;

   class txfee_manager {
      public:

         explicit txfee_manager();

         bool check_transaction( const transaction& trx)const;

         asset get_required_fee( const controller& ctl, const transaction& trx)const;


      private:

        inline void init_native_fee(const account_name &acc, const action_name &act, const asset &fee) {
           fee_map[std::make_pair(acc, act)] = fee;
        }

        inline asset get_native_fee(const uint32_t block_num, const account_name &acc, const action_name &act) const {
           const auto itr = fee_map.find(std::make_pair(acc, act));
           if( itr != fee_map.end() ){
              return itr->second;
           }

           // no find
           return asset(0);
        }

        std::map<std::pair<account_name, action_name>, asset> fee_map;
   };


   class fee_paramter {
     public:
       account_name name;
       asset fee;
       account_name producer;
       fee_paramter(account_name name, asset fee, account_name producer) : name(name), fee(fee), producer(producer) {};
   };

   // action fee info in db, for action exec by user def code
   class action_fee_object : public chainbase::object<action_fee_object_type, action_fee_object> {
      OBJECT_CTOR(action_fee_object);

      id_type      id;
      account_name account;
      action_name  message_type;
      asset        fee;

      uint32_t cpu_limit = 0;
      uint32_t net_limit = 0;
      uint32_t ram_limit = 0;
   };

   struct by_action_name;
   struct by_contract_account;
   using action_fee_object_index = chainbase::shared_multi_index_container<
            action_fee_object,
            indexed_by<
                  ordered_unique<tag<by_id>,
                        BOOST_MULTI_INDEX_MEMBER(action_fee_object, action_fee_object::id_type, id)
                  >,
                  ordered_unique< tag<by_action_name>,
                        composite_key<action_fee_object,
                              BOOST_MULTI_INDEX_MEMBER(action_fee_object, account_name, account),
                              BOOST_MULTI_INDEX_MEMBER(action_fee_object, action_name, message_type)
                        >
                  >,
                  ordered_non_unique< tag<by_contract_account>,
                        BOOST_MULTI_INDEX_MEMBER(action_fee_object, account_name, account)
                  >
            >
      >;

} } /// namespace eosio::chain

FC_REFLECT(eosio::chain::fee_paramter, (name)(fee)(producer))
FC_REFLECT(eosio::chain::action_fee_object, (id)(account)(message_type)(fee))

CHAINBASE_SET_INDEX_TYPE(eosio::chain::action_fee_object, eosio::chain::action_fee_object_index)
