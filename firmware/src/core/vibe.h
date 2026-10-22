// Non-blocking vibration patterns (guide Section 8). Hardware-free: the caller
// asks isOn(now) every loop and drives the motor.
#pragma once
#include <stdint.h>

namespace ft {

enum class VibePattern : uint8_t { None, Eating, Water, Combined, Tick };

class VibeSequencer {
 public:
  void start(VibePattern p, uint32_t nowMs);
  // True while the motor should run. Becomes idle when the pattern ends.
  bool isOn(uint32_t nowMs);
  bool active() const { return pattern_ != VibePattern::None; }

 private:
  VibePattern pattern_ = VibePattern::None;
  uint32_t startMs_ = 0;
};

}  // namespace ft
