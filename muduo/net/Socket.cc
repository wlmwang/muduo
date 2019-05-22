// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/net/Socket.h"

#include "muduo/base/Logging.h"
#include "muduo/net/InetAddress.h"
#include "muduo/net/SocketsOps.h"

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>  // snprintf

// tips:
// SO_REUSEADDR
// 1. 在为 socket 设置了 SO_REUSEADDR 以后，判断冲突的方式就变了。只要地址不是正好相
//    同，那么多个 socket 就能绑定到同一 ip 上。
//    a.比如 0.0.0.0 和 192.168.0.100 ，虽然逻辑意义上前者包含了后者，但是 0.0.0.0 泛
//      指所有本地ip，而 192.168.0.100 特指某一 ip，两者并不是完全相同，所以 socket尝
//      试绑定的时候，不会再报 EADDRINUSE ，而是绑定成功。（Linux 下会报 EADDRINUSE）
//    b.但若想绑定 addr 字符串是完全相同的 ip，那么无论 SO_REUSEADDR 设置与否，都会报
//      地址已使用。
//  2. SO_REUSEADDR 的另一个作用是，可以绑定 TIME_WAIT（主动关闭） 状态的地址。
//    a.TIME_WAIT 本来是为了 TCP 通信的可靠性而存在的。比如程序关闭了某个 socket，它经
//      过挥手协议，总会进入 TIME_WAIT 状态的最后阶段，而保留一段时间（一般系统为 2 分
//      钟），该段时间是为了：
//      i.防止最后一次发送给客户端的 FIN-ACK 包丢失，做重发准备。
//      ii.防止服务端新开启的 socket 立即被绑定到该 socket，而客户端又没有完全关闭，从
//        而造成传话。
//    b.但实际工作中，可能会面临一个问题：假如一个 systemd 托管的 service 异常退出了，
//      留下了 TIME_WAIT 状态的 socket，那么 systemd 将会尝试重启这个 service。但是因为
//      端口被占用，会导致启动失败，造成两分钟的服务空档期，systemd 也可能在这期间放弃
//      重启服务。
//    c.设置了 SO_REUSEADDR 以后，处于 TIME_WAIT 状态的地址也可以被绑定，就杜绝了这个问
//      题。虽然这样重用 TIME_WAIT 可能会造成不可预料的副作用，但是在现实中问题很少发生，
//      所以也忽略了它的副作用。

// SO_REUSEPORT
// 1. SO_REUSEPORT 干的其实就是大众期望 SO_REUSEADDR 能够干的事，可将多个 socket 绑定到同
//    一 ip 和 port。并且它要求所有绑定同一 ip/port 的 socket 都设置了 SO_REUSEPORT。
//    不过可能有的操作系统并没有这个 option。
//    Linux >= 3.9 内核将均匀地分发连接请求（也就是 accept() 阶段）。
// 2. 在默认情况下，一般在 bind() 时可能会出现 EADDRINUSE 问题。connect() 时因为 src ip 和
//    src port 已经不同，不可能报 EADDRINUSE 。但是在 SO_REUSEADDR 和 SO_REUSEPORT 下，因为
//    地址有重用，那么当重用的地址端口尝试连接同一个远端主机的同一端口时，就会报 EADDRINUSE

// Windows 中没有 SO_REUSEPORT 选项，SO_REUSEADDR 承担了 SO_REUSEPORT 的功能。
// SO_EXCLUSIVEADDRUS的选项，使 socket绑定到 ip/port 上，其他 socket即使设置了 SO_REUSEPORT 
// 也无法重用此端口

using namespace muduo;
using namespace muduo::net;

Socket::~Socket()
{
  sockets::close(sockfd_);
}

bool Socket::getTcpInfo(struct tcp_info* tcpi) const
{
  socklen_t len = sizeof(*tcpi);
  memZero(tcpi, len);
  return ::getsockopt(sockfd_, SOL_TCP, TCP_INFO, tcpi, &len) == 0;
}

bool Socket::getTcpInfoString(char* buf, int len) const
{
  struct tcp_info tcpi;
  bool ok = getTcpInfo(&tcpi);
  if (ok)
  {
    snprintf(buf, len, "unrecovered=%u "
             "rto=%u ato=%u snd_mss=%u rcv_mss=%u "
             "lost=%u retrans=%u rtt=%u rttvar=%u "
             "sshthresh=%u cwnd=%u total_retrans=%u",
             tcpi.tcpi_retransmits,  // Number of unrecovered [RTO] timeouts
             tcpi.tcpi_rto,          // Retransmit timeout in usec
             tcpi.tcpi_ato,          // Predicted tick of soft clock in usec
             tcpi.tcpi_snd_mss,
             tcpi.tcpi_rcv_mss,
             tcpi.tcpi_lost,         // Lost packets
             tcpi.tcpi_retrans,      // Retransmitted packets out
             tcpi.tcpi_rtt,          // Smoothed round trip time in usec
             tcpi.tcpi_rttvar,       // Medium deviation
             tcpi.tcpi_snd_ssthresh,
             tcpi.tcpi_snd_cwnd,
             tcpi.tcpi_total_retrans);  // Total retransmits for entire connection
  }
  return ok;
}

void Socket::bindAddress(const InetAddress& addr)
{
  sockets::bindOrDie(sockfd_, addr.getSockAddr());
}

void Socket::listen()
{
  sockets::listenOrDie(sockfd_);
}

// 接受一个 socket 连接。并设置连接文件描述符为 non-block, close-on-exec
// 成功时，返回客户端 fd 文件描述符，同时 peeraddr* 被填充
// 失败时，返回 -1，peeraddr* 保持不变
int Socket::accept(InetAddress* peeraddr)
{
  struct sockaddr_in6 addr;
  memZero(&addr, sizeof addr);
  int connfd = sockets::accept(sockfd_, &addr);
  if (connfd >= 0)
  {
    // 设置 InetAddress 地址信息
    peeraddr->setSockAddrInet6(addr);
  }
  return connfd;
}

void Socket::shutdownWrite()
{
  sockets::shutdownWrite(sockfd_);
}

void Socket::setTcpNoDelay(bool on)
{
  int optval = on ? 1 : 0;
  ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY,
               &optval, static_cast<socklen_t>(sizeof optval));
  // FIXME CHECK
}

void Socket::setReuseAddr(bool on)
{
  int optval = on ? 1 : 0;
  ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR,
               &optval, static_cast<socklen_t>(sizeof optval));
  // FIXME CHECK
}

void Socket::setReusePort(bool on)
{
#ifdef SO_REUSEPORT
  int optval = on ? 1 : 0;
  int ret = ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT,
                         &optval, static_cast<socklen_t>(sizeof optval));
  if (ret < 0 && on)
  {
    LOG_SYSERR << "SO_REUSEPORT failed.";
  }
#else
  if (on)
  {
    LOG_ERROR << "SO_REUSEPORT is not supported.";
  }
#endif
}

void Socket::setKeepAlive(bool on)
{
  int optval = on ? 1 : 0;
  ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE,
               &optval, static_cast<socklen_t>(sizeof optval));
  // FIXME CHECK
}

