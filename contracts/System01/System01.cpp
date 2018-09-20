#include "System01.hpp"

namespace eosiosystem {

void system_contract::transfer( const account_name from, const account_name to, const asset quantity, const string memo ) {
  require_auth( from );
  accounts_table acnts_tbl( _self, _self );
  const auto & from_act = acnts_tbl.get( from, "from account is not found in accounts table" );
  const auto & to_act = acnts_tbl.get( to, "to account is not found in accounts table" );

  eosio_assert( quantity.symbol == SYMBOL, "only support EOS which has 4 precision" );
  //from_act.available is already handling fee
  eosio_assert( 0 <= quantity.amount && quantity.amount <= from_act.available.amount, "need 0.0000 EOS < quantity < available balance" );
  eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );

  acnts_tbl.modify( from_act, 0, [&]( account_info & a ) {
    a.available -= quantity;
  });

  acnts_tbl.modify( to_act, 0, [&]( account_info & a ){
    a.available += quantity;
  });
}

void system_contract::updatebp( const account_name bpname, const public_key block_signing_key,
                                const uint32_t commission_rate, const std::string & url ) {
  require_auth( bpname );
  eosio_assert( url.size() < 64, "url too long" );
  eosio_assert( 1 <= commission_rate && commission_rate <= 10000, "need 1 <= commission rate <= 10000" );

  bps_table bps_tbl( _self, _self );
  auto bp = bps_tbl.find( bpname );
  if ( bp == bps_tbl.end() ) {
    bps_tbl.emplace( bpname, [&]( bp_info & b ) {
      b.name = bpname;
      b.update( block_signing_key, commission_rate, url );
    });
  } else {
    bps_tbl.modify(bp, 0, [&]( bp_info & b) {
      b.update( block_signing_key, commission_rate, url );
    });
  }
}

void system_contract::vote( const account_name voter, const account_name bpname, const asset stake ) {
  require_auth( voter );
  accounts_table acnts_tbl( _self, _self );
  const auto & act = acnts_tbl.get( voter, "voter is not found in accounts table" );

  bps_table bps_tbl( _self, _self );
  const auto & bp  = bps_tbl.get( bpname, "bpname is not registered" );

  eosio_assert( stake.symbol == SYMBOL, "only support EOS which has 4 precision" );
  eosio_assert( 0 <= stake.amount && stake.amount % 10000 == 0, "need stake quantity >= 0.0000 EOS and quantity is integer" );

  int64_t change = 0;
  votes_table votes_tbl( _self, voter );
  auto vts = votes_tbl.find( bpname );
  if( vts == votes_tbl.end() ){
    change = stake.amount;
    //act.available is already handling fee
    eosio_assert( stake.amount <= act.available.amount, "need stake quantity < your available balance" );

    votes_tbl.emplace( voter,[&]( vote_info & v ) {
      v.bpname = bpname;
      v.staked = stake;
    });
  } else {
    change = stake.amount - vts->staked.amount;
    //act.available is already handling fee
    eosio_assert( change <= act.available.amount, "need stake change quantity < your available balance" );

    votes_tbl.modify( vts, 0, [&]( vote_info & v ) {
      v.voteage += v.staked.amount / 10000 * ( current_block_num() - v.voteage_update_height );
      v.voteage_update_height = current_block_num();
      v.staked = stake;
      if( change < 0 ){
        v.unstaking.amount += -change;
        v.unstake_height   = current_block_num();
      }
    });
  }

  if( change > 0 ){
    acnts_tbl.modify( act, 0, [&]( account_info & a ) {
      a.available.amount -= change;
    });
  }

  bps_tbl.modify( bp, 0, [&]( bp_info & b ) {
    b.total_voteage         += b.total_staked * ( current_block_num() - b.voteage_update_height );
    b.voteage_update_height = current_block_num();
    b.total_staked          += change / 10000;
  });
}

void system_contract::unfreeze( const account_name voter, const account_name bpname ){
  require_auth( voter );
  accounts_table acnts_tbl( _self, _self );
  const auto & act = acnts_tbl.get( voter, "voter is not found in accounts table" );

  votes_table votes_tbl( _self, voter );
  const auto & vts = votes_tbl.get( bpname, "voter have not add votes to the the producer yet" );

  eosio_assert( vts.unstake_height + FROZEN_DELAY < current_block_num(), "unfreeze is not available yet" );
  eosio_assert( 0 < vts.unstaking.amount , "need unstaking quantity > 0.0000 EOS" );

  acnts_tbl.modify( act, 0, [&]( account_info & a ) {
    a.available += vts.unstaking;
  });

  votes_tbl.modify( vts, 0, [&]( vote_info & v ) {
    v.unstaking.set_amount( 0 );
  });
}

