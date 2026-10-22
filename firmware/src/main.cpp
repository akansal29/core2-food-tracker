// Core2 food tracker — Milestone 1 hardware proof of concept.
// See docs/M1_CHECKLIST.md for what to test on the device.
#include <M5Unified.h>

#include "app.h"
#include "core/timeutil.h"
#include "rtc.h"
#include "serial_cmd.h"
#include "storage.h"
#include "ui.h"

namespace app {

State S;

bool nowLocal(int64_t& out) {
  bool ok = rtc::nowLocal(out);
  S.clockValid = ok;
  return ok;
}

void showToast(const String& msg, uint32_t ms) {
  S.toast = msg;
  S.toastUntilMs = millis() + ms;
}

void markInteraction() { S.lastInteractionMs = millis(); }

}  // namespace app

using app::S;

namespace {

constexpr uint32_t kUndoWindowMs = 5000;
constexpr uint32_t kHoldMs = 1000;
uint32_t lastDrawMs = 0;
uint32_t lastBatteryCheckMs = 0;
bool motorOn = false;

void setMotor(bool on) {
  if (on == motorOn) return;
  motorOn = on;
  M5.Power.setVibration(on ? 200 : 0);
}

void buzz(ft::VibePattern p) {
  if (S.settings.vibration) S.vibe.start(p, millis());
}

void screenOn() {
  if (S.screenOn) return;
  setCpuFrequencyMhz(240);
  M5.Display.wakeup();
  M5.Display.setBrightness(S.settings.brightness);
  S.screenOn = true;
  app::markInteraction();
}

void screenOff() {
  if (!S.screenOn) return;
  M5.Display.setBrightness(0);
  M5.Display.sleep();
  S.screenOn = false;
  S.locked = true;              // auto-lock on sleep (guide Section 8)
  S.screen = app::Screen::Home; // wake always shows Home
  setCpuFrequencyMhz(80);
}

// Writes the journal line first; only then updates memory. Never claims success otherwise.
bool commitLine(const std::string& line) {
  if (!storage::appendJournal(line)) {
    S.storageOk = false;
    app::showToast("Couldn't save. Not logged.", 3000);
    return false;
  }
  S.storageOk = true;
  return S.ledger.applyLine(line);
}

void logEntry(ft::Entry e, const String& confirmation) {
  int64_t now;
  if (!app::nowLocal(now)) {
    app::showToast("Set the clock first", 2500);
    return;
  }
  uint32_t seq = S.ledger.lastSeq() + 1;
  e.ts = now;
  e.id = ft::makeEntryId(now, seq);
  if (!commitLine(ft::encodeAdd(seq, e))) return;
  S.undoId = e.id;
  S.undoUntilMs = millis() + kUndoWindowMs;
  app::showToast(confirmation);
  buzz(ft::VibePattern::Tick);
}

void addWater(float oz) {
  ft::Entry e;
  e.kind = ft::EntryKind::Water;
  e.name = "Water";
  e.waterOz = oz;
  logEntry(e, "+" + String((int)oz) + " oz water  (Undo)");
}

void addTestFood(float kcal, float protein, const char* name, const String& msg) {
  ft::Entry e;
  e.kind = ft::EntryKind::Custom;
  e.name = name;
  e.kcal = kcal;
  e.protein = protein;
  logEntry(e, msg);
}

void undoLast() {
  if (S.undoId.empty() || millis() > S.undoUntilMs || !S.ledger.find(S.undoId)) {
    app::showToast("Nothing to undo");
    return;
  }
  if (commitLine(ft::encodeDelete(S.ledger.lastSeq() + 1, S.undoId))) app::showToast("Undone");
  S.undoId.clear();
}

void lockedHint() { app::showToast("Locked. Hold the middle to unlock."); }

void handleZones() {
  // Any zone press while the screen is off only wakes it (and stays locked).
  if (!S.screenOn) {
    if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed() || M5.BtnC.wasPressed()) {
      S.lastZoneEvent = "wake";
      screenOn();
    }
    return;
  }
  if (M5.BtnA.wasClicked()) {
    S.lastZoneEvent = "Left tap";
    app::markInteraction();
    if (S.locked) lockedHint();
    else app::showToast("Food logging arrives in Milestone 2");
  }
  if (M5.BtnB.wasHold()) {
    S.lastZoneEvent = "Middle hold";
    app::markInteraction();
    if (S.locked) {
      S.locked = false;
      buzz(ft::VibePattern::Tick);
      app::showToast("Unlocked");
    } else {
      app::showToast("Today's Log arrives in Milestone 3");
    }
  } else if (M5.BtnB.wasClicked()) {
    S.lastZoneEvent = "Middle tap";
    app::markInteraction();
    if (S.screen != app::Screen::Reminder) S.screen = app::Screen::Home;
  }
  if (M5.BtnC.wasClicked()) {
    S.lastZoneEvent = "Right tap";
    app::markInteraction();
    if (S.locked) lockedHint();
    else addWater(8);
  }
}

bool swallowUntilRelease = false;  // the tap that wakes the screen does nothing else

