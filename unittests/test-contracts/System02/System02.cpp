#include <utility>

#include "System02.hpp"

namespace eosiosystem {
   static constexpr uint64_t SYMBOL = S(4, EOS);

   void system_contract::transfer( const account_name from, const account_name to, const asset quantity,
                                   const string memo ) {
      require_auth(from);
      accounts_table acnts_tbl(_self, _self);
      const auto& from_act = acnts_tbl.get(from, "from account is not found in accounts table");
      const auto& to_act = acnts_tbl.get(to, "to account is not found in accounts table");

      eosio_assert(quantity.symbol == SYMBOL, "only support EOS which has 4 precision");
      //from_act.available is already handling fee
      eosio_assert(0 <= quantity.amount && quantity.amount <= from_act.available.amount,
                   "need 0.0000 EOS < quantity < available balance");
      eosio_assert(memo.size() <= 256, "memo has more than 256 bytes");

      require_recipient(from);
      require_recipient(to);

      acnts_tbl.modify(from_act, 0, [&]( account_info& a ) {
         a.available -= quantity;
      });

      acnts_tbl.modify(to_act, 0, [&]( account_info& a ) {
         a.available += quantity;
      });
   }

   void system_contract::updatebp( const account_name bpname, const public_key block_signing_key,
                                   const uint32_t commission_rate, const std::string& url ) {
      require_auth(bpname);
      eosio_assert(url.size() < 64, "url too long");
      eosio_assert(1 <= commission_rate && commission_rate <= 10000, "need 1 <= commission rate <= 10000");

      bps_table bps_tbl(_self, _self);
      auto bp = bps_tbl.find(bpname);
      if( bp == bps_tbl.end()) {
         bps_tbl.emplace(bpname, [&]( bp_info& b ) {
            b.name              = bpname;
            b.block_signing_key = block_signing_key;
            b.commission_rate   = commission_rate;
            b.url               = url;
         });
      } else {
         bps_tbl.modify(bp, 0, [&]( bp_info& b ) {
            b.block_signing_key = block_signing_key;
            b.commission_rate   = commission_rate;
            b.url               = url;
         });
      }
   }

   void system_contract::revote( const account_name voter, const account_name frombp, const account_name tobp, const asset restake ) {
      require_auth(voter);
      eosio_assert(frombp != tobp, " from and to cannot same");
      eosio_assert(restake.symbol == SYMBOL, "only support EOS which has 4 precision");
      eosio_assert(asset{0} < restake && restake.amount % 10000 == 0,
                   "need restake quantity > 0.0000 EOS and quantity is integer");

      bps_table bps_tbl(_self, _self);
      const auto& bpf = bps_tbl.get(frombp, "bpname is not registered");
      const auto& bpt = bps_tbl.get(tobp, "bpname is not registered");

      const auto curr_block_num = current_block_num();

      // votes_table from bp
      votes_table votes_tbl(_self, voter);
      auto vtsf = votes_tbl.find(frombp);
      eosio_assert(vtsf != votes_tbl.end(), "no vote on this bp");
      eosio_assert(restake <= vtsf->staked, "need restake <= frombp stake");
      votes_tbl.modify(vtsf, 0, [&]( vote_info& v ) {
          v.voteage += v.staked.amount / 10000 * ( curr_block_num - v.voteage_update_height );
          v.voteage_update_height = curr_block_num;
          v.staked -= restake;
      });

      // votes_table to bp
      auto vtst = votes_tbl.find(tobp);
      if( vtst == votes_tbl.end()) {
         votes_tbl.emplace(voter, [&]( vote_info& v ) {
             v.bpname = tobp;
             v.staked = restake;
         });
      } else {
         votes_tbl.modify(vtst, 0, [&]( vote_info& v ) {
             v.voteage += v.staked.amount / 10000 * ( curr_block_num - v.voteage_update_height );
             v.voteage_update_height = curr_block_num;
             v.staked += restake;
         });
      }

      bps_tbl.modify(bpf, 0, [&]( bp_info& b ) {
          b.total_voteage += b.total_staked * ( curr_block_num - b.voteage_update_height );
          b.voteage_update_height = curr_block_num;
          b.total_staked -= restake.amount / 10000;
      });

      bps_tbl.modify(bpt, 0, [&]( bp_info& b ) {
          b.total_voteage += b.total_staked * ( curr_block_num - b.voteage_update_height );
          b.voteage_update_height = curr_block_num;
          b.total_staked += restake.amount / 10000;
      });
   }

