#pragma once
#include <memory>
#include <unordered_map>
#include "tunnel.h"
#include "lib/ctl_conn.h"

class Server;

class Control : public std::enable_shared_from_this<Control> {
 public:
  Control(int fd, std::string ctl_id, SP_EventLoop loop, Server* server)
      : ctl_conn_fd_(fd), ctl_id_(ctl_id), loop_(loop), server_(server) {
    // 封装conn
    conn_ = SP_CtlConn(new CtlConn(fd, loop_));
    conn_->SetCtlId(ctl_id);
    // 设置相关处理函数
    conn_->SetNewCtlReqHandler(
        std::bind(&Control::handleNewCtlReq, this, std::placeholders::_1, std::placeholders::_2));
    conn_->SetNewTunnelReqHandler(std::bind(&Control::handleNewTunnelReq, this,
                                            std::placeholders::_1, std::placeholders::_2));
    conn_->SetCloseHandler(std::bind(&Control::handleCtlConnClose, this, std::placeholders::_1));
    conn_->SetNotifyProxyShutdownPeerConnHandler(std::bind(
        &Control::handleShutdownPublicConn, this, std::placeholders::_1, std::placeholders::_2));
    conn_->SetFreeProxyConnReqHandler(std::bind(&Control::handleFreeProxyConnReq, this,
                                                 std::placeholders::_1, std::placeholders::_2));
    loop_->AddToPoller(conn_->GetChannel());
  };
  void upsertTunnel(std::string tun_id, SP_Tunnel tun) { tunnel_map_.add(tun_id, tun); };
  SP_CtlConn getCtlConn() { return conn_; }
  void notifyClientNeedProxy(std::string tun_id);
  void shutdownFromPublic(std::string tun_id, std::string proxy_id, u_int32_t tran_count);

 private:
  int ctl_conn_fd_;
  SP_CtlConn conn_;
  std::string ctl_id_;
  SP_EventLoop loop_;
  Server* server_;
  safe_unordered_map<std::string, SP_Tunnel> tunnel_map_;
  void handleNewCtlReq(void*, SP_CtlConn);
  void handleNewTunnelReq(void*, SP_CtlConn);
  void handleCtlConnClose(SP_CtlConn conn);
  void handleShutdownPublicConn(void*, SP_CtlConn);
  void handleFreeProxyConnReq(void*, SP_CtlConn);
  friend class Server;
};

using SP_Control = std::shared_ptr<Control>;