// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_LOGSTREAM_H
#define MUDUO_BASE_LOGSTREAM_H

#include "muduo/base/noncopyable.h"
#include "muduo/base/StringPiece.h"
#include "muduo/base/Types.h"
#include <assert.h>
#include <string.h> // memcpy

namespace muduo
{

namespace detail
{

const int kSmallBuffer = 4000;  // ~4k 单条日志大小限制
const int kLargeBuffer = 4000*1000; // ~4m 单个异步日志缓冲区大小（可缓存最少 1000条）

// 固定大小 buffer 缓冲区
template<int SIZE>
class FixedBuffer : noncopyable
{
 public:
  FixedBuffer()
    : cur_(data_)
  {
    setCookie(cookieStart);
  }

  ~FixedBuffer()
  {
    setCookie(cookieEnd);
  }

  // 追加方式，拷贝 buf 日志字节流到当前缓冲区中。单条日志不能多于 4k
  void append(const char* /*restrict*/ buf, size_t len)
  {
    // FIXME: append partially
    if (implicit_cast<size_t>(avail()) > len)
    {
      memcpy(cur_, buf, len);
      cur_ += len;
    }
  }

  // 获取日志字节流（头地址、字节长度）接口
  const char* data() const { return data_; }
  int length() const { return static_cast<int>(cur_ - data_); }

  // write to data_ directly
  // 
  // 以下函数可供使用者直接写入字节流缓冲区时，可能要调用的接口方法
  char* current() { return cur_; }
  int avail() const { return static_cast<int>(end() - cur_); }
  void add(size_t len) { cur_ += len; }

  // 清除/初始化字节流缓冲区
  void reset() { cur_ = data_; }
  void bzero() { memZero(data_, sizeof data_); }

  // for used by GDB
  //
  // todo
  const char* debugString();
  void setCookie(void (*cookie)()) { cookie_ = cookie; }
  // for used by unit test
  string toString() const { return string(data_, length()); }
  StringPiece toStringPiece() const { return StringPiece(data_, length()); }

 private:
  const char* end() const { return data_ + sizeof data_; }
  // Must be outline function for cookies.
  static void cookieStart();
  static void cookieEnd();

  void (*cookie_)();  // todo
  char data_[SIZE];   // 日志字节流缓冲区（单条日志）
  char* cur_;         // 当前已使用缓冲区的偏移地址
};

}  // namespace detail

// “后端”日志类：operator<< 类型安全、日志流缓冲
class LogStream : noncopyable
{
  typedef LogStream self;
 public:
  typedef detail::FixedBuffer<detail::kSmallBuffer> Buffer;

  // 重载各种数据类型的输出操作符 operator<<
  // 即，类型安全

  self& operator<<(bool v)
  {
    buffer_.append(v ? "1" : "0", 1);
    return *this;
  }

  self& operator<<(short);
  self& operator<<(unsigned short);
  self& operator<<(int);
  self& operator<<(unsigned int);
  self& operator<<(long);
  self& operator<<(unsigned long);
  self& operator<<(long long);
  self& operator<<(unsigned long long);

  // “无类型”数据会被当成 16 进制值输出（一般用在输出地址值）
  // 本质上，参数是一个十进制整型值，底层将其转换成 16 进制的字符串
  self& operator<<(const void*);

  self& operator<<(float v)
  {
    *this << static_cast<double>(v);
    return *this;
  }
  self& operator<<(double);
  // self& operator<<(long double);

  self& operator<<(char v)
  {
    buffer_.append(&v, 1);
    return *this;
  }

  // self& operator<<(signed char);
  // self& operator<<(unsigned char);

  self& operator<<(const char* str)
  {
    if (str)
    {
      buffer_.append(str, strlen(str));
    }
    else
    {
      buffer_.append("(null)", 6);
    }
    return *this;
  }

  self& operator<<(const unsigned char* str)
  {
    return operator<<(reinterpret_cast<const char*>(str));
  }

  self& operator<<(const string& v)
  {
    buffer_.append(v.c_str(), v.size());
    return *this;
  }

  self& operator<<(const StringPiece& v)
  {
    buffer_.append(v.data(), v.size());
    return *this;
  }

  self& operator<<(const Buffer& v)
  {
    *this << v.toStringPiece();
    return *this;
  }

  // 追加拷贝日志数据、重置缓冲区
  void append(const char* data, int len) { buffer_.append(data, len); }
  const Buffer& buffer() const { return buffer_; }
  void resetBuffer() { buffer_.reset(); }

 private:
  void staticCheck();

  template<typename T>
  void formatInteger(T);

  Buffer buffer_;   // 日志缓冲区

  static const int kMaxNumericSize = 32; // 最大数字长度（精度）
};

// 算术数据类型格式化（c-printf style）
class Fmt // : noncopyable
{
 public:
  template<typename T>
  Fmt(const char* fmt, T val);

  const char* data() const { return buf_; }
  int length() const { return length_; }

 private:
  char buf_[32];
  int length_;
};

// 将格式化后的字节流，追加拷贝到日志流中
// os << muduo::Fmt("%4.2f", 1.2);
inline LogStream& operator<<(LogStream& s, const Fmt& fmt)
{
  s.append(fmt.data(), fmt.length());
  return s;
}

// Format quantity n in SI units (k, M, G, T, P, E).
// The returned string is atmost 5 characters long.
// Requires n >= 0
string formatSI(int64_t n);

// Format quantity n in IEC (binary) units (Ki, Mi, Gi, Ti, Pi, Ei).
// The returned string is atmost 6 characters long.
// Requires n >= 0
string formatIEC(int64_t n);

}  // namespace muduo

#endif  // MUDUO_BASE_LOGSTREAM_H
