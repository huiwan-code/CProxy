#pragma once
#include <memory>
#include <mutex>
#include "Buffer.h"
#include "Conn.h"
#include "EventLoop.h"
#include "Msg.h"

class CtlConn : public Conn, public std::enable_shared_from_this<CtlConn> {
 public:
  typedef std::shared_ptr<CtlConn> SP_CtlConn;
  typedef std::function<void(void*, SP_CtlConn)> MsgHandler;
  CtlConn(int fd, SP_EventLoop loop)
      : Conn(fd, loop), out_buffer_(new Buffer(1024, 65536)), mutex_() {
    channel_->setEvents(EPOLLET | EPOLLIN | EPOLLOUT | EPOLLRDHUP);
    channel_->setReadHandler(std::bind(&CtlConn::handleRead, this));
    channel_->setWriteHandler(std::bind(&CtlConn::handleWrite, this));
    channel_->setPostHandler(std::bind(&CtlConn::postHandle, this));
  };
  ~CtlConn() { printf("ctl killing\n"); }
  int getFd() { return fd_; }
  void setNewCtlReqHandler(MsgHandler handler) { newCtlReqHandler_ = handler; }
  void setNewCtlRspHandler(MsgHandler handler) { newCtlRspHandler_ = handler; }
  void setNewTunnelReqHandler(MsgHandler handler) { newTunnelReqHandler_ = handler; }
  void setNewTunnelRspHandler(MsgHandler handler) { newTunnelRspHandler_ = handler; }
  void setCloseHandler_(std::function<void(SP_CtlConn conn)> handler) { closeHandler_ = handler; }
  void setNotifyClientNeedProxyHandler(MsgHandler handler) {
    notifyClientNeedProxyHandler_ = handler;
  }
  void setNotifyProxyShutdownPeerConnHandler_(MsgHandler handler) {
    notifyProxyShutdownPeerConnHandler_ = handler;
  }
  void setFreeProxyConnReqHandler_(MsgHandler handler) { freeProxyConnReqHandler_ = handler; }
  void send_msg(CtlMsg& msg);
  void set_ctl_id(std::string id) { ctl_id_ = id; }
  std::string get_ctl_id() { return ctl_id_; }

 private:
  SP_Buffer out_buffer_;
  std::string ctl_id_;
  void handleRead();
  void handleWrite();
  void postHandle();
  std::mutex mutex_;
  std::function<void(SP_CtlConn conn)> closeHandler_ = [](SP_CtlConn) {};
  MsgHandler newTunnelReqHandler_ = [](void*, SP_CtlConn) {};
  MsgHandler newTunnelRspHandler_ = [](void*, SP_CtlConn) {};
  MsgHandler newCtlReqHandler_ = [](void*, SP_CtlConn) {};
  MsgHandler newCtlRspHandler_ = [](void*, SP_CtlConn) {};
  MsgHandler notifyClientNeedProxyHandler_ = [](void*, SP_CtlConn) {};
  MsgHandler notifyProxyShutdownPeerConnHandler_ = [](void*, SP_CtlConn) {};
  MsgHandler freeProxyConnReqHandler_ = [](void*, SP_CtlConn) {};
};

typedef std::shared_ptr<CtlConn> SP_CtlConn;
