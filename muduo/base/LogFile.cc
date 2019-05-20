// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/base/LogFile.h"

#include "muduo/base/FileUtil.h"
#include "muduo/base/ProcessInfo.h"

#include <assert.h>
#include <stdio.h>
#include <time.h>

using namespace muduo;

LogFile::LogFile(const string& basename,
                 off_t rollSize,
                 bool threadSafe,
                 int flushInterval,
                 int checkEveryN)
  : basename_(basename),
    rollSize_(rollSize),
    flushInterval_(flushInterval),
    checkEveryN_(checkEveryN),
    count_(0),
    mutex_(threadSafe ? new MutexLock : NULL),
    startOfPeriod_(0),
    lastRoll_(0),
    lastFlush_(0)
{
  // 不能带有文件路径
  assert(basename.find('/') == string::npos);
  rollFile();
}

LogFile::~LogFile() = default;

void LogFile::append(const char* logline, int len)
{
  if (mutex_)
  {
    MutexLockGuard lock(*mutex_);
    append_unlocked(logline, len);
  }
  else
  {
    append_unlocked(logline, len);
  }
}

void LogFile::flush()
{
  if (mutex_)
  {
    MutexLockGuard lock(*mutex_);
    file_->flush();
  }
  else
  {
    file_->flush();
  }
}

void LogFile::append_unlocked(const char* logline, int len)
{
  // 写入日志文件缓冲区（非线程安全）
  file_->append(logline, len);

  if (file_->writtenBytes() > rollSize_)
  {
    // 滚动日志
    rollFile();
  }
  else
  {
    ++count_;
    // 每写入 1024 条日志，检测一次日志文件（是否滚动或刷新到磁盘）
    if (count_ >= checkEveryN_)
    {
      count_ = 0;
      time_t now = ::time(NULL);

      // 今日 0 点时间戳（当日开始时间）
      time_t thisPeriod_ = now / kRollPerSeconds_ * kRollPerSeconds_;
      if (thisPeriod_ != startOfPeriod_)
      {
        // 隔天必须滚动日志
        rollFile();
      }
      else if (now - lastFlush_ > flushInterval_)
      {
        // 大于刷新频率间隔，主动刷新到磁盘
        lastFlush_ = now;
        file_->flush();
      }
    }
  }
}

// 滚动日志（被滚动的 FileUtil 日志对象析构时，会将数据刷新到磁盘）
bool LogFile::rollFile()
{
  time_t now = 0;
  string filename = getLogFileName(basename_, &now);

  // 今日 0 点时间戳（当日开始时间）
  time_t start = now / kRollPerSeconds_ * kRollPerSeconds_;

  if (now > lastRoll_)
  {
    lastRoll_ = now;
    lastFlush_ = now;
    startOfPeriod_ = start;
    file_.reset(new FileUtil::AppendFile(filename));
    return true;
  }
  return false;
}

// 日志文件名：basename.Ymd-HMS.hostname.pid.log
string LogFile::getLogFileName(const string& basename, time_t* now)
{
  string filename;
  filename.reserve(basename.size() + 64);
  filename = basename;

  char timebuf[32];
  struct tm tm;
  *now = time(NULL);
  gmtime_r(now, &tm); // FIXME: localtime_r ?
  strftime(timebuf, sizeof timebuf, ".%Y%m%d-%H%M%S.", &tm);
  filename += timebuf;

  filename += ProcessInfo::hostname();

  char pidbuf[32];
  snprintf(pidbuf, sizeof pidbuf, ".%d", ProcessInfo::pid());
  filename += pidbuf;

  filename += ".log";

  return filename;
}

