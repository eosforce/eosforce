/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#include <eosio/chain/genesis_state.hpp>

// these are required to serialize a genesis_state
#include <fc/smart_ref_impl.hpp>   // required for gcc in release mode

namespace eosio { namespace chain {

genesis_state::genesis_state() {
   initial_timestamp = fc::time_point::from_iso_string( "2018-05-28T12:00:00" );
   initial_key = fc::variant("EOS1111111111111111111111111111111114T1Anm").as<public_key_type>();
}

const fc::sha256 old_chain_id = fc::sha256("bd61ae3a031e8ef2f97ee3b0e62776d6d30d4833c8f7c1645c657b149151004b");
const fc::sha256 new_chain_id = fc::sha256("471f5273196ce3b0c5266a3b6e0fa9c44ddaab5e222429a3293f39b358309240");

chain::chain_id_type genesis_state::compute_chain_id() const {
   digest_type::encoder enc;
   fc::raw::pack( enc, *this );
   //return chain_id_type{enc.result()};
   // now is 471f5273196ce3b0c5266a3b6e0fa9c44ddaab5e222429a3293f39b358309240
   // as fc change, we can not got a same id from compute chain id
   const auto res = chain_id_type{enc.result()};
   if(res == new_chain_id){
       return chain_id_type{old_chain_id};
   }else{
       return res;
   }
}

} } // namespace eosio::chain
