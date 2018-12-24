#include <boost/test/unit_test.hpp>
#include <eosio/testing/tester.hpp>
#include <eosio/chain/abi_serializer.hpp>
#include <eosio/chain/wast_to_wasm.hpp>

#include <eosio.msig/eosio.msig.wast.hpp>
#include <eosio.msig/eosio.msig.abi.hpp>

#include <test_api/test_api.wast.hpp>

#include <eosio.system/eosio.system.wast.hpp>
#include <eosio.system/eosio.system.abi.hpp>

#include <eosio.token/eosio.token.wast.hpp>
#include <eosio.token/eosio.token.abi.hpp>

#include <Runtime/Runtime.h>

#include <fc/variant_object.hpp>
#include <regex>

using namespace eosio::testing;
using namespace eosio;
using namespace eosio::chain;
using namespace eosio::testing;
using namespace fc;

using mvo = fc::mutable_variant_object;

class eosio_msig_tester : public tester {
public:

   eosio_msig_tester() {
      produce_blocks(6);
      create_accounts( { N(eosio.msig), N(eosio.stake), N(eosio.ram), N(eosio.ramfee), N(alice), N(bob), N(carol), N(wang), N(zhang),N(force.config) } );
      set_fee(name("force.test"),name(config::system_account_name),"setconfig",asset(100),10,10,10);
      set_config("f.system1",20);
      set_config("f.msig",30);
      produce_blocks(100);
      const auto& accnt = control->db().get<account_object,by_name>( N(eosio.msig) );
      abi_def abi;
      BOOST_REQUIRE_EQUAL(abi_serializer::to_abi(accnt.abi, abi), true);
      abi_ser.set_abi(abi, abi_serializer_max_time);
   }

   fc::variant json_from_file_or_string(const string& file_or_str, fc::json::parse_type ptype = fc::json::legacy_parser)
   {
      std::regex r("^[ \t]*[\{\[]");
      if ( !regex_search(file_or_str, r) && fc::is_regular_file(file_or_str) ) {
            return fc::json::from_file(file_or_str, ptype);
      } else {
            return fc::json::from_string(file_or_str, ptype);
      }
   }

   authority parse_json_authority(const std::string& authorityJsonOrFile) {
      try {
         return json_from_file_or_string(authorityJsonOrFile).as<authority>();
      } EOS_RETHROW_EXCEPTIONS(authority_type_exception, "Fail to parse Authority JSON '${data}'", ("data",authorityJsonOrFile))
   }

   bytes variant_to_bin( const account_name& account, const action_name& action, const fc::variant& action_args_var ) {
      
      const auto& acnt = control->get_account(account);
      auto abi = acnt.get_abi();
      chain::abi_serializer abis(abi, abi_serializer_max_time);
      
      auto action_type = abis.get_action_type( action );
      FC_ASSERT( !action_type.empty(), "Unknown action ${action} in contract ${contract}", ("action", action)( "contract", account ));
      return abis.variant_to_binary( action_type, action_args_var, abi_serializer_max_time );
   }

   void propose(const string &proposal_name ,const string &requested_perm,const string &transaction_perm,const account_name &contract_name,
      const action_name &act_name,const string &proposed_transaction,const account_name &proposer);
   void approve(const account_name &proposer,const string &proposal_name,const string &perm,const account_name &approver);
   void unapprove(const account_name &proposer,const string &proposal_name,const string &perm,const account_name &approver);
   void exec(const account_name &proposer,const string &proposal_name,const account_name &executer);
   void set_config(const std::string typ,int num);
   transaction_trace_ptr push_action( const account_name& signer, const action_name& name, const variant_object& data, bool auth = true ) {
      vector<account_name> accounts;
      if( auth )
         accounts.push_back( signer );
      auto trace = base_tester::push_action( N(eosio.msig), name, accounts, data );
      produce_block();
      BOOST_REQUIRE_EQUAL( true, chain_has_transaction(trace->id) );
      return trace;
   }
   abi_serializer abi_ser;
};

void eosio_msig_tester::set_config(const std::string typ,int num)
{
      auto args = fc::mutable_variant_object()
                  ("typ", typ )
                  ("num", num)
                  ("key", "")
                  ("fee", "0.0100 EOS");
      vector<permission_level> auths;
      auths.push_back( permission_level{"force.config", "active"} );

      base_tester::push_action(action{
         auths,
         setconfig{
               .typ   = typ,
               .num    = num,
               .key = "",
               .fee       = asset(100)
         }
      },N(force.config));
}

