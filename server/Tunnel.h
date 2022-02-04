#pragma once
#include <iostream>
#include <memory>
#include <unordered_map>
#include <mutex>

#include "lib/Channel.h"
#include "lib/EventLoopThread.h"
#include "lib/EventLoopThreadPool.h"
#include "lib/PublicConn.h"
#include "lib/Util.h"
#include "lib/ProxyConn.h"

class Control;

class Tunnel {
  public:
    typedef std::shared_ptr<Control> SP_Control;
    Tunnel(std::string tun_id, u_int32_t inner_server_port, SP_EventLoopThread listenThread, SP_EventLoopThreadPool workThreadPool, SP_Control ctl)
    : tun_id_(tun_id),
      inner_server_port_(inner_server_port),
      listen_fd_(socketBindListen(8777)),
      listen_thread_(listenThread),
      work_pool_(workThreadPool),
      ctl_(ctl),
      mutex_()
       {
        acceptor_ = SP_Channel(new Channel(listen_fd_));
        acceptor_->setEvents(EPOLLIN | EPOLLET | EPOLLRDHUP);
        acceptor_->setReadHandler(std::bind(&Tunnel::newPublicConnHandler, this));
        listen_thread_->addChannel(acceptor_);
      };
    
    int getAndPopUndealPublicFd();
    void claimProxyConn(SP_ProxyConn);
  private:
    void newPublicConnHandler();
    std::string tun_id_;
    u_int32_t inner_server_port_;
    int listen_fd_;
    SP_EventLoopThread listen_thread_;
    SP_EventLoopThreadPool work_pool_;
    SP_Channel acceptor_;
    SP_Control ctl_;
    std::mutex mutex_;
    std::unordered_map<int, SP_ProxyConn> proxy_conn_map_;
    std::unordered_map<int, SP_PublicConn> public_conn_map_;
    std::vector<int> undeal_public_fds_;
};

using SP_Tunnel = std::shared_ptr<Tunnel>;