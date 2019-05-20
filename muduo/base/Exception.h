// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_EXCEPTION_H
#define MUDUO_BASE_EXCEPTION_H

#include "muduo/base/Types.h"
#include <exception>

namespace muduo
{

// 自定义异常类
class Exception : public std::exception
{
 public:
  Exception(string what);
  ~Exception() noexcept override = default;

  // default copy-ctor and operator= are okay.

  // 获取异常字符串
  const char* what() const noexcept override
  {
    return message_.c_str();
  }

  // 获取调用栈
  const char* stackTrace() const noexcept
  {
    return stack_.c_str();
  }

 private:
  string message_;
  string stack_;
};

}  // namespace muduo

#endif  // MUDUO_BASE_EXCEPTION_H