   void system_contract::vote( const account_name voter, const account_name bpname, const asset stake ) {
      require_auth(voter);
      accounts_table acnts_tbl(_self, _self);
      const auto& act = acnts_tbl.get(voter, "voter is not found in accounts table");

      bps_table bps_tbl(_self, _self);
      const auto& bp = bps_tbl.get(bpname, "bpname is not registered");

      eosio_assert(stake.symbol == SYMBOL, "only support EOS which has 4 precision");
      eosio_assert(0 <= stake.amount && stake.amount % 10000 == 0,
                   "need stake quantity >= 0.0000 EOS and quantity is integer");

      const auto curr_block_num = current_block_num();

      int64_t change = 0;
      votes_table votes_tbl(_self, voter);
      auto vts = votes_tbl.find(bpname);
      if( vts == votes_tbl.end()) {
         change = stake.amount;
         //act.available is already handling fee
         eosio_assert(stake.amount <= act.available.amount, "need stake quantity < your available balance");

         votes_tbl.emplace(voter, [&]( vote_info& v ) {
            v.bpname = bpname;
            v.staked = stake;
         });
      } else {
         change = stake.amount - vts->staked.amount;
         //act.available is already handling fee
         eosio_assert(change <= act.available.amount, "need stake change quantity < your available balance");

         votes_tbl.modify(vts, 0, [&]( vote_info& v ) {
            v.voteage += v.staked.amount / 10000 * ( curr_block_num - v.voteage_update_height );
            v.voteage_update_height = curr_block_num;
            v.staked = stake;
            if( change < 0 ) {
               v.unstaking.amount += -change;
               v.unstake_height = curr_block_num;
            }
         });
      }

      blackproducer_table blackproducer(_self, _self);
      auto blackpro = blackproducer.find(bpname);
       eosio_assert(blackpro == blackproducer.end() || blackpro->isactive || (!blackpro->isactive && change < 0), "bp is not active");
      if( change > 0 ) {
         acnts_tbl.modify(act, 0, [&]( account_info& a ) {
            a.available.amount -= change;
         });
      }
      
      bps_tbl.modify(bp, 0, [&]( bp_info& b ) {
         b.total_voteage += b.total_staked * ( curr_block_num - b.voteage_update_height );
         b.voteage_update_height = curr_block_num;
         b.total_staked += (change / 10000);
      });
   }

   void system_contract::unfreeze( const account_name voter, const account_name bpname ) {
      require_auth(voter);
      accounts_table acnts_tbl(_self, _self);
      const auto& act = acnts_tbl.get(voter, "voter is not found in accounts table");

      votes_table votes_tbl(_self, voter);
      const auto& vts = votes_tbl.get(bpname, "voter have not add votes to the the producer yet");

      eosio_assert(vts.unstake_height + FROZEN_DELAY < current_block_num(), "unfreeze is not available yet");
      eosio_assert(0 < vts.unstaking.amount, "need unstaking quantity > 0.0000 EOS");

      acnts_tbl.modify(act, 0, [&]( account_info& a ) {
         a.available += vts.unstaking;
      });

      votes_tbl.modify(vts, 0, [&]( vote_info& v ) {
         v.unstaking.set_amount(0);
      });
   }

