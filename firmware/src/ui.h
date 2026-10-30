// Screens (guide Sections 4 and 7). Immediate-mode: each draw registers the
// tappable areas for that frame; tap/hold/drag are matched against them.
#pragma once
#include <stdint.h>

#include "app.h"

namespace ui {

void begin();
void draw();          // draws the current screen from app::S
void requestRedraw(); // draw on the next loop instead of waiting for the timer
bool redrawRequested();

// Touch gestures on the screen area (y < 240). main.cpp handles lock gating.
void tap(int x, int y);
void hold(int x, int y);
void drag(int dy);

// Navigation used by the touch zones and actions.
void goHome();
void openLogFood();
void openToday();   // Today's Log
// Called when the screen goes to sleep mid-task (keeps the entry for "Resume?").
void onSleep();

}  // namespace ui
