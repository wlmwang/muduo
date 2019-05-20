// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/base/Condition.h"

#include <errno.h>

// returns true if time out, false otherwise.
//
// 等待条件变量通知。超时返回 true，否则返回 false
bool muduo::Condition::waitForSeconds(double seconds)
{
  struct timespec abstime;
  // FIXME: use CLOCK_MONOTONIC or CLOCK_MONOTONIC_RAW to prevent time rewind.
  clock_gettime(CLOCK_REALTIME, &abstime);

  const int64_t kNanoSecondsPerSecond = 1000000000;
  int64_t nanoseconds = static_cast<int64_t>(seconds * kNanoSecondsPerSecond);

  abstime.tv_sec += static_cast<time_t>((abstime.tv_nsec + nanoseconds) / kNanoSecondsPerSecond);
  abstime.tv_nsec = static_cast<long>((abstime.tv_nsec + nanoseconds) % kNanoSecondsPerSecond);
  

  // 条件变量进入等待时，内核会原子释放锁；并在结束时，调用线程会再次获取锁
  // 即使返回错误时也是如此

  // 释放持锁状态
  MutexLock::UnassignGuard ug(mutex_);
  
  // 带超时阻塞等待通知
  // pthread_cond_timedwait() 在成功完成之后，会返回零；其他任何返回值，都表示出现了错误
  return ETIMEDOUT == pthread_cond_timedwait(&pcond_, mutex_.getPthreadMutex(), &abstime);
}

