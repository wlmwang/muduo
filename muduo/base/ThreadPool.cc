// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/base/ThreadPool.h"

#include "muduo/base/Exception.h"

#include <assert.h>
#include <stdio.h>

using namespace muduo;

ThreadPool::ThreadPool(const string& nameArg)
  : mutex_(),
    notEmpty_(mutex_),
    notFull_(mutex_),
    name_(nameArg),
    maxQueueSize_(0),
    running_(false)
{
}

ThreadPool::~ThreadPool()
{
  if (running_)
  {
    stop();
  }
}

void ThreadPool::start(int numThreads)
{
  assert(threads_.empty());
  running_ = true;

  threads_.reserve(numThreads);
  for (int i = 0; i < numThreads; ++i)
  {
    char id[32];
    snprintf(id, sizeof id, "%d", i+1);
    
    // 在 std::vector 就地构造 Thread 对象，省去复制或移动操作
    threads_.emplace_back(new muduo::Thread(
          std::bind(&ThreadPool::runInThread, this), name_+id));
    
    // 启动该线程（并等待该线程创建成功）
    threads_[i]->start();
  }

  // 线程池数量为 0 时，当前线程充当“任务线程”。
  // 生产任务时，在创建线程池的线程中直接运行任务。ThreadPool::run()
  if (numThreads == 0 && threadInitCallback_)
  {
    threadInitCallback_();
  }
}

void ThreadPool::stop()
{
  // 广播所有线程停止消费
  {
  MutexLockGuard lock(mutex_);
  running_ = false;
  notEmpty_.notifyAll();
  }

  // 等待所有线程退出
  for (auto& thr : threads_)
  {
    thr->join();
  }
}

// 当前任务队列大小
size_t ThreadPool::queueSize() const
{
  MutexLockGuard lock(mutex_);
  return queue_.size();
}

// 任务生产者
void ThreadPool::run(Task task)
{
  if (threads_.empty())
  {
    // 线程池数量为 0 时，在创建线程池的线程中直接运行任务
    task();
  }
  else
  {
    MutexLockGuard lock(mutex_);
    while (isFull())
    {
      // 队列已满，等待消费者消费（未满时，通知我）
      notFull_.wait();
    }
    assert(!isFull());

    queue_.push_back(std::move(task));

    // 通知消费者，队列可用（非空）
    notEmpty_.notify();
  }
}

// 抢占式获取任务（阻塞方式）
ThreadPool::Task ThreadPool::take()
{
  MutexLockGuard lock(mutex_);
  // always use a while-loop, due to spurious wakeup
  while (queue_.empty() && running_)
  {
    // 队列为空，等待生产者（非空时，通知我）
    notEmpty_.wait();
  }

  Task task;
  if (!queue_.empty())
  {
    // 先进先出, FIFO
    task = queue_.front();
    queue_.pop_front();
    if (maxQueueSize_ > 0)
    {
      // 通知生产者，队列可用（未满）
      notFull_.notify();
    }
  }
  return task;
}

// 任务队列是否已满
bool ThreadPool::isFull() const
{
  mutex_.assertLocked();
  return maxQueueSize_ > 0 && queue_.size() >= maxQueueSize_;
}

// 线程池中，每个线程中运行的入口函数
void ThreadPool::runInThread()
{
  // 捕获线程任务消费执行异常
  try
  {
    // 线程初始化
    if (threadInitCallback_)
    {
      threadInitCallback_();
    }

    // 循环消费任务队列
    while (running_)
    {
      // 抢占式获取任务（阻塞方式）
      Task task(take());
      if (task)
      {
        task();
      }
    }
  }
  catch (const Exception& ex)
  {
    fprintf(stderr, "exception caught in ThreadPool %s\n", name_.c_str());
    fprintf(stderr, "reason: %s\n", ex.what());
    fprintf(stderr, "stack trace: %s\n", ex.stackTrace());
    abort();
  }
  catch (const std::exception& ex)
  {
    fprintf(stderr, "exception caught in ThreadPool %s\n", name_.c_str());
    fprintf(stderr, "reason: %s\n", ex.what());
    abort();
  }
  catch (...)
  {
    fprintf(stderr, "unknown exception caught in ThreadPool %s\n", name_.c_str());
    throw; // rethrow
  }
}

