// Hardware-free helpers for Today's Log, entry editing and Past Days (guide Sections 4 and 5).
#pragma once
#include <stdint.h>

#include <string>
#include <vector>

#include "journal.h"

namespace ft {

// Tracking days (YYYYMMDD) that have at least one entry, newest first.
std::vector<uint32_t> loggedDays(const Ledger& ledger);

// "Wed Oct 29"
std::string dayLabel(uint32_t trackingDay);

// Rescales a food entry's snapshot to a new number of servings. Uses the entry's
// own logged numbers, never the current food list, so history stays as logged.
Entry rescaleEntry(const Entry& e, float newServings);

// Moves an entry's time by deltaSec, never past `nowLocal`.
int64_t shiftTime(int64_t ts, int64_t deltaSec, int64_t nowLocal);

// "1,180 kcal and 46 g to go" / "Calories reached, 12 g protein to go" / "Both targets reached"
std::string remainingText(float kcal, float kcalTarget, float protein, float proteinTarget);

}  // namespace ft