   void system_contract::vote4ram( const account_name voter, const account_name bpname, const asset stake ) {
      require_auth(voter);
      accounts_table acnts_tbl(_self, _self);
      const auto& act = acnts_tbl.get(voter, "voter is not found in accounts table");

      bps_table bps_tbl(_self, _self);
      const auto& bp = bps_tbl.get(bpname, "bpname is not registered");

      eosio_assert(stake.symbol == SYMBOL, "only support EOS which has 4 precision");
      eosio_assert(0 <= stake.amount && stake.amount % 10000 == 0,
                   "need stake quantity >= 0.0000 EOS and quantity is integer");

      const auto curr_block_num = current_block_num();

      int64_t change = 0;
      votes4ram_table votes_tbl(_self, voter);
      auto vts = votes_tbl.find(bpname);
      if( vts == votes_tbl.end()) {
         change = stake.amount;
         //act.available is already handling fee
         eosio_assert(stake.amount <= act.available.amount, "need stake quantity < your available balance");

         votes_tbl.emplace(voter, [&]( vote_info& v ) {
            v.bpname = bpname;
            v.staked = stake;
         });
      } else {
         change = stake.amount - vts->staked.amount;
         //act.available is already handling fee
         eosio_assert(change <= act.available.amount, "need stake change quantity < your available balance");

         votes_tbl.modify(vts, 0, [&]( vote_info& v ) {
            v.voteage += v.staked.amount / 10000 * ( curr_block_num - v.voteage_update_height );
            v.voteage_update_height = curr_block_num;
            v.staked = stake;
            if( change < 0 ) {
               v.unstaking.amount += -change;
               v.unstake_height = curr_block_num;
            }
         });
      }
      blackproducer_table blackproducer(_self, _self);
      auto blackpro = blackproducer.find(bpname);
       eosio_assert(blackpro == blackproducer.end() || blackpro->isactive || (!blackpro->isactive && change < 0), "bp is not active");
      if( change > 0 ) {
         acnts_tbl.modify(act, 0, [&]( account_info& a ) {
            a.available.amount -= change;
         });
      }

      bps_tbl.modify(bp, 0, [&]( bp_info& b ) {
         b.total_voteage += b.total_staked * ( curr_block_num - b.voteage_update_height );
         b.voteage_update_height = curr_block_num;
         b.total_staked += change / 10000;
      });

      vote4ramsum_table vote4ramsum_tbl(_self, _self);
      auto vtss = vote4ramsum_tbl.find(voter);
      if(vtss == vote4ramsum_tbl.end()){
         vote4ramsum_tbl.emplace(voter, [&]( vote4ram_info& v ) {
            v.voter = voter;
            v.staked = stake; // for first vote all staked is stake
         });
      }else{
         vote4ramsum_tbl.modify(vtss, 0, [&]( vote4ram_info& v ) {
            v.staked += asset{change};
         });
      }

      set_need_check_ram_limit( voter );
   }

   void system_contract::unfreezeram( const account_name voter, const account_name bpname ) {
      require_auth(voter);
      accounts_table acnts_tbl(_self, _self);
      const auto& act = acnts_tbl.get(voter, "voter is not found in accounts table");

      votes4ram_table votes_tbl(_self, voter);
      const auto& vts = votes_tbl.get(bpname, "voter have not add votes to the the producer yet");

      eosio_assert(vts.unstake_height + FROZEN_DELAY < current_block_num(), "unfreeze is not available yet");
      eosio_assert(0 < vts.unstaking.amount, "need unstaking quantity > 0.0000 EOS");

      acnts_tbl.modify(act, 0, [&]( account_info& a ) {
         a.available += vts.unstaking;
      });

      votes_tbl.modify(vts, 0, [&]( vote_info& v ) {
         v.unstaking.set_amount(0);
      });
   }

