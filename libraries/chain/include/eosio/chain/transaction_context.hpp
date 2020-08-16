#pragma once
#include <eosio/chain/controller.hpp>
#include <eosio/chain/trace.hpp>
#include <eosio/chain/platform_timer.hpp>
#include <signal.h>

namespace eosio { namespace chain {

   struct transaction_checktime_timer {
      public:
         transaction_checktime_timer() = delete;
         transaction_checktime_timer(const transaction_checktime_timer&) = delete;
         transaction_checktime_timer(transaction_checktime_timer&&) = default;
         ~transaction_checktime_timer();

         void start(fc::time_point tp);
         void stop();

         /* Sets a callback for when timer expires. Be aware this could might fire from a signal handling context and/or
            on any particular thread. Only a single callback can be registered at once; trying to register more will
            result in an exception. Use nullptr to disable a previously set callback. */
         void set_expiration_callback(void(*func)(void*), void* user);

         std::atomic_bool& expired;
      private:
         platform_timer& _timer;

         transaction_checktime_timer(platform_timer& timer);
         friend controller_impl;
   };

   class transaction_context {
      private:
         void init( uint64_t initial_net_usage);

      public:

         transaction_context( controller& c,
                              const signed_transaction& t,
                              const transaction_id_type& trx_id,
                              transaction_checktime_timer&& timer,
                              fc::time_point start = fc::time_point::now() );

         void init_for_implicit_trx( uint64_t initial_net_usage = 0 );

         void init_for_input_trx( uint64_t packed_trx_unprunable_size,
                                  uint64_t packed_trx_prunable_size,
                                  bool skip_recording);

         void init_for_deferred_trx( fc::time_point published );

         // make_fee_act insert onfee act in trx
         void make_fee_act( const asset& require_fee );

         void exec();
         void finalize();
         void squash();
         void undo();

         inline void add_net_usage( uint64_t u ) { net_usage += u; check_net_usage(); }

         void check_net_usage()const;

         void checktime()const;

         void pause_billing_timer();
         void resume_billing_timer();

         uint32_t update_billed_cpu_time( fc::time_point now );

         std::tuple<int64_t, int64_t, bool, bool> max_bandwidth_billed_accounts_can_pay( bool force_elastic_limits = false )const;

         void validate_referenced_accounts( const transaction& trx, bool enforce_actor_whitelist_blacklist )const;

      private:

         friend struct controller_impl;
         friend class apply_context;

         inline bool is_need_fee_action() const { return fee_payer != name{}; }
         const action mk_fee_action( const action& act, const asset& fee ) const;
         uint32_t schedule_fee_action( const action& act, const account_name receiver );
         void make_limit_by_contract( const asset &fee_ext );
         void add_limit_by_fee( const action &act );

         void add_ram_usage( account_name account, int64_t ram_delta );

         action_trace& get_action_trace( uint32_t action_ordinal );
         const action_trace& get_action_trace( uint32_t action_ordinal )const;

         /** invalidates any action_trace references returned by get_action_trace */
         uint32_t schedule_action( const action& act, account_name receiver, bool context_free,
                                   uint32_t creator_action_ordinal, uint32_t closest_unnotified_ancestor_action_ordinal );

         /** invalidates any action_trace references returned by get_action_trace */
         uint32_t schedule_action( action&& act, account_name receiver, bool context_free,
                                   uint32_t creator_action_ordinal, uint32_t closest_unnotified_ancestor_action_ordinal );

         /** invalidates any action_trace references returned by get_action_trace */
         uint32_t schedule_action( uint32_t action_ordinal, account_name receiver, bool context_free,
                                   uint32_t creator_action_ordinal, uint32_t closest_unnotified_ancestor_action_ordinal );

         void execute_action( uint32_t action_ordinal, uint32_t recurse_depth );

         void schedule_transaction();
         void record_transaction( const transaction_id_type& id, fc::time_point_sec expire );

         void validate_cpu_usage_to_bill( int64_t billed_us, int64_t account_cpu_limit, bool check_minimum )const;
         void validate_account_cpu_usage( int64_t billed_us, int64_t account_cpu_limit, bool estimate )const;

         void disallow_transaction_extensions( const char* error_msg )const;

      /// Fields:
      public:

         controller&                   control;
         const signed_transaction&     trx;
         transaction_id_type           id;
         optional<chainbase::database::session>  undo_session;
         transaction_trace_ptr         trace;
         fc::time_point                start;

         fc::time_point                published;


         vector<action_receipt>        executed;
         flat_set<account_name>        bill_to_accounts;
         flat_set<account_name>        validate_ram_usage;

         /// the maximum number of virtual CPU instructions of the transaction that can be safely billed to the billable accounts
         uint64_t                      initial_max_billable_cpu = 0;

         fc::microseconds              delay;
         bool                          is_input           = false;
         bool                          apply_context_free = true;
         bool                          enforce_whiteblacklist = true;

         fc::time_point                deadline = fc::time_point::maximum();
         fc::microseconds              leeway = fc::microseconds( config::default_subjective_cpu_leeway_us );
         int64_t                       billed_cpu_time_us = 0;
         bool                          explicit_billed_cpu_time = false;

         transaction_checktime_timer   transaction_timer;

         // some info to process fee for eosc
         account_name                  fee_payer      = name{};
         asset                         fee_costed     = asset{0};
         asset                         max_fee_to_pay = asset{0};

      private:
         bool                          is_initialized = false;


         uint64_t                      net_limit = 0;
         bool                          net_limit_due_to_block = true;
         bool                          net_limit_due_to_greylist = false;
         uint64_t                      eager_net_limit = 0;
         uint64_t&                     net_usage; /// reference to trace->net_usage

         bool                          cpu_limit_due_to_greylist = false;

         fc::microseconds              initial_objective_duration_limit;
         fc::microseconds              objective_duration_limit;
         fc::time_point                _deadline = fc::time_point::maximum();
         int64_t                       deadline_exception_code = block_cpu_usage_exceeded::code_value;
         int64_t                       billing_timer_exception_code = block_cpu_usage_exceeded::code_value;
         fc::time_point                pseudo_start;
         fc::microseconds              billed_time;
         fc::microseconds              billing_timer_duration_limit;


         // limit by contract tmp imp, will del when new res manager complate
         uint64_t cpu_limit_by_contract = 0;
         uint64_t net_limit_by_contract = 0;
         bool use_limit_by_contract = false;
   };

} }
