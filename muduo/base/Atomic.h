// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_ATOMIC_H
#define MUDUO_BASE_ATOMIC_H

#include "muduo/base/noncopyable.h"

#include <stdint.h>

namespace muduo
{

namespace detail
{

// 整型原子类型：封装常用整型算术运算原子操作
template<typename T>
class AtomicIntegerT : noncopyable
{
 public:
  AtomicIntegerT()
    : value_(0)
  {
  }

  // uncomment if you need copying and assignment
  //
  // AtomicIntegerT(const AtomicIntegerT& that)
  //   : value_(that.get())
  // {}
  //
  // AtomicIntegerT& operator=(const AtomicIntegerT& that)
  // {
  //   getAndSet(that.get());
  //   return *this;
  // }

  T get()
  {
    // in gcc >= 4.7: __atomic_load_n(&value_, __ATOMIC_SEQ_CST)
    //
    // tips:
    // type __sync_val_compare_and_swap(type *ptr, type oldval, type newval, ...);
    // type 适用 1, 2, 4 或 8 字节长度的 int 类型。
    // 原子比较和交换。如果 *ptr == oldval 就将 newval 写入 *ptr, 并返回操作之前的值。
    return __sync_val_compare_and_swap(&value_, 0, 0);
  }

  T getAndAdd(T x)
  {
    // in gcc >= 4.7: __atomic_fetch_add(&value_, x, __ATOMIC_SEQ_CST)
    //
    // 表达式 return value_++ 的原子操作
    return __sync_fetch_and_add(&value_, x);
  }

  T addAndGet(T x)
  {
    return getAndAdd(x) + x;
  }

  T incrementAndGet()
  {
    return addAndGet(1);
  }

  T decrementAndGet()
  {
    return addAndGet(-1);
  }

  void add(T x)
  {
    getAndAdd(x);
  }

  void increment()
  {
    incrementAndGet();
  }

  void decrement()
  {
    decrementAndGet();
  }

  T getAndSet(T newValue)
  {
    // in gcc >= 4.7: __atomic_exchange_n(&value, newValue, __ATOMIC_SEQ_CST)
    // 
    // type __sync_lock_test_and_set(type *ptr, type value, ...);
    // type 适用 1, 2, 4 或 8 字节长度的 int 类型。
    // 将 *ptr 设为 value ，并返回 *ptr 操作之前的值
    return __sync_lock_test_and_set(&value_, newValue);
  }

 private:
  volatile T value_;
};
}  // namespace detail


// 模板实例化常用整型
typedef detail::AtomicIntegerT<int32_t> AtomicInt32;
typedef detail::AtomicIntegerT<int64_t> AtomicInt64;

}  // namespace muduo

#endif  // MUDUO_BASE_ATOMIC_H
