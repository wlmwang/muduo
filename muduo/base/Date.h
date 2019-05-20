// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_DATE_H
#define MUDUO_BASE_DATE_H

#include "muduo/base/copyable.h"
#include "muduo/base/Types.h"

struct tm;

namespace muduo
{

///
/// Date in Gregorian calendar.
///
/// This class is immutable.
/// It's recommended to pass it by value, since it's passed in register on x64.
///
// tips:
// 儒略日（Julian date, JD）是在儒略周期内以连续的日数计算时间的计时法
// 儒略日的起点定在西元前 4713 年（天文学上记为 -4712 年）1 月 1 日格林威治时
// 间平午（世界时 12:00）
//
// 公历（内部使用儒略日数计时法）
class Date : public muduo::copyable
          // public boost::less_than_comparable<Date>,
          // public boost::equality_comparable<Date>
{
 public:

  struct YearMonthDay
  {
    int year; // [1900..2500]
    int month;  // [1..12]
    int day;  // [1..31]
  };

  static const int kDaysPerWeek = 7;
  static const int kJulianDayOf1970_01_01;

  ///
  /// Constucts an invalid Date.
  ///
  Date()
    : julianDayNumber_(0)
  {}

  ///
  /// Constucts a yyyy-mm-dd Date.
  ///
  /// 1 <= month <= 12
  Date(int year, int month, int day);

  ///
  /// Constucts a Date from Julian Day Number.
  ///
  explicit Date(int julianDayNum)
    : julianDayNumber_(julianDayNum)
  {}

  ///
  /// Constucts a Date from struct tm
  ///
  // tips:
  // struct tm *gmtime_r(const time_t *timep, struct tm *result);
  // 即：将日历时间 timep 转换为用 GMT 时间表示的时间结构
  //
  // struct tm 结构转换成 Date 公历对象
  explicit Date(const struct tm&);

  // default copy/assignment/dtor are Okay

  void swap(Date& that)
  {
    std::swap(julianDayNumber_, that.julianDayNumber_);
  }

  // 是否初始化
  bool valid() const { return julianDayNumber_ > 0; }

  ///
  /// Converts to yyyy-mm-dd format.
  ///
  string toIsoString() const;

  struct YearMonthDay yearMonthDay() const;

  int year() const
  {
    return yearMonthDay().year;
  }

  int month() const
  {
    return yearMonthDay().month;
  }

  int day() const
  {
    return yearMonthDay().day;
  }

  // [0, 1, ..., 6] => [Sunday, Monday, ..., Saturday ]
  int weekDay() const
  {
    return (julianDayNumber_+1) % kDaysPerWeek;
  }

  int julianDayNumber() const { return julianDayNumber_; }

 private:
  int julianDayNumber_;   // 儒略日数
};

inline bool operator<(Date x, Date y)
{
  return x.julianDayNumber() < y.julianDayNumber();
}

inline bool operator==(Date x, Date y)
{
  return x.julianDayNumber() == y.julianDayNumber();
}

}  // namespace muduo

#endif  // MUDUO_BASE_DATE_H
