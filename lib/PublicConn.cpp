#include<unistd.h>
#include <fcntl.h>

#include "PublicConn.h"

void PublicConn::handleRead() {
  int bs = splice(fd_, NULL, pipe_fds_[1], NULL, 4096, SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
  bs = splice(pipe_fds_[0], NULL, peer_conn_fd_, NULL, bs, SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
  printf("[PublicConn]send_bs = %d\n", bs);
}

void PublicConn::postHandle() {
  channel_->setEvents(EPOLLET | EPOLLIN  | EPOLLRDHUP);
  loop_->updatePoller(channel_);
}