// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_EVENTLOOP_H
#define MUDUO_NET_EVENTLOOP_H

#include <atomic>
#include <functional>
#include <vector>

#include <boost/any.hpp>

#include "muduo/base/Mutex.h"
#include "muduo/base/CurrentThread.h"
#include "muduo/base/Timestamp.h"
#include "muduo/net/Callbacks.h"
#include "muduo/net/TimerId.h"

namespace muduo
{
namespace net
{

class Channel;
class Poller;
class TimerQueue;

///
/// Reactor, at most one per thread.
///
/// This is an interface class, so don't expose too much details.
//
// 事件循环类。其主要作用有：
// 1. 粘合各个组件：Tcpserver/Acceptor/Poller 等以 EventLoop 为参数的组件
// 2. 标识一个线程。即，每个 IO 线程有且仅有一个 EventLoop 对象；同理，创
//    建 EventLoop 的那个线程定义为 IO 线程
// 3. 使用 EventLoop 机制实现各个组件对象线程安全。当某个组件方法在非所属
//    的 IO 线程中被调用，实际的方法执行动作会被转到所属 IO线程中执行：也
//    就是该组件实例化时，传递的那个 EventLoop 标识的那个线程
//
// 其作用颇像一个“上帝”的角色
class EventLoop : noncopyable
{
 public:
  // EventLoop 唤醒回调
  typedef std::function<void()> Functor;

  EventLoop();
  ~EventLoop();  // force out-line dtor, for std::unique_ptr members.

  ///
  /// Loops forever.
  ///
  /// Must be called in the same thread as creation of the object.
  ///
  // 启动该 IO 线程事件主循环
  void loop();

  /// Quits loop.
  ///
  /// This is not 100% thread safe, if you call through a raw pointer,
  /// better to call through shared_ptr<EventLoop> for 100% safety.
  void quit();

  ///
  /// Time when poll returns, usually means data arrival.
  ///
  Timestamp pollReturnTime() const { return pollReturnTime_; }

  int64_t iteration() const { return iteration_; }

  /// Runs callback immediately in the loop thread.
  /// It wakes up the loop, and run the cb.
  /// If in the same loop thread, cb is run within the function.
  /// Safe to call from other threads.
  void runInLoop(Functor cb);
  /// Queues callback in the loop thread.
  /// Runs after finish pooling.
  /// Safe to call from other threads.
  void queueInLoop(Functor cb);

  size_t queueSize() const;

  // timers

  ///
  /// Runs callback at 'time'.
  /// Safe to call from other threads.
  ///
  TimerId runAt(Timestamp time, TimerCallback cb);
  ///
  /// Runs callback after @c delay seconds.
  /// Safe to call from other threads.
  ///
  TimerId runAfter(double delay, TimerCallback cb);
  ///
  /// Runs callback every @c interval seconds.
  /// Safe to call from other threads.
  ///
  TimerId runEvery(double interval, TimerCallback cb);
  ///
  /// Cancels the timer.
  /// Safe to call from other threads.
  ///
  void cancel(TimerId timerId);

  // internal usage
  void wakeup();
  // 更新多路复用 Poller 监听队列
  void updateChannel(Channel* channel);
  void removeChannel(Channel* channel);
  bool hasChannel(Channel* channel);

  // pid_t threadId() const { return threadId_; }
  //
  // 调用线程不在所属的 IO 线程时，退出程序
  void assertInLoopThread()
  {
    if (!isInLoopThread())
    {
      abortNotInLoopThread();
    }
  }
  // 调用线程是否是所属的 IO 线程
  bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }

  // bool callingPendingFunctors() const { return callingPendingFunctors_; }
  bool eventHandling() const { return eventHandling_; }

  // 设置上下文
  void setContext(const boost::any& context)
  { context_ = context; }

  const boost::any& getContext() const
  { return context_; }

  boost::any* getMutableContext()
  { return &context_; }

  // 获取当前线程的 EventLoop 对象
  static EventLoop* getEventLoopOfCurrentThread();

 private:
  void abortNotInLoopThread();
  void handleRead();  // waked up
  void doPendingFunctors();

  void printActiveChannels() const; // DEBUG

  typedef std::vector<Channel*> ChannelList;

  bool looping_; /* atomic */
  std::atomic<bool> quit_;
  bool eventHandling_; /* atomic */   // 事件分发
  bool callingPendingFunctors_; /* atomic */
  int64_t iteration_;             // 事件循环被调用次数
  const pid_t threadId_;          // 实例化 EventLoop 对象的线程 tid
  Timestamp pollReturnTime_;                // poll() 返回时间戳
  std::unique_ptr<Poller> poller_;          // IO 多路复用对象
  std::unique_ptr<TimerQueue> timerQueue_;  // 定时器队列
  int wakeupFd_;          // 事件相关文件描述符（主要用于 EventLoop 唤醒通知）
  // unlike in TimerQueue, which is an internal class,
  // we don't expose Channel to client.
  std::unique_ptr<Channel> wakeupChannel_;  // 事件相关 channel
  boost::any context_;

  // scratch variables
  ChannelList activeChannels_;      // 当前被触发事件列表
  Channel* currentActiveChannel_;

  mutable MutexLock mutex_;
  std::vector<Functor> pendingFunctors_ GUARDED_BY(mutex_);
};

}  // namespace net
}  // namespace muduo

#endif  // MUDUO_NET_EVENTLOOP_H