   void system_contract::claim( const account_name voter, const account_name bpname ) {
      require_auth(voter);

      const auto curr_block_num = current_block_num();

      accounts_table acnts_tbl(_self, _self);
      const auto& act = acnts_tbl.get(voter, "voter is not found in accounts table");

      bps_table bps_tbl(_self, _self);
      const auto& bp = bps_tbl.get(bpname, "bpname is not registered");

      votes_table votes_tbl(_self, voter);
      const auto& vts = votes_tbl.get(bpname, "voter have not add votes to the the producer yet");

      int64_t newest_voteage =
            vts.voteage + vts.staked.amount / 10000 * ( curr_block_num - vts.voteage_update_height );
      int64_t newest_total_voteage =
            bp.total_voteage + bp.total_staked * ( curr_block_num - bp.voteage_update_height );
      eosio_assert(0 < newest_total_voteage, "claim is not available yet");

      int128_t amount_voteage = ( int128_t ) bp.rewards_pool.amount * ( int128_t ) newest_voteage;
      asset reward = asset(static_cast<int64_t>(( int128_t ) amount_voteage / ( int128_t ) newest_total_voteage ),
                           SYMBOL);
      eosio_assert(0 <= reward.amount && reward.amount <= bp.rewards_pool.amount,
                   "need 0 <= claim reward quantity <= rewards_pool");

      acnts_tbl.modify(act, 0, [&]( account_info& a ) {
         a.available += reward;
      });

      votes_tbl.modify(vts, 0, [&]( vote_info& v ) {
         v.voteage = 0;
         v.voteage_update_height = curr_block_num;
      });

      bps_tbl.modify(bp, 0, [&]( bp_info& b ) {
         b.rewards_pool -= reward;
         b.total_voteage = newest_total_voteage - newest_voteage;
         b.voteage_update_height = curr_block_num;
      });
   }

   void system_contract::onblock( const block_timestamp, const account_name bpname, const uint16_t, const block_id_type,
                                  const checksum256, const checksum256, const uint32_t schedule_version ) {
      bps_table bps_tbl(_self, _self);
      accounts_table acnts_tbl(_self, _self);
      schedules_table schs_tbl(_self, _self);

      const auto curr_block_num = current_block_num();

      account_name block_producers[NUM_OF_TOP_BPS] = {};
      get_active_producers(block_producers, sizeof(account_name) * NUM_OF_TOP_BPS);

      auto sch = schs_tbl.find(uint64_t(schedule_version));
      if( sch == schs_tbl.end()) {
         schs_tbl.emplace(bpname, [&]( schedule_info& s ) {
            s.version = schedule_version;
            s.block_height = curr_block_num;
            for( int i = 0; i < NUM_OF_TOP_BPS; i++ ) {
               s.producers[i].amount = block_producers[i] == bpname ? 1 : 0;
               s.producers[i].bpname = block_producers[i];
            }
         });
      } else {
         schs_tbl.modify(sch, 0, [&]( schedule_info& s ) {
            for( int i = 0; i < NUM_OF_TOP_BPS; i++ ) {
               if( s.producers[i].bpname == bpname ) {
                  s.producers[i].amount += 1;
                  break;
               }
            }
         });
      }

      heartbeat_imp( bpname, time_point_sec(uint32_t(current_time() / 1000000ll)) );

      //reward bps
      reward_bps(block_producers);

      //update schedule
      if( curr_block_num % UPDATE_CYCLE == 0 ) {
         //reward block.one
         const auto& b1 = acnts_tbl.get(N(b1), "b1 is not found in accounts table");
         acnts_tbl.modify(b1, 0, [&]( account_info& a ) {
            a.available += asset(BLOCK_REWARDS_B1 * UPDATE_CYCLE, SYMBOL);
         });

         update_elected_bps();
      }
   }

   void system_contract::removebp( account_name bpname ) {
      require_auth(_self);

      blackproducer_table blackproducer(_self, _self);
      auto bp = blackproducer.find(bpname);
      if( bp == blackproducer.end()) {
        blackproducer.emplace(bpname, [&]( producer_blacklist& b ) {
            b.bpname = bpname;
            b.deactivate();
         });
      } else {
         blackproducer.modify(bp, 0, [&]( producer_blacklist& b ) {
            b.deactivate();
         });
      }
   }

