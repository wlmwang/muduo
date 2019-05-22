// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/net/InetAddress.h"

#include "muduo/base/Logging.h"
#include "muduo/net/Endian.h"
#include "muduo/net/SocketsOps.h"

#include <netdb.h>      // For struct hostent
#include <netinet/in.h>

// tips:
// \file <netinet/in.h>
// #define INADDR_ANY       ((in_addr_t) 0x00000000)
// #define INADDR_LOOPBACK  ((in_addr_t) 0x7f000001) /* Inet 127.0.0.1. */
//
// extern const struct in6_addr in6addr_any;      /* :: */
// extern const struct in6_addr in6addr_loopback; /* ::1 */


// INADDR_ANY use (type)value casting.
#pragma GCC diagnostic ignored "-Wold-style-cast"
static const in_addr_t kInaddrAny = INADDR_ANY;
static const in_addr_t kInaddrLoopback = INADDR_LOOPBACK;
#pragma GCC diagnostic error "-Wold-style-cast"

// tips:
//      /* Structure sockaddr */
//      typedef unsigned short int sa_family_t;
//      struct sockaddr {
//          sa_family_t sa_family;  /* address family: AF_INET */
//          char sa_data[14];       /* Address data.  */
//      };

//     /* Structure describing an Internet socket address.  */
//     struct sockaddr_in {
//         sa_family_t    sin_family; /* address family: AF_INET */
//         uint16_t       sin_port;   /* port in network byte order */
//         struct in_addr sin_addr;   /* internet address */
//     };

//     /* Internet address. */
//     typedef uint32_t in_addr_t;
//     struct in_addr {
//         in_addr_t       s_addr;     /* address in network byte order */
//     };

//     struct sockaddr_in6 {
//         sa_family_t     sin6_family;   /* address family: AF_INET6 */
//         uint16_t        sin6_port;     /* port in network byte order */
//         uint32_t        sin6_flowinfo; /* IPv6 flow information */
//         struct in6_addr sin6_addr;     /* IPv6 address */
//         uint32_t        sin6_scope_id; /* IPv6 scope-id */
//     };

// 1.
// sockaddr 数据结构是 bind/connect/recvfrom/sendto 等网络库函数的参数，
// 用来指明地址信息。
// 但一般实际在 socket 编程中并不直接针对此数据结构操作，而是使用另一个
// 与 sockaddr 等价的数据结构 sockaddr_in[6]
// 注：主要是因为 sockaddr 将 ip/port 编码成一个字段，不易操作！
// 
// 2.
// sockaddr_in[6] 与 sockaddr 结构转换，不需要强制内存大小相同。因为在
// 套接字 bind/connect/recvfrom/sendto 等网络函数簇中，其底层会根据首字
// 段 sin[6]_family 的取值 AF_INET/AF_INET6 又强转回 sockaddr_in[6]
//
//
// \file <netinet/in.h>
// #define INET_ADDRSTRLEN 16
// #define INET6_ADDRSTRLEN 46


// tips:
//     /* Description of data base entry for a single host.  */
//     struct hostent
//     {
//         char *h_name;         /* Official name of host.  */
//         char **h_aliases;     /* Alias list.  */
//         int h_addrtype;       /* Host address type. AF_INET/AF_INET6  */
//         int h_length;         /* Length of address.  */
//         char **h_addr_list;   /* List of addresses from name server.  */
//         #if defined __USE_MISC || defined __USE_GNU
//          # define    h_addr  h_addr_list[0] /* Address, for backward compatibility.*/
//         #endif
//     };
//
// 1.
// struct hostent *gethostbyname(const char *name);
// DNS 解析。返回的是一个指向静态变量的指针。非线程安全、不可重入。
//
// 注：由于 DNS 的递归查询，导致 gethostbyname 函数在查询一个域名时严重超时。而该函数
// 又不能像 connect 和 read 等函数那样通过 setsockopt 或者 select函数那样设置超时时间，
// 因此常常成为程序的瓶颈。
// 在多线程中，gethostbyname 会面临一个更严重的问题：如果有一个线程的 gethostbyname发
// 生阻塞，其它线程都会在 gethostbyname 处发生阻塞。
// 另外，Unix 的 gethostbyname 无法并发处理使用（非线程安全），这是先天的缺陷，也是无
// 法改变的。
//
// 2.
// int gethostbyname_r(const char *name, struct hostent *ret, char *buf, size_t buflen,
//                    struct hostent **result, int *h_errnop);
// name: 解析的域名/主机名
// ret: 成功的情况下存储结果用
// buf: 临时缓冲区，存储解析的信息
// result: 如果成功，则这个 hostent 指针指向 ret，也就是结果；如果失败，result = NULL
// h_errnop: 存储错误码
//
// 调用成功时返回 0，*result 指向解析成功的数据结构, *result 如果为 NULL 则表示解析出错
//
// 注：如果被解析的字符串是类似 "xxx.xxx.xxx.xxx" 的字符串，那么 gethostbyname_r 不会发
// 出 DNS 请求，若该地址不合法，gethostbyname_r() 也会返回 0，但此时 *result 是 NULL。
// gethostbyname_r 单机测试可以到达 100/s 。高于 gethostbyname


