// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_LOGFILE_H
#define MUDUO_BASE_LOGFILE_H

#include "muduo/base/Mutex.h"
#include "muduo/base/Types.h"

#include <memory>

namespace muduo
{

namespace FileUtil
{
class AppendFile;
}

class LogFile : noncopyable
{
 public:
  LogFile(const string& basename,
          off_t rollSize,
          bool threadSafe = true,
          int flushInterval = 3,
          int checkEveryN = 1024);
  ~LogFile();

  void append(const char* logline, int len);
  void flush();

  // 滚动日志（被滚动的 FileUtil 日志对象析构时，会将数据刷新到磁盘）
  bool rollFile();

 private:
  void append_unlocked(const char* logline, int len);

  // 日志文件名：basename.Ymd-HMS.hostname.pid.log
  static string getLogFileName(const string& basename, time_t* now);

  const string basename_;
  const off_t rollSize_;
  const int flushInterval_;
  const int checkEveryN_;

  int count_;

  std::unique_ptr<MutexLock> mutex_;
  time_t startOfPeriod_;// 滚动日志当日 0 点时间戳
  time_t lastRoll_;     // 上一次滚动日志文件时间戳
  time_t lastFlush_;    // 上一次刷新磁盘时间戳
  std::unique_ptr<FileUtil::AppendFile> file_;  // 日志文件句柄

  const static int kRollPerSeconds_ = 60*60*24;
};

}  // namespace muduo
#endif  // MUDUO_BASE_LOGFILE_H
