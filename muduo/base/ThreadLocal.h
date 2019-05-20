// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_THREADLOCAL_H
#define MUDUO_BASE_THREADLOCAL_H

#include "muduo/base/Mutex.h"
#include "muduo/base/noncopyable.h"

#include <pthread.h>  // For pthread_key_*, pthread_setspecific/pthread_getspecific

namespace muduo
{

// tips:
// 用全局变量实现多个函数间数据共享。但在多线程环境下，由于地址空间是共享的，因此
// 全局变量也是所有线程所共享。
// 
// 有时，应用程序设计中可能要提供线程级别的全局变量，仅在某个线程中有效，它可以跨
// 多个函数访问，但却是线程安全的。
// 比如，程序可能需要每个线程维护一个链表，而使用相同的函数操作。最简单的办法就是
// 使用同名而不同变量地址的线程相关数据结构，把这样的数据结构交给 POSIX线程库维护。
// 称为线程私有数据（Thread-specific Data, TSD）。


// 线程本地内存存储区 TLS(thread local storage) 包装器。模板参数 T 为用户自定义类
// RAII 手法管理 TLS
template<typename T>
class ThreadLocal : noncopyable
{
 public:
  ThreadLocal()
  {
    // 从 TSD 池中分配一项，将其值赋给 key 供以后访问使用。
    // 如果 destr_func 不为空，线程退出 pthread_exit() 时，线程将以 key 所关联的数
    // 据为参数（void*）调用 destr_func()，以释放分配的缓冲区。
    // 
    // 不论哪个线程调用 pthread_key_create()，所创建的 key 都是所有线程可访问的，
    // 但各个线程会根据自己的需要，往 key 中填入不同的值，这就相当于提供了一个同名
    // 而不同值的全局变量。
    MCHECK(pthread_key_create(&pkey_, &ThreadLocal::destructor));
  }

  ~ThreadLocal()
  {
    // 这个函数并不检查当前是否有线程正使用该 TSD ，也不会调用清理函数，而只是将 TSD
    // 释放以供下一次调用 pthread_key_create() 使用。
    // 在 Linux中，它还会将与之相关的线程数据项置空（pthread_getspecific(pkey_)==NULL）
    MCHECK(pthread_key_delete(pkey_));
  }

  // 获取/创建线程本地存储区关联的 T 类型对象的引用
  T& value()
  {
    // 读取本线程与 key 相关联的 TSD 数据中的地址
    T* perThreadValue = static_cast<T*>(pthread_getspecific(pkey_));
    if (!perThreadValue)
    {
      T* newObj = new T();  // 创建 T 对象实例
      MCHECK(pthread_setspecific(pkey_, newObj)); // 关联地址到 TSD 中
      perThreadValue = newObj;
    }
    return *perThreadValue;
  }

 private:

  // 线程退出，delete 析构用户自定义对象
  static void destructor(void *x)
  {
    T* obj = static_cast<T*>(x);
    
    // 是否是完全类校验
    typedef char T_must_be_complete_type[sizeof(T) == 0 ? -1 : 1];
    T_must_be_complete_type dummy; (void) dummy;

    // delete 析构用户对象
    delete obj;
  }

 private:
  pthread_key_t pkey_;
};

}  // namespace muduo

#endif  // MUDUO_BASE_THREADLOCAL_H
