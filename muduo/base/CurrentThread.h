// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_CURRENTTHREAD_H
#define MUDUO_BASE_CURRENTTHREAD_H

#include "muduo/base/Types.h"

namespace muduo
{
namespace CurrentThread
{
  // internal
  //
  // 缓存当前线程的 tid,threadname 。以减少系统调用
  extern __thread int t_cachedTid;
  extern __thread char t_tidString[32];
  extern __thread int t_tidStringLength;
  extern __thread const char* t_threadName;

  void cacheTid();

  // 返回当前线程 tid
  inline int tid()
  {
    // expect 优化大多数时候，t_cachedTid 都已被初始化
    if (__builtin_expect(t_cachedTid == 0, 0))
    {
      cacheTid();
    }
    return t_cachedTid;
  }

  inline const char* tidString() // for logging
  {
    return t_tidString;
  }

  inline int tidStringLength() // for logging
  {
    return t_tidStringLength;
  }

  inline const char* name()
  {
    return t_threadName;
  }
  
  bool isMainThread();

  void sleepUsec(int64_t usec);  // for testing

  // 返回函数调用栈字符串
  string stackTrace(bool demangle);
}  // namespace CurrentThread
}  // namespace muduo

#endif  // MUDUO_BASE_CURRENTTHREAD_H
