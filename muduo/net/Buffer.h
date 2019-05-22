// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_BUFFER_H
#define MUDUO_NET_BUFFER_H

#include "muduo/base/copyable.h"
#include "muduo/base/StringPiece.h"
#include "muduo/base/Types.h"

#include "muduo/net/Endian.h"

#include <algorithm>
#include <vector>

#include <assert.h>
#include <string.h>
//#include <unistd.h>  // ssize_t

namespace muduo
{
namespace net
{

/// A buffer class modeled after org.jboss.netty.buffer.ChannelBuffer
///
/// @code
/// +-------------------+------------------+------------------+
/// | prependable bytes |  readable bytes  |  writable bytes  |
/// |                   |     (CONTENT)    |                  |
/// +-------------------+------------------+------------------+
/// |                   |                  |                  |
/// 0      <=      readerIndex   <=   writerIndex    <=     size
/// @endcode
//
// 无界接收、发送缓冲区
class Buffer : public muduo::copyable
{
 public:
  static const size_t kCheapPrepend = 8;    // 保留头（方便先写消息，再附加头部信息）
  static const size_t kInitialSize = 1024;  // 初始化 1k + 8byte

  explicit Buffer(size_t initialSize = kInitialSize)
    : buffer_(kCheapPrepend + initialSize),
      readerIndex_(kCheapPrepend),
      writerIndex_(kCheapPrepend)
  {
    assert(readableBytes() == 0);
    assert(writableBytes() == initialSize);
    assert(prependableBytes() == kCheapPrepend);
  }

  // implicit copy-ctor, move-ctor, dtor and assignment are fine
  // NOTE: implicit move-ctor is added in g++ 4.6

  void swap(Buffer& rhs)
  {
    buffer_.swap(rhs.buffer_);
    std::swap(readerIndex_, rhs.readerIndex_);
    std::swap(writerIndex_, rhs.writerIndex_);
  }

  // 可读（未读）字节流大小
  size_t readableBytes() const
  { return writerIndex_ - readerIndex_; }

  // 可写（剩余）缓冲区大小
  size_t writableBytes() const
  { return buffer_.size() - writerIndex_; }

  // 已读（保留头+已读）字节流缓冲区偏移量
  size_t prependableBytes() const
  { return readerIndex_; }

  // 已读字节流缓冲区地址（下一次读取字节流开始地址）
  const char* peek() const
  { return begin() + readerIndex_; }

  // 在可读（未读）字节流中查找 "\r\n"。返回找到的地址值，没有返回 NULL
  const char* findCRLF() const
  {
    // FIXME: replace with memmem()?
    const char* crlf = std::search(peek(), beginWrite(), kCRLF, kCRLF+2);
    return crlf == beginWrite() ? NULL : crlf;
  }

  // 在可读（未读）字节流中查找 "\r\n"。可指定开始地址（必须在可读字节流范围内）
  const char* findCRLF(const char* start) const
  {
    assert(peek() <= start);
    assert(start <= beginWrite());
    // FIXME: replace with memmem()?
    const char* crlf = std::search(start, beginWrite(), kCRLF, kCRLF+2);
    return crlf == beginWrite() ? NULL : crlf;
  }

  // 在可读（未读）字节流中查找 '\n'。返回找到的地址值，没有返回 NULL
  const char* findEOL() const
  {
    const void* eol = memchr(peek(), '\n', readableBytes());
    return static_cast<const char*>(eol);
  }

  // 在可读（未读）字节流中查找 '\n'。可指定开始地址（必须在可读字节流范围内）
  const char* findEOL(const char* start) const
  {
    assert(peek() <= start);
    assert(start <= beginWrite());
    const void* eol = memchr(start, '\n', beginWrite() - start);
    return static_cast<const char*>(eol);
  }

  // retrieve returns void, to prevent
  // string str(retrieve(readableBytes()), readableBytes());
  // the evaluation of two functions are unspecified
  //
  // 删除（回收）指定长度缓冲区
  void retrieve(size_t len)
  {
    assert(len <= readableBytes());
    if (len < readableBytes())
    {
      readerIndex_ += len;
    }
    else
    {
      // 清空
      retrieveAll();
    }
  }

  // 删除（回收）指定结束地址缓冲区
  void retrieveUntil(const char* end)
  {
    assert(peek() <= end);
    assert(end <= beginWrite());
    retrieve(end - peek());
  }

