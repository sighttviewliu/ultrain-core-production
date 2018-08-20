/**
 *  @file
 *  @copyright defined in ultrain/LICENSE.txt
 */
#pragma once
#include <appbase/application.hpp>
#include <ultrainio/chain_plugin/chain_plugin.hpp>
#include <ultrainio/net_plugin/protocol.hpp>

namespace ultrainio {
   using namespace appbase;

   struct connection_status {
      string            peer;
      bool              connecting = false;
      bool              syncing    = false;
      handshake_message last_handshake;
   };

   class net_plugin : public appbase::plugin<net_plugin>
   {
      public:
        net_plugin();
        virtual ~net_plugin();

        APPBASE_PLUGIN_REQUIRES((chain_plugin))
        virtual void set_program_options(options_description& cli, options_description& cfg) override;

        void plugin_initialize(const variables_map& options);
        void plugin_startup();
        void plugin_shutdown();

        //void   broadcast_block(const chain::signed_block &sb);
        void   broadcast(const ProposeMsg& propose);
        void   broadcast(const EchoMsg& echo);
        void   send_block(const string& ip_addr, const Block& block);
        bool   send_apply(const SyncRequestMessage& msg);
        void   send_last_block_num(const string& ip_addr, const RspLastBlockNumMsg& last_block_num);
        void   stop_sync_block();

        string                       connect( const string& endpoint );
        string                       disconnect( const string& endpoint );
        optional<connection_status>  status( const string& endpoint )const;
        vector<connection_status>    connections()const;

        size_t num_peers() const;
      private:
        std::unique_ptr<class net_plugin_impl> my;
   };

}

FC_REFLECT( ultrainio::connection_status, (peer)(connecting)(syncing)(last_handshake) )
