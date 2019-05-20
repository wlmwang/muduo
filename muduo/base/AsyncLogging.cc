// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/base/AsyncLogging.h"
#include "muduo/base/LogFile.h"
#include "muduo/base/Timestamp.h"

#include <stdio.h>

using namespace muduo;

AsyncLogging::AsyncLogging(const string& basename,
                           off_t rollSize,
                           int flushInterval)
  : flushInterval_(flushInterval),
    running_(false),
    basename_(basename),
    rollSize_(rollSize),
    thread_(std::bind(&AsyncLogging::threadFunc, this), "Logging"),
    latch_(1),
    mutex_(),
    cond_(mutex_),
    currentBuffer_(new Buffer),
    nextBuffer_(new Buffer),
    buffers_()
{
  currentBuffer_->bzero();
  nextBuffer_->bzero();
  buffers_.reserve(16);
}

// 日志数据生产者（必须是一行完整日志，否则多线程数据混乱）
void AsyncLogging::append(const char* logline, int len)
{
  muduo::MutexLockGuard lock(mutex_);
  if (currentBuffer_->avail() > len)
  {
    // 直接拷贝日志数据到缓冲区
    // 注意：此时不会主动唤醒日志消费线程（没有必要）
    currentBuffer_->append(logline, len);
  }
  else
  {
    // 将“已满”的日志缓冲区移动到日志缓冲区队列
    // 先进先出（最新日志在队列尾部）
    buffers_.push_back(std::move(currentBuffer_));

    // 备用缓冲区是否为空
    if (nextBuffer_)
    {
      // 将备用缓冲区移动到当前缓冲区
      currentBuffer_ = std::move(nextBuffer_);
    }
    else
    {
      // 日志生产者主动分配当前缓冲区的内存
      currentBuffer_.reset(new Buffer); // Rarely happens
    }

    // 拷贝日志数据到缓冲区
    currentBuffer_->append(logline, len);
    
    // 注意：此时会主动唤醒日志消费线程
    cond_.notify();
  }
}

// 后端日志消费线程
void AsyncLogging::threadFunc()
{
  assert(running_ == true);
  latch_.countDown();

  // 初始化日志输出媒介（磁盘文件）
  LogFile output(basename_, rollSize_, false);

  // 消费线程主动创建两个新缓冲区内存
  BufferPtr newBuffer1(new Buffer);
  BufferPtr newBuffer2(new Buffer);
  newBuffer1->bzero();
  newBuffer2->bzero();

  // 复制将要消费的日志缓冲区队列副本到局部变量中
  // 可让实际写入动作，能够放到操作线程安全的“日志缓冲区队列”的临界区之外
  BufferVector buffersToWrite;
  buffersToWrite.reserve(16);
  while (running_)
  {
    assert(newBuffer1 && newBuffer1->length() == 0);
    assert(newBuffer2 && newBuffer2->length() == 0);
    assert(buffersToWrite.empty());

    // 操作日志队列临界区
    {
      muduo::MutexLockGuard lock(mutex_);
      if (buffers_.empty())  // unusual usage!
      {
        // 等待缓冲区写满加入队列通知、或超时返回
        cond_.waitForSeconds(flushInterval_);
      }

      // 将当前缓冲区中日志数据“移动”到 buffers_ 日志缓冲区队列的尾部
      // 交换一个全新的缓冲区给当前缓冲区，以供日志“生产者”写入
      // 注意：交换后的 newBuffer1 中应该为空
      buffers_.push_back(std::move(currentBuffer_));
      currentBuffer_ = std::move(newBuffer1);

      // 复制将要消费的日志缓冲区队列副本到局部变量中
      // 让实际写入操作放在临界区之外
      buffersToWrite.swap(buffers_);

      // 交换一个空的缓冲区到备用缓冲区
      if (!nextBuffer_)
      {
        nextBuffer_ = std::move(newBuffer2);
      }
    }

    assert(!buffersToWrite.empty());

    // 瞬时日志积压了100M(25*4M)。即，每秒产生的日志数据，超过磁盘 I/O 吞吐量
    // 假设磁盘 I/O 吞吐量 500MB/s ，日志每秒产生了日志却多于了 500M+100M
    if (buffersToWrite.size() > 25)
    {
      char buf[256];
      snprintf(buf, sizeof buf, "Dropped log messages at %s, %zd larger buffers\n",
               Timestamp::now().toFormattedString().c_str(),
               buffersToWrite.size()-2);
      
      // 输出到标准错误（意外、紧急错误报告）
      fputs(buf, stderr);

      // 输出到日志文件
      output.append(buf, static_cast<int>(strlen(buf)));
      
      // 只保留最老（队列最前面）的两个日志缓冲区内容
      // 注：当日志写入如此频繁，一般情况下可能是程序陷入了死循环。最老日志价值最高
      buffersToWrite.erase(buffersToWrite.begin()+2, buffersToWrite.end());
    }

    // 消费日志队列。先进先出（最新日志在队列尾部）
    for (const auto& buffer : buffersToWrite)
    {
      // FIXME: use unbuffered stdio FILE ? or use ::writev ?
      output.append(buffer->data(), buffer->length());
    }

    // todo???
    // newBuffer1/newBuffer2 此时应该为空？？？

    if (buffersToWrite.size() > 2)
    {
      // drop non-bzero-ed buffers, avoid trashing
      buffersToWrite.resize(2);
    }

    if (!newBuffer1)
    {
      assert(!buffersToWrite.empty());
      newBuffer1 = std::move(buffersToWrite.back());
      buffersToWrite.pop_back();
      newBuffer1->reset();
    }

    if (!newBuffer2)
    {
      assert(!buffersToWrite.empty());
      newBuffer2 = std::move(buffersToWrite.back());
      buffersToWrite.pop_back();
      newBuffer2->reset();
    }

    buffersToWrite.clear();

    // 刷新到磁盘
    output.flush();
  }
  output.flush();
}

