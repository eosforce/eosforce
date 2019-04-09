/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once

#include <eosio/chain_plugin/chain_plugin.hpp>
#include <appbase/application.hpp>
#include <memory>

namespace eosio {

using bus_plugin_impl_ptr = std::shared_ptr<class bus_plugin_impl>;

/**
 *   See data dictionary (DB Schema Definition - EOS API) for description of MongoDB schema.
 *
 *   If cmake -DBUILD_bus_plugin=true  not specified then this plugin not compiled/included.
 */
class bus_plugin : public plugin<bus_plugin> {
public:
   APPBASE_PLUGIN_REQUIRES((chain_plugin))

   bus_plugin();
   virtual ~bus_plugin();

   virtual void set_program_options(options_description& cli, options_description& cfg) override;

   void plugin_initialize(const variables_map& options);
   void plugin_startup();
   void plugin_shutdown();

private:
   bus_plugin_impl_ptr my;
  // bool b_need_start = false;
};

}

