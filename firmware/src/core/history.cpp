#include "history.h"

#include <math.h>

#include <algorithm>

#include "format.h"
#include "timeutil.h"

namespace ft {

std::vector<uint32_t> loggedDays(const Ledger& ledger) {
  std::vector<uint32_t> days;
  for (const Entry& e : ledger.all()) {
    uint32_t d = trackingDay(e.ts);
    if (std::find(days.begin(), days.end(), d) == days.end()) days.push_back(d);
  }
  std::sort(days.begin(), days.end(), [](uint32_t a, uint32_t b) { return a > b; });
  return days;
}

std::string dayLabel(uint32_t day) {
  static const char* kDays[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
  static const char* kMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  int32_t y = day / 10000;
  uint32_t m = (day / 100) % 100, d = day % 100;
  if (m < 1 || m > 12) return std::to_string(day);
  int64_t noon = daysFromCivil(y, m, d) * 86400 + 12 * 3600;
  return std::string(kDays[weekday(noon)]) + " " + kMonths[m - 1] + " " + std::to_string(d);
}

Entry rescaleEntry(const Entry& e, float newServings) {
  Entry out = e;
  if (e.kind == EntryKind::Water || e.servings <= 0) return out;
  float k = newServings / e.servings;
  out.servings = newServings;
  out.kcal = e.kcal * k;
  out.protein = e.protein * k;
  return out;
}

int64_t shiftTime(int64_t ts, int64_t deltaSec, int64_t nowLocal) {
  int64_t t = ts + deltaSec;
  return t > nowLocal ? nowLocal : t;
}

std::string remainingText(float kcal, float kcalTarget, float protein, float proteinTarget) {
  long kLeft = lroundf(kcalTarget - kcal), pLeft = lroundf(proteinTarget - protein);
  if (kLeft <= 0 && pLeft <= 0) return "Both targets reached";
  if (kLeft <= 0) return "Calories reached, " + std::to_string(pLeft) + " g protein to go";
  if (pLeft <= 0) return withCommas(kLeft) + " kcal to go, protein reached";
  return withCommas(kLeft) + " kcal and " + std::to_string(pLeft) + " g to go";
}

}  // namespace ft
