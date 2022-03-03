## 项目介绍
CProxy是一个反向代理，用户可在自己内网环境中启动一个业务服务，并在同一网络下启动CProxyClient，用于向CProxyServer注册服务。CProxyClient和CProxyServer之间会创建一个隧道，外网可以通过访问CProxyServer，数据转发到CProxyClient，从而被业务服务接收到。实现内网服务被外网访问。

## 使用方法
```
bash build.sh
// 启动服务端
{ProjectDir}/build/server/Server --proxy_port=8090 --work_thread_nums=4
(另一个终端) 
// 启动客户端
{ProjectDir}/build/client/Client --local_server=127.0.0.1:7777 --cproxy_server=127.0.0.1:8080
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
![1](https://files.mdnice.com/user/13956/4c1ae031-d42c-43c4-991f-176a1dab6195.jpg)


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
4. 最后再根据Tunnel配置文件完成多个Tunnel的注册。需要注意的是，每注册一个Tunnel，Server端就会多监听一个PublicPort，作为外部访问LocalServer的入口。

#### 数据转发流程
1. Web上的PublicClient请求CProxyServer上的PublicPort建立连接；CProxyServer接收连接请求，将public_accept_fd封装成PublicConn。
2. CProxyServer通过ctl_conn向client发送NotifyClientNeedProxyMsg通知Client需要创建一个proxy。
3. Client收到后，会分别连接LocalServer和CProxyServer：
    3.1. 连接LocalServer，将local_conn_fd封装成LocalConn。
    3.2. 连接ProxyServer的ProxyPort，将proxy_conn_fd封装成ProxyConn，并将LocalConn和ProxyConn绑定。
4. CProxyServer的ProxyPort收到请求后，将proxy_accept_fd封装成ProxyConn，将ProxyConn与PublicConn绑定。
5. 此后的数据在PublicConn、ProxyConn和LocalConn上完成转发传输。

![2](https://files.mdnice.com/user/13956/0011836a-11d3-46fe-9107-1b31368ff427.jpg)


## 连接管理
### 复用proxy连接
为了避免频繁创建销毁proxy连接，在完成数据转发后，会将proxyConn放到空闲队列中，等待下次使用。
proxy_conn有两种模式 - 数据传输模式和空闲模式。在数据传输模式中，proxy_conn不会去读取解析缓冲区中的数据，只会把数据通过pipe管道转发到local_conn; 空闲模式时，会读取并解析缓冲区中的数据，此时的数据是一些控制信息，用于调整proxy_conn本身。

当有新publicClient连接时，会先从空闲列表中获取可用的proxy_conn，此时proxy_conn处于空闲模式，CProxyServer端会通过proxy_conn向CProxyClient端发送StartProxyConnReqMsg，
CLient端收到后，会为这个proxy_conn绑定一个local_conn, 并将工作模式置为数据传输模式。之后数据在这对proxy_conn上进行转发。
![3](https://files.mdnice.com/user/13956/f3864925-b618-4970-9e7a-2f623daf9bca.jpg)


### 数据连接断开处理
> close和shutdown的区别
> 1. close
> ```c++
> int close(int sockfd)
> ```
> 在不考虑so_linger的情况下，close会关闭两个方向的数据流。
> 1. 读方向上，内核会将套接字设置为不可读，任何读操作都会返回异常；
> 2. 输出方向上，内核会尝试将发送缓冲区的数据发送给对端，之后发送fin包结束连接，这个过程中，往套接字写入数据都会返回异常。
> 3. 若对端还发送数据过来，会返回一个rst报文。
>
> 注意：套接字会维护一个计数，当有一个进程持有，计数加一，close调用时会检查计数，只有当计数为0时，才会关闭连接，否则，只是将套接字的计数减一。
> 2. shutdown
> ```c++
> int shutdown(int sockfd, int howto)
> ```
> shutdown显得更加优雅，能控制只关闭连接的一个方向
> 1. `howto = 0` 关闭连接的读方向，对该套接字进行读操作直接返回EOF；将接收缓冲区中的数据丢弃，之后再有数据到达，会对数据进行ACK，然后悄悄丢弃。
> 2. `howto = 1` 关闭连接的写方向，会将发送缓冲区上的数据发送出去，然后发送fin包；应用程序对该套接字的写入操作会返回异常（shutdown不会检查套接字的计数情况，会直接关闭连接）
> 3. `howto = 2` 0+1各操作一遍，关闭连接的两个方向。

项目使用shutdown去处理数据连接的断开，当CProxyServer收到publicClient的fin包(CProxyClient收到LocalServer的fin包)后，通过ctlConn通知对端，
对端收到后，调用shutdown(local_conn_fd/public_conn_fd, 2)关闭写方向。等收到另一个方向的fin包后，将proxyConn置为空闲模式，并放回空闲队列中。
![4](https://files.mdnice.com/user/13956/0efd16a9-83da-47ea-a60e-ffeaa066bef2.jpg)

在处理链接断开和复用代理链接这块遇到的坑比较多
1. 控制对端去shutdown连接是通过ctl_conn去通知的，可能这一方向上对端的数据还没有全部转发完成就收到断开通知了，需要确保数据全部转发完才能调用shutdown去关闭连接。
2. 从空闲列表中拿到一个proxy_conn后，需要发送StartProxyConnReq，告知对端开始工作，如果此时对端的这一proxy_conn还处于数据传输模式，就会报错了。


## 数据传输
数据在Server和Client都需进行转发，将数据从一个连接的接收缓冲区转发到另一个连接的发送缓冲区。如果使用write/read系统调用，整个流程如下图
![5](https://files.mdnice.com/user/13956/e0866a30-1eeb-45d9-97bf-b44e80353276.jpg)


数据先从内核空间复制到用户空间，之后再调用write系统调用将数据复制到内核空间。每次系统调用，都需要切换CPU上下文，而且，两次拷贝都需要CPU去执行(CPU copy)，所以，大量的拷贝操作，会成为整个服务的性能瓶颈。

在CProxy中，使用splice的零拷贝方案，数据直接从内核空间的Source Socket Buffer转移到Dest Socket Buffer，不需要任何CPU copy。

![6](https://files.mdnice.com/user/13956/8c87815b-7b05-401d-aef1-715b94ae54a6.jpg)


splice通过pipe管道“传递”数据，基本原理是通过pipe管道修改source socket buffer和dest socket buffer的物理内存页

![7](https://files.mdnice.com/user/13956/a32e43ea-33e9-4e41-99d6-ec4c8ee2361c.jpg)


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

![8](https://files.mdnice.com/user/13956/11320d8d-2699-4e0e-9993-6fec15569dbb.jpg)
![9](https://files.mdnice.com/user/13956/3fe26b2d-8445-45f7-97f0-5396fe197cd0.jpg)


![](https://cdn.jsdelivr.net/gh/lzs123/mdnice_picture/2021-5-23/1621731683200-image.png)
