
/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once

#include <eosio/chain/chain_config.hpp>
#include <eosio/chain/types.hpp>
#include <eosio/chain/asset.hpp>

#include <fc/crypto/sha256.hpp>

#include <string>
#include <vector>

namespace eosio { namespace chain {

struct account_tuple {
    public_key_type key;
    asset           asset;
    account_name    name = N(a);
};

struct producer_tuple {
    account_name name;
    public_key_type bpkey;
    uint32_t commission_rate;
    std::string url;
};

struct genesis_state {
   genesis_state();

   static const string eosio_root_key;

   chain_config   initial_configuration = {
      .max_block_net_usage                  = config::default_max_block_net_usage,
      .target_block_net_usage_pct           = config::default_target_block_net_usage_pct,
      .max_transaction_net_usage            = config::default_max_transaction_net_usage,
      .base_per_transaction_net_usage       = config::default_base_per_transaction_net_usage,
      .net_usage_leeway                     = config::default_net_usage_leeway,
      .context_free_discount_net_usage_num  = config::default_context_free_discount_net_usage_num,
      .context_free_discount_net_usage_den  = config::default_context_free_discount_net_usage_den,

      .max_block_cpu_usage                  = config::default_max_block_cpu_usage,
      .target_block_cpu_usage_pct           = config::default_target_block_cpu_usage_pct,
      .max_transaction_cpu_usage            = config::default_max_transaction_cpu_usage,
      .min_transaction_cpu_usage            = config::default_min_transaction_cpu_usage,

      .max_transaction_lifetime             = config::default_max_trx_lifetime,
      .deferred_trx_expiration_window       = config::default_deferred_trx_expiration_window,
      .max_transaction_delay                = config::default_max_trx_delay,
      .max_inline_action_size               = config::default_max_inline_action_size,
      .max_inline_action_depth              = config::default_max_inline_action_depth,
      .max_authority_depth                  = config::default_max_auth_depth,
   };

   time_point                               initial_timestamp;
   public_key_type                          initial_key;
   bytes                                    code;
   bytes                                    abi;
   bytes                                    token_code;
   bytes                                    token_abi;

   /**
    * Get the chain_id corresponding to this genesis state.
    *
    * This is the SHA256 serialization of the genesis_state.
    */
   chain_id_type compute_chain_id() const;
   std::vector<account_tuple>                                 initial_account_list;
   std::vector<producer_tuple>                                initial_producer_list;
};

} } // namespace eosio::chain


FC_REFLECT(eosio::chain::account_tuple, (key)(asset)(name))
FC_REFLECT(eosio::chain::producer_tuple, (name)(bpkey)(commission_rate)(url))
FC_REFLECT(eosio::chain::genesis_state,
           (initial_timestamp)(initial_key)(code)(abi)(token_code)(token_abi)
           (initial_configuration)(initial_account_list)(initial_producer_list))
