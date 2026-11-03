// Connects the hardware-free scheduler (core/reminders.h) to the device:
// clock, today's totals, settings, the reminder overlay, and food suggestions.
#pragma once
#include <Arduino.h>

#include <string>

#include "core/reminders.h"

namespace remind {

// Returns a reminder that just became due (caller wakes the screen, buzzes, shows
// the overlay), or Due::None. Safe to call every loop.
ft::Due poll();

// Logging from anywhere resolves the matching reminder and reschedules it.
void foodLogged(int64_t ts);
void waterLogged();

// Overlay answers
void eatSnooze();
void eatSkip();
void waterSnooze();
void silenceHour();
// Closing the overlay without answering (timeout, sleep, navigating away).
void leaveOverlay();

// Do Not Disturb. until = local seconds, ft::kForever, or 0 to turn off. Persists.
void setDnd(int64_t until);
bool dndActive();
// "Do not disturb until 4:30 PM" / "...until you turn it off" / "" when off
std::string dndText();

// Settings toggles (persist and reschedule)
void setEatingOn(bool on);
void setWaterOn(bool on);

// "Next: snack 4:30 PM | water 3:50 PM"
std::string nextText();
// Rotating friendly line for the overlay (guide Section 2: never guilt-based).
const char* message(ft::Due d);
// Basic snack idea for an eating reminder ("" if none). Milestone 6 replaces the ranking.
const std::string& suggestion();
void nextSuggestion();

// The clock was set or corrected: recompute everything from the new time.
void clockChanged();

bool eatPending();
bool waterPending();

}  // namespace remind
