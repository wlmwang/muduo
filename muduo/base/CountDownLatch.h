// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_COUNTDOWNLATCH_H
#define MUDUO_BASE_COUNTDOWNLATCH_H

#include "muduo/base/Condition.h"
#include "muduo/base/Mutex.h"

namespace muduo
{

// 阻塞倒计数
class CountDownLatch : noncopyable
{
 public:

  explicit CountDownLatch(int count/**倒计数量*/);

  void wait();

  // 广播通知所有阻塞的监听者
  void countDown();

  int getCount() const;

 private:
  mutable MutexLock mutex_;
  Condition condition_ GUARDED_BY(mutex_);
  int count_ GUARDED_BY(mutex_);	// 倒计数量
};

}  // namespace muduo
#endif  // MUDUO_BASE_COUNTDOWNLATCH_H
