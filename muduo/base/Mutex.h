// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_MUTEX_H
#define MUDUO_BASE_MUTEX_H

#include "muduo/base/CurrentThread.h"
#include "muduo/base/noncopyable.h"
#include <assert.h>   // For __assert_perror_fail()
#include <pthread.h>

// Thread safety annotations {
// https://clang.llvm.org/docs/ThreadSafetyAnalysis.html

// tips:
// see: https://zhuanlan.zhihu.com/p/47837673
// 线程安全注解的基本用法是，通过代码注解（annotations ）告诉编译器
// 哪些成员变量和成员函数是受哪个 mutex保护，这样如果忘记了加锁，编
// 译器会给警告。
// 因为在后续维护别人的代码时候，往往不像原作者那样深刻理解设计意图，
// 特别容易遗漏线程安全的假设。这个工具正是为了应对这种情况，让原作
// 者能把设计意图用代码注解清楚地写出来并让编译器能自动检查，让后来
// 的维护者就不容易犯低级错误了。


// clang 启用线程安全注解宏 -Wthread-safety
//
// 只要在自己的代码中用 GUARDED_BY 表明哪个成员变量是被哪个 mutex 
// 保护的，就可以让 clang 帮你检查有没有遗漏加锁的情况了。别忘了用 
// clang -Wthread-safety 编译你的代码。
// 
// Use like:
// class Test {
// private:
//   mutable MutexLock mutex_;
//   std::vector<int> data_ GUARDED_BY(mutex_);
// }


// Enable thread safety attributes only with clang.
// The attributes can be safely erased when compiling with other compilers.
#if defined(__clang__) && (!defined(SWIG))
#define THREAD_ANNOTATION_ATTRIBUTE__(x)   __attribute__((x))
#else
#define THREAD_ANNOTATION_ATTRIBUTE__(x)   // no-op
#endif

#define CAPABILITY(x) \
  THREAD_ANNOTATION_ATTRIBUTE__(capability(x))

#define SCOPED_CAPABILITY \
  THREAD_ANNOTATION_ATTRIBUTE__(scoped_lockable)

#define GUARDED_BY(x) \
  THREAD_ANNOTATION_ATTRIBUTE__(guarded_by(x))

#define PT_GUARDED_BY(x) \
  THREAD_ANNOTATION_ATTRIBUTE__(pt_guarded_by(x))

#define ACQUIRED_BEFORE(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(acquired_before(__VA_ARGS__))

#define ACQUIRED_AFTER(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(acquired_after(__VA_ARGS__))

#define REQUIRES(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(requires_capability(__VA_ARGS__))

#define REQUIRES_SHARED(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(requires_shared_capability(__VA_ARGS__))

#define ACQUIRE(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(acquire_capability(__VA_ARGS__))

#define ACQUIRE_SHARED(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(acquire_shared_capability(__VA_ARGS__))

#define RELEASE(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(release_capability(__VA_ARGS__))

#define RELEASE_SHARED(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(release_shared_capability(__VA_ARGS__))

#define TRY_ACQUIRE(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(try_acquire_capability(__VA_ARGS__))

#define TRY_ACQUIRE_SHARED(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(try_acquire_shared_capability(__VA_ARGS__))

#define EXCLUDES(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(locks_excluded(__VA_ARGS__))

#define ASSERT_CAPABILITY(x) \
  THREAD_ANNOTATION_ATTRIBUTE__(assert_capability(x))

#define ASSERT_SHARED_CAPABILITY(x) \
  THREAD_ANNOTATION_ATTRIBUTE__(assert_shared_capability(x))

#define RETURN_CAPABILITY(x) \
  THREAD_ANNOTATION_ATTRIBUTE__(lock_returned(x))

#define NO_THREAD_SAFETY_ANALYSIS \
  THREAD_ANNOTATION_ATTRIBUTE__(no_thread_safety_analysis)

// End of thread safety annotations }


// 始终检测 pthread 函数簇的返回值（无论是 DEBUG/RELEASE 编译）
#ifdef CHECK_PTHREAD_RETURN_VALUE

