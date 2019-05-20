// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_BASE_PROCESSINFO_H
#define MUDUO_BASE_PROCESSINFO_H

#include "muduo/base/StringPiece.h"
#include "muduo/base/Types.h"
#include "muduo/base/Timestamp.h"
#include <vector>
#include <sys/types.h>

namespace muduo
{

// 进程信息
namespace ProcessInfo
{
  pid_t pid();
  string pidString();
  uid_t uid();    // 进程实际用户 uid
  string username();

  // 倘若执行文件的 SUID 位已被设置，该文件执行时，其进程的 euid 值
  // 便会设成该文件所有者的 uid
  uid_t euid();   // 进程有效用户 uid
  
  Timestamp startTime();  // 进程启动时间戳
  int clockTicksPerSecond();  // 时钟每秒 tick 数
  int pageSize();   // 页大小
  bool isDebugBuild();  // constexpr

  string hostname();  // gethostname 当前主机名（\0 结尾）
  string procname();  // 进程名
  StringPiece procname(const string& stat);

  /// read /proc/self/status
  string procStatus();

  /// read /proc/self/stat
  string procStat();

  /// read /proc/self/task/tid/stat
  string threadStat();

  /// readlink /proc/self/exe
  string exePath();

  int openedFiles();
  int maxOpenFiles();

  struct CpuTime
  {
    double userSeconds;
    double systemSeconds;

    CpuTime() : userSeconds(0.0), systemSeconds(0.0) { }

    double total() const { return userSeconds + systemSeconds; }
  };
  CpuTime cpuTime();

  int numThreads();
  std::vector<pid_t> threads();
}  // namespace ProcessInfo

}  // namespace muduo

#endif  // MUDUO_BASE_PROCESSINFO_H