void eosio_msig_tester::propose(const string &proposal_name ,const string &requested_perm,const string &transaction_perm,const account_name &contract_name,
      const action_name &act_name,const string &proposed_transaction,const account_name &proposer){
            fc::variant requested_perm_var;
            requested_perm_var = json_from_file_or_string(requested_perm);
            fc::variant transaction_perm_var;
            transaction_perm_var = json_from_file_or_string(transaction_perm);
            fc::variant trx_var;
            trx_var = json_from_file_or_string(proposed_transaction);
            bytes proposed_trx_serialized = variant_to_bin( contract_name, act_name, trx_var );
            vector<permission_level> trxperm;
            trxperm = transaction_perm_var.as<vector<permission_level>>();
            transaction trx;

            trx.expiration = fc::time_point_sec( fc::time_point::now() + fc::hours(24) );
            trx.ref_block_num = 0;
            trx.ref_block_prefix = 0;
            trx.max_net_usage_words = 0;
            trx.max_cpu_usage_ms = 0;
            trx.delay_sec = 0;
            trx.actions = { chain::action(trxperm, contract_name, act_name, proposed_trx_serialized) };
            auto get_arg = fc::mutable_variant_object("transaction", trx);
            fc::to_variant(trx, trx_var);
            auto args = fc::mutable_variant_object()
                  ("proposer", proposer )
                  ("proposal_name", proposal_name)
                  ("requested", requested_perm_var)
                  ("trx", trx_var);
            push_action(proposer,"propose",args);
}

void eosio_msig_tester::approve(const account_name &proposer,const string &proposal_name,const string &perm,const account_name &approver){
      fc::variant perm_var;
      perm_var = json_from_file_or_string(perm);
      auto args = fc::mutable_variant_object()
                  ("proposer", proposer )
                  ("proposal_name", proposal_name)
                  ("level", perm_var);
      push_action(approver,"approve",args);
}

void eosio_msig_tester::unapprove(const account_name &proposer,const string &proposal_name,const string &perm,const account_name &approver){
      fc::variant perm_var;
      perm_var = json_from_file_or_string(perm);
      auto args = fc::mutable_variant_object()
                  ("proposer", proposer )
                  ("proposal_name", proposal_name)
                  ("level", perm_var);
      push_action(approver,"unapprove",args);
}

void eosio_msig_tester::exec(const account_name &proposer,const string &proposal_name,const account_name &executer){
      auto args = fc::mutable_variant_object()
                  ("proposer", proposer )
                  ("proposal_name", proposal_name)
                  ("executer", executer);
      push_action(executer,"exec",args);
}


BOOST_AUTO_TEST_SUITE(eosio_msig_tests)

BOOST_FIXTURE_TEST_CASE( propose_approve_execute, eosio_msig_tester ) try {
   std::cout<<"---------------------xuyapeng-------------------------------------"<<std::endl;
    set_authority(N(alice),
      "active", authority(1,vector<key_weight>{{get_private_key("alice", "active").get_public_key(), 1}},
      vector<permission_level_weight>{{{"bob", config::active_name}, 1}}), "owner",
      { { N(alice), "active" } }, { get_private_key( N(alice), "active" ) });
    propose("ptt1",R"([{"actor":"bob","permission":"active"}])",R"([{"actor":"alice","permission":"active"}])",
            "eosio","transfer",R"({"from":"alice","to":"bob","quantity":"1.0000 EOS","memo":"msig transfer"})","bob");     
    approve("bob","ptt1",R"({"actor":"bob","permission":"active"})","bob");  

    exec("bob","ptt1","bob");

} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE( propose_approve_unapprove, eosio_msig_tester ) try {
   set_authority(N(alice),
      "active", authority(2,vector<key_weight>{{get_private_key("alice", "active").get_public_key(), 1}},
      vector<permission_level_weight>{{{"bob", config::active_name}, 1},{{"carol", config::active_name}, 1}}), "owner",
      { { N(alice), "active" } }, { get_private_key( N(alice), "active" ) });

   propose("ptt1",R"([{"actor":"bob","permission":"active"},{"actor":"carol","permission":"active"}])",R"([{"actor":"alice","permission":"active"}])",
            "eosio","transfer",R"({"from":"alice","to":"bob","quantity":"1.0000 EOS","memo":"msig transfer"})","bob");
          
      
   approve("bob","ptt1",R"({"actor":"bob","permission":"active"})","bob");
   approve("bob","ptt1",R"({"actor":"carol","permission":"active"})","carol");

   unapprove("bob","ptt1",R"({"actor":"bob","permission":"active"})","bob");
   BOOST_REQUIRE_EXCEPTION(
      exec("bob","ptt1","bob"),
      eosio_assert_message_exception,
      eosio_assert_message_is("transaction authorization failed")
      );
} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE( propose_approve_by_two, eosio_msig_tester ) try {
   set_authority(N(alice),
      "active", authority(2,vector<key_weight>{{get_private_key("alice", "active").get_public_key(), 1}},
      vector<permission_level_weight>{{{"bob", config::active_name}, 1},{{"carol", config::active_name}, 1}}), "owner",
      { { N(alice), "active" } }, { get_private_key( N(alice), "active" ) });

   propose("ptt1",R"([{"actor":"bob","permission":"active"},{"actor":"carol","permission":"active"}])",R"([{"actor":"alice","permission":"active"}])",
            "eosio","transfer",R"({"from":"alice","to":"bob","quantity":"1.0000 EOS","memo":"msig transfer"})","bob");

   approve("bob","ptt1",R"({"actor":"bob","permission":"active"})","bob");

   BOOST_REQUIRE_EXCEPTION(
      exec("bob","ptt1","bob"),
      eosio_assert_message_exception,
      eosio_assert_message_is("transaction authorization failed")
      );
   approve("bob","ptt1",R"({"actor":"carol","permission":"active"})","carol");
   exec("bob","ptt1","bob");

} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE( propose_with_wrong_requested_auth, eosio_msig_tester ) try {
      set_authority(N(alice),
            "active", authority(2,vector<key_weight>{{get_private_key("alice", "active").get_public_key(), 1}},
            vector<permission_level_weight>{{{"bob", config::active_name}, 1},{{"carol", config::active_name}, 1}}), "owner",
            { { N(alice), "active" } }, { get_private_key( N(alice), "active" ) });
   //try with not enough requested auth
      BOOST_REQUIRE_EXCEPTION(propose("ptt1",R"([{"actor":"bob","permission":"active"}])",R"([{"actor":"alice","permission":"active"}])",
            "eosio","transfer",R"({"from":"alice","to":"bob","quantity":"1.0000 EOS","memo":"msig transfer"})","bob"),
      eosio_assert_message_exception,
      eosio_assert_message_is("transaction authorization failed")
      );
} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE( big_transaction, eosio_msig_tester ) try {
   set_authority(N(alice),
      "active", authority(2,vector<key_weight>{},
      vector<permission_level_weight>{{{"bob", config::active_name}, 1},{{"carol", config::active_name}, 1}}), "owner",
      { { N(alice), "active" } }, { get_private_key( N(alice), "active" ) });

   auto wasm = wast_to_wasm( eosio_token_wast );

   auto data = fc::mutable_variant_object()
                ("account", "eosio")
                ("vmtype", 0)
                ("vmversion", 0)
                ("code", bytes( wasm.begin(), wasm.end() ));
      auto str_json = fc::json::to_string(data);

propose("ptt1",R"([{"actor":"bob","permission":"active"},{"actor":"carol","permission":"active"}])",R"([{"actor":"alice","permission":"active"}])",
            "eosio","setcode",str_json,"bob");
approve("bob","ptt1",R"({"actor":"bob","permission":"active"})","bob");
approve("bob","ptt1",R"({"actor":"carol","permission":"active"})","carol");
exec("bob","ptt1","bob");
} FC_LOG_AND_RETHROW()



