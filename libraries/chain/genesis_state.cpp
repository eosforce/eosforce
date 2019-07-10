/**
 *  @file
 *  @copyright defined in eos/LICENSE
 */

#include <eosio/chain/genesis_state.hpp>

// these are required to serialize a genesis_state
#include <fc/smart_ref_impl.hpp>   // required for gcc in release mode

namespace eosio { namespace chain {

genesis_state::genesis_state() {
   initial_timestamp = fc::time_point::from_iso_string( "2018-05-28T12:00:00" );
   initial_key = fc::variant("EOS1111111111111111111111111111111114T1Anm").as<public_key_type>();
}

const static fc::sha256 old_chain_id = fc::sha256("bd61ae3a031e8ef2f97ee3b0e62776d6d30d4833c8f7c1645c657b149151004b");
const static fc::sha256 new_chain_id = fc::sha256("30dd80f5e2f0e9c9f14d348990f3ca82d42efdd1414097dd5094e21f2ed595aa");

chain::chain_id_type genesis_state::compute_chain_id() const {
   digest_type::encoder enc;
   fc::raw::pack( enc, *this );
   //return chain_id_type{enc.result()};
   // now is 30dd80f5e2f0e9c9f14d348990f3ca82d42efdd1414097dd5094e21f2ed595aa
   // as fc change and we no calc contract, we can not got a same id from compute chain id
   const auto res = chain_id_type{enc.result()};
   //ilog("chain id is ${id}", ("id", res));

   if( res == new_chain_id ){
       return chain_id_type{old_chain_id};
   } else {
       return res;
   }
}

} } // namespace eosio::chain
