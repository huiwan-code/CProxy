# 聊聊第一个开源项目 - CProxy

## 初衷
刚学习了C++，对网络比较感兴趣，之前使用过ngrok（GO版本的内网穿透项目），看了部分源码，想把自己的一些优化想法用C++实现一下，练练手，所以便有了这个项目。

## 项目介绍
CProxy是一个反向代理，用户可在自己内网环境中启动一个业务服务，并在同一网络下启动CProxyClient，用于向CProxyServer注册服务。CProxyClient和CProxyServer之间会创建一个隧道，外网可以通过访问CProxyServer，数据转发到CProxyClient，从而被业务服务接收到。实现内网服务被外网访问。

## 使用方法
```
bash build.sh
bash run_server.sh
(另一个终端) bash run_client.sh
```
## 项目亮点
* 使用epoll作为IO多路复用的实现
* 数据转发时，使用splice零拷贝，减少IO性能瓶颈
* 数据连接和控制连接接耦，避免互相影响
* 采用Reactor多线程模型，充分利用多核CPU性能


## 流程架构
### 角色
1. LocalServer: 内网业务服务
2. CProxyClient: CProxy客户端，一般与LocalServer部署在一起，对接CProxyServer和InnerServer
3. CProxyServer: CProxy服务端
4. PublicClient: 业务客户端

### 数据流
PublicClient先将请求打到CProxyServer，CProxyServer识别请求是属于哪个CProxyClient，然后将数据转发到CProxyClient，CProxyClient再识别请求是属于哪个LocalServer的，将请求再转发到LocalServer，完成数据的转发。
![](media/16443911812619/16444013185708.jpg)

### 工作流程

先介绍CProxyServer端的两个概念：

* Control：在CProxyServer中会维护一个ControlMap，一个Control对应一个CProxyClient，存储CProxyClient的一些元信息和控制信息
* Tunnel：每个Control中会维护一个TunnelMap，一个Tunnel对应一个LocalServer服务

在CProxyClient端，也会维护一个TunnelMap，每个Tunnel对应一个LocalServer服务，只不过Client端的Tunnel与Server端的Tunnel存储的内容略有差异

#### 启动流程
##### CProxyServer
1. 完成几种工作线程的初始化。
2. 监听一个CtlPort，等待CProxyClient连接。

##### CProxyClient
1. 完成对应线程的初始化。
2. 然后连接Server的CtlPort，此连接称为ctl_conn, 用于client和server之前控制信息的传递。
3. 请求注册Control，获取ctl_id。
4. 最后再根据Tunnel配置文件完成多个Tunnel的注册。需要注意的是，每注册一个Tunnel，Server端就会多监听一个PublicPort，作为web访问LocalServer的入口。

#### 新连接&&传输数据
1. Web上的PublicClient请求CProxyServer上的PublicPort建立连接；CProxyServer接收连接请求，将public_accept_fd封装成PublicConn。
2. CProxyServer通过ctl_conn向client发送NotifyClientNeedProxyMsg通知Client需要创建一个proxy。
3. Client收到后，会分别连接LocalServer和CProxyServer：
    3.1. 连接LocalServer，将local_conn_fd封装成LocalConn。
    3.2. 连接ProxyServer的ProxyPort，将proxy_conn_fd封装成ProxyConn，并将LocalConn和ProxyConn绑定。
4. CProxyServer的ProxyPort收到请求后，将proxy_accept_fd封装成ProxyConn，将ProxyConn与PublicConn绑定。
5. 此后的数据在PublicConn、ProxyConn和LocalConn上完成转发传输。


![](media/16443911812619/16444776861013.jpg)

当前设计中，并没有复用ProxyConn，每次新连接都会重新创建ProxyConn，可能会有以下问题：
1. Client中频繁主动断开连接（LocalConn和ProxyConn），当CProxyClient所在机器上的LocalServer访问并发量大时，会导致有大量TIME_WAIT状态的连接。
2. 增大连接延迟，每次创建ProxyConn都需要三次握手创建连接，CProxyClient和CProxyServer处于不同的网络环境，可能会有延迟隐患。

解决办法：
1. 针对第一个问题，简单的处理方法可以先把client所在机器的tcp_tw_reuse设为1。
2. 复用ProxyConn，维护一个FreeProxyConnPool，每次新连接先从Pool中获取空闲ProxyConn，用完后再放回去；Server端和Client端都需要定期发送心跳保证连接的可用，当时嫌这个连接保活设计增加系统复杂度，所以就没搞了。。（就是懒）

## 数据传输
数据在Server和Client都需进行转发，将数据从一个连接的接收缓冲区转发到另一个连接的发送缓冲区。如果使用write/read系统调用，整个流程如下图
![](media/16443911812619/16444826632416.jpg)

数据先从内核空间复制到用户空间，之后再调用write系统调用将数据复制到内核空间。每次系统调用，都需要切换CPU上下文，而且，两次拷贝都需要CPU去执行(CPU copy)，所以，大量的拷贝操作，会成为整个服务的性能瓶颈。

在CProxy中，使用splice的零拷贝方案，数据直接从内核空间的Source Socket Buffer转移到Dest Socket Buffer，不需要任何CPU copy。

![](media/16443911812619/16444936049292.jpg)

splice通过pipe管道“传递”数据，基本原理是通过pipe管道修改source socket buffer和dest socket buffer的物理内存页

![](media/16443911812619/16445036778379.jpg)

splice并不涉及数据的实际复制，只是修改了socket buffer的物理内存页指针。
## 并发模型
CProxyClient和CProxyServer均采用多线程reactor模型，利用线程池提高并发度。并使用epoll作为IO多路复用的实现方式。每个线程都有一个事件循环（One loop per thread）。线程分多类，各自处理不同的连接读写。

### CProxyServer端
为了避免业务连接处理影响到Client和Server之间控制信息的传递。我们将业务数据处理与控制数据处理解耦。在Server端中设置了三种线程：
1. mainThread: 用于监听ctl_conn和proxy_conn的连接请求以及ctl_conn上的相关读写
2. publicListenThread: 监听并接收外来连接
3. eventLoopThreadPool: 线程池，用于处理public_conn和proxy_conn之间的数据交换。

### CProxyClient端
client端比较简单，只有两种线程：
1. mainThread: 用于处理ctl_conn的读写
2. eventLoopThreadPool: 线程池，用于处理proxy_conn和local_conn之间的数据交换

![](media/16443911812619/16445815553782.jpg)
![](media/16443911812619/16447163153206.jpg)

## TODO
- [ ] 当前没有处理连接的断开，已有初步方案，确保连接优雅断开
- [ ] 压测

