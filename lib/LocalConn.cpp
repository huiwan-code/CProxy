#include <fcntl.h>

#include "LocalConn.h"

void LocalConn::handleRead() {
  int bs = splice(fd_, NULL, pipe_fds_[1], NULL, 4096, SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
  bs = splice(pipe_fds_[0], NULL, peer_conn_fd_, NULL, bs, SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
  printf("[LocalConn]send_bs = %d\n", bs);
}

void LocalConn::postHandle() {
  channel_->setEvents(EPOLLET | EPOLLIN  | EPOLLRDHUP);
  loop_->updatePoller(channel_);
}