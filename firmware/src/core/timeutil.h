// Hardware-free time helpers. All times are "local seconds": seconds since
// 1970-01-01 00:00 in the user's local wall-clock time (no timezone math on the
// device; the sync script sets the RTC to local time).
#pragma once
#include <stdint.h>

namespace ft {

constexpr int32_t kDayResetHour = 3;  // Tracking day starts at 3:00 AM (fixed in V1)

struct CivilTime {
  int32_t year;
  uint8_t month;   // 1-12
  uint8_t day;     // 1-31
  uint8_t hour;    // 0-23
  uint8_t minute;  // 0-59
  uint8_t second;  // 0-59
};

int64_t daysFromCivil(int32_t y, uint32_t m, uint32_t d);
void civilFromDays(int64_t z, int32_t& y, uint32_t& m, uint32_t& d);

int64_t toLocalSeconds(const CivilTime& t);
CivilTime fromLocalSeconds(int64_t s);

// Tracking day as YYYYMMDD. 2026-10-22 01:30 -> 20261021; 03:00 -> 20261022.
uint32_t trackingDay(int64_t localSeconds);

// A clock is considered valid once it reads 2025 or later.
bool isPlausible(const CivilTime& t);

// 0 = Monday ... 6 = Sunday
int weekday(int64_t localSeconds);

}  // namespace ft