// \file <assert.h>
// __assert_perror_fail 函数根据 errnum 值，输出相应的错误信息到标准
// 错误流中，然后调用 abort 函数终止程序（生成 coredump）
// __assert_perror_fail 是调用 strerror_r 来输出 errnum 诊断信息的
//
// 在 Linux 中 __assert_perror_fail 函数只在未定义 NDEBUG 下才申明
#ifdef NDEBUG
__BEGIN_DECLS
extern void __assert_perror_fail (int errnum,
                                  const char *file,
                                  unsigned int line,
                                  const char *function)
    noexcept __attribute__ ((__noreturn__));
__END_DECLS
#endif

// 无论是 DEBUG/RELEASE 编译，都进行返回值断言
#define MCHECK(ret) ({ __typeof__ (ret) errnum = (ret);         \
                       if (__builtin_expect(errnum != 0, 0))    \
                         __assert_perror_fail (errnum, __FILE__, __LINE__, __func__);})

#else  // CHECK_PTHREAD_RETURN_VALUE

#define MCHECK(ret) ({ __typeof__ (ret) errnum = (ret);         \
                       assert(errnum == 0); (void) errnum;})

#endif // CHECK_PTHREAD_RETURN_VALUE

namespace muduo
{

// Use as data member of a class, eg.
//
// class Foo
// {
//  public:
//   int size() const;
//
//  private:
//   mutable MutexLock mutex_;
//   std::vector<int> data_ GUARDED_BY(mutex_);
// };
//
// 互斥体
class CAPABILITY("mutex") MutexLock : noncopyable
{
 public:
  MutexLock()
    : holder_(0)
  {
    MCHECK(pthread_mutex_init(&mutex_, NULL));
  }

  ~MutexLock()
  {
    assert(holder_ == 0);
    MCHECK(pthread_mutex_destroy(&mutex_));
  }

  // must be called when locked, i.e. for assertion
  bool isLockedByThisThread() const
  {
    return holder_ == CurrentThread::tid();
  }

  void assertLocked() const ASSERT_CAPABILITY(this)
  {
    assert(isLockedByThisThread());
  }

  // internal usage

  void lock() ACQUIRE()
  {
    MCHECK(pthread_mutex_lock(&mutex_));
    assignHolder();
  }

  void unlock() RELEASE()
  {
    unassignHolder();
    MCHECK(pthread_mutex_unlock(&mutex_));
  }

  pthread_mutex_t* getPthreadMutex() /* non-const */
  {
    return &mutex_;
  }

 private:
  // 友元类。方便条件变量访问互斥体内部属性
  friend class Condition;

  // RAII 手法释放互斥体内部维护的持锁状态 holder_
  // 一般用在条件变量中，进入阻塞等待通知前，先释放锁
  //
  // 条件变量进入等待时，内核会原子释放锁；并在结束时，调用线程会
  // 再次获取锁。即使返回错误时也是如此
  class UnassignGuard : noncopyable
  {
   public:
    explicit UnassignGuard(MutexLock& owner)
      : owner_(owner)
    {
      owner_.unassignHolder();
    }

    ~UnassignGuard()
    {
      owner_.assignHolder();
    }

   private:
    MutexLock& owner_;
  };

  void unassignHolder()
  {
    holder_ = 0;
  }

  void assignHolder()
  {
    holder_ = CurrentThread::tid();
  }

  pthread_mutex_t mutex_;
  pid_t holder_;
};

// Use as a stack variable, eg.
// int Foo::size() const
// {
//   MutexLockGuard lock(mutex_);
//   return data_.size();
// }
//
// RAII 手法互斥体
class SCOPED_CAPABILITY MutexLockGuard : noncopyable
{
 public:
  explicit MutexLockGuard(MutexLock& mutex) ACQUIRE(mutex)
    : mutex_(mutex)
  {
    mutex_.lock();
  }

  ~MutexLockGuard() RELEASE()
  {
    mutex_.unlock();
  }

 private:

  MutexLock& mutex_;
};

}  // namespace muduo

// Prevent misuse like:
// MutexLockGuard(mutex_);
// A tempory object doesn't hold the lock for long!
#define MutexLockGuard(x) error "Missing guard object name"

#endif  // MUDUO_BASE_MUTEX_H
