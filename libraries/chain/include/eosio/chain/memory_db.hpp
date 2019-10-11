#pragma once

#include <eosio/chain/contract_table_objects.hpp>
#include <eosio/chain/controller.hpp>
#include <fc/utility.hpp>
#include <sstream>
#include <algorithm>
#include <set>
#include <eosio/chain/apply_context.hpp>

namespace chainbase { class database; }

namespace eosio { namespace chain {

class controller;

// memory_db
class memory_db {
   /// Constructor
public:
   explicit memory_db( controller& con )
         : db(con.mutable_db()) {
   }

   explicit memory_db( chainbase::database& db )
         : db(db) {
   }

   /// Database methods:
public:
   constexpr static size_t max_stack_buffer_size = 512;

   template <typename T>
   void insert(const uint64_t &code,
               const uint64_t &scope,
               const uint64_t &table,
               const account_name& payer,
               const T &obj){
      const auto data = fc::raw::pack(obj);
      db_store_i64(
            code, scope, table, payer,
            obj.primary_key(),
            data.data(), data.size());
   }

   template <typename T>
   bool get( const uint64_t &code,
             const uint64_t &scope,
             const uint64_t &table,
             const uint64_t &id,
             T &out ) {
      const auto* tab = find_table( code, scope, table );
      if( !tab ) return false;

      const key_value_object* obj = db.find<key_value_object, by_scope_primary>(
            boost::make_tuple( tab->id, id ) );

      if( !obj ) return false;

      auto size = db_get_i64( obj, nullptr, 0 );
      EOS_ASSERT( size >= 0, asset_type_exception, "error reading iterator" );

      //using malloc/free here potentially is not exception-safe, although WASM doesn't support exceptions
      void* buffer = max_stack_buffer_size < size_t(size) ? malloc(size_t(size)) : alloca(size_t(size));

      db_get_i64( obj, (char *)buffer, uint32_t(size) );

      datastream<const char*> ds( (char*)buffer, uint32_t(size) );

      if ( max_stack_buffer_size < size_t(size) ) {
         free(buffer);
      }

      fc::raw::unpack(ds, out);

      return true;
   }

private:
   int db_store_i64( uint64_t code,
                     uint64_t scope,
                     uint64_t table,
                     const account_name& payer,
                     uint64_t id,
                     const char *buffer,
                     size_t buffer_size );

   int db_get_i64( const key_value_object* obj , char* buffer, size_t buffer_size ) const;

   const table_id_object *find_table( name code, name scope, name table );
   const table_id_object& find_or_create_table( name code, name scope, name table, const account_name& payer );
   void remove_table( const table_id_object& tid );

public:
   chainbase::database& db;  ///< database where state is stored

   // account_info
   struct account_info {
      account_name name;
      asset available;

      account_info() {}
      account_info( const account_info& ) = default;
      ~account_info() = default;

      explicit account_info( const account_name& n )
         : name(n), available( asset{ 0 } ) {}

      account_info( const account_name& n, const asset& a )
         : name(n), available( a ) {}

      uint64_t primary_key() const { return name; }
   };

   struct assetage {
      asset     staked        = asset{ 0 };
      int64_t   age           = 0;
      uint32_t  update_height = 0;
   };

    struct eoslock_account {
        account_name owner;
        asset     balance;

        uint64_t primary_key()const { return owner; }
    };

   struct vote4ram_info {
      account_name voter;
      asset staked = asset{0};
      uint64_t primary_key() const { return voter; }
   };

   struct votefix_info {
      uint64_t     key                = 0;
      account_name voter              = 0;
      account_name bpname             = 0;
      name         fvote_typ          = name{ 0 };
      assetage     votepower_age;
      asset        vote               = asset{ 0 };
      uint32_t     start_block_num    = 0;
      uint32_t     withdraw_block_num = 0;
      bool         is_withdraw        = false;

      uint64_t primary_key() const { return key; }
   };

   struct bp_info {
      account_name    name;
      public_key_type producer_key;
      uint32_t        commission_rate;
      int64_t         total_staked             = 0;
      asset           rewards_pool;
      int64_t         total_voteage            = 0;
      uint32_t        voteage_update_height    = 0;
      std::string     url;
      bool            emergency                = false;

      bp_info() : commission_rate(0) {
      }

      bp_info( const account_name& name,
               const public_key_type& pub_key,
               const uint32_t& rate,
               const std::string& url ) :
            name(name),
            producer_key(pub_key),
            commission_rate(rate),
            url(url) {
      }

      uint64_t primary_key() const { return name; }
   };

   struct chain_status {
      account_name name;
      bool emergency = false;

      uint64_t primary_key() const { return name; }
   };

   // currency_stats
   struct currency_stats {
      asset supply;
      asset max_supply;
      account_name issuer;

      uint64_t primary_key() const { return supply.get_symbol().to_symbol_code(); }
   };