  // 删除（回收）指定类型长度缓冲区

  void retrieveInt64()
  {
    retrieve(sizeof(int64_t));
  }
  void retrieveInt32()
  {
    retrieve(sizeof(int32_t));
  }
  void retrieveInt16()
  {
    retrieve(sizeof(int16_t));
  }
  void retrieveInt8()
  {
    retrieve(sizeof(int8_t));
  }

  // 删除（清空）所有缓冲区
  void retrieveAll()
  {
    readerIndex_ = kCheapPrepend;
    writerIndex_ = kCheapPrepend;
  }

  // 返回缓冲区所有字节流字符串 string，并清空缓冲区
  string retrieveAllAsString()
  {
    return retrieveAsString(readableBytes());
  }

  // 返回缓冲区指定长度字节流字符串 string，并删除（回收）该长度的缓冲区
  string retrieveAsString(size_t len)
  {
    assert(len <= readableBytes());
    string result(peek(), len);
    retrieve(len);
    return result;
  }
  
  // 返回缓冲区所有字节流字符串切片 StringPiece
  // 使用者需确保底层缓冲区的生命周期、线程安全
  StringPiece toStringPiece() const
  {
    return StringPiece(peek(), static_cast<int>(readableBytes()));
  }

  // 新增、写入字符串切片到缓冲区
  void append(const StringPiece& str)
  {
    append(str.data(), str.size());
  }

  // 新增、写入字节流到缓冲区
  void append(const char* /*restrict*/ data, size_t len)
  {
    // 确保缓冲区可写入指定长度字节流
    ensureWritableBytes(len);

    // 拷贝字节流到缓冲区
    std::copy(data, data+len, beginWrite());

    // 更新已写入偏移
    hasWritten(len);
  }

  void append(const void* /*restrict*/ data, size_t len)
  {
    append(static_cast<const char*>(data), len);
  }

  // 确保缓冲区可写入指定长度字节流
  void ensureWritableBytes(size_t len)
  {
    if (writableBytes() < len)
    {
      // 新增分配指定长度的内存空间
      makeSpace(len);
    }
    assert(writableBytes() >= len);
  }

  // 已写入节流缓冲区地址（下一次写入字节流开始地址）
  char* beginWrite()
  { return begin() + writerIndex_; }

  const char* beginWrite() const
  { return begin() + writerIndex_; }

  // 更新已写入偏移量
  void hasWritten(size_t len)
  {
    assert(len <= writableBytes());
    writerIndex_ += len;
  }

  // 删除指定长度的写入的字节流
  void unwrite(size_t len)
  {
    assert(len <= readableBytes());
    writerIndex_ -= len;
  }


  // 新增、写入指定类型的整型值到缓冲区中（内部变换网络序）

  ///
  /// Append int64_t using network endian
  ///
  void appendInt64(int64_t x)
  {
    int64_t be64 = sockets::hostToNetwork64(x);
    append(&be64, sizeof be64);
  }

  ///
  /// Append int32_t using network endian
  ///
  void appendInt32(int32_t x)
  {
    int32_t be32 = sockets::hostToNetwork32(x);
    append(&be32, sizeof be32);
  }

  void appendInt16(int16_t x)
  {
    int16_t be16 = sockets::hostToNetwork16(x);
    append(&be16, sizeof be16);
  }

  void appendInt8(int8_t x)
  {
    append(&x, sizeof x);
  }


  // 从缓冲区中读取指定类型的整型值（内部变换主机序）
  //
  // todo
  // 对于强制要求类型内存对齐CPU架构中，以下接口可能有 bug

  ///
  /// Read int64_t from network endian
  ///
  /// Require: buf->readableBytes() >= sizeof(int32_t)
  int64_t readInt64()
  {
    int64_t result = peekInt64();
    retrieveInt64();
    return result;
  }

  ///
  /// Read int32_t from network endian
  ///
  /// Require: buf->readableBytes() >= sizeof(int32_t)
  int32_t readInt32()
  {
    int32_t result = peekInt32();
    retrieveInt32();
    return result;
  }

  int16_t readInt16()
  {
    int16_t result = peekInt16();
    retrieveInt16();
    return result;
  }

  int8_t readInt8()
  {
    int8_t result = peekInt8();
    retrieveInt8();
    return result;
  }

