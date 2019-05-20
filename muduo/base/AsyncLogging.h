// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_ASYNCLOGGING_H
#define MUDUO_BASE_ASYNCLOGGING_H

#include "muduo/base/BlockingQueue.h"
#include "muduo/base/BoundedBlockingQueue.h"
#include "muduo/base/CountDownLatch.h"
#include "muduo/base/Mutex.h"
#include "muduo/base/Thread.h"
#include "muduo/base/LogStream.h"

#include <atomic>
#include <vector>

namespace muduo
{

// 异步日志类（可以作为“前端”日志类执行“写入/刷新”操作时的底层实现）
class AsyncLogging : noncopyable
{
 public:

  AsyncLogging(const string& basename,
               off_t rollSize,
               int flushInterval = 3);

  ~AsyncLogging()
  {
    if (running_)
    {
      stop();
    }
  }

  // 日志数据生产者（必须是一行完整日志，以免造成多线程数据混乱）
  void append(const char* logline, int len);

  // 启动后端日志消费线程
  void start()
  {
    running_ = true;
    thread_.start();
    latch_.wait();
  }

  void stop() NO_THREAD_SAFETY_ANALYSIS
  {
    running_ = false;
    cond_.notify();
    thread_.join();
  }

 private:

  void threadFunc();

  typedef muduo::detail::FixedBuffer<muduo::detail::kLargeBuffer> Buffer;
  typedef std::vector<std::unique_ptr<Buffer>> BufferVector;
  typedef BufferVector::value_type BufferPtr;

  const int flushInterval_;   // 后端日志写入磁盘频率
  std::atomic<bool> running_; // 是否正在消费
  const string basename_;     // 日志文件名
  const off_t rollSize_;      // 日志“滚动”阈值（单个文件最大字节）

  // 后端日志消费线程
  muduo::Thread thread_;
  muduo::CountDownLatch latch_;

  // 保护以下所有条件变量/缓冲区的互斥体
  muduo::MutexLock mutex_;
  muduo::Condition cond_ GUARDED_BY(mutex_);

  // 当前日志缓冲区（与外部线程交互）。也是最新添加的日志数据
  BufferPtr currentBuffer_ GUARDED_BY(mutex_);
  // 备用日志缓冲区（当前日志缓冲区写满时，会移动该缓冲区内存到当前日志缓存区中）
  BufferPtr nextBuffer_ GUARDED_BY(mutex_);
  // 日志缓冲区队列（写满的日志缓存区，暂时都先插入到该队列，供后端消费线程消费）
  BufferVector buffers_ GUARDED_BY(mutex_);
};

}  // namespace muduo

#endif  // MUDUO_BASE_ASYNCLOGGING_H