   // vote_info
   struct vote_info {
      account_name    bpname         = 0;
      asset           staked         = asset{0};
      uint32_t        voteage_update_height = 0;
      int64_t         voteage        = 0;
      asset           unstaking      = asset{0};
      uint32_t        unstake_height = 0;

      uint64_t        primary_key() const { return bpname; }
   };

};

// some imp same as api in wasm interface
void eosio_contract_assert( bool condition, const char* msg );

// native_multi_index a simple multi index interface for call in cpp
template<uint64_t TableName, typename T>
class native_multi_index{
private:
   name current_receiver() {
      return _ctx.get_receiver();
   }

private:
   constexpr static bool validate_table_name( uint64_t n ) {
      // Limit table names to 12 characters so that the last character (4 bits) can be used to distinguish between the secondary indices.
      return (n & 0x000000000000000FULL) == 0;
   }
   constexpr static size_t max_stack_buffer_size = 512;
   static_assert( validate_table_name(TableName), "multi_index does not support table names with a length greater than 12");

   apply_context &_ctx;

   uint64_t _code;
   uint64_t _scope;

public:
   native_multi_index( apply_context &ctx, uint64_t code, uint64_t scope )
   :_ctx(ctx), _code(code), _scope(scope)
   {}

   uint64_t get_code()const  { return _code; }
   uint64_t get_scope()const { return _scope; }

   const bool get( uint64_t primary, T& out, const char* error_msg = "unable to find key" )const {
      auto ok = find( primary, out );
      eosio_contract_assert( ok, error_msg );
      return ok;
   }

   void load_object_by_primary_iterator( int32_t itr, T& out )const {
      auto size = _ctx.db_get_i64( itr, nullptr, 0 );
      eosio_contract_assert( size >= 0, "error reading iterator" );

      //using malloc/free here potentially is not exception-safe, although WASM doesn't support exceptions
      void* buffer = max_stack_buffer_size < size_t(size) ? malloc(size_t(size)) : alloca(size_t(size));

      _ctx.db_get_i64( itr, (char *)buffer, uint32_t(size) );

      datastream<const char*> ds( (char*)buffer, uint32_t(size) );

      if ( max_stack_buffer_size < size_t(size) ) {
         free(buffer);
      }

      fc::raw::unpack(ds, out);
   }

   bool find( uint64_t primary, T& out )const {
      return get_by_itr( find_itr(primary), out );
   }

   int32_t find_itr( uint64_t primary ) const {
      return _ctx.db_find_i64( _code, _scope, TableName, primary );
   }

   bool get_by_itr(const int32_t &itr, T& out)const{
      if( itr < 0 ) return false;
      load_object_by_primary_iterator( itr, out );
      return true;
   }

   template<typename Lambda>
   void modify( const int32_t &itr, const T& obj, uint64_t payer, Lambda&& updater ) {
      eosio_contract_assert( _code == current_receiver().value,
            "cannot modify objects in table of another contract" );
      // Quick fix for mutating db using multi_index that shouldn't allow mutation. Real fix can come in RC2.

      auto pk = obj.primary_key();

      auto& mutableobj = const_cast<T&>(obj); // Do not forget the auto& otherwise it would make a copy and thus not update at all.
      updater( mutableobj );

      eosio_contract_assert( pk == obj.primary_key(), "updater cannot change primary key when modifying an object" );

      datastream<size_t> ps;
      fc::raw::pack(ps, obj);
      size_t size = ps.tellp();

      //using malloc/free here potentially is not exception-safe, although WASM doesn't support exceptions
      void* buffer = max_stack_buffer_size < size ? malloc(size) : alloca(size);

      datastream<char*> ds( (char*)buffer, size );
      //ds << obj;
      fc::raw::pack(ds, obj);

      _ctx.db_update_i64( itr, payer, (char*)buffer, size );

      if ( max_stack_buffer_size < size ) {
         free( buffer );
      }
   }
};


} } // namespace eosio::chain

FC_REFLECT(eosio::chain::memory_db::bp_info, (name)(producer_key)
            (commission_rate)(total_staked)(rewards_pool)(total_voteage)(voteage_update_height)(url)(emergency))
FC_REFLECT(eosio::chain::memory_db::account_info, (name)(available))
FC_REFLECT(eosio::chain::memory_db::eoslock_account, (owner)(balance))
FC_REFLECT(eosio::chain::memory_db::chain_status, (name)(emergency))
FC_REFLECT(eosio::chain::memory_db::currency_stats, (supply)(max_supply)(issuer))
FC_REFLECT(eosio::chain::memory_db::vote_info,
           (bpname)(staked)(voteage)(voteage_update_height)(unstaking)(unstake_height))
FC_REFLECT(eosio::chain::memory_db::vote4ram_info, (voter)(staked))
FC_REFLECT(eosio::chain::memory_db::assetage, (staked)(age)(update_height))
FC_REFLECT(eosio::chain::memory_db::votefix_info, 
               (key)(voter)(bpname)(fvote_typ)(votepower_age)(vote)
               (start_block_num)(withdraw_block_num)(is_withdraw))