// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/net/SocketsOps.h"

#include "muduo/base/Logging.h"
#include "muduo/base/Types.h"
#include "muduo/net/Endian.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>  // snprintf
#include <sys/socket.h> // SOMAXCONN,struct sockaddr,sockaddr_in[6]
#include <sys/uio.h>  // struct iovec,readv
#include <unistd.h>

using namespace muduo;
using namespace muduo::net;

namespace
{

// tips:
//      /* Structure sockaddr */
//      typedef unsigned short int sa_family_t;
//      struct sockaddr {
//          sa_family_t sa_family;  /* address family: AF_INET */
//          char sa_data[14];       /* Address data.  */
//      };
//
//     /* Structure describing an Internet socket address.  */
//     struct sockaddr_in {
//         sa_family_t    sin_family; /* address family: AF_INET */
//         uint16_t       sin_port;   /* port in network byte order */
//         struct in_addr sin_addr;   /* internet address */
//         unsigned char sin_zero[8]; /* Pad to size of `struct sockaddr' */
//     };
//
//     /* Internet address. */
//     typedef uint32_t in_addr_t;
//     struct in_addr {
//         in_addr_t       s_addr;     /* address in network byte order */
//     };
//
//     struct sockaddr_in6 {
//         sa_family_t     sin6_family;   /* address family: AF_INET6 */
//         uint16_t        sin6_port;     /* port in network byte order */
//         uint32_t        sin6_flowinfo; /* IPv6 flow information */
//         struct in6_addr sin6_addr;     /* IPv6 address */
//         uint32_t        sin6_scope_id; /* IPv6 scope-id */
//     };
//
//     /* IPv6 address */
//     struct in6_addr
//     {
//        union
//        {
//            uint8_t __u6_addr8[16];
//            #if defined __USE_MISC || defined __USE_GNU
//              uint16_t __u6_addr16[8];
//              uint32_t __u6_addr32[4];
//            #endif
//        } __in6_u;
//        #define s6_addr __in6_u.__u6_addr8
//        #if defined __USE_MISC || defined __USE_GNU
//          # define s6_addr16      __in6_u.__u6_addr16
//          # define s6_addr32      __in6_u.__u6_addr32
//        #endif
//   };
//
// 1.
// sockaddr 数据结构是 bind/connect/recvfrom/sendto 等网络库函数的参数，
// 用来指明地址信息。
// 但一般实际在 socket 编程中并不直接针对此数据结构操作，而是使用另一个
// 与 sockaddr 等价的数据结构 sockaddr_in[6]
// 注：主要是因为 sockaddr 将 ip/port 编码成一个字段，不易操作！
// 
// 2.
// sockaddr_in[6] 与 sockaddr 结构转换，不需要强制内存大小相同。因为在
// 套接字 bind/connect/recvfrom/sendto 等网络函数簇中，其内核会根据首字
// 段 sin[6]_family 的取值 (AF_INET/AF_INET6) 又强转回 sockaddr_in[6]
//
//
// \file <netinet/in.h>
// #define INET_ADDRSTRLEN 16
// #define INET6_ADDRSTRLEN 46
//

typedef struct sockaddr SA;

// Linux 新增系统调用 accept4() 可直接设置 non-block, close-on-exec
#if VALGRIND || defined (NO_ACCEPT4)
void setNonBlockAndCloseOnExec(int sockfd)
{
  // non-block
  int flags = ::fcntl(sockfd, F_GETFL, 0);
  flags |= O_NONBLOCK;
  int ret = ::fcntl(sockfd, F_SETFL, flags);
  // FIXME check

  // close-on-exec
  flags = ::fcntl(sockfd, F_GETFD, 0);
  flags |= FD_CLOEXEC;
  ret = ::fcntl(sockfd, F_SETFD, flags);
  // FIXME check

  (void)ret;
}
#endif

}  // namespace


// sockaddr_in[6] 与 sockaddr 数据结构相互转换（强制类型转换即可）

const struct sockaddr* sockets::sockaddr_cast(const struct sockaddr_in6* addr)
{
  return static_cast<const struct sockaddr*>(implicit_cast<const void*>(addr));
}

struct sockaddr* sockets::sockaddr_cast(struct sockaddr_in6* addr)
{
  return static_cast<struct sockaddr*>(implicit_cast<void*>(addr));
}

