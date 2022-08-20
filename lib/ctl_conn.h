#pragma once
#include <memory>
#include <mutex>
#include "buffer.h"
#include "conn.h"
#include "event_loop.h"
#include "msg.h"

class CtlConn : public Conn, public std::enable_shared_from_this<CtlConn> {
 public:
  typedef std::shared_ptr<CtlConn> SP_CtlConn;
  typedef std::function<void(void*, SP_CtlConn)> MsgHandler;
  CtlConn(int fd, SP_EventLoop loop)
      : Conn(fd, loop), out_buffer_(new Buffer(1024, 65536)), mutex_() {
    channel_->SetEvents(EPOLLET | EPOLLIN | EPOLLOUT | EPOLLRDHUP);
    channel_->SetReadHandler(std::bind(&CtlConn::handleRead, this));
    channel_->SetWriteHandler(std::bind(&CtlConn::handleWrite, this));
    channel_->SetPostHandler(std::bind(&CtlConn::postHandle, this));
  };
  ~CtlConn() { printf("ctl killing\n"); }
  int getFd() { return fd_; }
  void SetNewCtlReqHandler(MsgHandler handler) { new_ctl_req_handler_ = handler; }
  void SetNewCtlRspHandler(MsgHandler handler) { new_ctl_rsp_handler_ = handler; }
  void SetNewTunnelReqHandler(MsgHandler handler) { new_tunnel_req_handler_ = handler; }
  void SetNewTunnelRspHandler(MsgHandler handler) { new_tunnel_rsp_handler_ = handler; }
  void SetCloseHandler(std::function<void(SP_CtlConn conn)> handler) { close_handler_ = handler; }
  void SetNotifyClientNeedProxyHandler(MsgHandler handler) {
    notify_client_need_proxy_handler_ = handler;
  }
  void SetNotifyProxyShutdownPeerConnHandler(MsgHandler handler) {
    notify_proxy_shutdown_peer_conn_handler_ = handler;
  }
  void SetFreeProxyConnReqHandler(MsgHandler handler) { free_proxy_conn_req_handler_ = handler; }
  void SendMsg(CtlMsg& msg);
  void SetCtlId(std::string id) { ctl_id_ = id; }
  std::string GetCtlId() { return ctl_id_; }

 private:
  SP_Buffer out_buffer_;
  std::string ctl_id_;
  void handleRead();
  void handleWrite();
  void postHandle();
  std::mutex mutex_;
  std::function<void(SP_CtlConn conn)> close_handler_ = [](SP_CtlConn) {};
  MsgHandler new_tunnel_req_handler_ = [](void*, SP_CtlConn) {};
  MsgHandler new_tunnel_rsp_handler_ = [](void*, SP_CtlConn) {};
  MsgHandler new_ctl_req_handler_ = [](void*, SP_CtlConn) {};
  MsgHandler new_ctl_rsp_handler_ = [](void*, SP_CtlConn) {};
  MsgHandler notify_client_need_proxy_handler_ = [](void*, SP_CtlConn) {};
  MsgHandler notify_proxy_shutdown_peer_conn_handler_ = [](void*, SP_CtlConn) {};
  MsgHandler free_proxy_conn_req_handler_ = [](void*, SP_CtlConn) {};
};

typedef std::shared_ptr<CtlConn> SP_CtlConn;
