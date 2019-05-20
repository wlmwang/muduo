// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/base/Timestamp.h"

#include <sys/time.h>   // For gmtime_r, gettimeofday
#include <stdio.h>

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif

#include <inttypes.h>   // For PRId32: d, PRId64: %lld, <stdint.h>

using namespace muduo;

// 静态检查：确保时间戳类仅有一个微妙数成员。（确认编译器可优化时间戳类）
static_assert(sizeof(Timestamp) == sizeof(int64_t),
              "Timestamp is same size as int64_t");

// 格式化时间戳字符串：秒.微妙
string Timestamp::toString() const
{
  char buf[32] = {0};
  int64_t seconds = microSecondsSinceEpoch_ / kMicroSecondsPerSecond;
  int64_t microseconds = microSecondsSinceEpoch_ % kMicroSecondsPerSecond;
  snprintf(buf, sizeof(buf)-1, "%" PRId64 ".%06" PRId64 "", seconds, microseconds);
  return buf;
}

// 格式化时间戳字符串（转换为 GMT）：年月日 时:分:秒.微妙
string Timestamp::toFormattedString(bool showMicroseconds) const
{
  char buf[64] = {0};
  time_t seconds = static_cast<time_t>(microSecondsSinceEpoch_ / kMicroSecondsPerSecond);
  struct tm tm_time;
  // 转换为 GMT(Greenwich Mean Time，格林威治时间)。虽然它也是个时区时间，但一般值与 UTC 相同。
  // 北京时间 = GMT 时间 + 8 小时
  //
  // 注意：gmtime_r 参数 seconds 必须是 Unix 的日历时间
  gmtime_r(&seconds, &tm_time);

  if (showMicroseconds)
  {
    int microseconds = static_cast<int>(microSecondsSinceEpoch_ % kMicroSecondsPerSecond);
    snprintf(buf, sizeof(buf), "%4d%02d%02d %02d:%02d:%02d.%06d",
             tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
             tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec,
             microseconds);
  }
  else
  {
    snprintf(buf, sizeof(buf), "%4d%02d%02d %02d:%02d:%02d",
             tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
             tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
  }
  return buf;
}

// todo
// 最好将其转换成 UTC 时间，与 Timestamp::fromUnixTime() 保持统一。让 Timestamp 始终表示 UTC 时间。
//
// 获得当地时间戳
Timestamp Timestamp::now()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  int64_t seconds = tv.tv_sec;
  return Timestamp(seconds * kMicroSecondsPerSecond + tv.tv_usec);
}

