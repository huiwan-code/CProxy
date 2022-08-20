#pragma once
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <iostream>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "public_conn.h"
#include "lib/channel.h"
#include "lib/event_loop_thread.h"
#include "lib/event_loop_thread_pool.h"
#include "lib/proxy_conn.h"
#include "lib/util.h"

class Control;

class Tunnel {
 public:
  typedef std::shared_ptr<Control> SP_Control;
  Tunnel(std::string tun_id, SP_EventLoopThread listenThread, SP_EventLoopThreadPool workThreadPool,
         SP_Control ctl)
      : tun_id_(tun_id),
        listen_fd_(socketBindListen(0)),
        listen_thread_(listenThread),
        work_pool_(workThreadPool),
        ctl_(ctl),
        public_fds_mutex_(),
        free_proxy_conns_mutex_() {
    struct sockaddr_in listenAddr;
    socklen_t listenAddrLen = sizeof(listenAddr);
    getsockname(listen_fd_, (struct sockaddr*)&listenAddr, &listenAddrLen);
    listen_port_ = ntohs(listenAddr.sin_port);
    listen_addr_ = inet_ntoa(listenAddr.sin_addr);
    acceptor_ = SP_Channel(new Channel(listen_fd_));
    acceptor_->SetEvents(EPOLLIN | EPOLLET | EPOLLRDHUP);
    acceptor_->SetReadHandler(std::bind(&Tunnel::newPublicConnHandler, this));
    listen_thread_->AddChannel(acceptor_);
  };

  int getAndPopUndealPublicFd();
  void claimProxyConn(SP_ProxyConn);
  int getListenPort() { return listen_port_; }
  std::string getListenAddr() { return listen_addr_; }
  void shutdownFromPublic(std::string proxy_id, u_int32_t tran_count);
  SP_ProxyConn popFreeProxyConn(bool&);
  void reqStartProxy(int public_fd, SP_ProxyConn proxy_conn);
  void bindPublicFdToProxyConn(int public_fd, SP_ProxyConn proxy_conn);
  void freeProxyConn(std::string);
  void shutdownPublicConn(SP_ProxyConn);
  SP_ProxyConn getProxyConn(std::string proxy_id, bool& isExist);

 private:
  void newPublicConnHandler();
  void addCtlPendingFunctor(voidFunctor&&);
  void handleStartProxyConnRsp(void*, SP_ProxyConn);
  void handlePeerProxyConnClose(SP_ProxyConn);
  int public_fd_in_;
  int public_fd_finish_;
  std::string tun_id_;
  int listen_fd_;
  std::string listen_addr_;
  int listen_port_;
  SP_EventLoopThread listen_thread_;
  SP_EventLoopThreadPool work_pool_;
  SP_Channel acceptor_;
  SP_Control ctl_;
  std::mutex public_fds_mutex_;
  std::mutex free_proxy_conns_mutex_;
  // std::unordered_map<std::string, SP_ProxyConn> proxy_conn_map_;
  safe_unordered_map<std::string, SP_ProxyConn> proxy_conn_map_;
  safe_unordered_map<std::string, SP_ProxyConn> wait_for_start_proxy_conn_map_;
  std::vector<SP_ProxyConn> free_proxy_conns_;
  std::vector<int> undeal_public_fds_;
};

using SP_Tunnel = std::shared_ptr<Tunnel>;