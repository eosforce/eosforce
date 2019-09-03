#ifndef EOSFORCE_CLEOS_CMDS_HPP
#define EOSFORCE_CLEOS_CMDS_HPP

#pragma once

#include <eosio/chain/name.hpp>
#include <eosio/chain/config.hpp>
#include <fc/io/fstream.hpp>

#include "CLI11.hpp"
#include "help_text.hpp"
#include "localize.hpp"
#include "httpc.hpp"

using namespace std;
using namespace eosio;
using namespace eosio::chain;
using namespace eosio::client::help;
using namespace eosio::client::http;
using namespace eosio::client::localize;
using namespace eosio::client::config;
using namespace boost::filesystem;

template<typename T>
fc::variant call( const std::string& url, const std::string& path, const T& v );
template<typename T>
fc::variant call( const std::string& path, const T& v );
template<>
fc::variant call( const std::string& url, const std::string& path);

void send_actions(std::vector<chain::action>&& actions, packed_transaction::compression_type compression = packed_transaction::none );
void add_standard_transaction_options(CLI::App* cmd, string default_permission = "");
chain::action create_action(const vector<permission_level>& authorization, const account_name& code, const action_name& act, const fc::variant& args);
fc::variant regproducer_variant(const account_name& producer, const public_key_type& key, const string& url, uint32_t location);

// vote_producer_list_subcommand
struct vote_producer_list_subcommand {
   string voter_str;

   vote_producer_list_subcommand(CLI::App* actionRoot) {
      auto vote_proxy = actionRoot->add_subcommand("list", localized("Get all of Voting information for the voter"));
      vote_proxy->add_option("voter", voter_str, localized("The voting account"))->required();

      vote_proxy->set_callback([this] {
         auto result = call(get_table_func, fc::mutable_variant_object("json", true)
               ("code", name(config::system_account_name).to_string())
               ("scope", voter_str)
               ("table", "votes")
         );
         std::cout << fc::json::to_pretty_string(result)
                   << std::endl;
      });
   }
};

struct update_bp_subcommand {
   string producer_str;
   string producer_key_str;
   string url;
   uint32_t loc = 0;

   update_bp_subcommand(CLI::App* actionRoot) {
      auto register_producer = actionRoot->add_subcommand("updatebp", localized("update bp information"));
      register_producer->add_option("account", producer_str, localized("The account to register as a producer"))->required();
      register_producer->add_option("producer_key", producer_key_str, localized("The producer's public key"))->required();
      register_producer->add_option("commission_rate", loc, localized("relative location for purpose of nearest neighbor scheduling"), true);
      register_producer->add_option("url", url, localized("url where info about producer can be found"), true);
      add_standard_transaction_options(register_producer);


      register_producer->set_callback([this] {
         public_key_type producer_key;
         try {
            producer_key = public_key_type(producer_key_str);
         } EOS_RETHROW_EXCEPTIONS(public_key_type_exception, "Invalid producer public key: ${public_key}", ("public_key", producer_key_str))
         auto regprod_var = regproducer_variant(producer_str, producer_key, url, loc );
         send_actions({create_action({permission_level{producer_str,config::active_name}}, config::system_account_name, N(updatebp), regprod_var)});
      });
   }
};


struct vote_producer_claim_subcommand {
   string voter_str;
   string bpname_str;

   vote_producer_claim_subcommand(CLI::App* actionRoot) {
      auto vote_proxy = actionRoot->add_subcommand("claim", localized("Receive dividends on the bp"));
      vote_proxy->add_option("voter", voter_str, localized("The voting account"))->required();
      vote_proxy->add_option("bpname", bpname_str, localized("Receive the dividend bp name"))->required();
      add_standard_transaction_options(vote_proxy);


      vote_proxy->set_callback([this] {
         fc::variant act_payload = fc::mutable_variant_object()
               ("voter", voter_str)
               ("bpname", bpname_str)
         ;
         send_actions({create_action({permission_level{voter_str,config::active_name}}, config::system_account_name, N(claim), act_payload)});
      });
   }
};

struct vote_producer_unfreeze_subcommand {
   string voter_str;
   string bpname_str;

