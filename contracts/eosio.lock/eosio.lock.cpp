#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>

using eosio::asset;

static constexpr uint64_t SYMBOL = S(4, EOSLOCK);

class eoslock : public eosio::contract {
   public:
        eoslock( account_name self ) :contract(self),_accounts( _self, _self){}

   private:
      struct account {
         account_name owner;
         asset     balance = asset(0, SYMBOL);

         uint64_t primary_key()const { return owner; }
      };

      eosio::multi_index<N(accounts), account> _accounts;

};
