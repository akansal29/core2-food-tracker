#include "actions.h"

#include <math.h>

#include "app.h"
#include "core/format.h"
#include "core/timeutil.h"
#include "food_store.h"
#include "storage.h"

using app::S;

namespace actions {

namespace {

constexpr uint32_t kUndoWindowMs = 5000;

bool commitLine(const std::string& line) {
  if (!storage::appendJournal(line)) {
    S.storageOk = false;
    app::showToast("Couldn't save. Not logged.", 3000);
    return false;
  }
  S.storageOk = true;
  return S.ledger.applyLine(line);
}

String amountText(float kcal, float protein, float water) {
  if (water > 0) return "+" + String((int)lroundf(water)) + " oz water";
  String s = "+" + String(ft::withCommas(lroundf(kcal)).c_str()) + " kcal";
  if (protein > 0.05f) s += ", +" + String(protein, 1) + " g";
  return s;
}

bool commitEntry(ft::Entry e, bool isWater) {
  int64_t now;
  if (!app::nowLocal(now)) {
    app::showToast("Set the clock first", 2500);
    return false;
  }
  uint32_t seq = S.ledger.lastSeq() + 1;
  e.ts = now;
  e.id = ft::makeEntryId(now, seq);
  if (!commitLine(ft::encodeAdd(seq, e))) return false;
  S.undoId = e.id;
  S.undoUntilMs = millis() + kUndoWindowMs;
  S.bannerText = (isWater ? String("") : String("Logged! ")) + amountText(e.kcal, e.protein, e.waterOz);
  S.screen = app::Screen::Home;
  S.listScroll = 0;
  buzz(ft::VibePattern::Tick);
  return true;
}

}  // namespace

void buzz(ft::VibePattern p) {
  if (S.settings.vibration) S.vibe.start(p, millis());
}

bool logFood(const std::string& foodId, float servings) {
  const ft::FoodDef* f = foods::db().find(foodId);
  if (!f) return false;
  ft::Amount a = ft::scale(*f, servings);
  ft::Entry e;
  e.kind = f->isCombo() ? ft::EntryKind::Combo : ft::EntryKind::Food;
  e.foodId = f->id;
  e.name = f->name;
  e.servings = servings;
  e.kcal = a.kcal;
  e.protein = a.protein;
  return commitEntry(e, false);
}

bool logAdjustedCombo(const std::string& comboId) {
  const ft::FoodDef* f = foods::db().find(comboId);
  if (!f) return false;
  ft::Amount a = ft::ingredientsTotal(foods::db(), S.comboIngredients);
  ft::Entry e;
  e.kind = ft::EntryKind::Combo;
  e.foodId = f->id;
  e.name = f->name + " (adjusted)";
  e.servings = 1;
  e.kcal = a.kcal;
  e.protein = a.protein;
  return commitEntry(e, false);
}

bool logCustom(float kcal, float protein, bool saveAsFood) {
  int64_t now;
  if (!app::nowLocal(now)) {
    app::showToast("Set the clock first", 2500);
    return false;
  }
  ft::CivilTime t = ft::fromLocalSeconds(now);
  String name = String("Custom ") + ft::clock12(t.hour, t.minute).c_str();
  ft::Entry e;
  e.kind = ft::EntryKind::Custom;
  e.name = name.c_str();
  e.kcal = kcal;
  e.protein = protein;
  if (saveAsFood) {
    std::string id = foods::saveCustom(name, kcal, protein, now);
    if (id.empty()) app::showToast("Couldn't save as food; logging anyway", 2500);
    else e.foodId = id;
  }
  return commitEntry(e, false);
}

bool addWater(float oz) {
  ft::Entry e;
  e.kind = ft::EntryKind::Water;
  e.name = "Water";
  e.waterOz = oz;
  return commitEntry(e, true);
}

void undoLast() {
  if (S.undoId.empty() || millis() > S.undoUntilMs || !S.ledger.find(S.undoId)) {
    app::showToast("Nothing to undo");
    return;
  }
  if (commitLine(ft::encodeDelete(S.ledger.lastSeq() + 1, S.undoId))) app::showToast("Undone");
  S.undoId.clear();
  S.undoUntilMs = 0;
}

}  // namespace actions
