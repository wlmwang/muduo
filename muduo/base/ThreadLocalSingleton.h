// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_THREADLOCALSINGLETON_H
#define MUDUO_BASE_THREADLOCALSINGLETON_H

#include "muduo/base/noncopyable.h"

#include <assert.h>
#include <pthread.h>

namespace muduo
{

// 线程级全局单例实例。T 为用户自定义类
template<typename T>
class ThreadLocalSingleton : noncopyable
{
 public:
  ThreadLocalSingleton() = delete;
  ~ThreadLocalSingleton() = delete;

  static T& instance()
  {
    if (!t_value_)
    {
      t_value_ = new T();
      deleter_.set(t_value_);
    }
    return *t_value_;
  }

  static T* pointer()
  {
    return t_value_;
  }

 private:
  // 析构用户自定义 T
  static void destructor(void* obj)
  {
    // 必须析构当前线程的 TLS 变量
    assert(obj == t_value_);

    // 是否是完全类校验
    typedef char T_must_be_complete_type[sizeof(T) == 0 ? -1 : 1];
    T_must_be_complete_type dummy; (void) dummy;
    
    delete t_value_;
    t_value_ = 0;
  }

  // 线程局部变量“析构器”，及 TLS 实际绑定的 pthread_key_t
  class Deleter
  {
   public:
    Deleter()
    {
      pthread_key_create(&pkey_, &ThreadLocalSingleton::destructor);
    }

    ~Deleter()
    {
      pthread_key_delete(pkey_);
    }

    void set(T* newObj)
    {
      assert(pthread_getspecific(pkey_) == NULL);
      pthread_setspecific(pkey_, newObj);
    }

    pthread_key_t pkey_;
  };

  static __thread T* t_value_;  // 线程级单例指针
  static Deleter deleter_;      // 线程 TLS 底层实现
};

// 静态变量申明及初始化
template<typename T>
__thread T* ThreadLocalSingleton<T>::t_value_ = 0;

template<typename T>
typename ThreadLocalSingleton<T>::Deleter ThreadLocalSingleton<T>::deleter_;

}  // namespace muduo
#endif  // MUDUO_BASE_THREADLOCALSINGLETON_H
