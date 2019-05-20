// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_SINGLETON_H
#define MUDUO_BASE_SINGLETON_H

#include "muduo/base/noncopyable.h"

#include <assert.h>
#include <pthread.h>
#include <stdlib.h> // atexit

namespace muduo
{

namespace detail
{
// This doesn't detect inherited member functions!
// http://stackoverflow.com/questions/1966362/sfinae-to-check-for-inherited-member-functions
//
// 编译器检查类型 T 是否存在 no_destroy 方法
template<typename T>
struct has_no_destroy
{
  template <typename C> static char test(decltype(&C::no_destroy));
  template <typename C> static int32_t test(...);
  const static bool value = sizeof(test<T>(0)) == 1;
};
}  // namespace detail

template<typename T>
class Singleton : noncopyable
{
 public:
  Singleton() = delete;
  ~Singleton() = delete;

  static T& instance()
  {
    // 获取单例实例化对象。
    // POSIX 平台下，进程级别控制运行一次 pthread_once
    pthread_once(&ponce_, &Singleton::init);
    assert(value_ != NULL);
    return *value_;
  }

 private:
  static void init()
  {
    // 分配对象
    value_ = new T();
    if (!detail::has_no_destroy<T>::value)
    {
      // 未定义 T::no_destroy 方法时，进程退出时，执行 delete 释放内存
      ::atexit(destroy);
    }
  }

  // 默认创建的单例“析构”函数（除非定义了 T::no_destroy 方法）
  static void destroy()
  {
    // 编译器静态检查是否为“完成类”
    typedef char T_must_be_complete_type[sizeof(T) == 0 ? -1 : 1];
    T_must_be_complete_type dummy; (void) dummy;

    delete value_;
    value_ = NULL;
  }

 private:
  static pthread_once_t ponce_;
  static T*             value_;
};

template<typename T>
pthread_once_t Singleton<T>::ponce_ = PTHREAD_ONCE_INIT;

template<typename T>
T* Singleton<T>::value_ = NULL;

}  // namespace muduo

#endif  // MUDUO_BASE_SINGLETON_H