   vote_producer_unfreeze_subcommand(CLI::App* actionRoot) {
      auto vote_proxy = actionRoot->add_subcommand("unfreeze", localized("unfreeze the EOSC which Withdrawal of voting"));
      vote_proxy->add_option("voter", voter_str, localized("The voting account"))->required();
      vote_proxy->add_option("bpname", bpname_str, localized("Receive the dividend bp name"))->required();
      add_standard_transaction_options(vote_proxy);

      vote_proxy->set_callback([this] {
         fc::variant act_payload = fc::mutable_variant_object()
               ("voter", voter_str)
               ("bpname", bpname_str)
         ;
         send_actions({create_action({permission_level{voter_str,config::active_name}}, config::system_account_name, N(unfreeze), act_payload)});
      });
   }
};

struct vote_producer_vote_subcommand {
   string voter_str;
   string bpname_str;
   double amount;

   vote_producer_vote_subcommand(CLI::App* actionRoot) {
      auto vote_proxy = actionRoot->add_subcommand("vote", localized("Vote your stake to a bp"));
      vote_proxy->add_option("voter", voter_str, localized("The voting account"))->required();
      vote_proxy->add_option("bpname", bpname_str, localized("The account(s) to vote for."))->required();
      vote_proxy->add_option("amount", amount, localized("The amount of EOS to vote"))->required();
      add_standard_transaction_options(vote_proxy);

      vote_proxy->set_callback([this] {
         char doubleamount[20] = {0};
         sprintf(doubleamount,"%.4f",amount);
         auto vote_amount = string(doubleamount) + " EOS";
         fc::variant act_payload = fc::mutable_variant_object()
               ("voter", voter_str)
               ("bpname", bpname_str)
               ("stake", asset::from_string( vote_amount ));
         send_actions({create_action({permission_level{voter_str,config::active_name}}, config::system_account_name, N(vote), act_payload)});
      });
   }
};

struct list_bp_subcommand {
   uint32_t limit = 50;
   list_bp_subcommand(CLI::App* actionRoot) {
      auto list_bps = actionRoot->add_subcommand("listbps", localized("Get all of bp information"));
      list_bps->add_option("-l,--limit", limit, localized("The maximum number of rows to return"));
      list_bps->set_callback([this] {
         auto result = call(get_table_func, fc::mutable_variant_object("json", true)
               ("code", name(config::system_account_name).to_string())
               ("scope", "eosio")
               ("table", "bps")
               ("limit",limit));
         std::cout << fc::json::to_pretty_string(result)
                   << std::endl;
      });
   }
};

struct set_fee_subcommand {
   string account;
   string action_to_set_fee;
   string fee_to_set;
   uint32_t cpu_limit_by_contract = 0;
   uint32_t net_limit_by_contract = 0;
   uint32_t ram_limit_by_contract = 0;

   set_fee_subcommand( CLI::App* actionRoot ) {
      auto set_fee_subcmd = actionRoot->add_subcommand("setfee", localized("Set Fee need to action"));
      set_fee_subcmd->add_option("account", account, localized("The account to set the Fee for"))->required();
      set_fee_subcmd->add_option("action", action_to_set_fee, localized("The action to set the Fee for"))->required();
      set_fee_subcmd->add_option("fee", fee_to_set, localized("The Fee to set to action"))->required();
      set_fee_subcmd->add_option("cpu_limit", cpu_limit_by_contract, localized("The cpu max use limit to set to action"));
      set_fee_subcmd->add_option("net_limit", net_limit_by_contract, localized("The net max use limit to set to action"));
      set_fee_subcmd->add_option("ram_limit", ram_limit_by_contract, localized("The ram max use limit to set to action"));
      add_standard_transaction_options(set_fee_subcmd);

      set_fee_subcmd->set_callback([this] {
         asset a;
         a = a.from_string(fee_to_set);
         dlog("set fee ${act}, ${fee}", ("act", action_to_set_fee)("fee", a));

         const auto permission_account =
               ((cpu_limit_by_contract == 0) && (net_limit_by_contract == 0) && (ram_limit_by_contract == 0))
               ? account             // if no set res limit, just need account permission
               : name("force.test"); // if set res limit, need force.test

         fc::variant setfee_data = fc::mutable_variant_object()
               ("account", account)
               ("action", action_to_set_fee)
               ("fee", a)
               ("cpu_limit", cpu_limit_by_contract)
               ("net_limit", net_limit_by_contract)
               ("ram_limit", ram_limit_by_contract);

         send_actions({create_action({permission_level{ permission_account, config::active_name }}, config::system_account_name, N(setfee), setfee_data )});
      });
   }
};



#endif //EOSFORCE_CLEOS_CMDS_HPP