void system_contract::claim( const account_name voter, const account_name bpname ){
  require_auth( voter );
  accounts_table acnts_tbl( _self, _self );
  const auto & act = acnts_tbl.get( voter, "voter is not found in accounts table" );

  bps_table bps_tbl( _self, _self );
  const auto & bp = bps_tbl.get( bpname, "bpname is not registered" );

  votes_table votes_tbl( _self, voter );
  const auto & vts = votes_tbl.get( bpname, "voter have not add votes to the the producer yet" );

  int64_t newest_voteage = vts.voteage + vts.staked.amount / 10000 * ( current_block_num() - vts.voteage_update_height );
  int64_t newest_total_voteage = bp.total_voteage + bp.total_staked * ( current_block_num() - bp.voteage_update_height );
  eosio_assert( 0 < newest_total_voteage, "claim is not available yet" );

  int128_t amount_voteage = (int128_t)bp.rewards_pool.amount * (int128_t)newest_voteage;
  asset reward = asset( static_cast<int64_t>( (int128_t)amount_voteage / (int128_t)newest_total_voteage ), SYMBOL );
  eosio_assert( 0 <= reward.amount && reward.amount <= bp.rewards_pool.amount, "need 0 <= claim reward quantity <= rewards_pool" );

  acnts_tbl.modify( act, 0, [&]( account_info & a ) {
    a.available += reward;
  });

  votes_tbl.modify( vts, 0, [&]( vote_info & v ) {
    v.voteage = 0;
    v.voteage_update_height = current_block_num();
  });

  bps_tbl.modify( bp, 0, [&]( bp_info & b ) {
    b.rewards_pool          -= reward;
    b.total_voteage         = newest_total_voteage - newest_voteage;
    b.voteage_update_height = current_block_num();
  });
}

void system_contract::onblock( const block_timestamp, const account_name bpname, const uint16_t, const block_id_type,
                               const checksum256, const checksum256, const uint32_t schedule_version ) {
  bps_table bps_tbl( _self, _self );
  accounts_table acnts_tbl( _self, _self );
  schedules_table schs_tbl( _self, _self );
  
  account_name block_producers[NUM_OF_TOP_BPS] = {};
  get_active_producers( block_producers, sizeof(account_name) * NUM_OF_TOP_BPS );

  auto sch = schs_tbl.find(uint64_t(schedule_version));
  if( sch == schs_tbl.end() ) {
    schs_tbl.emplace( bpname,[&]( schedule_info & s ) {
      s.version      = schedule_version;
      s.block_height = current_block_num();
      for (int i = 0 ; i < NUM_OF_TOP_BPS ; i++) {
        s.producers[i].amount = block_producers[i] == bpname ? 1 : 0;
        s.producers[i].bpname = block_producers[i];
      }
    });
  } else {
    schs_tbl.modify( sch, 0, [&]( schedule_info & s ) {
      for (int i = 0; i < NUM_OF_TOP_BPS ; i++) {
        if ( s.producers[i].bpname == bpname ) {
          s.producers[i].amount += 1;
          break;
        }
      }
    });
  }
  
  //reward bps
  reward_bps(block_producers);

  //reward block.one
  const auto & b1 = acnts_tbl.get( N(b1),"b1 is not found in accounts table" );
  acnts_tbl.modify( b1, 0, [&]( account_info & a ) {
    a.available +=  asset( BLOCK_REWARDS_B1, SYMBOL );
  });

  //update schedule
  if( current_block_num() % UPDATE_CYCLE == 0 ){
    update_elected_bps();
  }
}

void system_contract::onfee( const account_name actor, const asset fee, const account_name bpname ) {
  accounts_table acnts_tbl( _self, _self );
  const auto & act = acnts_tbl.get( actor, "account is not found in accounts table" );
  eosio_assert( fee.amount <= act.available.amount, "overdrawn available balance" );

  bps_table bps_tbl( _self, _self );
  const auto & bp = bps_tbl.get( bpname, "bpname is not registered" );

  acnts_tbl.modify( act, 0, [&]( account_info & a ) {
    a.available -= fee ;
  });

  bps_tbl.modify( bp, 0, [&]( bp_info & b ) {
    b.rewards_pool += fee;
  });
}

void system_contract::setemergency( const account_name bpname, const bool emergency ){
  require_auth( bpname );
  bps_table bps_tbl( _self, _self );
  const auto & bp = bps_tbl.get( bpname, "bpname is not registered" );

  cstatus_table cstatus_tbl( _self, _self );
  const auto & cstatus = cstatus_tbl.get( N(chainstatus), "get chainstatus fatal" );

  bps_tbl.modify(bp, 0, [&]( bp_info & b ) {
    b.emergency = emergency;
  });

  account_name block_producers[NUM_OF_TOP_BPS] = {};
  get_active_producers( block_producers, sizeof(account_name) * NUM_OF_TOP_BPS );

  int proposal = 0;
  for ( auto name : block_producers ){
    const auto & b = bps_tbl.get( name, "setemergency: bpname is not registered" );
    proposal += b.emergency ? 1 : 0;
  }

  cstatus_tbl.modify( cstatus, 0, [&]( chain_status & cs ) {
    cs.emergency = proposal > NUM_OF_TOP_BPS * 2 / 3;
  });
}

