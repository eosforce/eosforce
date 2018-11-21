#include <algorithm>
#include <eosio/chain/exceptions.hpp>
#include <eosio/chain/memory_db.hpp>
#include <eosio/chain/authorization_manager.hpp>
#include <eosio/chain/account_object.hpp>
#include <eosio/chain/global_property_object.hpp>
#include <boost/container/flat_set.hpp>

using boost::container::flat_set;

namespace eosio { namespace chain {

const table_id_object *memory_db::find_table( name code, name scope, name table ) {
   return db.find<table_id_object, by_code_scope_table>(boost::make_tuple(code, scope, table));
}

const table_id_object& memory_db::find_or_create_table( name code, name scope, name table, const account_name& payer ) {
   const auto *existing_tid = db.find<table_id_object, by_code_scope_table>(boost::make_tuple(code, scope, table));
   if( existing_tid != nullptr ) {
      return *existing_tid;
   }

   return db.create<table_id_object>([&]( table_id_object& t_id ) {
      t_id.code = code;
      t_id.scope = scope;
      t_id.table = table;
      t_id.payer = payer;
   });
}

void memory_db::remove_table( const table_id_object& tid ) {
   db.remove(tid);
}

// db_store_i64 store data to db, WARNNING!!! this can not change RAM use
int memory_db::db_store_i64(
      uint64_t code,
      uint64_t scope,
      uint64_t table,
      const account_name& payer,
      uint64_t id,
      const char *buffer,
      size_t buffer_size ) {
//   require_write_lock( scope );
   const auto& tab = find_or_create_table(code, scope, table, payer);
   auto tableid = tab.id;

   EOS_ASSERT( payer != account_name(), invalid_table_payer, "must specify a valid account to pay for new record" );

   const auto& obj = db.create<key_value_object>([&]( auto& o ) {
      o.t_id        = tableid;
      o.primary_key = id;
      o.value.resize(buffer_size);
      o.payer       = payer;
      memcpy( o.value.data(), buffer, buffer_size );
   });

   db.modify(tab, [&]( auto& t ) {
      ++t.count;
   });

   return 1;
}

// some imp same as api in wasm interface
void eosio_contract_assert( bool condition, const char* msg ) {
   if( !condition ) {
      std::string message( msg );
      edump((message));
      EOS_THROW( eosio_assert_message_exception,
                 "assertion failure with message: ${s}", ("s",message) );
   }
}


int memory_db::db_get_i64( const key_value_object* obj , char* buffer, size_t buffer_size ) const {
   auto s = obj->value.size();
   if( buffer_size == 0 ) return s;

   auto copy_size = std::min( buffer_size, s );
   memcpy( buffer, obj->value.data(), copy_size );

   return copy_size;
}

} } /// eosio::chain
