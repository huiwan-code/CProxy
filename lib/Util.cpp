#include <arpa/inet.h>
#include <errno.h>
#include <execinfo.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <ctime>
#include <random>

#include "spdlog/spdlog.h"

int socketBindListen(int port) {
  if (port < 0 || port > 65535) {
    return -1;
  }

  // 设置reuseaddr
  int listen_fd = 0;
  if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) return -1;
  int optval = 1;
  if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
    close(listen_fd);
    return -1;
  }

  // bind
  struct sockaddr_in server_addr;
  bzero((char *)&server_addr, sizeof(server_addr));
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    close(listen_fd);
    return -1;
  }

  // listen：2048代表backlog，用于确定半链接和全链接队列的大小
  if (listen(listen_fd, 2048) < 0) {
    close(listen_fd);
    return -1;
  }

  return listen_fd;
}

// 忽略sigpipe信号，当收到rst包后，继续往关闭的socket中写数据，会触发SIGPIPE信号, 该信号默认结束进程
void ignoreSigpipe() {
  struct sigaction sa;
  // 初始化变量
  memset(&sa, '\0', sizeof(sa));
  sa.sa_handler = SIG_IGN;
  sa.sa_flags = 0;
  sigaction(SIGPIPE, &sa, NULL);
};

int setfdNonBlock(int fd) {
  int flag = fcntl(fd, F_GETFL, 0);
  if (flag == -1) return -1;
  flag |= O_NONBLOCK;
  if (fcntl(fd, F_SETFL, flag) == -1) return -1;
  return 0;
};

size_t readn(int fd, char *buffer, size_t size, bool &bufferEmpty) {
  char *buffer_ptr = buffer;
  int len_left = size;
  while (len_left > 0) {
    int readNum = read(fd, buffer_ptr, len_left);
    if (readNum < 0) {
      // EINTR：当阻塞于某个慢系统调用的一个进程捕获某个信号且相应信号处理函数返回时，该系统调用可能返回一个EINTR错误
      // EAGAIN:
      // 以O_NONBLOCK的标志打开文件/socket/FIFO，如果你连续做read操作而没有数据可读。此时程序不会阻塞起来等待数据准备就绪返回，read函数会返回一个错误EAGAIN
      if (errno == EINTR || errno == EAGAIN) {
        continue;
      } else {
        return -1;
      }
    } else if (readNum == 0) {  // 读到EOF，对端发送fin包
      bufferEmpty = true;
      break;
    }
    len_left -= readNum;
    buffer_ptr += readNum;
  }
  return size - len_left;
}

// 尽量写入size大小的数据，当缓冲区满时会直接返回
size_t writen(int fd, const char *buffer, size_t size) {
  size_t len_left = size;
  const char *buffer_ptr = buffer;
  while (len_left > 0) {
    size_t writeNum = write(fd, buffer_ptr, len_left);
    if (writeNum == 0) {
      if (errno == EINTR) {
        continue;
      } else if (errno == EAGAIN) {
        // 缓冲区满了，直接退出，让外层决定如何处理
        break;
      } else {
        return -1;
      }
    }

    buffer_ptr += writeNum;
    len_left -= writeNum;
  }
  return size - len_left;
}

int tcp_connect(const char *host, u_int32_t server_port) {
  sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(sockaddr_in));
  server_addr.sin_family = AF_INET;
  in_addr_t s_addr = inet_addr(host);
  if (s_addr == INADDR_NONE) {
    hostent *h = gethostbyname(host);
    if (h == NULL) {
      return -1;
    }
    memcpy(&s_addr, h->h_addr, h->h_length);
  }
  server_addr.sin_addr.s_addr = s_addr;
  server_addr.sin_port = htons(server_port);
  int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (sock_fd < 0) {
    return sock_fd;
  }

  if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    close(sock_fd);
    return -1;
  }
  if (setfdNonBlock(sock_fd)) {
    SPDLOG_CRITICAL("设置非阻塞失败: {}", strerror(errno));
  };
  return sock_fd;
}

std::string rand_str(int len) {
  char tmp;                                 // tmp: 暂存一个随机数
  std::string buffer;                       // buffer: 保存返回值
  std::random_device rd;                    // 产生一个 std::random_device 对象 rd
  std::default_random_engine random(rd());  // 用 rd 初始化一个随机数发生器 random

  for (int i = 0; i < len; i++) {
    tmp = random() % 36;  // 随机一个小于 36 的整数，0-9、A-Z 共 36 种字符
    if (tmp < 10) {       // 如果随机数小于 10，变换成一个阿拉伯数字的 ASCII
      tmp += '0';
    } else {  // 否则，变换成一个大写字母的 ASCII
      tmp -= 10;
      tmp += 'A';
    }
    buffer += tmp;
  }
  return buffer;
};

void parse_host_port(char *addr, std::string &host, u_int32_t &port) {
  char addr_arr[100];
  strcpy(addr_arr, addr);
  char *splitPtr = strtok(addr_arr, ":");
  char *split_ret[2];
  for (int i = 0; i < 2; i++) {
    split_ret[i] = splitPtr;
    splitPtr = strtok(NULL, ":");
    if (splitPtr == nullptr) {
      break;
    }
  }
  host = split_ret[0];
  port = atoi(split_ret[1]);
};