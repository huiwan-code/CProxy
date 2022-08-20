#pragma once
#include <sys/types.h>
#include <mutex>
#include <unordered_map>
#include "tunnel.h"
#include "lib/channel.h"
#include "lib/ctl_conn.h"
#include "lib/event_loop.h"
#include "lib/event_loop_thread_pool.h"
#include "lib/proxy_conn.h"
#include "lib/util.h"

class Client : public std::enable_shared_from_this<Client> {
 public:
  Client(int workThreadNum, std::string proxy_server_host, u_int32_t proxy_server_port,
         std::string local_server_host, u_int32_t local_server_port);
  void start();
  void shutdownFromLocal(std::string tun_id, std::string proxy_id, u_int32_t tran_count);
  SP_CtlConn getCtlConn() { return ctl_conn_; }

 private:
  SP_EventLoop loop_;
  SP_EventLoopThreadPool eventLoopThreadPool_;
  std::string proxy_server_host_;
  u_int32_t proxy_server_port_;
  std::string local_server_host_;
  u_int32_t local_server_port_;
  std::string client_id_;
  std::unordered_map<std::string, SP_Tunnel> tunnel_map_;
  SP_CtlConn ctl_conn_;
  void initCtlConn();
  void reqNewCtl();
  void reqNewTunnel();
  void handleNewCtlRsp(void*, SP_CtlConn);
  void handleNewTunnelRsp(void*, SP_CtlConn);
  void handleCtlConnClose(SP_CtlConn conn) { abort(); };
  void handleProxyNotify(void*, SP_CtlConn);
  void handleShutdownLocalConn(void*, SP_CtlConn);
  friend class Tunnel;
};