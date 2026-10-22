// Number formatting for screens: "1,820 / 3,000 kcal" (guide Section 7).
#pragma once
#include <stdint.h>

#include <string>

namespace ft {

std::string withCommas(long value);
// Rounds kcal to whole numbers, protein and water to whole numbers on Home.
std::string progressText(float value, float target, const char* unit);
// Fraction 0..1 for a progress bar (values above target clamp to 1; the text is never capped).
float progressFraction(float value, float target);
// "4:12 PM"
std::string clock12(uint8_t hour, uint8_t minute);

}  // namespace ft
