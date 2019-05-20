// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/base/CurrentThread.h"

#include <cxxabi.h>     // For abi::__cxa_demangle
#include <execinfo.h>   // For backtrace, backtrace_symbols
#include <stdlib.h>

namespace muduo
{
namespace CurrentThread
{
__thread int t_cachedTid = 0;
__thread char t_tidString[32];
__thread int t_tidStringLength = 6;
__thread const char* t_threadName = "unknown";

// 编译器检查 pid_t 是否为 int。（缓存 pid_t 使用了 int）
static_assert(std::is_same<int, pid_t>::value, "pid_t should be int");

// tips:
// 1. backtrace() 函数获取当前线程的函数调用堆栈的地址信息
// 2. backtrace_symbols() 把从 backtrace 函数获取的信息转化为一个字符串数组 char**
//    backtrace_symbols() 的返回值调用了 malloc 以分配存储空间，为了防止内存泄露，
//    需要手动调用 free 来释放这块内存
// 
// a.如果使用的是 GCC 编译链接的话，建议加上 "-rdynamic" 参数，这个参数的意思是告诉
//   ELF 连接器添加 "-export-dynamic" 标记，这样所有的符号信息 symbols 就会添加到动
//   态符号表中，以便查看完整的堆栈信息
// b.static 函数不会导出符号信息 symbols ，在 backtrace 中无效。
// c.某些编译器的优化选项对获取正确的函数调用堆栈有干扰，内联函数没有堆栈框架，删除
//   框架指针也会导致无法正确解析堆栈内容
//
// 返回函数调用栈字符串
string stackTrace(bool demangle)
{
  string stack;
  const int max_frames = 200; // 调用栈最大深度限制
  void* frame[max_frames];
  int nptrs = ::backtrace(frame, max_frames);
  char** strings = ::backtrace_symbols(frame, nptrs);
  if (strings)
  {
    size_t len = 256;   // 函数名最长字符串限制
    char* demangled = demangle ? static_cast<char*>(::malloc(len)) : nullptr;
    for (int i = 1; i < nptrs; ++i)  // skipping the 0-th, which is this function
    {
      if (demangle)
      {
        // https://panthema.net/2008/0901-stacktrace-demangled/
        // bin/exception_test(_ZN3Bar4testEv+0x79) [0x401909]
        char* left_par = nullptr;
        char* plus = nullptr;
        for (char* p = strings[i]; *p; ++p)   // 利用了 backtrace_symbols 符号信息的格式
        {
          if (*p == '(')    // 函数符号
            left_par = p;
          else if (*p == '+')   // 地址偏移符号
            plus = p;
        }

        if (left_par && plus)
        {
          *plus = '\0';
          int status = 0;
          char* ret = abi::__cxa_demangle(left_par+1, demangled, &len, &status);
          *plus = '+';
          if (status == 0)
          {
            demangled = ret;  // ret could be realloc()
            stack.append(strings[i], left_par+1);
            stack.append(demangled);
            stack.append(plus);
            stack.push_back('\n');
            continue;
          }
        }
      }
      // Fallback to mangled names
      stack.append(strings[i]);
      stack.push_back('\n');
    }
    free(demangled);
    free(strings);
  }
  return stack;
}

}  // namespace CurrentThread
}  // namespace muduo
