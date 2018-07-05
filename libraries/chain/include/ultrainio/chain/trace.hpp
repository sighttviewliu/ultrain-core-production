/**
 *  @file
 *  @copyright defined in ultrain/LICENSE.txt
 */
#pragma once

#include <ultrainio/chain/action.hpp>
#include <ultrainio/chain/action_receipt.hpp>
#include <ultrainio/chain/block.hpp>

namespace ultrainio { namespace chain {

   struct base_action_trace {
      base_action_trace( const action_receipt& r ):receipt(r){}
      base_action_trace(){}

      action_receipt       receipt;
      action               act;
      fc::microseconds     elapsed;
      uint64_t             cpu_usage = 0;
      string               console;
      string               return_value;

      uint64_t             total_cpu_usage = 0; /// total of inline_traces[x].cpu_usage + cpu_usage
      transaction_id_type  trx_id; ///< the transaction that generated this action
   };

   struct action_trace : public base_action_trace {
      using base_action_trace::base_action_trace;

      vector<action_trace> inline_traces;
   };

   struct transaction_trace;
   using transaction_trace_ptr = std::shared_ptr<transaction_trace>;

   struct transaction_trace {
      transaction_id_type                        id;
      fc::optional<transaction_receipt_header>   receipt;
      fc::microseconds                           elapsed;
      uint64_t                                   net_usage = 0;
      bool                                       scheduled = false;
      vector<action_trace>                       action_traces; ///< disposable

      transaction_trace_ptr                      failed_dtrx_trace;
      fc::optional<fc::exception>                except;
      std::exception_ptr                         except_ptr;
   };

   struct block_trace {
      fc::microseconds                elapsed;
      uint64_t                        billed_cpu_usage_us;
      vector<transaction_trace_ptr>   trx_traces;
   };
   using block_trace_ptr = std::shared_ptr<block_trace>;

} }  /// namespace ultrainio::chain

FC_REFLECT( ultrainio::chain::base_action_trace,
                    (receipt)(act)(elapsed)(cpu_usage)(console)(total_cpu_usage)(trx_id)(return_value) )

FC_REFLECT_DERIVED( ultrainio::chain::action_trace,
                    (ultrainio::chain::base_action_trace), (inline_traces) )

FC_REFLECT( ultrainio::chain::transaction_trace, (id)(receipt)(elapsed)(net_usage)(scheduled)
                                             (action_traces)(failed_dtrx_trace)(except) )
FC_REFLECT( ultrainio::chain::block_trace, (elapsed)(billed_cpu_usage_us)(trx_traces) )
