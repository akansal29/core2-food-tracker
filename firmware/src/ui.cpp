#include "ui.h"

#include <M5Unified.h>

#include "core/format.h"
#include "core/timeutil.h"
#include "storage.h"

using app::S;

namespace ui {

namespace {

M5Canvas canvas(&M5.Display);
bool canvasOk = false;

struct Rect {
  int16_t x, y, w, h;
  bool contains(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

struct Btn {
  Button id;
  Rect r;
  const char* label;
};

// Palette (guide Section 7)
uint32_t cBg, cText, cMuted, cTrack, cKcal, cProtein, cWater, cButton, cDone, cWarn;

// Home layout bands (y px): status 0-22, rows 24-150, next 152-172, hub 176-214, zone labels 218-240
const Btn kHomeButtons[] = {
    {kAddKcal, {4, 178, 76, 36}, "+100 kcal"},
    {kAddProtein, {84, 178, 76, 36}, "+5 g"},
    {kUndo, {164, 178, 72, 36}, "Undo"},
    {kOpenDiag, {240, 178, 76, 36}, "Test"},
};

const Btn kDiagButtons[] = {
    {kVibeEat, {4, 150, 100, 38}, "Buzz: eat"},
    {kVibeWater, {110, 150, 100, 38}, "Buzz: water"},
    {kVibeBoth, {216, 150, 100, 38}, "Buzz: both"},
    {kTestReminder, {4, 194, 120, 40}, "Reminder 30s"},
    {kSaveSettings, {130, 194, 100, 40}, "Save test"},
    {kBack, {236, 194, 80, 40}, "Back"},
};

const Btn kReminderButtons[] = {
    {kSipped, {16, 160, 140, 56}, "Sipped +2 oz"},
    {kDismiss, {164, 160, 140, 56}, "Dismiss"},
};

template <size_t N>
Button hit(const Btn (&btns)[N], int x, int y) {
  for (const Btn& b : btns)
    if (b.r.contains(x, y)) return b.id;
  return kNone;
}

template <size_t N>
void drawButtons(const Btn (&btns)[N], bool dimmed) {
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextDatum(middle_center);
  for (const Btn& b : btns) {
    canvas.fillRoundRect(b.r.x, b.r.y, b.r.w, b.r.h, 6, cButton);
    canvas.setTextColor(dimmed ? cMuted : cText);
    canvas.drawString(b.label, b.r.x + b.r.w / 2, b.r.y + b.r.h / 2);
  }
}

void drawStatusBar() {
  canvas.setFont(&fonts::FreeSans9pt7b);
  canvas.setTextColor(cText);
  canvas.setTextDatum(top_left);
  int64_t now;
  if (app::nowLocal(now)) {
    ft::CivilTime t = ft::fromLocalSeconds(now);
    canvas.drawString(ft::clock12(t.hour, t.minute).c_str(), 6, 3);
  } else {
    canvas.setTextColor(cWarn);
    canvas.drawString("Clock not set", 6, 3);
    canvas.setTextColor(cText);
  }
  if (S.locked) {  // small padlock
    canvas.drawRoundRect(150, 4, 12, 9, 3, cMuted);
    canvas.fillRoundRect(147, 10, 18, 11, 2, cMuted);
  }
  int level = M5.Power.getBatteryLevel();
  bool charging = M5.Power.isCharging() == m5::Power_Class::is_charging;
  uint32_t battColor = (level >= 0 && level <= 20 && !charging) ? cWarn : cMuted;
  canvas.drawRect(282, 5, 30, 13, battColor);
  canvas.fillRect(312, 9, 3, 5, battColor);
  if (level > 0) canvas.fillRect(284, 7, (26 * min(level, 100)) / 100, 9, battColor);
  canvas.setTextDatum(top_right);
  canvas.setTextColor(battColor);
  String pct = level >= 0 ? String(level) + (charging ? "%+" : "%") : String("?");
  canvas.drawString(pct.c_str(), 278, 3);
}

void drawProgressRow(int y, const char* label, float value, float target, const char* unit, uint32_t color) {
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextColor(cText);
  canvas.setTextDatum(top_left);
  canvas.drawString(label, 8, y);
  canvas.setFont(&fonts::FreeSansBold12pt7b);
  canvas.setTextDatum(top_right);
  canvas.drawString(ft::progressText(value, target, unit).c_str(), 312, y - 3);
  int barY = y + 22, barW = 304;
  canvas.fillRoundRect(8, barY, barW, 10, 5, cTrack);
  float f = ft::progressFraction(value, target);
  int w = static_cast<int>(barW * f);
  if (w > 0) canvas.fillRoundRect(8, barY, max(w, 10), 10, 5, value >= target ? cDone : color);
}

void drawZoneLabels() {
  canvas.setFont(&fonts::FreeSans9pt7b);
  canvas.setTextColor(cMuted);
  canvas.setTextDatum(bottom_center);
  // Centered over the three touch circles below the screen.
  canvas.drawString("Log food", 55, 239);
  canvas.drawString(S.locked ? "Hold: unlock" : "Home", 160, 239);
  canvas.drawString("+8 oz", 265, 239);
}

void drawToast() {
  if (S.toast.length() == 0 || millis() > S.toastUntilMs) return;
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  int w = canvas.textWidth(S.toast.c_str()) + 24;
  int x = (320 - w) / 2;
  canvas.fillRoundRect(x, 96, w, 34, 8, cButton);
  canvas.drawRoundRect(x, 96, w, 34, 8, cMuted);
  canvas.setTextDatum(middle_center);
  canvas.setTextColor(cText);
  canvas.drawString(S.toast.c_str(), 160, 113);
}

void drawHome() {
  drawStatusBar();
  int64_t now = 0;
  ft::Totals t;
  if (app::nowLocal(now)) t = S.ledger.totalsFor(ft::trackingDay(now));
  drawProgressRow(28, "Calories", t.kcal, S.settings.kcalTarget, "kcal", cKcal);
  drawProgressRow(70, "Protein", t.protein, S.settings.proteinTarget, "g", cProtein);
  drawProgressRow(112, "Water", t.waterOz, S.settings.waterTargetOz, "oz", cWater);

  canvas.setFont(&fonts::FreeSans9pt7b);
  canvas.setTextDatum(top_left);
  if (!S.clockValid) {
    canvas.setTextColor(cWarn);
    canvas.drawString("Set the clock: python sync/m1_tool.py settime", 8, 154);
  } else if (!S.storageOk) {
    canvas.setTextColor(cWarn);
    canvas.drawString("Storage error: entries may not save", 8, 154);
  } else if (S.testReminderAtMs) {
    canvas.setTextColor(cMuted);
    long secs = (long)(S.testReminderAtMs - millis()) / 1000;
    canvas.drawString(("Test reminder in " + String(max(secs, 0L)) + " s").c_str(), 8, 154);
  } else {
    canvas.setTextColor(cMuted);
    canvas.drawString("Milestone 1 test build", 8, 154);
  }
  drawButtons(kHomeButtons, S.locked);
  drawZoneLabels();
}

void drawDiagnostics() {
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextColor(cText);
  canvas.setTextDatum(top_left);
  canvas.drawString("Hardware test", 8, 4);
  canvas.setFont(&fonts::FreeSans9pt7b);
  int64_t now = 0;
  String clock = "not set";
  String day = "-";
  if (app::nowLocal(now)) {
    ft::CivilTime c = ft::fromLocalSeconds(now);
    char buf[32];
    snprintf(buf, sizeof(buf), "%04ld-%02u-%02u %02u:%02u:%02u", (long)c.year, c.month, c.day, c.hour, c.minute, c.second);
    clock = buf;
    day = String(ft::trackingDay(now));
  }
  bool charging = M5.Power.isCharging() == m5::Power_Class::is_charging;
  String lines[] = {
      "Clock: " + clock + "   Day: " + day,
      "Battery: " + String(M5.Power.getBatteryLevel()) + "%  " + String(M5.Power.getBatteryVoltage()) + " mV" +
          (charging ? "  charging" : ""),
      "Flash: " + String(storage::usedBytes() / 1024) + " / " + String(storage::totalBytes() / 1024) +
          " KB   Journal: " + String(storage::journalBytes()) + " B",
      "Entries: " + String((unsigned)S.ledger.size()) + "  Skipped lines: " + String(S.journalBadLines) +
          "  Saves: " + String(S.settings.saveCounter),
      "Last zone: " + S.lastZoneEvent,
      "Uptime: " + String((millis() - S.bootMs) / 60000) + " min   FW " + app::kFirmwareVersion,
  };
  int y = 26;
  for (const String& l : lines) {
    canvas.drawString(l.c_str(), 8, y);
    y += 20;
  }
  drawButtons(kDiagButtons, false);
}

void drawReminder() {
  canvas.setFont(&fonts::FreeSansBold12pt7b);
  canvas.setTextColor(cWater);
  canvas.setTextDatum(top_center);
  canvas.drawString("Quick water check.", 160, 40);
  canvas.setFont(&fonts::FreeSans9pt7b);
  canvas.setTextColor(cMuted);
  canvas.drawString("Test reminder. These buttons work", 160, 90);
  canvas.drawString("even while the screen is locked.", 160, 110);
  drawButtons(kReminderButtons, false);
}

}  // namespace

void begin() {
  M5.Display.setRotation(1);
  canvas.setColorDepth(16);
  canvas.setPsram(true);
  canvasOk = canvas.createSprite(320, 240) != nullptr;
  cBg = M5.Display.color888(0x0B, 0x0B, 0x0B);
  cText = M5.Display.color888(0xF2, 0xF2, 0xF2);
  cMuted = M5.Display.color888(0x9A, 0x9A, 0x9A);
  cTrack = M5.Display.color888(0x2A, 0x2A, 0x2A);
  cKcal = M5.Display.color888(0xE8, 0xA2, 0x4A);
  cProtein = M5.Display.color888(0x5B, 0x9A, 0x7A);
  cWater = M5.Display.color888(0x5F, 0xA3, 0xE0);
  cButton = M5.Display.color888(0x24, 0x24, 0x24);
  cDone = M5.Display.color888(0xD8, 0xD8, 0xD8);  // neutral "complete", never red
  cWarn = M5.Display.color888(0xF0, 0xC0, 0x40);  // amber for device problems only
}

void draw() {
  if (!canvasOk) return;
  canvas.fillSprite(cBg);
  switch (S.screen) {
    case app::Screen::Home: drawHome(); break;
    case app::Screen::Diagnostics: drawDiagnostics(); break;
    case app::Screen::Reminder: drawReminder(); break;
  }
  drawToast();
  canvas.pushSprite(0, 0);
}

Button hitTest(int x, int y) {
  switch (S.screen) {
    case app::Screen::Home: return hit(kHomeButtons, x, y);
    case app::Screen::Diagnostics: return hit(kDiagButtons, x, y);
    case app::Screen::Reminder: return hit(kReminderButtons, x, y);
  }
  return kNone;
}

}  // namespace ui
