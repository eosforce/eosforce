#include <fc/exception/exception.hpp>
#include <fc/io/json.hpp>
#include <fc/variant_object.hpp>

#include <eosio/chain/genesis_state.hpp>
#include <eosio/chain/name.hpp>

#include <fstream>

//using namespace eosio;
using namespace eosio::chain;

struct key_map {
    std::map<account_name, fc::crypto::private_key> keymap;
};
FC_REFLECT(key_map, (keymap))

int main(int argc, const char **argv) {
  eosio::chain::genesis_state gs;
  const std::string path = "./genesis.json";
  key_map my_keymap;
  key_map my_sign_keymap;
  std::ofstream out("./config.ini");
  for (int i = 0; i < 23; i++) {
      auto key = fc::crypto::private_key::generate<fc::ecc::private_key_shim>();
      auto pub_key = key.get_public_key();
      eosio::chain::account_tuple tu;
      tu.key = pub_key;
      tu.asset = eosio::chain::asset(10000);
      std::string name("biosbp");
      char mark = 'a' + i;
      name.append(1u, mark);
      tu.name = string_to_name(name.c_str());
      gs.initial_account_list.push_back(tu);
      auto sig_key = fc::crypto::private_key::generate<fc::ecc::private_key_shim>();
      auto sig_pub_key = sig_key.get_public_key();
      out << "producer-name = " << name << "\n";
      out << "private-key = [\"" << string(sig_pub_key) << "\",\"" << string(sig_key) << "\"]\n";

      producer_tuple tp;
      tp.bpkey = sig_pub_key;
      tp.name = string_to_name(name.c_str());
      tp.commission_rate = i == 0 ? 0 : i == 22 ? 10000 : 10000 / i;
      gs.initial_producer_list.push_back(tp);
      my_keymap.keymap[string_to_name(name.c_str())] = key; 
      my_sign_keymap.keymap[string_to_name(name.c_str())] = sig_key; 
  }
  
      auto key = fc::crypto::private_key::generate<fc::ecc::private_key_shim>();
      auto pub_key = key.get_public_key();
      eosio::chain::account_tuple tu;
      tu.key = pub_key;
      tu.asset = eosio::chain::asset(10000000000);
      tu.name = N(eosforce);
      gs.initial_account_list.push_back(tu);
      my_keymap.keymap[N(eosforce)] = key; 

  
  out.close();
  const std::string keypath = "./key.json";
  const std::string sigkeypath = "./sigkey.json";
  const std::string configini = "./config.ini";
  fc::json::save_to_file<eosio::chain::genesis_state>( gs, path, true );
  fc::json::save_to_file<key_map>( my_keymap, keypath, true );
  fc::json::save_to_file<key_map>( my_sign_keymap, sigkeypath, true );

  return 0;
}