   void system_contract::onfee( const account_name actor, const asset fee, const account_name bpname ) {
      accounts_table acnts_tbl(_self, _self);
      const auto& act = acnts_tbl.get(actor, "account is not found in accounts table");
      eosio_assert(fee.amount <= act.available.amount, "overdrawn available balance");

      bps_table bps_tbl(_self, _self);
      const auto& bp = bps_tbl.get(bpname, "bpname is not registered");

      acnts_tbl.modify(act, 0, [&]( account_info& a ) {
         a.available -= fee;
      });

      bps_tbl.modify(bp, 0, [&]( bp_info& b ) {
         b.rewards_pool += fee;
      });
   }

   void system_contract::setemergency( const account_name bpname, const bool emergency ) {
      require_auth(bpname);
      bps_table bps_tbl(_self, _self);
      const auto& bp = bps_tbl.get(bpname, "bpname is not registered");

      cstatus_table cstatus_tbl(_self, _self);
      const auto& cstatus = cstatus_tbl.get(N(chainstatus), "get chainstatus fatal");

      bps_tbl.modify(bp, 0, [&]( bp_info& b ) {
         b.emergency = emergency;
      });

      account_name block_producers[NUM_OF_TOP_BPS] = {};
      get_active_producers(block_producers, sizeof(account_name) * NUM_OF_TOP_BPS);

      int proposal = 0;
      for( auto name : block_producers ) {
         const auto& b = bps_tbl.get(name, "setemergency: bpname is not registered");
         proposal += b.emergency ? 1 : 0;
      }

      cstatus_tbl.modify(cstatus, 0, [&]( chain_status& cs ) {
         cs.emergency = proposal > NUM_OF_TOP_BPS * 2 / 3;
      });
   }
   
   void system_contract::heartbeat( const account_name bpname, const time_point_sec timestamp ) {
      bps_table bps_tbl(_self, _self);
      eosio_assert( bps_tbl.find( bpname ) != bps_tbl.end(), 
         "bpname is not registered" );
      heartbeat_imp(bpname, timestamp);
   }

   void system_contract::update_elected_bps() {
      bps_table bps_tbl(_self, _self);

      constexpr auto bps_top_size = static_cast<size_t>(NUM_OF_TOP_BPS);

      std::vector< std::pair<eosio::producer_key, int64_t> > vote_schedule;
      vote_schedule.reserve(32);

      // Note this output is not same after updated
      blackproducer_table blackproducer(_self, _self);

      // TODO: use table sorted datas
      for( const auto& it : bps_tbl ) {
         const auto blackpro =  blackproducer.find(it.name);
         if( blackpro != blackproducer.end() && (!blackpro->isactive) ) {
            continue;
         }

         const auto vs_size = vote_schedule.size();
         if(   vs_size >= bps_top_size 
            && vote_schedule[vs_size - 1].second > it.total_staked ) {
            continue;
         }

         // Just 23 node, just find by for
         for( int i = 0; i < bps_top_size; ++i ) {
            if( vote_schedule[i].second <= it.total_staked ) {
               vote_schedule.insert(vote_schedule.begin() + i, 
                  std::make_pair( eosio::producer_key{ it.name, it.block_signing_key }, 
                                  it.total_staked ) );

               if( vote_schedule.size() > bps_top_size ) {
                  vote_schedule.resize( bps_top_size );
               }
               break;
            }
         }
      }

      if( vote_schedule.size() > bps_top_size ) {
         vote_schedule.resize(bps_top_size);
      }

      /// sort by producer name
      std::sort(vote_schedule.begin(), vote_schedule.end(), [](const auto& l, const auto& r) -> bool{
          return l.first.producer_name < r.first.producer_name;
      });

      std::vector<eosio::producer_key> vote_schedule_data;
      vote_schedule_data.reserve(vote_schedule.size());

      for( const auto& v : vote_schedule ) {
         vote_schedule_data.push_back(v.first);
      }

      auto packed_schedule = pack(vote_schedule_data);
      set_proposed_producers(packed_schedule.data(), packed_schedule.size());
   }

