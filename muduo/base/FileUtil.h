// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_BASE_FILEUTIL_H
#define MUDUO_BASE_FILEUTIL_H

#include "muduo/base/noncopyable.h"
#include "muduo/base/StringPiece.h"
#include <sys/types.h>  // for off_t

namespace muduo
{
namespace FileUtil
{

// read small file < 64KB
//
// “小”文件读取类
class ReadSmallFile : noncopyable
{
 public:
  ReadSmallFile(StringArg filename);
  ~ReadSmallFile();

  // return errno
  //
  // 读取文件基本信息和数据内容
  // 最大读取 maxSize 字节（读取不受 64k 限制。底层会循环读取）
  template<typename String>
  int readToString(int maxSize,
                   String* content,
                   int64_t* fileSize,
                   int64_t* modifyTime,
                   int64_t* createTime);

  /// Read at maxium kBufferSize into buf_
  // return errno
  //
  // 读取文件数据内容到 buf_ 缓冲区（\0 结尾）。最大 64k 限制
  int readToBuffer(int* size);

  const char* buffer() const { return buf_; }

  static const int kBufferSize = 64*1024;

 private:
  int fd_;
  int err_;
  char buf_[kBufferSize];   // 自定义缓冲区 64k
};

// read the file content, returns errno if error happens.
//
// 包装小文件读取函数。最大读取 maxSize 字节（读取不受 64k 限制，内部会循环读取）
template<typename String>
int readFile(StringArg filename,
             int maxSize,
             String* content,
             int64_t* fileSize = NULL,
             int64_t* modifyTime = NULL,
             int64_t* createTime = NULL)
{
  ReadSmallFile file(filename);
  return file.readToString(maxSize, content, fileSize, modifyTime, createTime);
}

// not thread safe
//
// 文件内容追加类（非线程安全。使用了 ::fwrite_unlocked()）
// 使用者需保证线程安全
class AppendFile : noncopyable
{
 public:
  explicit AppendFile(StringArg filename);

  ~AppendFile();

  // 循环阻塞的将指定长度数据全部写入文件流句柄（全部写入或遇到错误时返回）
  void append(const char* logline, size_t len);

  void flush();

  off_t writtenBytes() const { return writtenBytes_; }

 private:

  size_t write(const char* logline, size_t len);

  FILE* fp_;              // 文件句柄指针
  char buffer_[64*1024];  // FILE* 自定义缓冲区 64k
  off_t writtenBytes_;    // 当前文件已写入总字节数
};

}  // namespace FileUtil
}  // namespace muduo

#endif  // MUDUO_BASE_FILEUTIL_H

