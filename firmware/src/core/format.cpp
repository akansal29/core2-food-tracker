#include "format.h"

#include <math.h>
#include <stdio.h>

namespace ft {

std::string withCommas(long value) {
  bool neg = value < 0;
  unsigned long v = neg ? static_cast<unsigned long>(-value) : static_cast<unsigned long>(value);
  std::string digits = std::to_string(v);
  std::string out;
  int count = 0;
  for (int i = static_cast<int>(digits.size()) - 1; i >= 0; --i) {
    out.insert(out.begin(), digits[i]);
    if (++count % 3 == 0 && i > 0) out.insert(out.begin(), ',');
  }
  return neg ? "-" + out : out;
}

std::string progressText(float value, float target, const char* unit) {
  return withCommas(lroundf(value)) + " / " + withCommas(lroundf(target)) + " " + unit;
}

float progressFraction(float value, float target) {
  if (target <= 0) return value > 0 ? 1.0f : 0.0f;
  float f = value / target;
  if (f < 0) return 0;
  return f > 1 ? 1 : f;
}

std::string clock12(uint8_t hour, uint8_t minute) {
  char buf[12];
  int h = hour % 12;
  snprintf(buf, sizeof(buf), "%d:%02u %s", h == 0 ? 12 : h, minute, hour < 12 ? "AM" : "PM");
  return buf;
}

}  // namespace ft
