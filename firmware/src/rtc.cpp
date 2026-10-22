#include "rtc.h"

#include <M5Unified.h>

#include "app.h"

namespace rtc {

namespace {
int64_t cached = 0;
uint32_t cachedAtMs = 0;
bool haveCache = false;
bool valid = false;

void refresh() {
  valid = false;
  haveCache = true;
  cachedAtMs = millis();
  if (!M5.Rtc.isEnabled()) return;
  if (M5.Rtc.getVoltLow()) return;  // oscillator stopped: time is untrustworthy
  m5::rtc_datetime_t dt = M5.Rtc.getDateTime();
  ft::CivilTime t{dt.date.year, (uint8_t)dt.date.month, (uint8_t)dt.date.date,
                  (uint8_t)dt.time.hours, (uint8_t)dt.time.minutes, (uint8_t)dt.time.seconds};
  if (!ft::isPlausible(t)) return;
  cached = ft::toLocalSeconds(t);
  valid = true;
}
}  // namespace

bool nowLocal(int64_t& out) {
  if (!haveCache || millis() - cachedAtMs >= 1000) refresh();
  if (!valid) return false;
  out = cached + (millis() - cachedAtMs) / 1000;
  return true;
}

bool isValid() {
  int64_t ignored;
  return nowLocal(ignored);
}

bool set(const ft::CivilTime& t) {
  if (!M5.Rtc.isEnabled()) return false;
  m5::rtc_datetime_t dt;
  dt.date.year = t.year;
  dt.date.month = t.month;
  dt.date.date = t.day;
  dt.date.weekDay = static_cast<int8_t>((ft::weekday(ft::toLocalSeconds(t)) + 1) % 7);  // RTC: 0 = Sunday
  dt.time.hours = t.hour;
  dt.time.minutes = t.minute;
  dt.time.seconds = t.second;
  M5.Rtc.setDateTime(dt);  // also clears the voltage-low flag
  haveCache = false;
  bool ok = isValid();
  app::S.clockValid = ok;
  return ok;
}

}  // namespace rtc
