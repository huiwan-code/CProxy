#pragma once
#include <string>
#include <sys/types.h>
int socketBindListen(int port);
// 忽略掉sigpipe，避免由此异常导致程序退出
void ignoreSigpipe();

int setfdNonBlock(int);

size_t readn(int fd, char *buffer, size_t size, bool& bufferEmpty);

size_t writen(int fd, const char *buffer, size_t size);

int tcp_connect(const char *host, u_int32_t server_port);

std::string rand_str(int len);