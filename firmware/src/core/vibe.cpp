#include "vibe.h"

namespace ft {

namespace {
// Alternating on/off durations in ms, starting with "on". 0 terminates.
const uint16_t kEating[] = {600, 0};
const uint16_t kWater[] = {150, 150, 150, 0};
const uint16_t kCombined[] = {600, 400, 150, 150, 150, 0};
const uint16_t kTick[] = {60, 0};

const uint16_t* steps(VibePattern p) {
  switch (p) {
    case VibePattern::Eating: return kEating;
    case VibePattern::Water: return kWater;
    case VibePattern::Combined: return kCombined;
    case VibePattern::Tick: return kTick;
    default: return nullptr;
  }
}
}  // namespace

void VibeSequencer::start(VibePattern p, uint32_t nowMs) {
  pattern_ = p;
  startMs_ = nowMs;
}

bool VibeSequencer::isOn(uint32_t nowMs) {
  const uint16_t* s = steps(pattern_);
  if (!s) return false;
  uint32_t elapsed = nowMs - startMs_;
  uint32_t t = 0;
  for (int i = 0; s[i] != 0; ++i) {
    t += s[i];
    if (elapsed < t) return (i % 2) == 0;
  }
  pattern_ = VibePattern::None;
  return false;
}

}  // namespace ft
