// Shared application state.
#pragma once
#include <Arduino.h>

#include <string>

#include "core/journal.h"
#include "core/nutrition.h"
#include "core/vibe.h"

namespace app {

constexpr const char* kFirmwareVersion = "0.4.0-m4";
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
  // Reminders (guide Section 2). Times are minutes after midnight; windows may cross midnight.
  bool eatingReminders = true;
  bool waterReminders = true;
  int eatStartMin = 11 * 60;   // 11:00 AM
  int eatEndMin = 30;          // 12:30 AM
  int eatIntervalMin = 150;
  int waterStartMin = 11 * 60; // 11:00 AM
  int waterEndMin = 60;        // 1:00 AM
  int waterIntervalMin = 20;
  int64_t dndUntil = 0;        // local seconds; ft::kForever = until turned off; kept across restarts
};

enum class Screen {
  Home, LogFood, Browse, Favorites, Picker, ComboAdjust, Custom,
  DayLog, PastDays, EntryEdit,
  SettingsMenu, Reminders, Diagnostics, Reminder
};

// Custom entry form (guide Section 4)
struct CustomForm {
  String kcal;
  String protein;
  int field = 0;       // 0 = calories, 1 = protein
  bool saveAsFood = false;
};

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

  // After-log banner on Home: "Logged! ..." with Undo / Log another (5 s).
  std::string undoId;
  uint32_t undoUntilMs = 0;
  String bannerText;

  // Toast message
  String toast;
  uint32_t toastUntilMs = 0;

  // Logging flow
  std::string pickerFoodId;
  float pickerServings = 1;
  bool comboAdjusted = false;
  ft::Ingredients comboIngredients;   // per-entry copy, never written back to the saved combo
  Screen pickerReturn = Screen::LogFood;
  int browseCategory = 0;             // ft::Category
  int listScroll = 0;                 // px, for list screens
  CustomForm custom;

  // Today's Log / Past Days (guide Section 5)
  uint32_t viewDay = 0;               // tracking day shown in DayLog; 0 = today
  ft::Entry editEntry;                // working copy; written only on Save
  float editOrigServings = 1;
  Screen editReturn = Screen::DayLog;
  uint32_t deleteArmedUntilMs = 0;    // second tap within 3 s confirms Delete
  // In-progress entry kept across a short sleep (guide Section 7): offer "Resume?"
  Screen resumeScreen = Screen::Home;
  uint32_t resumeUntilMs = 0;

  // Reminder overlay (guide Section 2)
  uint8_t reminderDue = 0;            // ft::Due being shown
  bool reminderEatOpen = false;       // eating half not answered yet
  bool reminderWaterOpen = false;     // water half not answered yet
  // Hardware test reminder (0 = none scheduled)
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
