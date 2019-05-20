// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/base/ProcessInfo.h"
#include "muduo/base/CurrentThread.h"
#include "muduo/base/FileUtil.h"

#include <algorithm>  // std::sort

#include <assert.h>
#include <dirent.h> // For scandir
#include <pwd.h>    // getpwuid_r
#include <stdio.h>  // snprintf
#include <stdlib.h> // For atoi
#include <unistd.h> // For getpid,geteuid, gethostname, readlink
#include <sys/resource.h>
#include <sys/times.h>

namespace muduo
{
namespace detail
{

// tips:
// int scandir(const char *dir, struct dirent **namelist, int (*filter)(const void *b),
//      int (*compare)(const struct dirent **, const struct dirent **));
// 
// 函数 scandir 扫描 dir 目录下以及 dir 子目录下满足 filter 过滤模式的文件，返回的结果
// 是 compare 函数经过排序的，并保存在 namelist 中。
// filter 函数若不想要将某个目录结构复制到 namelist 数组就返回 0，若 filter 为空指针则
// 代表选择所有的目录结构。
// scandir() 若是要排列目录名称字母，可使用 alphasort() 
// 注意：namelist 是通过 malloc 动态分配内存的，所以在使用时要注意释放内存。


// scandir() 中文件描述符过滤器。主要为了统计 fd 数量，而非本身（返回 0）
__thread int t_numOpenedFiles = 0;
int fdDirFilter(const struct dirent* d)
{
  if (::isdigit(d->d_name[0]))
  {
    ++t_numOpenedFiles;
  }
  return 0;
}

// scandir() 中线程过滤器。主要为了统计 task 数量，而非本身（返回 0）
__thread std::vector<pid_t>* t_pids = NULL;
int taskDirFilter(const struct dirent* d)
{
  if (::isdigit(d->d_name[0]))
  {
    t_pids->push_back(atoi(d->d_name));
  }
  return 0;
}

// scandir() 浏览过滤目录
int scanDir(const char *dirpath, int (*filter)(const struct dirent *))
{
  struct dirent** namelist = NULL;
  int result = ::scandir(dirpath, &namelist, filter, alphasort);
  assert(namelist == NULL);
  return result;
}

// 进程启动，全局初始化的时间戳、时钟 tick、pagesize
Timestamp g_startTime = Timestamp::now();
// assume those won't change during the life time of a process.
int g_clockTicks = static_cast<int>(::sysconf(_SC_CLK_TCK));
int g_pageSize = static_cast<int>(::sysconf(_SC_PAGE_SIZE));
}  // namespace detail
}  // namespace muduo

using namespace muduo;
using namespace muduo::detail;

pid_t ProcessInfo::pid()
{
  return ::getpid();
}

string ProcessInfo::pidString()
{
  char buf[32];
  snprintf(buf, sizeof buf, "%d", pid());
  return buf;
}

uid_t ProcessInfo::uid()
{
  return ::getuid();
}

// 用户名
string ProcessInfo::username()
{
  struct passwd pwd;
  struct passwd* result = NULL;
  char buf[8192];
  const char* name = "unknownuser";

  getpwuid_r(uid(), &pwd, buf, sizeof buf, &result);
  if (result)
  {
    name = pwd.pw_name;
  }
  return name;
}

// 倘若执行文件的 SUID 位已被设置，该文件执行时，其进程的 euid 值
// 便会设成该文件所有者的 uid
uid_t ProcessInfo::euid()
{
  return ::geteuid();
}

Timestamp ProcessInfo::startTime()
{
  return g_startTime;
}

int ProcessInfo::clockTicksPerSecond()
{
  return g_clockTicks;
}

int ProcessInfo::pageSize()
{
  return g_pageSize;
}

// 是否编译模式
bool ProcessInfo::isDebugBuild()
{
#ifdef NDEBUG
  return false;
#else
  return true;
#endif
}


// tips:
// \file #include <unistd.h>
// int gethostname(char *name, size_t len);
// 返回本地主机的标准主机名
//
// \file #include <netdb.h>
// \file #include <sys/socket.h>
// struct hostent *gethostbyname(const char *name);
// 用域名或主机名获取IP地址


// 当前主机名（\0 结尾）
string ProcessInfo::hostname()
{
  // HOST_NAME_MAX 64
  // _POSIX_HOST_NAME_MAX 255
  char buf[256];
  if (::gethostname(buf, sizeof buf) == 0)
  {
    buf[sizeof(buf)-1] = '\0';
    return buf;
  }
  else
  {
    return "unknownhost";
  }
}

// 进程名称
string ProcessInfo::procname()
{
  return procname(procStat()).as_string();
}

StringPiece ProcessInfo::procname(const string& stat)
{
  StringPiece name;
  size_t lp = stat.find('(');
  size_t rp = stat.rfind(')');
  if (lp != string::npos && rp != string::npos && lp < rp)
  {
    name.set(stat.data()+lp+1, static_cast<int>(rp-lp-1));
  }
  return name;
}

string ProcessInfo::procStatus()
{
  string result;
  FileUtil::readFile("/proc/self/status", 65536, &result);
  return result;
}

string ProcessInfo::procStat()
{
  string result;
  FileUtil::readFile("/proc/self/stat", 65536, &result);
  return result;
}

// 调用线程 stat 文件
string ProcessInfo::threadStat()
{
  char buf[64];
  snprintf(buf, sizeof buf, "/proc/self/task/%d/stat", CurrentThread::tid());
  string result;
  FileUtil::readFile(buf, 65536, &result);
  return result;
}

// 执行文件，启动时当前路径
string ProcessInfo::exePath()
{
  string result;
  char buf[1024];
  ssize_t n = ::readlink("/proc/self/exe", buf, sizeof buf);
  if (n > 0)
  {
    result.assign(buf, n);
  }
  return result;
}

// 统计打开 fd 数量
int ProcessInfo::openedFiles()
{
  t_numOpenedFiles = 0;
  scanDir("/proc/self/fd", fdDirFilter);
  return t_numOpenedFiles;
}

// 进程打开 fd 限制
int ProcessInfo::maxOpenFiles()
{
  struct rlimit rl;
  if (::getrlimit(RLIMIT_NOFILE, &rl))
  {
    return openedFiles();
  }
  else
  {
    return static_cast<int>(rl.rlim_cur);
  }
}

// CPU 时钟
ProcessInfo::CpuTime ProcessInfo::cpuTime()
{
  ProcessInfo::CpuTime t;
  struct tms tms;
  if (::times(&tms) >= 0)
  {
    const double hz = static_cast<double>(clockTicksPerSecond());
    t.userSeconds = static_cast<double>(tms.tms_utime) / hz;
    t.systemSeconds = static_cast<double>(tms.tms_stime) / hz;
  }
  return t;
}

// 统计线程数量
int ProcessInfo::numThreads()
{
  int result = 0;
  string status = procStatus();
  size_t pos = status.find("Threads:");
  if (pos != string::npos)
  {
    result = ::atoi(status.c_str() + pos + 8);
  }
  return result;
}

// 获取线程 tid 列表
std::vector<pid_t> ProcessInfo::threads()
{
  std::vector<pid_t> result;
  t_pids = &result;
  scanDir("/proc/self/task", taskDirFilter);
  t_pids = NULL;
  std::sort(result.begin(), result.end());
  return result;
}

