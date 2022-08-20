#pragma once
#include <memory>

#include "local_conn.h"
#include "lib/ctl_conn.h"
#include "lib/event_loop_thread_pool.h"
#include "lib/proxy_conn.h"
#include "lib/util.h"

class Client;

class Tunnel {
 public:
  Tunnel(std::string tun_id, std::string local_server_host, u_int32_t local_server_port,
         std::string proxy_server_host, u_int32_t proxy_server_port, Client* client,
         SP_EventLoopThreadPool workThreadPool)
      : tun_id_(tun_id),
        local_server_host_(local_server_host),
        local_server_port_(local_server_port),
        proxy_server_host_(proxy_server_host),
        proxy_server_port_(proxy_server_port),
        work_pool_(workThreadPool),
        client_(client){};

  safe_unordered_map<std::string, SP_ProxyConn> proxy_conn_map;
  std::string getValidProxyID();
  void shutdownFromLocal(std::string proxy_id, u_int32_t tran_count);
  SP_ProxyConn createProxyConn(u_int32_t proxy_port);
  SP_LocalConn createLocalConn(SP_ProxyConn);
  void shutdonwLocalConn(SP_ProxyConn);
  int local_fd_created_;
  int local_fd_finished_;

 private:
  std::string tun_id_;
  std::string local_server_host_;
  u_int32_t local_server_port_;
  std::string proxy_server_host_;
  u_int32_t proxy_server_port_;
  SP_EventLoopThreadPool work_pool_;
  Client* client_;
  void handleStartProxyConnReq(void*, SP_ProxyConn);
};

typedef std::shared_ptr<Tunnel> SP_Tunnel;