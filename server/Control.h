#pragma once
#include <memory>
#include <unordered_map>
#include "Tunnel.h"
#include "lib/CtlConn.h"

class Server;

class Control : public std::enable_shared_from_this<Control> {
 public:
  Control(int fd, std::string ctl_id, SP_EventLoop loop, Server* server)
      : ctl_conn_fd_(fd), ctl_id_(ctl_id), loop_(loop), server_(server) {
    // 封装conn
    conn_ = SP_CtlConn(new CtlConn(fd, loop_));
    conn_->set_ctl_id(ctl_id);
    // 设置相关处理函数
    conn_->setNewCtlReqHandler(
        std::bind(&Control::handleNewCtlReq, this, std::placeholders::_1, std::placeholders::_2));
    conn_->setNewTunnelReqHandler(std::bind(&Control::handleNewTunnelReq, this,
                                            std::placeholders::_1, std::placeholders::_2));
    conn_->setCloseHandler_(std::bind(&Control::handleCtlConnClose, this, std::placeholders::_1));
    conn_->setNotifyProxyShutdownPeerConnHandler_(std::bind(
        &Control::handleShutdownPublicConn, this, std::placeholders::_1, std::placeholders::_2));
    conn_->setFreeProxyConnReqHandler_(std::bind(&Control::handleFreeProxyConnReq, this,
                                                 std::placeholders::_1, std::placeholders::_2));
    loop_->addToPoller(conn_->getChannel());
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