void handleTouch() {
  if (M5.Touch.getCount() == 0) {
    swallowUntilRelease = false;
    return;
  }
  const auto& t = M5.Touch.getDetail();
  if (!S.screenOn) {
    if (t.wasPressed()) {
      screenOn();
      swallowUntilRelease = true;
    }
    return;
  }
  if (swallowUntilRelease) {
    if (!t.isPressed()) swallowUntilRelease = false;
    return;
  }
  if (t.isPressed()) app::markInteraction();
  if (!t.wasClicked() || t.y >= 240) return;  // y >= 240 is the touch-zone strip
  ui::Button b = ui::hitTest(t.x, t.y);
  if (b == ui::kNone) return;
  // While locked, only the reminder overlay's labeled buttons work (guide Section 8).
  if (S.locked && S.screen != app::Screen::Reminder) {
    lockedHint();
    return;
  }
  switch (b) {
    case ui::kAddKcal: addTestFood(100, 0, "M1 test +100 kcal", "Logged! +100 kcal  (Undo)"); break;
    case ui::kAddProtein: addTestFood(0, 5, "M1 test +5 g protein", "Logged! +5 g protein  (Undo)"); break;
    case ui::kUndo: undoLast(); break;
    case ui::kOpenDiag: S.screen = app::Screen::Diagnostics; break;
    case ui::kVibeEat: S.vibe.start(ft::VibePattern::Eating, millis()); break;
    case ui::kVibeWater: S.vibe.start(ft::VibePattern::Water, millis()); break;
    case ui::kVibeBoth: S.vibe.start(ft::VibePattern::Combined, millis()); break;
    case ui::kTestReminder:
      S.testReminderAtMs = millis() + 30000;
      S.screen = app::Screen::Home;
      app::showToast("Reminder in 30 s. Let the screen sleep.", 3000);
      break;
    case ui::kSaveSettings:
      S.settings.saveCounter++;
      app::showToast(storage::saveSettings(S.settings) ? "Settings saved (" + String(S.settings.saveCounter) + ")"
                                                       : String("Settings save FAILED"));
      break;
    case ui::kBack: S.screen = app::Screen::Home; break;
    case ui::kSipped:
      addWater(2);
      S.screen = app::Screen::Home;
      break;
    case ui::kDismiss: S.screen = app::Screen::Home; break;
    default: break;
  }
}

void checkReminder() {
  if (!S.testReminderAtMs || millis() < S.testReminderAtMs) return;
  S.testReminderAtMs = 0;
  screenOn();  // wakes the display even if it was asleep
  S.screen = app::Screen::Reminder;
  buzz(ft::VibePattern::Water);
}

void checkBattery() {
  if (millis() - lastBatteryCheckMs < 10000) return;
  lastBatteryCheckMs = millis();
  int level = M5.Power.getBatteryLevel();
  int mv = M5.Power.getBatteryVoltage();
  bool charging = M5.Power.isCharging() == m5::Power_Class::is_charging;
  if (level < 0 || charging) {
    if (charging) S.lowBatteryWarned = false;
    return;
  }
  if (level <= 20 && !S.lowBatteryWarned) {
    S.lowBatteryWarned = true;
    app::showToast("Battery low (" + String(level) + "%)", 4000);
  }
  if (level <= 4 && mv > 0 && mv < 3450) {
    // Journal lines are already flushed on every write; this is a clean stop.
    screenOn();
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setFont(&fonts::FreeSansBold9pt7b);
    M5.Display.drawString("Shutting down to save your data", 160, 120);
    delay(2500);
    M5.Power.powerOff();
  }
}

}  // namespace

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  M5.begin(cfg);
  S.bootMs = millis();
  M5.BtnA.setHoldThresh(kHoldMs);
  M5.BtnB.setHoldThresh(kHoldMs);
  M5.BtnC.setHoldThresh(kHoldMs);

  ui::begin();
  S.storageOk = storage::begin();
  if (S.storageOk) {
    if (!storage::loadSettings(S.settings)) app::showToast("Settings unreadable; using defaults", 4000);
    if (!storage::loadJournal(S.ledger, S.journalBadLines)) {
      S.storageOk = false;
      app::showToast("Could not read saved entries", 5000);
    }
  } else {
    app::showToast("Storage error", 5000);
  }
  M5.Display.setBrightness(S.settings.brightness);
  rtc::isValid();
  app::markInteraction();
  Serial.printf("{\"ok\":true,\"cmd\":\"boot\",\"fw\":\"%s\",\"entries\":%u,\"bad_lines\":%u}\n",
                app::kFirmwareVersion, (unsigned)S.ledger.size(), (unsigned)S.journalBadLines);
}

void loop() {
  M5.update();
  serial_cmd::poll();

  setMotor(S.vibe.isOn(millis()));
  handleZones();
  handleTouch();
  checkReminder();
  checkBattery();

  if (S.screenOn && millis() - S.lastInteractionMs > (uint32_t)S.settings.screenTimeoutSec * 1000 &&
      !S.vibe.active()) {
    screenOff();
  }
  if (S.screenOn && millis() - lastDrawMs >= 200) {
    lastDrawMs = millis();
    ui::draw();
  }
  delay(S.screenOn ? 10 : 30);
}