  ///
  /// Peek int64_t from network endian
  ///
  /// Require: buf->readableBytes() >= sizeof(int64_t)
  int64_t peekInt64() const
  {
    assert(readableBytes() >= sizeof(int64_t));
    int64_t be64 = 0;
    ::memcpy(&be64, peek(), sizeof be64);
    return sockets::networkToHost64(be64);
  }

  ///
  /// Peek int32_t from network endian
  ///
  /// Require: buf->readableBytes() >= sizeof(int32_t)
  int32_t peekInt32() const
  {
    assert(readableBytes() >= sizeof(int32_t));
    int32_t be32 = 0;
    ::memcpy(&be32, peek(), sizeof be32);
    return sockets::networkToHost32(be32);
  }

  int16_t peekInt16() const
  {
    assert(readableBytes() >= sizeof(int16_t));
    int16_t be16 = 0;
    ::memcpy(&be16, peek(), sizeof be16);
    return sockets::networkToHost16(be16);
  }

  int8_t peekInt8() const
  {
    assert(readableBytes() >= sizeof(int8_t));
    int8_t x = *peek();
    return x;
  }

  
  // 插入指定类型的整型值到缓冲区的头部（内部变换网络序）

  ///
  /// Prepend int64_t using network endian
  ///
  void prependInt64(int64_t x)
  {
    int64_t be64 = sockets::hostToNetwork64(x);
    prepend(&be64, sizeof be64);
  }

  ///
  /// Prepend int32_t using network endian
  ///
  void prependInt32(int32_t x)
  {
    int32_t be32 = sockets::hostToNetwork32(x);
    prepend(&be32, sizeof be32);
  }

  void prependInt16(int16_t x)
  {
    int16_t be16 = sockets::hostToNetwork16(x);
    prepend(&be16, sizeof be16);
  }

  void prependInt8(int8_t x)
  {
    prepend(&x, sizeof x);
  }

  void prepend(const void* /*restrict*/ data, size_t len)
  {
    assert(len <= prependableBytes());
    readerIndex_ -= len;
    const char* d = static_cast<const char*>(data);
    std::copy(d, d+len, begin()+readerIndex_);
  }

  // 收缩缓冲区内存为：实际字节流长度 + reserve
  void shrink(size_t reserve)
  {
    // FIXME: use vector::shrink_to_fit() in C++ 11 if possible.
    Buffer other;
    other.ensureWritableBytes(readableBytes()+reserve);
    other.append(toStringPiece());
    swap(other);

    // other 生命周期结束，即释放 swap 过来的缓冲区内存
  }

  // 缓冲区容量
  size_t internalCapacity() const
  {
    return buffer_.capacity();
  }

  /// Read data directly into buffer.
  ///
  /// It may implement with readv(2)
  /// @return result of read(2), @c errno is saved
  //
  // 从文件描述符 fd 中，读取内容到当前缓冲区中
  ssize_t readFd(int fd, int* savedErrno);

 private:

  // 缓冲区开始地址
  char* begin()
  { return &*buffer_.begin(); }

  const char* begin() const
  { return &*buffer_.begin(); }

  // 新增、分配指定长度的内存空间
  void makeSpace(size_t len)
  {
    if (writableBytes() + prependableBytes() < len + kCheapPrepend)
    {
      // 重新分配一个更大的缓冲区内存
      // 此操作可能会使迭代器失效。缓冲区内部使用了偏移量，问题不大

      // FIXME: move readable data
      buffer_.resize(writerIndex_+len);
    }
    else
    {
      // 回收已被读取的字节流内存空间

      // move readable data to the front, make space inside buffer
      assert(kCheapPrepend < readerIndex_);

      // 未读（剩余）字节流长度
      size_t readable = readableBytes();

      // 拷贝（移动）字节流到缓冲区开始地址
      std::copy(begin()+readerIndex_,
                begin()+writerIndex_,
                begin()+kCheapPrepend);

      readerIndex_ = kCheapPrepend;
      writerIndex_ = readerIndex_ + readable;
      assert(readable == readableBytes());
    }
  }

 private:
  std::vector<char> buffer_;  // 缓冲区

  // 相比迭代器，buffer_ 重新分配内存，偏移不会失效
  size_t readerIndex_;  // 已读取消息缓冲区偏移
  size_t writerIndex_;  // 已写入消息缓冲区偏移

  static const char kCRLF[];  // "\r\n"
};

}  // namespace net
}  // namespace muduo

#endif  // MUDUO_NET_BUFFER_H
