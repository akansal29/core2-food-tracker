#include "ui.h"

#include <M5Unified.h>
#include <math.h>

#include <functional>
#include <vector>

#include "actions.h"
#include "core/format.h"
#include "core/history.h"
#include "core/nutrition.h"
#include "core/timeutil.h"
#include "food_store.h"
#include "storage.h"

using app::S;
using app::Screen;

namespace ui {

namespace {

M5Canvas canvas(&M5.Display);
bool canvasOk = false;
bool redraw = true;

struct Rect {
  int16_t x, y, w, h;
  bool contains(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

struct Hit {
  Rect r;
  std::function<void()> onTap;
  std::function<void()> onHold;
};
std::vector<Hit> hits;

// Scrollable list viewport for the current frame
Rect listView{0, 0, 0, 0};
int listMax = 0;

// Palette (guide Section 7)
uint32_t cBg, cText, cMuted, cTrack, cKcal, cProtein, cWater, cButton, cButtonHi, cDone, cWarn;

constexpr int kRowH = 46;

// ---------- drawing helpers ----------

void addHit(Rect r, std::function<void()> onTap, std::function<void()> onHold = nullptr) {
  hits.push_back({r, std::move(onTap), std::move(onHold)});
}

std::string fit(const std::string& s, int maxW) {
  if (canvas.textWidth(s.c_str()) <= maxW) return s;
  std::string t = s;
  while (t.size() > 1 && canvas.textWidth((t + "..").c_str()) > maxW) t.pop_back();
  return t + "..";
}

void button(Rect r, const char* label, std::function<void()> onTap, uint32_t fill = 0, bool enabled = true) {
  canvas.fillRoundRect(r.x, r.y, r.w, r.h, 6, fill ? fill : cButton);
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextDatum(middle_center);
  canvas.setTextColor(enabled ? (fill ? cBg : cText) : cMuted);
  canvas.drawString(fit(label, r.w - 8).c_str(), r.x + r.w / 2, r.y + r.h / 2);
  if (enabled && onTap) addHit(r, std::move(onTap));
}

void drawStar(int cx, int cy, int r, uint32_t color, bool filled) {
  float px[10], py[10];
  for (int i = 0; i < 10; ++i) {
    float a = -M_PI / 2 + i * M_PI / 5;
    float rr = (i % 2 == 0) ? r : r * 0.45f;
    px[i] = cx + rr * cosf(a);
    py[i] = cy + rr * sinf(a);
  }
  for (int i = 0; i < 10; ++i) {
    int j = (i + 1) % 10;
    if (filled) canvas.fillTriangle(cx, cy, (int)px[i], (int)py[i], (int)px[j], (int)py[j], color);
    else canvas.drawLine((int)px[i], (int)py[i], (int)px[j], (int)py[j], color);
  }
}

std::string kcalText(float kcal) { return ft::withCommas(lroundf(kcal)) + " kcal"; }

std::string proteinText(float g) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%.1f g", (double)g);
  return buf;
}

void header(const char* title, bool back = true) {
  if (back) button({0, 0, 76, 30}, "< Back", [] { goHome(); });
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextDatum(middle_center);
  canvas.setTextColor(cText);
  canvas.drawString(fit(title, 150).c_str(), 160, 15);
}

void drawZoneLabels() {
  canvas.setFont(&fonts::FreeSans9pt7b);
  canvas.setTextColor(cMuted);
  canvas.setTextDatum(bottom_center);
  canvas.drawString("Log food", 55, 239);
  canvas.drawString(S.locked ? "Hold: unlock" : "Home", 160, 239);
  canvas.drawString("+8 oz", 265, 239);
}

void drawToast() {
  if (S.toast.length() == 0 || millis() > S.toastUntilMs) return;
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  int w = canvas.textWidth(S.toast.c_str()) + 24;
  if (w > 316) w = 316;
  int x = (320 - w) / 2;
  canvas.fillRoundRect(x, 96, w, 34, 8, cButton);
  canvas.drawRoundRect(x, 96, w, 34, 8, cMuted);
  canvas.setTextDatum(middle_center);
  canvas.setTextColor(cText);
  canvas.drawString(fit(S.toast.c_str(), w - 12).c_str(), 160, 113);
}

// A food row: name on top, "190 kcal | 8.0 g | 1 cup" below.
void foodRow(int y, const ft::FoodDef& f, Screen from) {
  Rect r{4, (int16_t)y, 312, kRowH - 4};
  canvas.fillRoundRect(r.x, r.y, r.w, r.h, 6, cButton);
  canvas.setTextDatum(top_left);
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextColor(cText);
  canvas.drawString(fit(f.name, 270).c_str(), 12, y + 4);
  if (foods::isFavorite(f.id)) drawStar(300, y + 12, 7, cKcal, true);
  canvas.setFont(&fonts::FreeSans9pt7b);
  canvas.setTextColor(cMuted);
  std::string sub = kcalText(f.kcal) + " | " + proteinText(f.protein) + " | " + f.serving;
  canvas.drawString(fit(sub, 296).c_str(), 12, y + 23);
  // Only register rows that are inside the visible list area.
  int top = y > listView.y ? y : listView.y;
  int bottom = (y + r.h) < (listView.y + listView.h) ? (y + r.h) : (listView.y + listView.h);
  if (bottom - top < 8) return;
  Rect visible{r.x, (int16_t)top, r.w, (int16_t)(bottom - top)};
  std::string id = f.id;
  addHit(visible,
         [id, from] {  // tap: open the serving picker
           S.pickerFoodId = id;
           S.pickerServings = 1;
           S.comboAdjusted = false;
           S.comboIngredients.clear();
           S.pickerReturn = from;
           S.screen = Screen::Picker;
         },
         [id] { actions::logFood(id, 1); });  // long-press: instant 1 serving
}

void sectionLabel(int y, const char* text) {
  canvas.setFont(&fonts::FreeSans9pt7b);
  canvas.setTextColor(cMuted);
  canvas.setTextDatum(top_left);
  canvas.drawString(text, 8, y + 4);
}

void beginList(Rect view) {
  listView = view;
  canvas.setClipRect(view.x, view.y, view.w, view.h);
}

void endList(int contentHeight) {
  canvas.clearClipRect();
  listMax = contentHeight > listView.h ? contentHeight - listView.h : 0;
  if (S.listScroll > listMax) S.listScroll = listMax;
  if (S.listScroll < 0) S.listScroll = 0;
  if (listMax > 0) {  // scroll bar
    int barH = listView.h * listView.h / contentHeight;
    int barY = listView.y + (listView.h - barH) * S.listScroll / listMax;
    canvas.fillRoundRect(316, barY, 3, barH, 1, cMuted);
  }
}

void progressRow(int y, const char* label, float value, float target, const char* unit, uint32_t color) {
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextColor(cText);
  canvas.setTextDatum(top_left);
  canvas.drawString(label, 8, y);
  canvas.setFont(&fonts::FreeSansBold12pt7b);
  canvas.setTextDatum(top_right);
  canvas.drawString(ft::progressText(value, target, unit).c_str(), 312, y - 3);
  int barY = y + 22, barW = 304;
  canvas.fillRoundRect(8, barY, barW, 10, 5, cTrack);
  int w = static_cast<int>(barW * ft::progressFraction(value, target));
  if (w > 0) canvas.fillRoundRect(8, barY, w < 10 ? 10 : w, 10, 5, value >= target ? cDone : color);
}

void statusBar() {
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
  }
  if (S.locked) {
    canvas.drawRoundRect(150, 4, 12, 9, 3, cMuted);
    canvas.fillRoundRect(147, 10, 18, 11, 2, cMuted);
  }
  int level = M5.Power.getBatteryLevel();
  bool charging = M5.Power.isCharging() == m5::Power_Class::is_charging;
  uint32_t battColor = (level >= 0 && level <= 20 && !charging) ? cWarn : cMuted;
  canvas.drawRect(282, 5, 30, 13, battColor);
  canvas.fillRect(312, 9, 3, 5, battColor);
  if (level > 0) canvas.fillRect(284, 7, (26 * (level > 100 ? 100 : level)) / 100, 9, battColor);
  canvas.setTextDatum(top_right);
  canvas.setTextColor(battColor);
  std::string pct = level >= 0 ? std::to_string(level) + (charging ? "%+" : "%") : "?";
  canvas.drawString(pct.c_str(), 278, 3);
}

ft::Totals todayTotals() {
  int64_t now;
  if (!app::nowLocal(now)) return {};
  return S.ledger.totalsFor(ft::trackingDay(now));
}

// ---------- screens ----------

void drawHome() {
  statusBar();
  ft::Totals t = todayTotals();
  progressRow(28, "Calories", t.kcal, S.settings.kcalTarget, "kcal", cKcal);
  progressRow(70, "Protein", t.protein, S.settings.proteinTarget, "g", cProtein);
  progressRow(112, "Water", t.waterOz, S.settings.waterTargetOz, "oz", cWater);

  bool banner = !S.undoId.empty() && millis() < S.undoUntilMs;
  if (banner) {
    canvas.setFont(&fonts::FreeSansBold9pt7b);
    canvas.setTextDatum(top_left);
    canvas.setTextColor(cText);
    canvas.drawString(fit(S.bannerText.c_str(), 304).c_str(), 8, 153);
    button({4, 178, 150, 36}, "Undo", [] { actions::undoLast(); });
    button({166, 178, 150, 36}, "Log another", [] { openLogFood(); });
  } else {
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextDatum(top_left);
    if (!S.clockValid) {
      canvas.setTextColor(cWarn);
      canvas.drawString("Set the clock: py ..\\sync\\m1_tool.py settime", 8, 154);
    } else if (!S.storageOk) {
      canvas.setTextColor(cWarn);
      canvas.drawString("Storage error: entries may not save", 8, 154);
    } else if (S.resumeScreen != Screen::Home && millis() < S.resumeUntilMs) {
      canvas.setTextColor(cKcal);
      const ft::FoodDef* f = foods::db().find(S.pickerFoodId);
      std::string what = S.resumeScreen == Screen::Custom      ? "custom entry"
                         : S.resumeScreen == Screen::EntryEdit ? "editing " + S.editEntry.name
                         : (f ? f->name : "entry");
      canvas.drawString(fit("Tap to resume: " + what, 304).c_str(), 8, 154);
      addHit({0, 148, 320, 26}, [] {
        S.screen = S.resumeScreen;
        S.resumeScreen = Screen::Home;
      });
    } else if (S.testReminderAtMs) {
      canvas.setTextColor(cMuted);
      long secs = (long)(S.testReminderAtMs - millis()) / 1000;
      canvas.drawString(("Test reminder in " + std::to_string(secs < 0 ? 0 : secs) + " s").c_str(), 8, 154);
    } else if (t.lastFoodTs > 0) {
      ft::CivilTime c = ft::fromLocalSeconds(t.lastFoodTs);
      canvas.setTextColor(cMuted);
      canvas.drawString(("Last food " + ft::clock12(c.hour, c.minute)).c_str(), 8, 154);
    }
    bool en = !S.locked;
    button({4, 178, 74, 36}, "Today", [] { openToday(); }, 0, en);
    button({82, 178, 74, 36}, "Ideas", [] { app::showToast("Suggestions arrive in Milestone 6"); }, 0, en);
    button({160, 178, 74, 36}, "Trends", [] { app::showToast("Trends arrive in Milestone 6"); }, 0, en);
    button({238, 178, 78, 36}, "Settings", [] { S.screen = Screen::Diagnostics; }, 0, en);
  }
}

void emptyNote(int y, const char* text, uint32_t color) {
  canvas.setFont(&fonts::FreeSans9pt7b);
  canvas.setTextColor(color);
  canvas.setTextDatum(top_left);
  canvas.drawString(fit(text, 304).c_str(), 8, y + 6);
}

void drawLogFood() {
  header("Log food");
  button({4, 34, 102, 34}, "Browse all", [] {
    S.listScroll = 0;
    S.screen = Screen::Browse;
  });
  button({109, 34, 102, 34}, "Custom", [] {
    S.custom = app::CustomForm{};
    S.screen = Screen::Custom;
  });
  button({214, 34, 102, 34}, "Favorites", [] {
    S.listScroll = 0;
    S.screen = Screen::Favorites;
  });

  beginList({0, 72, 320, 144});
  int y = 72 - S.listScroll;
  int top = y;
  if (!foods::hasFoods()) {
    emptyNote(y, foods::loadMessage().c_str(), cWarn);
    y += 30;
  }
  sectionLabel(y, "Recent");
  y += 24;
  std::vector<std::string> recent = ft::recentFoodIds(S.ledger, foods::db(), 8);
  if (recent.empty()) {
    emptyNote(y, "Foods you log show up here.", cMuted);
    y += 30;
  }
  for (const auto& id : recent) {
    foodRow(y, *foods::db().find(id), Screen::LogFood);
    y += kRowH;
  }
  endList(y - top);
}

void drawFavorites() {
  button({0, 0, 76, 30}, "< Back", [] { openLogFood(); });
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextDatum(middle_center);
  canvas.setTextColor(cText);
  canvas.drawString("Favorites", 160, 15);
  beginList({0, 34, 320, 182});
  int y = 34 - S.listScroll;
  int top = y;
  auto favs = foods::favorites();
  if (favs.empty()) {
    emptyNote(y, "No favorites yet.", cText);
    emptyNote(y + 24, "Open any food and tap the star.", cMuted);
    y += 56;
  }
  for (const ft::FoodDef* f : favs) {
    foodRow(y, *f, Screen::Favorites);
    y += kRowH;
  }
  endList(y - top);
}

void drawBrowse() {
  header("All foods");
  static const char* kTabs[] = {"Snacks", "Drinks", "Meals"};
  for (int i = 0; i < 3; ++i) {
    bool on = S.browseCategory == i;
    button({(int16_t)(4 + i * 105), 34, 102, 32}, kTabs[i],
           [i] {
             S.browseCategory = i;
             S.listScroll = 0;
           },
           on ? cKcal : 0);
  }
  beginList({0, 70, 320, 146});
  int y = 70 - S.listScroll;
  int top = y;
  auto list = foods::db().inCategory(static_cast<ft::Category>(S.browseCategory));
  if (list.empty()) {
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextColor(cMuted);
    canvas.setTextDatum(top_left);
    canvas.drawString(foods::hasFoods() ? "Nothing in this group yet." : foods::loadMessage().c_str(), 8, y + 6);
    y += 30;
  }
  for (const ft::FoodDef* f : list) {
    foodRow(y, *f, Screen::Browse);
    y += kRowH;
  }
  endList(y - top);
}

void backToPickerSource() {
  S.screen = S.pickerReturn;
}

void drawPicker() {
  const ft::FoodDef* f = foods::db().find(S.pickerFoodId);
  if (!f) {
    goHome();
    return;
  }
  button({0, 0, 76, 30}, "< Back", [] { backToPickerSource(); });
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextDatum(middle_center);
  canvas.setTextColor(cText);
  canvas.drawString(fit(f->name, 184).c_str(), 172, 15);
  bool fav = foods::isFavorite(f->id);
  drawStar(298, 15, 11, fav ? cKcal : cMuted, fav);
  std::string id = f->id;
  addHit({268, 0, 52, 32}, [id] {
    bool now = foods::toggleFavorite(id);
    app::showToast(now ? "Added to favorites" : "Removed from favorites");
  });

  canvas.setFont(&fonts::FreeSans9pt7b);
  canvas.setTextDatum(top_center);
  canvas.setTextColor(cMuted);
  canvas.drawString(fit("1 serving = " + f->serving, 304).c_str(), 160, 36);

  ft::Amount a;
  if (S.comboAdjusted) {
    a = ft::ingredientsTotal(foods::db(), S.comboIngredients);
    canvas.setFont(&fonts::FreeSansBold12pt7b);
    canvas.setTextColor(cText);
    canvas.setTextDatum(middle_center);
    canvas.drawString("Adjusted amounts", 160, 80);
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextColor(cMuted);
    canvas.drawString("Tap Adjust to change, or Back to discard", 160, 118);
  } else {
    a = ft::scale(*f, S.pickerServings);
    button({8, 58, 56, 40}, "-", [] { S.pickerServings = ft::snapServings(S.pickerServings - 0.25f); });
    button({256, 58, 56, 40}, "+", [] { S.pickerServings = ft::snapServings(S.pickerServings + 0.25f); });
    canvas.setFont(&fonts::FreeSansBold12pt7b);
    canvas.setTextColor(cText);
    canvas.setTextDatum(middle_center);
    canvas.drawString(ft::servingsLabel(S.pickerServings).c_str(), 160, 78);
    static const float kChips[] = {0.25f, 0.5f, 0.75f, 1.0f, 2.0f};
    static const char* kChipText[] = {"1/4", "1/2", "3/4", "1", "2"};
    for (int i = 0; i < 5; ++i) {
      float v = kChips[i];
      bool on = fabsf(S.pickerServings - v) < 0.01f;
      button({(int16_t)(8 + i * 61), 104, 58, 32}, kChipText[i], [v] { S.pickerServings = v; }, on ? cKcal : 0);
    }
  }

  ft::Totals t = todayTotals();
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextDatum(top_center);
  canvas.setTextColor(cKcal);
  canvas.drawString(("+" + kcalText(a.kcal) + ",  +" + proteinText(a.protein) + " protein").c_str(), 160, 142);
  canvas.setFont(&fonts::FreeSans9pt7b);
  canvas.setTextColor(cMuted);
  canvas.drawString(("New total: " + ft::progressText(t.kcal + a.kcal, S.settings.kcalTarget, "kcal")).c_str(), 160,
                    162);

  button({8, 182, 96, 34}, "Cancel", [] { backToPickerSource(); });
  if (f->isCombo()) {
    button({112, 182, 96, 34}, "Adjust", [] {
      if (!S.comboAdjusted) {
        const ft::FoodDef* c = foods::db().find(S.pickerFoodId);
        S.comboIngredients.clear();
        // Start from the chosen number of servings of the saved recipe.
        for (const auto& ing : c->ingredients)
          S.comboIngredients.emplace_back(ing.first, ft::snapServings(ing.second * S.pickerServings));
      }
      S.screen = Screen::ComboAdjust;
    });
  }
  button({216, 182, 96, 34}, "Log", [] {
    if (S.comboAdjusted) actions::logAdjustedCombo(S.pickerFoodId);
    else actions::logFood(S.pickerFoodId, S.pickerServings);
  }, cKcal);
}

void drawComboAdjust() {
  const ft::FoodDef* f = foods::db().find(S.pickerFoodId);
  if (!f) {
    goHome();
    return;
  }
  button({0, 0, 76, 30}, "< Back", [] { S.screen = Screen::Picker; });
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextDatum(middle_center);
  canvas.setTextColor(cText);
  canvas.drawString("Adjust this entry", 160, 15);
  button({244, 0, 76, 30}, "Reset", [] {
    S.comboAdjusted = false;
    S.comboIngredients.clear();
    S.screen = Screen::Picker;
  });

  int y = 36;
  for (size_t i = 0; i < S.comboIngredients.size() && i < 4; ++i) {
    const ft::FoodDef* g = foods::db().find(S.comboIngredients[i].first);
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextDatum(middle_left);
    canvas.setTextColor(cText);
    canvas.drawString(fit(g ? g->name : S.comboIngredients[i].first, 180).c_str(), 8, y + 16);
    button({196, (int16_t)y, 40, 32}, "-", [i] {
      auto& q = S.comboIngredients[i].second;
      q = q <= 0.25f ? 0.0f : ft::snapServings(q - 0.25f);  // 0 = leave this ingredient out
    });
    canvas.setFont(&fonts::FreeSansBold9pt7b);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(cText);
    char qty[12];
    snprintf(qty, sizeof(qty), "%g", (double)S.comboIngredients[i].second);
    canvas.drawString(qty, 258, y + 16);
    button({280, (int16_t)y, 36, 32}, "+", [i] {
      auto& q = S.comboIngredients[i].second;
      q = ft::snapServings(q + 0.25f);
    });
    y += 36;
  }
  ft::Amount a = ft::ingredientsTotal(foods::db(), S.comboIngredients);
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextDatum(middle_left);
  canvas.setTextColor(cKcal);
  canvas.drawString((kcalText(a.kcal) + ", " + proteinText(a.protein)).c_str(), 8, 199);
  button({216, 182, 96, 34}, "Done", [] {
    S.comboAdjusted = true;
    S.screen = Screen::Picker;
  }, cKcal);
}

float parseNum(const String& s) { return s.length() ? atof(s.c_str()) : 0.0f; }

void keypadPress(char k) {
  String& field = S.custom.field == 0 ? S.custom.kcal : S.custom.protein;
  if (k == '<') {
    if (field.length()) field = field.substring(0, field.length() - 1);
    return;
  }
  if (k == '.') {
    if (S.custom.field == 0 || field.indexOf('.') >= 0) return;  // calories are whole numbers
    if (!field.length()) field = "0";
  }
  int dot = field.indexOf('.');
  if (dot >= 0 && (int)field.length() - dot > 1) return;  // one decimal place
  if ((int)field.length() >= (S.custom.field == 0 ? 4 : 5)) return;
  field += k;
}

void drawCustom() {
  header("Custom entry");
  auto fieldBox = [](int y, const char* label, const String& value, int idx, uint32_t color) {
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextDatum(top_left);
    canvas.setTextColor(cMuted);
    canvas.drawString(label, 8, y);
    Rect r{8, (int16_t)(y + 18), 120, 36};
    canvas.fillRoundRect(r.x, r.y, r.w, r.h, 6, cButton);
    if (S.custom.field == idx) canvas.drawRoundRect(r.x, r.y, r.w, r.h, 6, color);
    canvas.setFont(&fonts::FreeSansBold12pt7b);
    canvas.setTextDatum(middle_right);
    canvas.setTextColor(value.length() ? cText : cMuted);
    canvas.drawString(value.length() ? value.c_str() : "0", r.x + r.w - 10, r.y + r.h / 2);
    addHit(r, [idx] { S.custom.field = idx; });
  };
  fieldBox(34, "Calories", S.custom.kcal, 0, cKcal);
  fieldBox(92, "Protein (g)", S.custom.protein, 1, cProtein);

  // "Save as food" checkbox
  Rect cb{8, 152, 120, 26};
  canvas.drawRect(cb.x, cb.y + 4, 18, 18, cMuted);
  if (S.custom.saveAsFood) canvas.fillRect(cb.x + 4, cb.y + 8, 10, 10, cKcal);
  canvas.setFont(&fonts::FreeSans9pt7b);
  canvas.setTextDatum(middle_left);
  canvas.setTextColor(cText);
  canvas.drawString("Save as food", cb.x + 24, cb.y + 13);
  addHit(cb, [] { S.custom.saveAsFood = !S.custom.saveAsFood; });

  static const char kKeys[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9', '.', '0', '<'};
  for (int i = 0; i < 12; ++i) {
    int col = i % 3, row = i / 3;
    char k = kKeys[i];
    char label[4] = {k, 0};
    if (k == '<') strcpy(label, "Del");
    button({(int16_t)(136 + col * 60), (int16_t)(34 + row * 37), 56, 33}, label, [k] { keypadPress(k); });
  }

  float kcal = parseNum(S.custom.kcal), prot = parseNum(S.custom.protein);
  bool valid = kcal > 0 && kcal <= 5000 && prot <= 300;
  button({136, 184, 176, 32}, "Log", [] {
    float kc = parseNum(S.custom.kcal), pr = parseNum(S.custom.protein);
    if (actions::logCustom(kc, pr, S.custom.saveAsFood)) S.custom = app::CustomForm{};
  }, valid ? cKcal : 0, valid);
  canvas.setFont(&fonts::FreeSans9pt7b);
  canvas.setTextDatum(top_left);
  canvas.setTextColor(cMuted);
  canvas.drawString(valid ? "Name: Custom + time" : "Enter calories", 8, 190);
}

// ---------- Today's Log, Past Days, entry editing (guide Section 5) ----------

uint32_t shownDay() {
  if (S.viewDay) return S.viewDay;
  int64_t now;
  return app::nowLocal(now) ? ft::trackingDay(now) : 0;
}

bool showingToday() {
  int64_t now;
  return S.viewDay == 0 || (app::nowLocal(now) && S.viewDay == ft::trackingDay(now));
}

std::string entryTime(const ft::Entry& e) {
  ft::CivilTime c = ft::fromLocalSeconds(e.ts);
  return ft::clock12(c.hour, c.minute);
}

void entryRow(int y, const ft::Entry& e) {
  Rect r{4, (int16_t)y, 312, 40};
  canvas.fillRoundRect(r.x, r.y, r.w, r.h, 6, cButton);
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextDatum(top_left);
  canvas.setTextColor(cText);
  canvas.drawString(fit(e.name, 210).c_str(), 12, y + 3);
  canvas.setFont(&fonts::FreeSans9pt7b);
  canvas.setTextDatum(top_right);
  canvas.setTextColor(cMuted);
  canvas.drawString(entryTime(e).c_str(), 308, y + 3);
  canvas.setTextDatum(top_left);
  std::string sub;
  if (e.kind == ft::EntryKind::Water) {
    canvas.setTextColor(cWater);
    sub = std::to_string((int)lroundf(e.waterOz)) + " oz water";
  } else {
    sub = kcalText(e.kcal) + " | " + proteinText(e.protein);
    if (e.kind != ft::EntryKind::Custom) sub += " | " + ft::servingsLabel(e.servings);
  }
  if (e.edited) sub += " | edited";
  canvas.drawString(fit(sub, 296).c_str(), 12, y + 21);

  int top = y > listView.y ? y : listView.y;
  int bottom = (y + r.h) < (listView.y + listView.h) ? (y + r.h) : (listView.y + listView.h);
  if (bottom - top < 8) return;
  std::string id = e.id;
  addHit({r.x, (int16_t)top, r.w, (int16_t)(bottom - top)}, [id] {
    const ft::Entry* found = S.ledger.find(id);
    if (!found) return;
    S.editEntry = *found;
    S.editOrigServings = found->servings;
    S.editReturn = Screen::DayLog;
    S.deleteArmedUntilMs = 0;
    S.screen = Screen::EntryEdit;
  });
}

void drawDayLog() {
  uint32_t day = shownDay();
  bool today = showingToday();
  if (today) {
    header("Today");
    button({222, 0, 98, 30}, "Past days", [] {
      S.listScroll = 0;
      S.screen = Screen::PastDays;
    });
  } else {
    button({0, 0, 76, 30}, "< Back", [] {
      S.listScroll = 0;
      S.screen = Screen::PastDays;
    });
    canvas.setFont(&fonts::FreeSansBold9pt7b);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(cText);
    canvas.drawString(ft::dayLabel(day).c_str(), 160, 15);
  }
  if (!day) {
    emptyNote(40, "Set the clock to see today's entries.", cWarn);
    return;
  }

  ft::Totals t = S.ledger.totalsFor(day);
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextDatum(top_left);
  canvas.setTextColor(cKcal);
  canvas.drawString(ft::progressText(t.kcal, S.settings.kcalTarget, "kcal").c_str(), 8, 34);
  canvas.setTextColor(cWater);
  canvas.drawString(ft::progressText(t.waterOz, S.settings.waterTargetOz, "oz").c_str(), 8, 52);
  canvas.setTextDatum(top_right);
  canvas.setTextColor(cProtein);
  canvas.drawString(ft::progressText(t.protein, S.settings.proteinTarget, "g").c_str(), 312, 34);
  canvas.setFont(&fonts::FreeSans9pt7b);
  canvas.setTextColor(cMuted);
  if (t.lastFoodTs) {
    ft::CivilTime c = ft::fromLocalSeconds(t.lastFoodTs);
    canvas.drawString(("Last food " + ft::clock12(c.hour, c.minute)).c_str(), 312, 52);
  }
  canvas.setTextDatum(top_left);
  canvas.drawString(ft::remainingText(t.kcal, S.settings.kcalTarget, t.protein, S.settings.proteinTarget).c_str(), 8,
                    70);

  beginList({0, 90, 320, 126});
  int y = 90 - S.listScroll;
  int top = y;
  auto entries = S.ledger.entriesFor(day);
  if (entries.empty()) {
    emptyNote(y, today ? "Nothing logged yet today." : "No entries this day.", cMuted);
    y += 30;
  }
  for (const ft::Entry* e : entries) {
    entryRow(y, *e);
    y += 44;
  }
  endList(y - top);
}

void drawPastDays() {
  button({0, 0, 76, 30}, "< Back", [] { openToday(); });
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextDatum(middle_center);
  canvas.setTextColor(cText);
  canvas.drawString("Past days", 160, 15);

  int64_t now = 0;
  uint32_t today = app::nowLocal(now) ? ft::trackingDay(now) : 0;
  beginList({0, 34, 320, 182});
  int y = 34 - S.listScroll;
  int top = y;
  auto days = ft::loggedDays(S.ledger);
  if (days.empty()) {
    emptyNote(y, "No entries yet.", cMuted);
    y += 30;
  }
  for (uint32_t d : days) {
    Rect r{4, (int16_t)y, 312, 40};
    canvas.fillRoundRect(r.x, r.y, r.w, r.h, 6, cButton);
    canvas.setFont(&fonts::FreeSansBold9pt7b);
    canvas.setTextDatum(top_left);
    canvas.setTextColor(cText);
    canvas.drawString((ft::dayLabel(d) + (d == today ? "  (today)" : "")).c_str(), 12, y + 3);
    ft::Totals t = S.ledger.totalsFor(d);
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextColor(cMuted);
    std::string sub = kcalText(t.kcal) + " | " + proteinText(t.protein) + " | " +
                      std::to_string((int)lroundf(t.waterOz)) + " oz";
    canvas.drawString(fit(sub, 296).c_str(), 12, y + 21);
    int vTop = y > listView.y ? y : listView.y;
    int vBottom = (y + r.h) < (listView.y + listView.h) ? (y + r.h) : (listView.y + listView.h);
    if (vBottom - vTop >= 8) {
      addHit({r.x, (int16_t)vTop, r.w, (int16_t)(vBottom - vTop)}, [d, today] {
        S.viewDay = (d == today) ? 0 : d;
        S.listScroll = 0;
        S.screen = Screen::DayLog;
      });
    }
    y += 44;
  }
  endList(y - top);
}

void drawEntryEdit() {
  const ft::Entry* orig = S.ledger.find(S.editEntry.id);
  if (!orig) {
    S.screen = S.editReturn;
    return;
  }
  ft::Entry& e = S.editEntry;
  button({0, 0, 76, 30}, "< Back", [] { S.screen = S.editReturn; });
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextDatum(middle_center);
  canvas.setTextColor(cText);
  canvas.drawString(fit(e.name, 230).c_str(), 196, 15);

  canvas.setFont(&fonts::FreeSans9pt7b);
  canvas.setTextDatum(top_center);
  canvas.setTextColor(cMuted);
  canvas.drawString((entryTime(e) + "  |  counts for " + ft::dayLabel(ft::trackingDay(e.ts))).c_str(), 160, 36);

  bool water = e.kind == ft::EntryKind::Water;
  button({8, 56, 56, 36}, "-", [water] {
    const ft::Entry* o = S.ledger.find(S.editEntry.id);
    if (!o) return;
    if (water) {
      if (S.editEntry.waterOz > 2) S.editEntry.waterOz -= 2;
    } else {
      int64_t ts = S.editEntry.ts;
      S.editEntry = ft::rescaleEntry(*o, ft::snapServings(S.editEntry.servings - 0.25f));
      S.editEntry.ts = ts;
    }
  });
  button({256, 56, 56, 36}, "+", [water] {
    const ft::Entry* o = S.ledger.find(S.editEntry.id);
    if (!o) return;
    if (water) {
      if (S.editEntry.waterOz < 64) S.editEntry.waterOz += 2;
    } else {
      int64_t ts = S.editEntry.ts;
      S.editEntry = ft::rescaleEntry(*o, ft::snapServings(S.editEntry.servings + 0.25f));
      S.editEntry.ts = ts;
    }
  });
  canvas.setFont(&fonts::FreeSansBold12pt7b);
  canvas.setTextDatum(middle_center);
  canvas.setTextColor(cText);
  std::string amount = water ? std::to_string((int)lroundf(e.waterOz)) + " oz" : ft::servingsLabel(e.servings);
  canvas.drawString(amount.c_str(), 160, 74);
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextColor(water ? cWater : cKcal);
  if (!water) canvas.drawString((kcalText(e.kcal) + ",  " + proteinText(e.protein) + " protein").c_str(), 160, 104);
  else canvas.drawString("Water", 160, 104);

  static const struct { const char* label; int delta; } kShifts[] = {
      {"-1 h", -3600}, {"-15 min", -900}, {"+15 min", 900}, {"+1 h", 3600}};
  for (int i = 0; i < 4; ++i) {
    int delta = kShifts[i].delta;
    button({(int16_t)(4 + i * 79), 124, 75, 34}, kShifts[i].label, [delta] {
      int64_t now;
      if (app::nowLocal(now)) S.editEntry.ts = ft::shiftTime(S.editEntry.ts, delta, now);
    });
  }

  bool armed = millis() < S.deleteArmedUntilMs;
  button({4, 180, 104, 36}, armed ? "Tap again" : "Delete", [] {
    if (millis() < S.deleteArmedUntilMs) {
      if (actions::deleteEntry(S.editEntry.id)) S.screen = S.editReturn;
      S.deleteArmedUntilMs = 0;
    } else {
      S.deleteArmedUntilMs = millis() + 3000;
    }
  }, armed ? cWarn : 0);
  bool changed = e.ts != orig->ts || fabsf(e.servings - orig->servings) > 0.001f ||
                 fabsf(e.waterOz - orig->waterOz) > 0.001f;
  button({112, 180, 96, 36}, "Cancel", [] { S.screen = S.editReturn; });
  button({212, 180, 104, 36}, "Save", [] {
    if (actions::saveEdit(S.editEntry)) S.screen = S.editReturn;
  }, changed ? cKcal : 0, changed);
}

void drawDiagnostics() {
  header("Settings (test build)");
  canvas.setFont(&fonts::FreeSans9pt7b);
  canvas.setTextColor(cText);
  canvas.setTextDatum(top_left);
  int64_t now = 0;
  std::string clock = "not set", day = "-";
  if (app::nowLocal(now)) {
    ft::CivilTime c = ft::fromLocalSeconds(now);
    char buf[32];
    snprintf(buf, sizeof(buf), "%04ld-%02u-%02u %02u:%02u:%02u", (long)c.year, c.month, c.day, c.hour, c.minute,
             c.second);
    clock = buf;
    day = std::to_string(ft::trackingDay(now));
  }
  bool charging = M5.Power.isCharging() == m5::Power_Class::is_charging;
  std::string lines[] = {
      "Clock: " + clock + "  Day: " + day,
      "Battery: " + std::to_string(M5.Power.getBatteryLevel()) + "%  " +
          std::to_string(M5.Power.getBatteryVoltage()) + " mV" + (charging ? "  charging" : ""),
      "Flash: " + std::to_string(storage::usedBytes() / 1024) + "/" + std::to_string(storage::totalBytes() / 1024) +
          " KB  Foods: " + std::to_string(foods::db().all().size()),
      "Entries: " + std::to_string(S.ledger.size()) + "  Skipped: " + std::to_string(S.journalBadLines) +
          "  Saves: " + std::to_string(S.settings.saveCounter),
      "Last zone: " + std::string(S.lastZoneEvent.c_str()) + "   FW " + app::kFirmwareVersion,
  };
  int y = 34;
  for (const std::string& l : lines) {
    canvas.drawString(fit(l, 308).c_str(), 8, y);
    y += 20;
  }
  button({4, 140, 100, 36}, "Buzz: eat", [] { S.vibe.start(ft::VibePattern::Eating, millis()); });
  button({110, 140, 100, 36}, "Buzz: water", [] { S.vibe.start(ft::VibePattern::Water, millis()); });
  button({216, 140, 100, 36}, "Buzz: both", [] { S.vibe.start(ft::VibePattern::Combined, millis()); });
  button({4, 180, 150, 36}, "Reminder 30s", [] {
    S.testReminderAtMs = millis() + 30000;
    S.screen = Screen::Home;
    app::showToast("Reminder in 30 s. Let the screen sleep.", 3000);
  });
  button({166, 180, 150, 36}, "Save test", [] {
    S.settings.saveCounter++;
    app::showToast(storage::saveSettings(S.settings)
                       ? String(("Settings saved (" + std::to_string(S.settings.saveCounter) + ")").c_str())
                       : String("Settings save FAILED"));
  });
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
  button({16, 160, 140, 52}, "Sipped +2 oz", [] { actions::addWater(2); });
  button({164, 160, 140, 52}, "Dismiss", [] { goHome(); });
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
  cButtonHi = M5.Display.color888(0x34, 0x34, 0x34);
  cDone = M5.Display.color888(0xD8, 0xD8, 0xD8);  // neutral "complete", never red
  cWarn = M5.Display.color888(0xF0, 0xC0, 0x40);  // amber for device problems only
}

void requestRedraw() { redraw = true; }
bool redrawRequested() { return redraw; }

void draw() {
  redraw = false;
  if (!canvasOk) return;
  hits.clear();
  listView = {0, 0, 0, 0};
  listMax = 0;
  canvas.fillSprite(cBg);
  switch (S.screen) {
    case Screen::Home: drawHome(); break;
    case Screen::LogFood: drawLogFood(); break;
    case Screen::Browse: drawBrowse(); break;
    case Screen::Favorites: drawFavorites(); break;
    case Screen::DayLog: drawDayLog(); break;
    case Screen::PastDays: drawPastDays(); break;
    case Screen::EntryEdit: drawEntryEdit(); break;
    case Screen::Picker: drawPicker(); break;
    case Screen::ComboAdjust: drawComboAdjust(); break;
    case Screen::Custom: drawCustom(); break;
    case Screen::Diagnostics: drawDiagnostics(); break;
    case Screen::Reminder: drawReminder(); break;
  }
  drawZoneLabels();
  drawToast();
  canvas.pushSprite(0, 0);
}

static const Hit* findHit(int x, int y) {
  for (auto it = hits.rbegin(); it != hits.rend(); ++it)
    if (it->r.contains(x, y)) return &*it;
  return nullptr;
}

void tap(int x, int y) {
  const Hit* h = findHit(x, y);
  if (h && h->onTap) {
    auto fn = h->onTap;  // copy: the action may redraw and clear `hits`
    fn();
  }
  redraw = true;
}

void hold(int x, int y) {
  const Hit* h = findHit(x, y);
  if (h && h->onHold) {
    auto fn = h->onHold;
    fn();
    redraw = true;
  }
}

void drag(int dy) {
  if (listMax <= 0) return;
  S.listScroll -= dy;
  if (S.listScroll < 0) S.listScroll = 0;
  if (S.listScroll > listMax) S.listScroll = listMax;
  redraw = true;
}

void goHome() {
  S.screen = Screen::Home;
  S.listScroll = 0;
  redraw = true;
}

void openToday() {
  S.viewDay = 0;
  S.screen = Screen::DayLog;
  S.listScroll = 0;
  redraw = true;
}

void openLogFood() {
  S.screen = Screen::LogFood;
  S.listScroll = 0;
  redraw = true;
}

void onSleep() {
  if (S.screen == Screen::Picker || S.screen == Screen::ComboAdjust || S.screen == Screen::Custom ||
      S.screen == Screen::EntryEdit) {
    S.resumeScreen = S.screen;
    S.resumeUntilMs = millis() + 120000;
  } else {
    S.resumeScreen = Screen::Home;
  }
  S.screen = Screen::Home;
  S.listScroll = 0;
}

}  // namespace ui
