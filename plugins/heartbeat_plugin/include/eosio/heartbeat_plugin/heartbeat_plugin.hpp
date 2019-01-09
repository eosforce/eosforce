/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#pragma once

#include <eosio/chain_plugin/chain_plugin.hpp>
#include <eosio/http_client_plugin/http_client_plugin.hpp>

#include <appbase/application.hpp>

namespace eosio {

using boost::signals2::signal;

class heartbeat_plugin : public appbase::plugin<heartbeat_plugin> {
public:
   APPBASE_PLUGIN_REQUIRES((chain_plugin)(http_client_plugin))


   heartbeat_plugin();
   virtual ~heartbeat_plugin();

   virtual void set_program_options(
      boost::program_options::options_description &command_line_options,
      boost::program_options::options_description &config_file_options
      ) override;
   virtual void plugin_initialize(const boost::program_options::variables_map& options);
   virtual void plugin_startup();
   virtual void plugin_shutdown();

private:
    std::shared_ptr<class heartbeat_plugin_impl> my;
};

} //eosio

//FC_REFLECT(eosio::heartbeat_plugin::runtime_options, (max_transaction_time)(max_irreversible_block_age)(produce_time_offset_us)(last_block_time_offset_us)(subjective_cpu_leeway_us)(incoming_defer_ratio));

