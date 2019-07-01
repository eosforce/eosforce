
#pragma once

#include <eosiolib/dispatcher.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/contract.hpp>
#include <eosiolib/multi_index.hpp>
#include <eosiolib/producer_schedule.hpp>
#include <eosiolib/time.hpp>
#include <eosiolib/chain.h>

namespace eosiosystem {

   using eosio::asset;
   using eosio::print;
   using eosio::bytes;
   using eosio::block_timestamp;
   using std::string;
   using eosio::time_point_sec;

   static constexpr uint32_t FROZEN_DELAY = 3 * 24 * 60 * 20; //3*24*60*20*3s;
   static constexpr int NUM_OF_TOP_BPS = 23;
   static constexpr int BLOCK_REWARDS_BP = 27000 ; //2.7000 EOS
   static constexpr int BLOCK_REWARDS_B1 = 3000; //0.3000 EOS
   static constexpr uint32_t UPDATE_CYCLE = 100; //every 100 blocks update

   class system_contract : private eosio::contract {
   public:
      system_contract( account_name self ) : contract(self) {}

   private:

      struct account_info {
         account_name name;
         asset available = asset{ 0 };

         uint64_t primary_key() const { return name; }

         EOSLIB_SERIALIZE(account_info, ( name )(available))
      };

      struct vote_info {
         account_name bpname;
         asset staked = asset{ 0 };
         uint32_t voteage_update_height = current_block_num();
         int64_t voteage = 0; // asset.amount * block height
         asset unstaking = asset{ 0 };
         uint32_t unstake_height = current_block_num();

         uint64_t primary_key() const { return bpname; }

         EOSLIB_SERIALIZE(vote_info, ( bpname )(staked)(voteage)(voteage_update_height)(unstaking)(unstake_height))
      };

      struct vote4ram_info {
         account_name voter;
         asset staked = asset{ 0 };
         uint64_t primary_key() const { return voter; }

         EOSLIB_SERIALIZE(vote4ram_info, (voter)(staked))
      };

      struct bp_info {
         account_name name;
         public_key block_signing_key;
         uint32_t commission_rate = 0; // 0 - 10000 for 0% - 100%
         int64_t total_staked = 0;
         asset rewards_pool = asset{ 0 };
         int64_t total_voteage = 0; // asset.amount * block height
         uint32_t voteage_update_height = current_block_num(); // this should be delete
         std::string url;
         bool emergency = false;

         uint64_t primary_key() const { return name; }

         EOSLIB_SERIALIZE(bp_info, ( name )(block_signing_key)(commission_rate)(total_staked)
               (rewards_pool)(total_voteage)(voteage_update_height)(url)(emergency))
      };

      struct producer {
         account_name bpname;
         uint32_t amount = 0;
      };

      struct producer_blacklist {
         account_name bpname;
         bool isactive = false;

         uint64_t primary_key() const { return bpname; }
         void     deactivate()       {isactive = false;}

         EOSLIB_SERIALIZE(producer_blacklist, ( bpname )(isactive))
      };


      struct schedule_info {
         uint64_t version;
         uint32_t block_height;
         producer producers[NUM_OF_TOP_BPS];

         uint64_t primary_key() const { return version; }

         EOSLIB_SERIALIZE(schedule_info, ( version )(block_height)(producers))
      };

      struct chain_status {
         account_name name = N(chainstatus);
         bool emergency = false;

         uint64_t primary_key() const { return name; }

         EOSLIB_SERIALIZE(chain_status, ( name )(emergency))
      };
      
      struct heartbeat_info {
         account_name bpname;
         time_point_sec timestamp;
         
         uint64_t primary_key() const { return bpname; }
         
         EOSLIB_SERIALIZE(heartbeat_info, ( bpname )(timestamp))
      };

      typedef eosio::multi_index<N(accounts), account_info> accounts_table;
      typedef eosio::multi_index<N(votes), vote_info> votes_table;
      typedef eosio::multi_index<N(votes4ram), vote_info> votes4ram_table;
      typedef eosio::multi_index<N(vote4ramsum), vote4ram_info> vote4ramsum_table;
      typedef eosio::multi_index<N(bps), bp_info> bps_table;
      typedef eosio::multi_index<N(schedules), schedule_info> schedules_table;
      typedef eosio::multi_index<N(chainstatus), chain_status> cstatus_table;
      typedef eosio::multi_index<N(heartbeat), heartbeat_info> hb_table;
      typedef eosio::multi_index<N(blackpro), producer_blacklist> blackproducer_table;

      void update_elected_bps();
      void reward_bps( account_name block_producers[] );

      inline void heartbeat_imp( const account_name bpname, const time_point_sec timestamp ){
         hb_table hb_tbl(_self, _self);

         const auto hb_itr = hb_tbl.find(bpname);
         if( hb_itr == hb_tbl.end()) {
            hb_tbl.emplace(bpname, [&]( heartbeat_info& hb ) {
               hb.bpname = bpname;
               hb.timestamp = timestamp;
            });
         } else {
            hb_tbl.modify(hb_itr, 0, [&]( heartbeat_info& hb ) {
               hb.timestamp = timestamp;
            });
         }
      }

   public:
      // @abi action
      void transfer( const account_name from, const account_name to, const asset quantity, const string memo );

      // @abi action
      void updatebp( const account_name bpname, const public_key producer_key,
                     const uint32_t commission_rate, const std::string& url );

      // @abi action
      void vote( const account_name voter, const account_name bpname, const asset stake );

      // @abi action
      void revote( const account_name voter, const account_name frombp, const account_name tobp, const asset restake );

      // @abi action
      void unfreeze( const account_name voter, const account_name bpname );

      // @abi action
      void vote4ram( const account_name voter, const account_name bpname, const asset stake );

      // @abi action
      void unfreezeram( const account_name voter, const account_name bpname );

      // @abi action
      void claim( const account_name voter, const account_name bpname );

      // @abi action
      void onblock( const block_timestamp, const account_name bpname, const uint16_t,
                    const block_id_type, const checksum256, const checksum256, const uint32_t schedule_version );

      // @abi action
      void onfee( const account_name actor, const asset fee, const account_name bpname );

      // @abi action
      void setemergency( const account_name bpname, const bool emergency );
      
      // @abi action
      void heartbeat( const account_name bpname, const time_point_sec timestamp );
      // @abi action
      void removebp( account_name producer );
   };

   EOSIO_ABI(system_contract,
             ( transfer )(updatebp)
                   (vote)(revote)(unfreeze)
                   (vote4ram)(unfreezeram)
                   (claim)
                   (onblock)(onfee)
                   (setemergency)
                   (heartbeat)
                   (removebp))
} /// eosiosystem
