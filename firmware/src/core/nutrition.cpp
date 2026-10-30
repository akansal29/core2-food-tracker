#include "nutrition.h"

#include <math.h>
#include <stdio.h>

#include <algorithm>

namespace ft {

uint16_t parseTag(const std::string& n) {
  static const struct { const char* name; uint16_t bit; } kTags[] = {
      {"portable", kPortable},         {"no-prep", kNoPrep},
      {"calorie-dense", kCalorieDense}, {"drinkable", kDrinkable},
      {"refrigeration-required", kRefrigeration},
      {"sweet", kSweet},               {"savory", kSavory},
      {"small-portion", kSmallPortion}, {"meal", kMeal},
      {"protein-rich", kProteinRich},  {"combo", kComboTag},
  };
  for (const auto& t : kTags)
    if (n == t.name) return t.bit;
  return 0;
}

Category categoryOf(const FoodDef& f) {
  if (f.isCombo() || (f.tags & (kMeal | kComboTag))) return Category::Meals;
  if (f.tags & kDrinkable) return Category::Drinks;
  return Category::Snacks;
}

const FoodDef* FoodDb::find(const std::string& id) const {
  for (const FoodDef& f : foods_)
    if (f.id == id) return &f;
  return nullptr;
}

bool FoodDb::computeCombos(std::vector<std::string>* problems) {
  bool ok = true;
  for (FoodDef& f : foods_) {
    if (!f.isCombo()) continue;
    float kcal = 0, protein = 0;
    bool bad = false;
    for (const auto& ing : f.ingredients) {
      const FoodDef* g = find(ing.first);
      if (!g || g->isCombo() || (g->archived && !f.archived)) {
        bad = true;
        if (problems) problems->push_back(f.id + ": bad ingredient " + ing.first);
        continue;
      }
      kcal += g->kcal * ing.second;
      protein += g->protein * ing.second;
    }
    if (bad) ok = false;
    f.kcal = kcal;
    f.protein = protein;
  }
  return ok;
}

std::vector<const FoodDef*> FoodDb::inCategory(Category c) const {
  std::vector<const FoodDef*> out;
  for (const FoodDef& f : foods_)
    if (!f.archived && categoryOf(f) == c) out.push_back(&f);
  std::sort(out.begin(), out.end(), [](const FoodDef* a, const FoodDef* b) {
    std::string x = a->name, y = b->name;
    std::transform(x.begin(), x.end(), x.begin(), ::tolower);
    std::transform(y.begin(), y.end(), y.begin(), ::tolower);
    return x < y;
  });
  return out;
}

float snapServings(float s) {
  float q = roundf(s * 4.0f) / 4.0f;
  if (q < 0.25f) q = 0.25f;
  if (q > 10.0f) q = 10.0f;
  return q;
}

std::string servingsLabel(float s) {
  char buf[24];
  float q = snapServings(s);
  if (fabsf(q - roundf(q)) < 0.001f) snprintf(buf, sizeof(buf), "%d", (int)roundf(q));
  else if (fabsf(q * 2 - roundf(q * 2)) < 0.001f) snprintf(buf, sizeof(buf), "%.1f", (double)q);
  else snprintf(buf, sizeof(buf), "%.2f", (double)q);
  return std::string(buf) + (fabsf(q - 1.0f) < 0.001f ? " serving" : " servings");
}

Amount scale(const FoodDef& f, float servings) {
  return {f.kcal * servings, f.protein * servings};
}

Amount ingredientsTotal(const FoodDb& db, const Ingredients& ingr) {
  Amount a;
  for (const auto& ing : ingr) {
    const FoodDef* g = db.find(ing.first);
    if (!g) continue;
    a.kcal += g->kcal * ing.second;
    a.protein += g->protein * ing.second;
  }
  return a;
}

std::vector<std::string> recentFoodIds(const Ledger& ledger, const FoodDb& db, size_t max) {
  std::vector<const Entry*> entries;
  for (const Entry& e : ledger.all())
    if (e.kind != EntryKind::Water && !e.foodId.empty()) entries.push_back(&e);
  std::sort(entries.begin(), entries.end(), [](const Entry* a, const Entry* b) { return a->ts > b->ts; });
  std::vector<std::string> out;
  for (const Entry* e : entries) {
    if (out.size() >= max) break;
    if (std::find(out.begin(), out.end(), e->foodId) != out.end()) continue;
    const FoodDef* f = db.find(e->foodId);
    if (!f || f->archived) continue;
    out.push_back(e->foodId);
  }
  return out;
}

}  // namespace ft
