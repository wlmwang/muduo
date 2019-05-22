// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_ACCEPTOR_H
#define MUDUO_NET_ACCEPTOR_H

#include <functional>

#include "muduo/net/Channel.h"
#include "muduo/net/Socket.h"

namespace muduo
{
namespace net
{

class EventLoop;
class InetAddress;

///
/// Acceptor of incoming TCP connections.
///
// acceptor TCP 连接分配器（listen socket 包装器）
class Acceptor : noncopyable
{
 public:
  typedef std::function<void (int sockfd, const InetAddress&)> NewConnectionCallback;

  Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport);
  ~Acceptor();

  void setNewConnectionCallback(const NewConnectionCallback& cb)
  { newConnectionCallback_ = cb; }

  // 端口 listen，并监听可读事件
  bool listenning() const { return listenning_; }
  void listen();

 private:
  // 可读事件入口函数（接受新连接）
  void handleRead();

  EventLoop* loop_;         // 所属 IO 线程
  Socket acceptSocket_;     // listen socket
  Channel acceptChannel_;   // listen socket channel
  NewConnectionCallback newConnectionCallback_; // 新连接回调函数
  bool listenning_;

  // 预留一个空闲 fd，防止 fd 达到系统最大限制的 max open files，从而造成 busy-loop
  int idleFd_;
};

}  // namespace net
}  // namespace muduo

#endif  // MUDUO_NET_ACCEPTOR_H
