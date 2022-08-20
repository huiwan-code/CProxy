#include <getopt.h>
#include <unistd.h>

#include "server.h"
#include "lib/util.h"
#include "spdlog/spdlog.h"

int main(int argc, char **argv) {
  // 初始化日志格式
  spdlog::set_pattern("[%@ %H:%M:%S:%e %z] [%^%L%$] [thread %t] %v");
  int opt;
  int option_index = 0;
  static struct option long_options[] = {{"ctl_port", required_argument, NULL, 'c'},
                                         {"proxy_port", required_argument, NULL, 'p'},
                                         {"work_thread_nums", required_argument, NULL, 't'},
                                         //  longopts的最后一个元素必须是全0填充，否则会报段错误
                                         {0, 0, 0, 0}};

  u_int32_t ctl_port = 0;
  u_int32_t proxy_port = 0;
  int work_thread_nums = 0;
  while ((opt = getopt_long(argc, argv, "", long_options, &option_index)) != -1) {
    switch (opt) {
      case 'c':
        ctl_port = atoi(optarg);
        break;
      case 'p':
        proxy_port = atoi(optarg);
        break;
      case 't':
        work_thread_nums = atoi(optarg);
        break;
      default:
        break;
    }
  }

  ctl_port = ctl_port > 0 ? ctl_port : 8080;
  proxy_port = proxy_port > 0 ? proxy_port : 8089;
  work_thread_nums = work_thread_nums > 0 ? work_thread_nums : 1;
  printf("ctl_port:%d; proxy_port:%d; work_thread_nums: %d\n", ctl_port, proxy_port,
         work_thread_nums);
  Server server(work_thread_nums, ctl_port, proxy_port);
  server.start();
}