const struct sockaddr* sockets::sockaddr_cast(const struct sockaddr_in* addr)
{
  return static_cast<const struct sockaddr*>(implicit_cast<const void*>(addr));
}

const struct sockaddr_in* sockets::sockaddr_in_cast(const struct sockaddr* addr)
{
  return static_cast<const struct sockaddr_in*>(implicit_cast<const void*>(addr));
}

const struct sockaddr_in6* sockets::sockaddr_in6_cast(const struct sockaddr* addr)
{
  return static_cast<const struct sockaddr_in6*>(implicit_cast<const void*>(addr));
}

// 创建一个 non-block, close-on-exec 的 socket 文件描述符（出错退出）
int sockets::createNonblockingOrDie(sa_family_t family)
{
#if VALGRIND
  int sockfd = ::socket(family, SOCK_STREAM, IPPROTO_TCP);
  if (sockfd < 0)
  {
    LOG_SYSFATAL << "sockets::createNonblockingOrDie";
  }

  setNonBlockAndCloseOnExec(sockfd);
#else
  // non-block, close-on-exec
  int sockfd = ::socket(family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
  if (sockfd < 0)
  {
    LOG_SYSFATAL << "sockets::createNonblockingOrDie";
  }
#endif
  return sockfd;
}

// 绑定地址信息到 socket 文件描述符上
void sockets::bindOrDie(int sockfd, const struct sockaddr* addr)
{
  // 假设客户端传递 sockaddr_in6 长度地址（内核会根据 addr->sa_family 做判断）
  int ret = ::bind(sockfd, addr, static_cast<socklen_t>(sizeof(struct sockaddr_in6)));
  if (ret < 0)
  {
    LOG_SYSFATAL << "sockets::bindOrDie";
  }
}

void sockets::listenOrDie(int sockfd)
{
  // #define SOMAXCONN   128
  // SOMAXCONN 端口最大的监听队列长度
  int ret = ::listen(sockfd, SOMAXCONN);
  if (ret < 0)
  {
    LOG_SYSFATAL << "sockets::listenOrDie";
  }
}

// accept 接受一个 socket 连接。并设置连接文件描述符为 non-block, close-on-exec
int sockets::accept(int sockfd, struct sockaddr_in6* addr)
{
  // 总是传递 sockaddr_in6 结构参数
  socklen_t addrlen = static_cast<socklen_t>(sizeof *addr);
#if VALGRIND || defined (NO_ACCEPT4)
  int connfd = ::accept(sockfd, sockaddr_cast(addr), &addrlen);
  setNonBlockAndCloseOnExec(connfd);
#else
  int connfd = ::accept4(sockfd, sockaddr_cast(addr),
                         &addrlen, SOCK_NONBLOCK | SOCK_CLOEXEC);
#endif

  // 检测返回值
  if (connfd < 0)
  {
    int savedErrno = errno;

    // 记录错误日志。注，此时进程不退出
    LOG_SYSERR << "Socket::accept";

    switch (savedErrno)
    {
      case EAGAIN:
      case ECONNABORTED:
      case EINTR:
      case EPROTO: // ???
      case EPERM:
      case EMFILE: // per-process lmit of open file desctiptor ???
        // expected errors
        errno = savedErrno;
        break;

      case EBADF:
      case EFAULT:
      case EINVAL:
      case ENFILE:
      case ENOBUFS:
      case ENOMEM:
      case ENOTSOCK:
      case EOPNOTSUPP:
        // unexpected errors
        //
        // 记录错误日志。注，此时进程退出
        LOG_FATAL << "unexpected error of ::accept " << savedErrno;
        break;

      default:
        // 记录错误日志。注，此时进程退出
        LOG_FATAL << "unknown error of ::accept " << savedErrno;
        break;
    }
  }
  return connfd;
}

int sockets::connect(int sockfd, const struct sockaddr* addr)
{
  return ::connect(sockfd, addr, static_cast<socklen_t>(sizeof(struct sockaddr_in6)));
}

ssize_t sockets::read(int sockfd, void *buf, size_t count)
{
  return ::read(sockfd, buf, count);
}

// 零拷贝读取 socket 数据流
ssize_t sockets::readv(int sockfd, const struct iovec *iov, int iovcnt)
{
  return ::readv(sockfd, iov, iovcnt);
}

ssize_t sockets::write(int sockfd, const void *buf, size_t count)
{
  return ::write(sockfd, buf, count);
}


// tips:
// \file #include <unistd.h>
// int close(int fd);
// 1.
// 优雅的（默认）关闭 sockfd 描述符双向通道，并释放该描述符相关资源。
// a. 强制关闭：l_onoff 为非 0，l_linger 为 0。若本地缓冲区有未读数据，被
//    丢弃；并始终向对端发送 RST 重置连接报文（可以避免进入 TIME_WAIT 状态，
//    但破坏了 TCP 协议的正常工作方式）。
//    注：主动发 FIN 包方必然会进入 TIME_WAIT 状态，除非不发送 FIN 而直接
//    发送 RST 结束连接。
// b. 优雅关闭：l_onoff 为非 0，l_linger 为非 0。则向客户端发送一个 FIN 报
//    文，客户端收到后发送 ACK，并进入 CLOSE_WAIT 阶段，服务端收到 ACK 后，
//    进入 FIN_WAIT_2 阶段；客户端发送完数据后，也选择关闭该连接，发送 FIN 
//    报文，进入 LAST_ACK 阶段，服务端收到 FIN 发送 ACK，并进入 TIME_WAIT，
//    客户端收到 ACK 后，关闭释放连接，服务端等待 2MSL后，关闭释放连接。
//    如果在 l_linger 的时间内仍未完成四次挥手，则强制关闭。
//
// 2.
// 不能保证会向对端发送 FIN 报文。只有当这个 sockfd 的引用计数为 0 时，才
// 会发送 FIN 段，否则只是将引用计数减 1 而已。也就是说只有当所有进程（可
// 能是 fork 多个子进程都打开了这个套接字）都关闭了这个套接字，close 才会
// 发送 FIN 报文。
//
// 3.
// 如果有多个进程共享一个 sockfd ，close 只影响本进程。也就是无论如何，本
// 进程中该 sockfd 描述符资源将被释放，任何的读写都是非法的。
//
void sockets::close(int sockfd)
{
  if (::close(sockfd) < 0)
  {
    LOG_SYSERR << "sockets::close";
  }
}

// tips:
// \file #include <sys/socket.h>
// int shutdown(int sockfd, int how);
// 1.
// 优雅地单方向或者双方向关闭 TCP 连接的方法。取决于 how 参数：SHUT_RD(0),
// SHUT_WR(1),SHUT_RDWR(2)，后两者会保证对端会收到一个 EOF 字符，即发送了
// 一个 FIN 报文，而不管其他进程是否已经打开了这个套接字。
//
// 2.
// 如果调用 how=1 时，则意味着调用方主动关闭了写入通道（表示调用方不再往该
// 连接中写入数据）；但对端仍可以往这个已经发送出 FIN段的套接字中写数据，也
// 就是接收到 FIN 报文仅代表主动关闭方不再发送数据，但它还是可以收取数据的。
//
// 3.
// 不管是关闭发送还是关闭接收通道，主动关闭方均向被动方发送 FIN 报文。被动方
// 收到 FIN 报文后，并不知道主动关闭方是以何种方式 shutdown ，甚至不知道对端
// 是 shutdown 还是 close。可以看出来 SHUT_RD(0)实际上是一个“空”操作，即使关
// 闭了连接的读通道，也可以继续收取连接上的数据。
//
// 4.
// shutdown 针对的是连接，与 socket 描述符没有关系，即使调用 shutdown 也不会
// 关闭 fd，最终还需要 close(fd)。
//
// 5.
// SO_LINGER 对 shutdown 无影响。
//
void sockets::shutdownWrite(int sockfd)
{
  if (::shutdown(sockfd, SHUT_WR) < 0)
  {
    LOG_SYSERR << "sockets::shutdownWrite";
  }
}

// 将 struct sockaddr 地址转换为字符串 "ip4|ip6:port"
void sockets::toIpPort(char* buf, size_t size,
                       const struct sockaddr* addr)
{
  // 转换字符串地址
  toIp(buf,size, addr);

  size_t end = ::strlen(buf); // c-style string

  // 转换字符串端口（先转换成主机序）
  const struct sockaddr_in* addr4 = sockaddr_in_cast(addr);
  uint16_t port = sockets::networkToHost16(addr4->sin_port);

  assert(size > end);
  snprintf(buf+end, size-end, ":%u", port);
}

// 将“二进制”的 ip4|ip6 地址转换为 “点分十进制”|“冒号地址” 字符串（c-style string）
void sockets::toIp(char* buf, size_t size,
                   const struct sockaddr* addr)
{
  if (addr->sa_family == AF_INET)
  {
    assert(size >= INET_ADDRSTRLEN);
    const struct sockaddr_in* addr4 = sockaddr_in_cast(addr);
    ::inet_ntop(AF_INET, &addr4->sin_addr, buf, static_cast<socklen_t>(size));
  }
  else if (addr->sa_family == AF_INET6)
  {
    assert(size >= INET6_ADDRSTRLEN);
    const struct sockaddr_in6* addr6 = sockaddr_in6_cast(addr);
    ::inet_ntop(AF_INET6, &addr6->sin6_addr, buf, static_cast<socklen_t>(size));
  }
}

// 将 "ip4 + port" 地址转换 struct sockaddr_in
void sockets::fromIpPort(const char* ip, uint16_t port,
                         struct sockaddr_in* addr)
{
  addr->sin_family = AF_INET;
  addr->sin_port = hostToNetwork16(port);
  if (::inet_pton(AF_INET, ip, &addr->sin_addr) <= 0)
  {
    LOG_SYSERR << "sockets::fromIpPort";
  }
}

// 将 "ip6 + port" 地址转换 struct sockaddr_in6
void sockets::fromIpPort(const char* ip, uint16_t port,
                         struct sockaddr_in6* addr)
{
  addr->sin6_family = AF_INET6;
  addr->sin6_port = hostToNetwork16(port);
  if (::inet_pton(AF_INET6, ip, &addr->sin6_addr) <= 0)
  {
    LOG_SYSERR << "sockets::fromIpPort";
  }
}

// 获取 socket 文件描述符的出错信息
int sockets::getSocketError(int sockfd)
{
  int optval;
  socklen_t optlen = static_cast<socklen_t>(sizeof optval);

  if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
  {
    return errno;
  }
  else
  {
    return optval;
  }
}

// 获取连接 socket 文件描述符的本地协议地址
struct sockaddr_in6 sockets::getLocalAddr(int sockfd)
{
  struct sockaddr_in6 localaddr;
  memZero(&localaddr, sizeof localaddr);
  socklen_t addrlen = static_cast<socklen_t>(sizeof localaddr);
  if (::getsockname(sockfd, sockaddr_cast(&localaddr), &addrlen) < 0)
  {
    LOG_SYSERR << "sockets::getLocalAddr";
  }
  return localaddr;
}

// 获取连接 socket 文件描述符的对端协议地址
struct sockaddr_in6 sockets::getPeerAddr(int sockfd)
{
  struct sockaddr_in6 peeraddr;
  memZero(&peeraddr, sizeof peeraddr);
  socklen_t addrlen = static_cast<socklen_t>(sizeof peeraddr);
  if (::getpeername(sockfd, sockaddr_cast(&peeraddr), &addrlen) < 0)
  {
    LOG_SYSERR << "sockets::getPeerAddr";
  }
  return peeraddr;
}

// 是否是自连接。比如：
// 1.服务端需要监听地址为 127.0.0.1:11001，但此时还并为启动。
// 2.客户端创建 socket，恰好为 127.0.0.1:11001，然后去连接服务端。此时
//  就出现了自连接。会导致端口被占用，服务端无法启动。
bool sockets::isSelfConnect(int sockfd)
{
  struct sockaddr_in6 localaddr = getLocalAddr(sockfd);
  struct sockaddr_in6 peeraddr = getPeerAddr(sockfd);
  if (localaddr.sin6_family == AF_INET)
  {
    // 强转回 IPv4
    const struct sockaddr_in* laddr4 = reinterpret_cast<struct sockaddr_in*>(&localaddr);
    const struct sockaddr_in* raddr4 = reinterpret_cast<struct sockaddr_in*>(&peeraddr);
    return laddr4->sin_port == raddr4->sin_port
        && laddr4->sin_addr.s_addr == raddr4->sin_addr.s_addr;
  }
  else if (localaddr.sin6_family == AF_INET6)
  {
    return localaddr.sin6_port == peeraddr.sin6_port
        && memcmp(&localaddr.sin6_addr, &peeraddr.sin6_addr, sizeof localaddr.sin6_addr) == 0;
  }
  else
  {
    return false;
  }
}

