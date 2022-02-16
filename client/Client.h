#pragma once
#include <sys/types.h>
#include <unordered_map>
#include <mutex>
#include "lib/EventLoop.h"
#include "lib/EventLoopThreadPool.h"
#include "lib/Channel.h"
#include "lib/CtlConn.h"
#include "lib/ProxyConn.h"

struct Tunnel {
  std::string tun_id;
  u_int32_t local_server_port;
  u_int32_t proxy_server_port;
  std::string proxy_server_addr;
  std::unordered_map<int, SP_ProxyConn> proxy_conn_map;
  std::mutex mutex;
};

using SP_Tunnel = std::shared_ptr<Tunnel>;

class Client : public std::enable_shared_from_this<Client> {
  public:
    Client(int workThreadNum, char *proxy_server_host, u_int32_t proxy_server_port, char *local_server_host, u_int32_t local_server_port);
    void start();
  private:
    void initCtlConn();
    void reqNewCtl();
    void reqNewTunnel();
    void handleNewCtlRsp(void*, SP_CtlConn);
    void handleNewTunnelRsp(void*, SP_CtlConn);
    void handleCtlConnClose(SP_CtlConn conn) {abort();};
    void handleProxyNotify(void*, SP_CtlConn);
    int ctl_conn_fd_;
    std::string client_id_;
    char *proxy_server_host_;
    u_int32_t proxy_server_port_;
    char *local_server_host_;
    u_int32_t local_server_port_;
    std::unordered_map<std::string, SP_Tunnel> tunnel_map_;
    SP_EventLoop loop_;
    SP_EventLoopThreadPool eventLoopThreadPool_;
    SP_CtlConn ctl_conn_;
};