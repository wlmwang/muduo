// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/base/FileUtil.h"
#include "muduo/base/Logging.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>      // For setbuffer
#include <sys/stat.h>
#include <unistd.h>

using namespace muduo;

FileUtil::AppendFile::AppendFile(StringArg filename)
  : fp_(::fopen(filename.c_str(), "ae")),  // 'e' for O_CLOEXEC
    writtenBytes_(0)
{
  assert(fp_);

  // #define BUFSIZ 8192  // buffer 默认大小
  // 设置文件流句柄缓冲区。sizeof(buffer_)==64k（写入每 64k 刷新到磁盘）
  ::setbuffer(fp_, buffer_, sizeof buffer_);
  // posix_fadvise POSIX_FADV_DONTNEED ?
}

FileUtil::AppendFile::~AppendFile()
{
  ::fclose(fp_);
}

// 循环阻塞的将指定长度数据全部写入文件流句柄（全部写入或遇到错误时返回）
void FileUtil::AppendFile::append(const char* logline, const size_t len)
{
  size_t n = write(logline, len);
  size_t remain = len - n;

  // 非阻塞 I/O 必须要循环写入
  while (remain > 0)
  {
    // 非线程安全方式写入日志（使用者需保证线程安全）
    size_t x = write(logline + n, remain);
    if (x == 0)
    {
      int err = ferror(fp_);
      if (err)
      {
        // 写入标准错误流中（紧急错误）
        fprintf(stderr, "AppendFile::append() failed %s\n", strerror_tl(err));
      }
      break;
    }
    n += x;
    remain = len - n; // remain -= x
  }

  // 更新总写入字节数
  writtenBytes_ += len;
}

void FileUtil::AppendFile::flush()
{
  ::fflush(fp_);
}

size_t FileUtil::AppendFile::write(const char* logline, size_t len)
{
  // 非线程安全方式写入日志（使用者需保证线程安全）
  // #undef fwrite_unlocked
  return ::fwrite_unlocked(logline, 1, len, fp_);
}

FileUtil::ReadSmallFile::ReadSmallFile(StringArg filename)
  : fd_(::open(filename.c_str(), O_RDONLY | O_CLOEXEC)),
    err_(0)
{
  buf_[0] = '\0';
  if (fd_ < 0)
  {
    err_ = errno;
  }
}

FileUtil::ReadSmallFile::~ReadSmallFile()
{
  if (fd_ >= 0)
  {
    ::close(fd_); // FIXME: check EINTR
  }
}

// return errno
//
// 读取文件基本信息和数据内容
// 最大读取 maxSize 字节（读取不受 64k 限制）
template<typename String>
int FileUtil::ReadSmallFile::readToString(int maxSize,
                                          String* content,
                                          int64_t* fileSize,
                                          int64_t* modifyTime,
                                          int64_t* createTime)
{
  // off_t 必须为 8 字节
  static_assert(sizeof(off_t) == 8, "_FILE_OFFSET_BITS = 64");
  assert(content != NULL);

  int err = err_;
  if (fd_ >= 0)
  {
    content->clear();

    if (fileSize)
    {
      // 获取文件状态信息
      struct stat statbuf;
      if (::fstat(fd_, &statbuf) == 0)
      {
        if (S_ISREG(statbuf.st_mode))
        {
          // 普通文件
          *fileSize = statbuf.st_size;
          content->reserve(static_cast<int>(std::min(implicit_cast<int64_t>(maxSize), *fileSize)));
        }
        else if (S_ISDIR(statbuf.st_mode))
        {
          // 目录
          err = EISDIR;
        }

        // 修改时间
        if (modifyTime)
        {
          *modifyTime = statbuf.st_mtime;
        }

        // 创建时间
        if (createTime)
        {
          *createTime = statbuf.st_ctime;
        }
      }
      else
      {
        err = errno;
      }
    }

    // 循环读取整个文件内容（最多读取 maxSize 字节）
    while (content->size() < implicit_cast<size_t>(maxSize))
    {
      // 每次最多读取 64*1024 == 64k
      size_t toRead = std::min(implicit_cast<size_t>(maxSize) - content->size(), sizeof(buf_));
      ssize_t n = ::read(fd_, buf_, toRead);
      if (n > 0)
      {
        // 拷贝内容，追加到输出参数 content 中
        content->append(buf_, n);
      }
      else
      {
        if (n < 0)
        {
          err = errno;
        }
        break;
      }
    }
  }
  return err;
}

// 读取文件数据内容到 buf_ 中（\0 结尾）。最大 64k 限制
int FileUtil::ReadSmallFile::readToBuffer(int* size)
{
  int err = err_;
  if (fd_ >= 0)
  {
    ssize_t n = ::pread(fd_, buf_, sizeof(buf_)-1, 0);
    if (n >= 0)
    {
      if (size)
      {
        *size = static_cast<int>(n);
      }
      buf_[n] = '\0';   // \0 结尾
    }
    else
    {
      err = errno;
    }
  }
  return err;
}

// 模板实例化（常用 std::string 输出参数）
template int FileUtil::readFile(StringArg filename,
                                int maxSize,
                                string* content,
                                int64_t*, int64_t*, int64_t*);

template int FileUtil::ReadSmallFile::readToString(
    int maxSize,
    string* content,
    int64_t*, int64_t*, int64_t*);

