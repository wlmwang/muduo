// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/base/Logging.h"

#include "muduo/base/CurrentThread.h"
#include "muduo/base/Timestamp.h"
#include "muduo/base/TimeZone.h"

#include <errno.h>
#include <stdio.h>  // fwrite,stdout
#include <string.h>

#include <sstream>

namespace muduo
{

/*
class LoggerImpl
{
 public:
  typedef Logger::LogLevel LogLevel;
  LoggerImpl(LogLevel level, int old_errno, const char* file, int line);
  void finish();

  Timestamp time_;
  LogStream stream_;
  LogLevel level_;
  int line_;
  const char* fullname_;
  const char* basename_;
};
*/

// 线程本地存储
// t_errnobuf ：errno 错误码对应的字符串缓冲区
// t_time ：当地时间戳的字符串形式（format = 年月日 时:分:秒）
// t_lastSecond ：为 t_time 字符串上一次格式化时间（秒），可以实现在同一秒内复用 t_time
__thread char t_errnobuf[512];
__thread char t_time[64];
__thread time_t t_lastSecond;

const char* strerror_tl(int savedErrno)
{
  return strerror_r(savedErrno, t_errnobuf, sizeof t_errnobuf);
}

// 默认 INFO 级别
Logger::LogLevel initLogLevel()
{
  if (::getenv("MUDUO_LOG_TRACE"))
    return Logger::TRACE;
  else if (::getenv("MUDUO_LOG_DEBUG"))
    return Logger::DEBUG;
  else
    return Logger::INFO;
}

// 全局初始化日志级别
Logger::LogLevel g_logLevel = initLogLevel();

// 日志级别字符串对照表（预留一个空格）
const char* LogLevelName[Logger::NUM_LOG_LEVELS] =
{
  "TRACE ",
  "DEBUG ",
  "INFO  ",
  "WARN  ",
  "ERROR ",
  "FATAL ",
};

// helper class for known string length at compile time
class T
{
 public:
  T(const char* str, unsigned len)
    :str_(str),
     len_(len)
  {
    // 长度不包含 \0 结尾
    assert(strlen(str) == len_);
  }

  const char* str_;
  const unsigned len_;  // 长度不包含 \0 结尾
};

inline LogStream& operator<<(LogStream& s, T v)
{
  s.append(v.str_, v.len_);
  return s;
}

// 文件名
inline LogStream& operator<<(LogStream& s, const Logger::SourceFile& v)
{
  s.append(v.data_, v.size_);
  return s;
}

// 默认日志写入媒介函数：日志字符串写入到标准输出
void defaultOutput(const char* msg, int len)
{
  size_t n = fwrite(msg, 1, len, stdout);
  //FIXME check n
  (void)n;
}

// 默认日志刷新媒介函数：日志字符串刷新标准输出
void defaultFlush()
{
  fflush(stdout);
}

// 全局日志“写入/刷新”回调。默认为标准输出媒介
Logger::OutputFunc g_output = defaultOutput;
Logger::FlushFunc g_flush = defaultFlush;

// 全局时区
TimeZone g_logTimeZone;

}  // namespace muduo

using namespace muduo;

// 添加公共日志字段
Logger::Impl::Impl(LogLevel level, int savedErrno, const SourceFile& file, int line)
  : time_(Timestamp::now()),  // 当地时间戳
    stream_(),
    level_(level),
    line_(line),
    basename_(file)
{
  // “时间”字段
  formatTime();
  
  // “线程 tid”字段
  CurrentThread::tid(); // tid 缓存
  stream_ << T(CurrentThread::tidString(), CurrentThread::tidStringLength());
  
  // “日志级别”字段
  stream_ << T(LogLevelName[level], 6);
  if (savedErrno != 0)
  {
    // “错误信息及 errno”字段。预留一个空格
    stream_ << strerror_tl(savedErrno) << " (errno=" << savedErrno << ") ";
  }
}

// todo
// 当不设置时区时，日志时间可能会有问题。
// 因为程序传递给 gmtime_r() 的参数是本地时间（应该是日历时间）
//
// 格式化时间
void Logger::Impl::formatTime()
{
  int64_t microSecondsSinceEpoch = time_.microSecondsSinceEpoch();
  time_t seconds = static_cast<time_t>(microSecondsSinceEpoch / Timestamp::kMicroSecondsPerSecond);
  int microseconds = static_cast<int>(microSecondsSinceEpoch % Timestamp::kMicroSecondsPerSecond);
  
  // 1 秒内只更新一次 t_time 时间字符串
  if (seconds != t_lastSecond)
  {
    t_lastSecond = seconds;
    struct tm tm_time;
    if (g_logTimeZone.valid())
    {
      tm_time = g_logTimeZone.toLocalTime(seconds);
    }
    else
    {
      // todo == bug
      ::gmtime_r(&seconds, &tm_time); // FIXME TimeZone::fromUtcTime
    }

    int len = snprintf(t_time, sizeof(t_time), "%4d%02d%02d %02d:%02d:%02d",
        tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
        tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
    
    // 防止 release 版本 unuse warning
    assert(len == 17); (void)len;
  }

  // 保留 6 位微妙数。预留一个空格
  if (g_logTimeZone.valid())
  {
    Fmt us(".%06d ", microseconds);
    assert(us.length() == 8);
    stream_ << T(t_time, 17) << T(us.data(), 8);
  }
  else
  {
    // todo == bug
    Fmt us(".%06dZ ", microseconds);
    assert(us.length() == 9);
    stream_ << T(t_time, 17) << T(us.data(), 9);
  }
}

// 追加日志最后“文件名:行号\n”的字段
void Logger::Impl::finish()
{
  stream_ << " - " << basename_ << ':' << line_ << '\n';
}

Logger::Logger(SourceFile file, int line)
  : impl_(INFO, 0, file, line)
{
}

Logger::Logger(SourceFile file, int line, LogLevel level, const char* func)
  : impl_(level, 0, file, line)
{
  // 追加“函数名”字段
  impl_.stream_ << func << ' ';
}

Logger::Logger(SourceFile file, int line, LogLevel level)
  : impl_(level, 0, file, line)
{
}

Logger::Logger(SourceFile file, int line, bool toAbort)
  : impl_(toAbort?FATAL:ERROR, errno, file, line)
{
}

// 析构时写入日志流（主动回调写入日志媒介接口）
// 发生严重错误(FATAL)，立即刷新磁盘
Logger::~Logger()
{
  // 追加“文件名:行号\n”字段
  impl_.finish();

  // 主动回调写入日志媒介接口
  const LogStream::Buffer& buf(stream().buffer());
  g_output(buf.data(), buf.length());

  if (impl_.level_ == FATAL)
  {
    // fatal 级别日志，立刻回调刷新日志接口，退出并生成 coredump
    g_flush();
    abort();
  }
}

void Logger::setLogLevel(Logger::LogLevel level)
{
  g_logLevel = level;
}

void Logger::setOutput(OutputFunc out)
{
  g_output = out;
}

void Logger::setFlush(FlushFunc flush)
{
  g_flush = flush;
}

void Logger::setTimeZone(const TimeZone& tz)
{
  g_logTimeZone = tz;
}