using namespace muduo;
using namespace muduo::net;

// 编译器静态检测（sin_family/sin_port 必须是前两个元素。以保证强转后这两字段偏移不变）
static_assert(sizeof(InetAddress) == sizeof(struct sockaddr_in6),
              "InetAddress is same size as sockaddr_in6");
static_assert(offsetof(sockaddr_in, sin_family) == 0, "sin_family offset 0");
static_assert(offsetof(sockaddr_in6, sin6_family) == 0, "sin6_family offset 0");
static_assert(offsetof(sockaddr_in, sin_port) == 2, "sin_port offset 2");
static_assert(offsetof(sockaddr_in6, sin6_port) == 2, "sin6_port offset 2");

InetAddress::InetAddress(uint16_t port, bool loopbackOnly, bool ipv6)
{
  static_assert(offsetof(InetAddress, addr6_) == 0, "addr6_ offset 0");
  static_assert(offsetof(InetAddress, addr_) == 0, "addr_ offset 0");
  if (ipv6)
  {
    memZero(&addr6_, sizeof addr6_);
    addr6_.sin6_family = AF_INET6;
    in6_addr ip = loopbackOnly ? in6addr_loopback : in6addr_any;
    addr6_.sin6_addr = ip;
    addr6_.sin6_port = sockets::hostToNetwork16(port);
  }
  else
  {
    memZero(&addr_, sizeof addr_);
    addr_.sin_family = AF_INET;
    in_addr_t ip = loopbackOnly ? kInaddrLoopback : kInaddrAny;
    addr_.sin_addr.s_addr = sockets::hostToNetwork32(ip);
    addr_.sin_port = sockets::hostToNetwork16(port);
  }
}

InetAddress::InetAddress(StringArg ip, uint16_t port, bool ipv6)
{
  if (ipv6)
  {
    memZero(&addr6_, sizeof addr6_);
    sockets::fromIpPort(ip.c_str(), port, &addr6_);
  }
  else
  {
    memZero(&addr_, sizeof addr_);
    sockets::fromIpPort(ip.c_str(), port, &addr_);
  }
}

string InetAddress::toIpPort() const
{
  char buf[64] = "";
  sockets::toIpPort(buf, sizeof buf, getSockAddr());
  return buf;
}

string InetAddress::toIp() const
{
  char buf[64] = "";
  sockets::toIp(buf, sizeof buf, getSockAddr());
  return buf;
}

// 获取 IPv4 网络序的地址
uint32_t InetAddress::ipNetEndian() const
{
  assert(family() == AF_INET);
  return addr_.sin_addr.s_addr;
}

uint16_t InetAddress::toPort() const
{
  return sockets::networkToHost16(portNetEndian());
}

// DNS 解析缓冲区。64k 线程局部变量（静态初始化）
static __thread char t_resolveBuffer[64 * 1024];

// IPv4 DNS 解析主机名（线程安全）
bool InetAddress::resolve(StringArg hostname /**域名|主机名*/, InetAddress* out)
{
  assert(out != NULL);
  struct hostent hent;
  struct hostent* he = NULL;
  int herrno = 0;
  memZero(&hent, sizeof(hent));

  int ret = gethostbyname_r(hostname.c_str(), &hent, t_resolveBuffer, sizeof t_resolveBuffer, &he, &herrno);
  if (ret == 0 && he != NULL)
  {
    // IPv4 类型及长度检测
    assert(he->h_addrtype == AF_INET && he->h_length == sizeof(uint32_t));

    // todo
    // 未改变 out->addr_->sin_family = AF_INET
    out->addr_.sin_addr = *reinterpret_cast<struct in_addr*>(he->h_addr);
    return true;
  }
  else
  {
    if (ret)
    {
      LOG_SYSERR << "InetAddress::resolve";
    }
    return false;
  }
}

void InetAddress::setScopeId(uint32_t scope_id)
{
  if (family() == AF_INET6)
  {
    addr6_.sin6_scope_id = scope_id;
  }
}