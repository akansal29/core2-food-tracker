// Shared application state for the Milestone 1 firmware.
#pragma once
#include <Arduino.h>

#include <string>

#include "core/journal.h"
#include "core/vibe.h"

namespace app {

constexpr const char* kFirmwareVersion = "0.1.0-m1";
constexpr int kSchemaVersion = 1;

struct Settings {
  int kcalTarget = 3000;
  int proteinTarget = 100;
  int waterTargetOz = 40;
  int waterLongTermOz = 100;   // reference only; never applied automatically
  int brightness = 128;        // 0-255, medium
  int screenTimeoutSec = 30;
  bool vibration = true;
  uint32_t saveCounter = 0;    // M1 test: proves atomic settings writes survive reboot
};

enum class Screen { Home, Diagnostics, Reminder };

struct State {
  Settings settings;
  ft::Ledger ledger;
  ft::VibeSequencer vibe;
  Screen screen = Screen::Home;
  bool screenOn = true;
  bool locked = false;
  bool clockValid = false;
  bool storageOk = true;
  uint32_t lastInteractionMs = 0;
  uint32_t journalBadLines = 0;
  // Undo: last entry written and when (5 s window).
  std::string undoId;
  uint32_t undoUntilMs = 0;
  // Toast message
  String toast;
  uint32_t toastUntilMs = 0;
  // M1 reminder test (0 = none scheduled)
  uint32_t testReminderAtMs = 0;
  // Diagnostics
  String lastZoneEvent = "none yet";
  uint32_t bootMs = 0;
  bool lowBatteryWarned = false;
};

extern State S;

// Current local time from the RTC, in local seconds. Returns false if the clock is not valid.
bool nowLocal(int64_t& out);
void showToast(const String& msg, uint32_t ms = 2000);
void markInteraction();

}  // namespace app
