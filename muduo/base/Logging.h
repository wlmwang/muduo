// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_LOGGING_H
#define MUDUO_BASE_LOGGING_H

#include "muduo/base/LogStream.h"
#include "muduo/base/Timestamp.h"

namespace muduo
{

class TimeZone;

// “前端”日志类：日志级别控制。追加公共日志字段、析构时主动回调刷新日志（到磁盘）
class Logger
{
 public:
  enum LogLevel
  {
    TRACE,
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL,
    NUM_LOG_LEVELS,
  };

  // compile time calculation of basename of source file
  //
  // 编译期计算 __FILE_ 源文件基础文件名 basename（调用日志的文件名）
  class SourceFile
  {
   public:
    template<int N>
    SourceFile(const char (&arr)[N])
      : data_(arr),
        size_(N-1)
    {
      // 路径字符串中搜索最后一次出现字符 / 的位置（未找到，会返回一个空指针）
      const char* slash = strrchr(data_, '/'); // builtin function
      if (slash)
      {
        data_ = slash + 1;
        size_ -= static_cast<int>(data_ - arr);
      }
    }

    explicit SourceFile(const char* filename)
      : data_(filename)
    {
      // 路径字符串中搜索最后一次出现字符 / 的位置（未找到，会返回一个空指针）
      const char* slash = strrchr(filename, '/');
      if (slash)
      {
        data_ = slash + 1;
      }
      size_ = static_cast<int>(strlen(data_));
    }

    // 源文件基础文件名 basename（调用日志的文件名）。不要求以 \0 结尾
    const char* data_;
    int size_;  // 不包括 \0 长度
  };

  // 构造时，添加公共日志字段
  Logger(SourceFile file, int line);
  Logger(SourceFile file, int line, LogLevel level);
  Logger(SourceFile file, int line, LogLevel level, const char* func);
  Logger(SourceFile file, int line, bool toAbort);

  // 析构时，结束一行日志字符串，并将日志流写入媒介（标准输出/磁盘）
  ~Logger();

  // “后端”日志流对象
  LogStream& stream() { return impl_.stream_; }

  static LogLevel logLevel();
  static void setLogLevel(LogLevel level);

  // 日志数据刷新回调（写入磁盘）
  typedef void (*OutputFunc)(const char* msg, int len);
  typedef void (*FlushFunc)();
  static void setOutput(OutputFunc);
  static void setFlush(FlushFunc);

  // 设置时区（注：Timestamp::now() 是当地时区的时间戳）
  static void setTimeZone(const TimeZone& tz);

 private:

// 封装“后端”日志流对象
class Impl
{
 public:
  typedef Logger::LogLevel LogLevel;
  Impl(LogLevel level, int old_errno, const SourceFile& file, int line);
  void formatTime();
  void finish();

  // 日志当地时间戳
  Timestamp time_;
  LogStream stream_;
  LogLevel level_;
  int line_;
  SourceFile basename_;
};
  
  // “后端”日志流
  Impl impl_;

};

// 全局日志级别
extern Logger::LogLevel g_logLevel;

inline Logger::LogLevel Logger::logLevel()
{
  return g_logLevel;
}

//
// CAUTION: do not write:
//
// if (good)
//   LOG_INFO << "Good news";
// else
//   LOG_WARN << "Bad news";
//
// this expends to
//
// if (good)
//   if (logging_INFO)
//     logInfoStream << "Good news";
//   else
//     logWarnStream << "Bad news";
//
#define LOG_TRACE if (muduo::Logger::logLevel() <= muduo::Logger::TRACE) \
  muduo::Logger(__FILE__, __LINE__, muduo::Logger::TRACE, __func__).stream()
#define LOG_DEBUG if (muduo::Logger::logLevel() <= muduo::Logger::DEBUG) \
  muduo::Logger(__FILE__, __LINE__, muduo::Logger::DEBUG, __func__).stream()
#define LOG_INFO if (muduo::Logger::logLevel() <= muduo::Logger::INFO) \
  muduo::Logger(__FILE__, __LINE__).stream()
#define LOG_WARN muduo::Logger(__FILE__, __LINE__, muduo::Logger::WARN).stream()
#define LOG_ERROR muduo::Logger(__FILE__, __LINE__, muduo::Logger::ERROR).stream()
#define LOG_FATAL muduo::Logger(__FILE__, __LINE__, muduo::Logger::FATAL).stream()
#define LOG_SYSERR muduo::Logger(__FILE__, __LINE__, false).stream()
#define LOG_SYSFATAL muduo::Logger(__FILE__, __LINE__, true).stream()

const char* strerror_tl(int savedErrno);

// Taken from glog/logging.h
//
// Check that the input is non NULL.  This very useful in constructor
// initializer lists.

// 检查 val 指针是否为不为 NULL 。用在构造函数初始化列表中很方便
#define CHECK_NOTNULL(val) \
  ::muduo::CheckNotNull(__FILE__, __LINE__, "'" #val "' Must be non NULL", (val))

// A small helper for CHECK_NOTNULL().
template <typename T>
T* CheckNotNull(Logger::SourceFile file, int line, const char *names, T* ptr)
{
  if (ptr == NULL)
  {
   Logger(file, line, Logger::FATAL).stream() << names;
  }
  return ptr;
}

}  // namespace muduo

#endif  // MUDUO_BASE_LOGGING_H
