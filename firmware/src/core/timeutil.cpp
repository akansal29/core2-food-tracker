#include "timeutil.h"

namespace ft {

// Howard Hinnant's civil-date algorithms (public domain).
int64_t daysFromCivil(int32_t y, uint32_t m, uint32_t d) {
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const uint32_t yoe = static_cast<uint32_t>(y - era * 400);
  const uint32_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<int64_t>(doe) - 719468;
}

void civilFromDays(int64_t z, int32_t& y, uint32_t& m, uint32_t& d) {
  z += 719468;
  const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const uint32_t doe = static_cast<uint32_t>(z - era * 146097);
  const uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const uint32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const uint32_t mp = (5 * doy + 2) / 153;
  d = doy - (153 * mp + 2) / 5 + 1;
  m = mp < 10 ? mp + 3 : mp - 9;
  y = static_cast<int32_t>(yoe) + static_cast<int32_t>(era * 400) + (m <= 2);
}

int64_t toLocalSeconds(const CivilTime& t) {
  return daysFromCivil(t.year, t.month, t.day) * 86400 + t.hour * 3600 + t.minute * 60 + t.second;
}

CivilTime fromLocalSeconds(int64_t s) {
  int64_t days = s / 86400;
  int64_t rem = s % 86400;
  if (rem < 0) { rem += 86400; days -= 1; }
  CivilTime t{};
  uint32_t m, d;
  civilFromDays(days, t.year, m, d);
  t.month = static_cast<uint8_t>(m);
  t.day = static_cast<uint8_t>(d);
  t.hour = static_cast<uint8_t>(rem / 3600);
  t.minute = static_cast<uint8_t>((rem % 3600) / 60);
  t.second = static_cast<uint8_t>(rem % 60);
  return t;
}

uint32_t trackingDay(int64_t localSeconds) {
  CivilTime t = fromLocalSeconds(localSeconds - kDayResetHour * 3600);
  return static_cast<uint32_t>(t.year) * 10000 + t.month * 100 + t.day;
}

bool isPlausible(const CivilTime& t) {
  return t.year >= 2025 && t.year < 2100 && t.month >= 1 && t.month <= 12 && t.day >= 1 && t.day <= 31 &&
         t.hour < 24 && t.minute < 60 && t.second < 60;
}

int weekday(int64_t localSeconds) {
  int64_t days = localSeconds / 86400;
  if (localSeconds % 86400 < 0) days -= 1;
  // 1970-01-01 was a Thursday (index 3 when Monday = 0).
  int64_t w = (days + 3) % 7;
  return static_cast<int>(w < 0 ? w + 7 : w);
}

}  // namespace ft
