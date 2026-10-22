// Real-time clock access (BM8563 via M5Unified). Guide Section 8: the RTC holds
// local wall-clock time; the sync script sets it.
#pragma once
#include <stdint.h>

#include "core/timeutil.h"

namespace rtc {

// Reads the RTC (cached for one second). Returns false if the clock lost power
// or reads an implausible date.
bool nowLocal(int64_t& out);
bool set(const ft::CivilTime& t);
bool isValid();

}  // namespace rtc
