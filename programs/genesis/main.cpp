#include <fc/exception/exception.hpp>
#include <fc/io/json.hpp>
#include <fc/variant_object.hpp>

#include <eosio/chain/genesis_state.hpp>
#include <eosio/chain/name.hpp>
#include <eosio/chain/config.hpp>

#include <fstream>

//using namespace eosio;
using namespace eosio::chain;

struct key_map {
   std::map<account_name, fc::crypto::private_key> keymap;

   void add_init_acc(
         eosio::chain::genesis_state &gs,
         const account_name &name,
         const uint64_t eos,
         const private_key_type key) {
      const auto tu = eosio::chain::account_tuple{
            key.get_public_key(), eosio::chain::asset(eos * 10000), name
      };

      keymap[name] = key;
      gs.initial_account_list.push_back(tu);
   }
};
FC_REFLECT(key_map, ( keymap ))



int main( int argc, const char **argv ) {
   eosio::chain::genesis_state gs;

   gs.initial_timestamp = fc::time_point::now();

   const std::string path = "./genesis.json";
   const std::string activeacc = "./activeacc.json";
   key_map my_keymap;
   key_map my_sign_keymap;
   std::ofstream out("./config.ini");

   // EOS5muUziYrETi5b6G2Ev91dCBrEm3qir7PK4S2qSFqfqcmouyzCr
   const auto bp_private_key = fc::crypto::private_key(std::string("5KYQvUwt6vMpLJxqg4jSQNkuRfktDHtYDp8LPoBpYo8emvS1GfG"));

   // EOS7R82SaGaJubv23GwXHyKT4qDCVXi66qkQrnjwmBUvdA4dyzEPG
   const auto bpsign_private_key = fc::crypto::private_key(std::string("5JfjatHRwbmY8SfptFRxHnYUctfnuaxANTGDYUtkfrrBDgkh3hB"));

   const auto& biosbp_str = std::string("biosbp");

   for( int i = 0; i < config::max_producers; i++ ) {
      const auto key = bp_private_key;
      const auto pub_key = key.get_public_key();

      const auto name_str = biosbp_str + static_cast<char>('a' + i);
      const auto bpname = string_to_name( name_str.c_str() );

      auto tu = eosio::chain::account_tuple {
         pub_key,
         eosio::chain::asset(1000 * 10000),
         bpname
      };

      gs.initial_account_list.push_back(tu);

      const auto sig_key = bpsign_private_key;
      const auto sig_pub_key = sig_key.get_public_key();

      out << "producer-name = " << name_str << "\n";
      out << "private-key = [\"" << string(sig_pub_key) << "\",\"" << string(sig_key) << "\"]\n";

      gs.initial_producer_list.push_back( producer_tuple {
         bpname, sig_pub_key, 100, std::string("https://www.eosforce.io/") });

      tu.key = sig_pub_key;
      tu.name = string_to_name( (name_str + 'a').c_str() );
      gs.initial_account_list.push_back(tu);

      my_keymap.keymap[bpname]      = key;
      my_keymap.keymap[tu.name]   = sig_key;
      my_sign_keymap.keymap[bpname] = sig_key;
   }

   // use fix key EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
   const auto private_key = fc::crypto::private_key(std::string("5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3"));

   my_keymap.add_init_acc(gs, N(eosforce), 10000*10000, private_key);
   my_keymap.add_init_acc(gs, N(devfund),            1, private_key);
   my_keymap.add_init_acc(gs, N(eosfund1),           1, private_key);
   my_keymap.add_init_acc(gs, N(b1),                 1, private_key);

   my_keymap.add_init_acc(gs, N(eosio.msig), 1000*10000, private_key);

   // for test
   my_keymap.add_init_acc(gs, N(force.test),     1*10000, private_key);
   my_keymap.add_init_acc(gs, N(force.ram),      1*10000, private_key);
   my_keymap.add_init_acc(gs, N(force.cpu),      1*10000, private_key);
   my_keymap.add_init_acc(gs, N(force.net),      1*10000, private_key);
   my_keymap.add_init_acc(gs, N(force.config), 100*10000, private_key);

   // for relay
   my_keymap.add_init_acc(gs, N(r.token.in),   100*10000, private_key);
   my_keymap.add_init_acc(gs, N(r.token.out),  100*10000, private_key);
   my_keymap.add_init_acc(gs, N(r.acc.map),    100*10000, private_key);
   my_keymap.add_init_acc(gs, N(r.t.exchange), 100*10000, private_key);

   my_keymap.add_init_acc(gs, N(test), 10000*10000, private_key);

   // test accounts
   for( int j = 0; j < 26; ++j ) {
      std::string n = "test";
      const char mark = static_cast<char>('a' + j);
      n += mark;
      my_keymap.add_init_acc(
            gs,
            string_to_name(n.c_str()),
            100000, private_key );
   }

   for( int j = 0; j < 26; ++j ) {
      std::string n = "t.";
      const char mark = static_cast<char>('a' + j);
      n += mark;
      my_keymap.add_init_acc(
            gs,
            string_to_name(n.c_str()),
            100000, private_key );
   }

   out.close();

   const std::string keypath    = "./key.json";
   const std::string sigkeypath = "./sigkey.json";
   const std::string configini  = "./config.ini";

   fc::json::save_to_file<eosio::chain::genesis_state>(gs, path, true);
   fc::json::save_to_file<std::vector<account_tuple>>(gs.initial_account_list, activeacc, true);
   fc::json::save_to_file<key_map>(my_keymap, keypath, true);
   fc::json::save_to_file<key_map>(my_sign_keymap, sigkeypath, true);

   return 0;
}
