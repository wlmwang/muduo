// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_WEAKCALLBACK_H
#define MUDUO_BASE_WEAKCALLBACK_H

#include <functional>
#include <memory>

namespace muduo
{

// A barely usable WeakCallback

// 类成员函数的弱回调包装器
template<typename CLASS, typename... ARGS>
class WeakCallback
{
 public:

  WeakCallback(const std::weak_ptr<CLASS>& object,
               const std::function<void (CLASS*, ARGS...)>& function)
    : object_(object), function_(function)
  {
  }

  // Default dtor, copy ctor and assignment are okay

  // 执行回调（弱回调）
  void operator()(ARGS&&... args) const
  {
    std::shared_ptr<CLASS> ptr(object_.lock());
    if (ptr)
    {
      // “完美”转发回调函数的参数
      function_(ptr.get(), std::forward<ARGS>(args)...);
    }
    // else
    // {
    //   LOG_TRACE << "expired";
    // }
  }

 private:

  // 回调函数主对象（调用类成员函数）
  std::weak_ptr<CLASS> object_;

  // 类成员函数回调原型
  std::function<void (CLASS*, ARGS...)> function_;
};

// 创建类成员函数的弱回调
template<typename CLASS, typename... ARGS>
WeakCallback<CLASS, ARGS...> makeWeakCallback(const std::shared_ptr<CLASS>& object,
                                              void (CLASS::*function)(ARGS...))
{
  return WeakCallback<CLASS, ARGS...>(object, function);
}

// 创建 const 类成员函数的弱回调
template<typename CLASS, typename... ARGS>
WeakCallback<CLASS, ARGS...> makeWeakCallback(const std::shared_ptr<CLASS>& object,
                                              void (CLASS::*function)(ARGS...) const)
{
  return WeakCallback<CLASS, ARGS...>(object, function);
}

}  // namespace muduo

#endif  // MUDUO_BASE_WEAKCALLBACK_H
