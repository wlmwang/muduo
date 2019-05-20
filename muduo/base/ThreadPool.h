// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_THREADPOOL_H
#define MUDUO_BASE_THREADPOOL_H

#include "muduo/base/Condition.h"
#include "muduo/base/Mutex.h"
#include "muduo/base/Thread.h"
#include "muduo/base/Types.h"

#include <deque>
#include <vector>

namespace muduo
{

// 线程池任务（回调函数）队列
// 所有线程以抢占式消费任务队列（maxSize>0 为有限任务队列）
class ThreadPool : noncopyable
{
 public:
  // 任务（回调）函数原型
  typedef std::function<void ()> Task;

  explicit ThreadPool(const string& nameArg = string("ThreadPool"));
  ~ThreadPool();

  // Must be called before start().
  //
  // 任务队列最大限制
  void setMaxQueueSize(int maxSize) { maxQueueSize_ = maxSize; }

  // 每个线程的初始化函数
  void setThreadInitCallback(const Task& cb)
  { threadInitCallback_ = cb; }

  // 启动、停止线程池
  void start(int numThreads/**线程数量*/);
  void stop();

  const string& name() const
  { return name_; }

  // 当前任务队列大小
  size_t queueSize() const;

  // Could block if maxQueueSize > 0
  // There is no move-only version of std::function in C++ as of C++14.
  // So we don't need to overload a const& and an && versions
  // as we do in (Bounded)BlockingQueue.
  // https://stackoverflow.com/a/25408989
  //
  // 任务生产（当队列已满时，生产线程可能会被阻塞）
  void run(Task f);

 private:
  // 任务是否已满
  bool isFull() const REQUIRES(mutex_);

  // 线程池中，每个线程中运行的入口函数
  void runInThread();
  
  // 抢占式获取任务（阻塞方式）
  Task take();

  mutable MutexLock mutex_;
  Condition notEmpty_ GUARDED_BY(mutex_); // 任务是否已空，条件变量
  Condition notFull_ GUARDED_BY(mutex_);  // 任务是否已满，条件变量

  string name_;   // 所有线程前缀名称。name_ + id
  Task threadInitCallback_; // 每个线程初始化的回调函数

  std::vector<std::unique_ptr<muduo::Thread>> threads_;   // 线程对象 Thread 队列

  std::deque<Task> queue_ GUARDED_BY(mutex_); // 任务队列
  size_t maxQueueSize_; // 任务最大数限制
  bool running_;  // 线程池是否正在运行
};

}  // namespace muduo

#endif  // MUDUO_BASE_THREADPOOL_H
