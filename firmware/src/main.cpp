// Core2 food tracker — Milestone 2: food logging.
// See docs/M2_CHECKLIST.md for what to test on the device.
#include <M5Unified.h>

#include "actions.h"
#include "app.h"
#include "core/timeutil.h"
#include "food_store.h"
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
  ui::requestRedraw();
}

void markInteraction() { S.lastInteractionMs = millis(); }

}  // namespace app

using app::S;

namespace {

constexpr uint32_t kHoldMs = 1000;       // touch zones: hold to unlock / open Today
constexpr uint32_t kLongPressMs = 600;   // screen: long-press a food to log instantly
constexpr int kDragThreshold = 10;       // px of movement before a touch counts as a scroll
uint32_t lastDrawMs = 0;
uint32_t lastBatteryCheckMs = 0;
bool motorOn = false;

void setMotor(bool on) {
  if (on == motorOn) return;
  motorOn = on;
  M5.Power.setVibration(on ? 200 : 0);
}

void screenOn() {
  if (S.screenOn) return;
  setCpuFrequencyMhz(240);
  M5.Display.wakeup();
  M5.Display.setBrightness(S.settings.brightness);
  S.screenOn = true;
  app::markInteraction();
  ui::requestRedraw();
}

void screenOff() {
  if (!S.screenOn) return;
  ui::onSleep();           // wake always shows Home; keeps an in-progress entry for "Resume?"
  M5.Display.setBrightness(0);
  M5.Display.sleep();
  S.screenOn = false;
  S.locked = true;         // auto-lock on sleep (guide Section 8)
  setCpuFrequencyMhz(80);
}

void lockedHint() { app::showToast("Locked. Hold the middle to unlock."); }

bool inputBlocked() {
  if (serial_cmd::isSyncing()) {
    app::showToast("Syncing... one moment");
    return true;
  }
  return false;
}

void handleZones() {
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
    else if (!inputBlocked()) ui::openLogFood();
  }
  if (M5.BtnB.wasHold()) {
    S.lastZoneEvent = "Middle hold";
    app::markInteraction();
    if (S.locked) {
      S.locked = false;
      actions::buzz(ft::VibePattern::Tick);
      app::showToast("Unlocked");
    } else if (!inputBlocked()) {
      ui::openToday();
    }
  } else if (M5.BtnB.wasClicked()) {
    S.lastZoneEvent = "Middle tap";
    app::markInteraction();
    if (S.screen != app::Screen::Reminder) ui::goHome();
  }
  if (M5.BtnC.wasClicked()) {
    S.lastZoneEvent = "Right tap";
    app::markInteraction();
    if (S.locked) lockedHint();
    else if (!inputBlocked()) actions::addWater(8);
  }
}

// Turns raw touch into tap / long-press / drag (screen area only; y >= 240 is the zone strip).
void handleTouch() {
  static bool down = false, dragging = false, holdFired = false, swallow = false;
  static int startX = 0, startY = 0, lastY = 0;
  static uint32_t downMs = 0;

  bool pressed = false;
  int x = 0, y = 0;
  if (M5.Touch.getCount() > 0) {
    const auto& d = M5.Touch.getDetail();
    pressed = d.isPressed();
    x = d.x;
    y = d.y;
  }

  if (pressed && !down) {  // touch started
    down = true;
    dragging = holdFired = false;
    startX = x;
    startY = lastY = y;
    downMs = millis();
    if (!S.screenOn) {  // the tap that wakes the screen does nothing else
      screenOn();
      swallow = true;
    } else {
      swallow = y >= 240;
    }
    app::markInteraction();
    return;
  }
  if (!pressed && down) {  // touch ended
    down = false;
    if (swallow || dragging || holdFired) return;
    bool lockedOk = S.screen == app::Screen::Reminder;  // reminder buttons work while locked
    if (S.locked && !lockedOk) return lockedHint();
    if (!lockedOk && inputBlocked()) return;
    ui::tap(startX, startY);
    return;
  }
  if (!pressed || swallow) return;

  app::markInteraction();
  if (!dragging && abs(y - startY) > kDragThreshold) dragging = true;
  if (dragging) {
    if (!S.locked) ui::drag(y - lastY);
    lastY = y;
    return;
  }
  if (!holdFired && millis() - downMs >= kLongPressMs) {
    holdFired = true;
    if (S.locked) return lockedHint();
    if (inputBlocked()) return;
    ui::hold(startX, startY);
  }
}

void checkReminder() {
  if (!S.testReminderAtMs || millis() < S.testReminderAtMs) return;
  S.testReminderAtMs = 0;
  screenOn();  // wakes the display even if it was asleep
  S.screen = app::Screen::Reminder;
  actions::buzz(ft::VibePattern::Water);
  ui::requestRedraw();
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
    app::showToast(String(("Battery low (" + std::to_string(level) + "%)").c_str()), 4000);
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
  // Must come before Serial.begin (inside M5.begin). The default 256-byte buffer
  // overflows while the screen redraws and silently drops incoming bytes.
  Serial.setRxBufferSize(4096);
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
    foods::load();
  } else {
    app::showToast("Storage error", 5000);
  }
  M5.Display.setBrightness(S.settings.brightness);
  rtc::isValid();
  app::markInteraction();
  Serial.printf("{\"ok\":true,\"cmd\":\"boot\",\"fw\":\"%s\",\"entries\":%u,\"foods\":%u}\n", app::kFirmwareVersion,
                (unsigned)S.ledger.size(), (unsigned)foods::db().all().size());
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
      !S.vibe.active() && !serial_cmd::isSyncing()) {
    screenOff();
  }
  if (S.screenOn && (ui::redrawRequested() || millis() - lastDrawMs >= 250)) {
    lastDrawMs = millis();
    ui::draw();
  }
  delay(S.screenOn ? 10 : 30);
}
