// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_CONDITION_H
#define MUDUO_BASE_CONDITION_H

#include "muduo/base/Mutex.h"

#include <pthread.h>

namespace muduo
{

// 条件变量
class Condition : noncopyable
{
 public:
  explicit Condition(MutexLock& mutex)
    : mutex_(mutex)
  {
    MCHECK(pthread_cond_init(&pcond_, NULL));
  }

  ~Condition()
  {
    MCHECK(pthread_cond_destroy(&pcond_));
  }

  void wait()
  {
    // 条件变量进入等待时，内核会原子释放锁；并在结束时，调用线程会再次获取锁
    // 即使返回错误时也是如此

    // 释放持锁状态
    MutexLock::UnassignGuard ug(mutex_);

    // 阻塞等待通知
    MCHECK(pthread_cond_wait(&pcond_, mutex_.getPthreadMutex()));
  }

  // returns true if time out, false otherwise.
  //
  // todo
  // 更好的设计：锁定成功，返回 true；超时、失败，返回 false
  //
  // 等待条件变量通知。超时返回 true，否则返回 false
  bool waitForSeconds(double seconds);

  // 通知
  void notify()
  {
    MCHECK(pthread_cond_signal(&pcond_));
  }

  // 广播
  void notifyAll()
  {
    MCHECK(pthread_cond_broadcast(&pcond_));
  }

 private:
  MutexLock& mutex_;
  pthread_cond_t pcond_;
};

}  // namespace muduo

#endif  // MUDUO_BASE_CONDITION_H
