// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/base/Thread.h"
#include "muduo/base/CurrentThread.h"
#include "muduo/base/Exception.h"
#include "muduo/base/Logging.h"

#include <type_traits>

#include <errno.h>
#include <stdio.h>        // For fprintf
#include <unistd.h>       // For abort
#include <sys/prctl.h>    // For prctl
#include <sys/syscall.h>  // For syscall(SYS_gettid)
#include <sys/types.h>
#include <linux/unistd.h>

namespace muduo
{
namespace detail
{

// 获取线程 tid （gettid() 是不可移植的。Linux 平台实现）。
//
// tips:
// 在一个进程中，主线程的线程 tid 和进程 pid 是相等的。该进程中其他的线
// 程 tid 在 Linux 系统内是唯一的（pid_t周期内），它与 pid 规则一样，因
// 为在 Linux 中线程就是进程，而进程号是唯一的
//
// gettid 是系统调用，返回值是 pid_t ，在 Linux 上是一个无符号整型
pid_t gettid()
{
  return static_cast<pid_t>(::syscall(SYS_gettid));
}


/**
 * tips:
 * int pthread_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void));
 * pthread_atfork() 在 fork() 之前调用。当调用 fork 时，内部创建子进程前，在父进程中会
 * 调用 prepare() ，内部创建子进程成功后，父进程会调用 parent() ，子进程会调用 child().
 */

void afterFork()
{
  muduo::CurrentThread::t_cachedTid = 0;
  muduo::CurrentThread::t_threadName = "main";
  CurrentThread::tid();
  // no need to call pthread_atfork(NULL, NULL, &afterFork);
}

class ThreadNameInitializer
{
 public:
  ThreadNameInitializer()
  {
    muduo::CurrentThread::t_threadName = "main";
    CurrentThread::tid();
    pthread_atfork(NULL, NULL, &afterFork);
  }
};

// 全局变量达到初始化 main 主线程中的名字
ThreadNameInitializer init;

// pthread_create 系统调用的线程入口函数的参数数据结构
//
// 线程创建者负责清除 ThreadData 内存。并 tid_,latch_ 变量所有权不属于线程，而是线程
// 创建者 Thread
struct ThreadData
{
  typedef muduo::Thread::ThreadFunc ThreadFunc;
  ThreadFunc func_;   // 用户自定义线程入口函数
  string name_; // 线程名称
  pid_t* tid_;  // 标识符
  CountDownLatch* latch_; // 阻塞倒计数（线程创建完成，递减计数器，并通知线程创建者）

  ThreadData(ThreadFunc func,
             const string& name,
             pid_t* tid,
             CountDownLatch* latch)
    : func_(std::move(func)),
      name_(name),
      tid_(tid),
      latch_(latch)
  { }

  // 子线程入口函数
  void runInThread()
  {
    // todo

    // 初始化线程 tid
    // 线程内，释放 tid_ 所有权。该标识符已被存储到线程级全局变量中
    *tid_ = muduo::CurrentThread::tid();
    tid_ = NULL;

    // 递减计数器（标识线程创建成功）
    // 线程内，释放 latch_ 所有权
    latch_->countDown();
    latch_ = NULL;

    // 初始化线程名
    muduo::CurrentThread::t_threadName = name_.empty() ? "muduoThread" : name_.c_str();
    ::prctl(PR_SET_NAME, muduo::CurrentThread::t_threadName);

    // 捕获线程用户函数执行异常
    try
    {
      // 子线程内，调用用户自定义入口函数
      func_();
      muduo::CurrentThread::t_threadName = "finished";
    }
    catch (const Exception& ex)
    {
      muduo::CurrentThread::t_threadName = "crashed";
      fprintf(stderr, "exception caught in Thread %s\n", name_.c_str());
      fprintf(stderr, "reason: %s\n", ex.what());
      fprintf(stderr, "stack trace: %s\n", ex.stackTrace());
      abort();
    }
    catch (const std::exception& ex)
    {
      muduo::CurrentThread::t_threadName = "crashed";
      fprintf(stderr, "exception caught in Thread %s\n", name_.c_str());
      fprintf(stderr, "reason: %s\n", ex.what());
      abort();
    }
    catch (...)
    {
      muduo::CurrentThread::t_threadName = "crashed";
      fprintf(stderr, "unknown exception caught in Thread %s\n", name_.c_str());
      throw; // rethrow
    }
  }
};

// 启动线程
// pthread_create 系统调用的线程入口函数
void* startThread(void* obj)
{
  ThreadData* data = static_cast<ThreadData*>(obj);
  data->runInThread();

  // 线程结束，清除内存
  delete data;
  return NULL;
}

}  // namespace detail

// 缓存线程 tid 标识符
void CurrentThread::cacheTid()
{
  if (t_cachedTid == 0)
  {
    t_cachedTid = detail::gettid();

    // tid 在 Linux 中默认最大值为 32768
    t_tidStringLength = snprintf(t_tidString, sizeof t_tidString, "%5d ", t_cachedTid);
  }
}

// 当前线程是否是主线程
bool CurrentThread::isMainThread()
{
  return tid() == ::getpid();
}

// 线程 nanosleep 睡眠函数
void CurrentThread::sleepUsec(int64_t usec)
{
  struct timespec ts = { 0, 0 };
  ts.tv_sec = static_cast<time_t>(usec / Timestamp::kMicroSecondsPerSecond);
  ts.tv_nsec = static_cast<long>(usec % Timestamp::kMicroSecondsPerSecond * 1000);
  ::nanosleep(&ts, NULL);
}

// 申明、初始化已创建线程总数的计数器
AtomicInt32 Thread::numCreated_;

Thread::Thread(ThreadFunc func, const string& n)
  : started_(false),
    joined_(false),
    pthreadId_(0),
    tid_(0),
    func_(std::move(func)),
    name_(n),
    latch_(1)
{
  // 初始化线程名称
  setDefaultName();
}

Thread::~Thread()
{
  // Thread 生命周期结束时，若未调用 join()，则分离线程，变成“背景线程”
  // 调用者需手动回收线程资源（ pthread_join 会自动调用线程的垃圾回收）
  if (started_ && !joined_)
  {
    pthread_detach(pthreadId_);
  }
}

// 线程默认名称
void Thread::setDefaultName()
{
  int num = numCreated_.incrementAndGet();
  if (name_.empty())
  {
    char buf[32];
    snprintf(buf, sizeof buf, "Thread%d", num);
    name_ = buf;  // 拷贝
  }
}

// 启动线程（并等待该线程创建成功）
void Thread::start()
{
  assert(!started_);
  started_ = true;

  // FIXME: move(func_)
  detail::ThreadData* data = new detail::ThreadData(func_, name_, &tid_, &latch_);

  // 创建线程，并等待子线程“创建完成”通知
  if (pthread_create(&pthreadId_, NULL, &detail::startThread, data))
  {
    started_ = false;
    delete data; // or no delete?
    LOG_SYSFATAL << "Failed in pthread_create";
  }
  else
  {
    latch_.wait();
    assert(tid_ > 0);
  }
}

// 将 Thread 对象管理的线程“加入”调用线程
// 调用线程将阻塞等待该线程运行结束
int Thread::join()
{
  assert(started_);
  assert(!joined_);
  joined_ = true;
  return pthread_join(pthreadId_, NULL);
}

}  // namespace muduo
