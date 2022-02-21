#include<unistd.h>
#include <fcntl.h>

#include "PublicConn.h"
#include "Tunnel.h"

void PublicConn::handleRead() {
  int bs = splice(fd_, NULL, pipe_fds_[1], NULL, 4096, SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
  if (bs == 0) {
    // 收到fin包,通知proxy
    printf("[%s]public_fd: %d proxy_id: %s get fin \n", getNowTime(),fd_, proxy_id_.c_str());
    tun_->shutdownFromPublic(proxy_id_);
    closing_ = true;
  }
  bs = splice(pipe_fds_[0], NULL, peer_conn_fd_, NULL, bs, SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
  printf("public->proxy: %s: %d\n", proxy_id_.c_str(), bs);
}

void PublicConn::postHandle() {
  if (closing_) {
    return;
  }
  channel_->setEvents(EPOLLET | EPOLLIN  | EPOLLRDHUP);
  loop_->updatePoller(channel_);
}