BOOST_FIXTURE_TEST_CASE( update_system_contract_all_approve, eosio_msig_tester ) try {

   // required to set up the link between (eosio active) and (eosio.prods active)
   //
   //                  zhang active
   //                       |
   //             alice active (2/2 threshold)
   //                      |        \             <--- implicitly updated in onblock action
   //                  bob active   carol active

   set_authority(N(alice),
      "active", authority(2,vector<key_weight>{},
      vector<permission_level_weight>{{{"bob", config::active_name}, 1},{{"carol", config::active_name}, 1}}), "owner",
      { { N(alice), "active" } }, { get_private_key( N(alice), "active" ) });

   set_authority(N(zhang),
      "active", authority(1,vector<key_weight>{},
      vector<permission_level_weight>{{{"alice", config::active_name}, 1}}), "owner",
      { { N(zhang), "active" } }, { get_private_key( N(zhang), "active" ) });

   propose("ptt1",R"([{"actor":"bob","permission":"active"},{"actor":"carol","permission":"active"}])",R"([{"actor":"zhang","permission":"active"}])",
            "eosio","transfer",R"({"from":"zhang","to":"bob","quantity":"1.0000 EOS","memo":"msig transfer"})","bob");

approve("bob","ptt1",R"({"actor":"bob","permission":"active"})","bob");
approve("bob","ptt1",R"({"actor":"carol","permission":"active"})","carol");
exec("bob","ptt1","bob");


} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( update_system_contract_major_approve, eosio_msig_tester ) try {

      set_authority(N(alice),
      "active", authority(2,vector<key_weight>{},
      vector<permission_level_weight>{{{"bob", config::active_name}, 1},{{"carol", config::active_name}, 1}}), "owner",
      { { N(alice), "active" } }, { get_private_key( N(alice), "active" ) });

   set_authority(N(zhang),
      "active", authority(1,vector<key_weight>{},
      vector<permission_level_weight>{{{"alice", config::active_name}, 1}}), "owner",
      { { N(zhang), "active" } }, { get_private_key( N(zhang), "active" ) });

   propose("ptt1",R"([{"actor":"bob","permission":"active"},{"actor":"carol","permission":"active"}])",R"([{"actor":"zhang","permission":"active"}])",
            "eosio","transfer",R"({"from":"zhang","to":"bob","quantity":"1.0000 EOS","memo":"msig transfer"})","bob");

approve("bob","ptt1",R"({"actor":"bob","permission":"active"})","bob");
      BOOST_REQUIRE_EXCEPTION(exec("bob","ptt1","bob"),
            eosio_assert_message_exception,
            eosio_assert_message_is("transaction authorization failed")
      );
approve("bob","ptt1",R"({"actor":"carol","permission":"active"})","carol");
exec("bob","ptt1","bob");
} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
