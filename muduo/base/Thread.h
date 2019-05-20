// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_THREAD_H
#define MUDUO_BASE_THREAD_H

#include "muduo/base/Atomic.h"
#include "muduo/base/CountDownLatch.h"
#include "muduo/base/Types.h"

#include <functional>
#include <memory>
#include <pthread.h>

namespace muduo
{

class Thread : noncopyable
{
 public:
  typedef std::function<void ()> ThreadFunc;

  explicit Thread(ThreadFunc, const string& name = string());
  // FIXME: make it movable in C++11
  ~Thread();

  // 启动线程（并等待该线程创建成功）
  void start();

  // 等待线程
  int join(); // return pthread_join()

  bool started() const { return started_; }
  // pthread_t pthreadId() const { return pthreadId_; }
  pid_t tid() const { return tid_; }
  const string& name() const { return name_; }

  static int numCreated() { return numCreated_.get(); }

 private:
  void setDefaultName();

  bool       started_;    // 线程启动标识
  bool       joined_;     // “加入式”线程标识（阻塞等待）
  pthread_t  pthreadId_;  // 线程 pthread_t
  pid_t      tid_;        // 线程 tid
  ThreadFunc func_;       // 用户自定义的线程入口函数
  string     name_;       // 线程名称（默认为 "Thread" + num）
  CountDownLatch latch_;  // 阻塞倒计器（线程创建完成，递减计数器，以通知线程创建者）

  static AtomicInt32 numCreated_; // 使用该 Thread 类创建的线程的总数
};

}  // namespace muduo
#endif  // MUDUO_BASE_THREAD_H
