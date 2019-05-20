#ifndef MUDUO_BASE_NONCOPYABLE_H
#define MUDUO_BASE_NONCOPYABLE_H

namespace muduo
{

// 标识、强调派生类不可复制
// “对象类型” class 标签。（对应其他语言“引用类型”的概念）
class noncopyable
{
 public:
  noncopyable(const noncopyable&) = delete;
  void operator=(const noncopyable&) = delete;

 protected:
  noncopyable() = default;
  ~noncopyable() = default;
};

}  // namespace muduo

#endif  // MUDUO_BASE_NONCOPYABLE_H
