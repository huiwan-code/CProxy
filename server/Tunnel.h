#pragma once
#include <iostream>
#include <memory>
#include <unordered_map>
#include <mutex>
#include<sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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
    Tunnel(std::string tun_id, SP_EventLoopThread listenThread, SP_EventLoopThreadPool workThreadPool, SP_Control ctl)
    : tun_id_(tun_id),
      listen_fd_(socketBindListen(0)),
      listen_thread_(listenThread),
      work_pool_(workThreadPool),
      ctl_(ctl),
      mutex_()
       {
        struct sockaddr_in listenAddr;
        socklen_t listenAddrLen = sizeof(listenAddr);
        getsockname(listen_fd_, (struct sockaddr *)&listenAddr, &listenAddrLen);
        listen_port_ = ntohs(listenAddr.sin_port);
        listen_addr_ = inet_ntoa(listenAddr.sin_addr);
        printf("addr: %s:%d\n", listen_addr_.c_str(), listen_port_);
        acceptor_ = SP_Channel(new Channel(listen_fd_));
        acceptor_->setEvents(EPOLLIN | EPOLLET | EPOLLRDHUP);
        acceptor_->setReadHandler(std::bind(&Tunnel::newPublicConnHandler, this));
        listen_thread_->addChannel(acceptor_);
      };
    
    int getAndPopUndealPublicFd();
    void claimProxyConn(SP_ProxyConn);
    int getListenPort() {return listen_port_;}
    std::string getListenAddr() {return listen_addr_;}
  private:
    void newPublicConnHandler();
    std::string tun_id_;
    int listen_fd_;
    std::string listen_addr_;
    int listen_port_;
    SP_EventLoopThread listen_thread_;
    SP_EventLoopThreadPool work_pool_;
    SP_Channel acceptor_;
    SP_Control ctl_;
    std::mutex mutex_;
    std::unordered_map<int, SP_ProxyConn> proxy_conn_map_;
    std::vector<int> undeal_public_fds_;
};

using SP_Tunnel = std::shared_ptr<Tunnel>;