//******** private ********//

void system_contract::update_elected_bps() {
  bps_table bps_tbl( _self, _self );

  std::vector<eosio::producer_key> vote_schedule;
  std::vector<int64_t> sorts( NUM_OF_TOP_BPS, 0 );

  for ( auto it = bps_tbl.cbegin(); it != bps_tbl.cend(); ++it ) {
    for ( int i = 0 ; i < NUM_OF_TOP_BPS ; ++i ) {
      if ( sorts[size_t(i)] <= it->total_staked ) {
        eosio::producer_key key;
        key.producer_name = it->name;
        key.block_signing_key = it->block_signing_key;
        vote_schedule.insert( vote_schedule.begin() + i, key );
        sorts.insert( sorts.begin() + i, it->total_staked );
        break;
      }
    }
  }

  if ( vote_schedule.size() > NUM_OF_TOP_BPS ) {
    vote_schedule.resize( NUM_OF_TOP_BPS );
  }

  /// sort by producer name
  std::sort( vote_schedule.begin(), vote_schedule.end() );

  bytes packed_schedule = pack( vote_schedule );
  set_proposed_producers( packed_schedule.data(), packed_schedule.size() );
}

void system_contract::reward_bps(account_name block_producers[]){
  bps_table bps_tbl(_self, _self);
  accounts_table acnts_tbl(_self, _self);
  schedules_table schs_tbl(_self, _self);

  //calculate total staked all of the bps
  int64_t staked_all_bps = 0;
  for (auto it = bps_tbl.cbegin(); it != bps_tbl.cend(); ++it){
    staked_all_bps += it->total_staked;
  }
  if(staked_all_bps <= 0){
    return;
  }
  //0.5% of staked_all_bps
  const auto rewarding_bp_staked_threshold = staked_all_bps / 200;//TODO: mv 200 to config
  
  //reward bps, (bp_reward => bp_account_reward + bp_rewards_pool + eosfund_reward;)
  auto sum_bps_reward = 0;
  for (auto it = bps_tbl.cbegin(); it != bps_tbl.cend(); ++it){
    if (it->total_staked <= rewarding_bp_staked_threshold || it->commission_rate < 1 || it->commission_rate > 10000){
      continue;
    }

    auto bp_reward = static_cast<int64_t>( BLOCK_REWARDS_BP * double(it->total_staked) / double(staked_all_bps) );

    //reward bp account
    auto bp_account_reward = bp_reward * 15 / 100 + bp_reward * 70 / 100 * it->commission_rate / 10000;
    if (is_super_bp(block_producers, it->name)){
      bp_account_reward += bp_reward * 15 / 100;
    }
    const auto &act = acnts_tbl.get(it->name, "bpname is not found in accounts table");
    acnts_tbl.modify(act, 0, [&](account_info &a) {
      a.available += asset(bp_account_reward, SYMBOL);
    });

    //reward pool
    auto bp_rewards_pool = bp_reward * 70 / 100 * (10000 - it->commission_rate) / 10000;
    const auto &bp = bps_tbl.get(it->name, "bpname is not registered");
    bps_tbl.modify(bp, 0, [&](bp_info &b) {
      b.rewards_pool += asset(bp_rewards_pool, SYMBOL);
    });

    sum_bps_reward += (bp_account_reward + bp_rewards_pool);

    print(" bp: ", eosio::name{.value=it->name}, ", staked:", it->total_staked, ", bp_stake_rate:", double(it->total_staked) / double(staked_all_bps), 
    ", is_super_bp: ", is_super_bp(block_producers, it->name), ", commission_rate:", it->commission_rate, ", bp_reward: ", bp_reward, 
    ", bp_account_reward+=", bp_account_reward, ", bp_rewards_pool+=", bp_rewards_pool, "\n");
  }

  //reward eosfund
  const auto &eosfund = acnts_tbl.get(N(eosfund1), "eosfund1 is not found in accounts table");
  auto total_eosfund_reward = BLOCK_REWARDS_BP - sum_bps_reward;
  acnts_tbl.modify(eosfund, 0, [&](account_info &a) {
    a.available += asset(total_eosfund_reward, SYMBOL);
  });
  print("---- total_eosfund_reward+= ", total_eosfund_reward, "\n");
}

bool system_contract::is_super_bp(account_name block_producers[], account_name name)
{
  for (int i = 0; i < NUM_OF_TOP_BPS; i++){
    if (name == block_producers[i]){
      return true;
    }
  }
  return false;
}
}