   void system_contract::reward_bps( account_name block_producers[] ) {
      bps_table bps_tbl(_self, _self);
      accounts_table acnts_tbl(_self, _self);
      schedules_table schs_tbl(_self, _self);
      hb_table hb_tbl(_self, _self);

      //calculate total staked all of the bps
      // TODO: use cache staked_all_bps
      int64_t staked_all_bps = 0;
      for( auto it = bps_tbl.cbegin(); it != bps_tbl.cend(); ++it ) {
         staked_all_bps += it->total_staked;
      }
      if( staked_all_bps <= 0 ) {
         return;
      }
      //0.5% of staked_all_bps
      const auto rewarding_bp_staked_threshold = staked_all_bps / 200;

      auto hb_max = get_num_config_on_chain(N(hb.max));
      if ( hb_max < 0 ) {
         hb_max = 3600;
      }

      std::set<uint64_t> super_bps;
      for( int i = 0; i < NUM_OF_TOP_BPS; i++ ) {
         super_bps.insert( block_producers[i] );
      }

      //reward bps, (bp_reward => bp_account_reward + bp_rewards_pool + eosfund_reward;)
      auto sum_bps_reward = 0;
      blackproducer_table blackproducer(_self, _self);
      for( auto it = bps_tbl.cbegin(); it != bps_tbl.cend(); ++it ) {
         const auto blackpro = blackproducer.find(it->name);
         if(   ( blackpro != blackproducer.end() && !blackpro->isactive) 
            || it->total_staked <= rewarding_bp_staked_threshold 
            || it->commission_rate < 1 
            || it->commission_rate > 10000 ) {
            continue;
         }
         
         auto hb = hb_tbl.find(it->name);
         if( hb == hb_tbl.end() || (hb->timestamp + static_cast<uint32_t>(hb_max)) < time_point_sec( now() ) ) {
             continue;
         }

         const auto bp_reward = static_cast<int64_t>( BLOCK_REWARDS_BP * double(it->total_staked) / double(staked_all_bps));

         //reward bp account
         auto bp_account_reward = bp_reward * 15 / 100 + bp_reward * 70 / 100 * it->commission_rate / 10000;
         if( super_bps.find( it->name ) != super_bps.end() ) {
            bp_account_reward += bp_reward * 15 / 100;
         }

         const auto& act = acnts_tbl.get(it->name, "bpname is not found in accounts table");
         acnts_tbl.modify(act, 0, [&]( account_info& a ) {
            a.available += asset(bp_account_reward, SYMBOL);
         });

         //reward pool
         const auto bp_rewards_pool = bp_reward * 70 / 100 * ( 10000 - it->commission_rate ) / 10000;
         const auto& bp = bps_tbl.get(it->name, "bpname is not registered");
         bps_tbl.modify(bp, 0, [&]( bp_info& b ) {
            b.rewards_pool += asset(bp_rewards_pool, SYMBOL);
         });

         sum_bps_reward += ( bp_account_reward + bp_rewards_pool );

         /* no log
         if( current_block_num() % 100 == 0 ) {
            print(" bp: ", eosio::name{.value=it->name}, ", staked:", it->total_staked, ", bp_stake_rate:",
                  double(it->total_staked) / double(staked_all_bps),
                  ", is_super_bp: ", is_super_bp(block_producers, it->name), ", commission_rate:", it->commission_rate,
                  ", bp_reward: ", bp_reward,
                  ", bp_account_reward+=", bp_account_reward, ", bp_rewards_pool+=", bp_rewards_pool, "\n");
         }
         */

      }

      //reward eosfund
      const auto& eosfund = acnts_tbl.get(N(devfund), "devfund is not found in accounts table");
      auto total_eosfund_reward = BLOCK_REWARDS_BP - sum_bps_reward;
      acnts_tbl.modify(eosfund, 0, [&]( account_info& a ) {
         a.available += asset(total_eosfund_reward, SYMBOL);
      });
   }
}
