// Screen drawing and hit-testing (guide Section 7). Draws into an off-screen
// canvas and pushes it in one go to avoid flicker.
#pragma once
#include <stdint.h>

#include "app.h"

namespace ui {

enum Button : int {
  kNone = 0,
  // Home (M1 test row)
  kAddKcal, kAddProtein, kUndo, kOpenDiag,
  // Diagnostics
  kVibeEat, kVibeWater, kVibeBoth, kTestReminder, kSaveSettings, kBack,
  // Reminder overlay
  kSipped, kDismiss,
};

void begin();
void draw();  // draws the current screen from app::S
Button hitTest(int x, int y);

}  // namespace ui
