// Hardware-free food model and nutrition math (guide Sections 3 and 4).
#pragma once
#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "journal.h"

namespace ft {

enum Tag : uint16_t {
  kPortable = 1 << 0,
  kNoPrep = 1 << 1,
  kCalorieDense = 1 << 2,
  kDrinkable = 1 << 3,
  kRefrigeration = 1 << 4,
  kSweet = 1 << 5,
  kSavory = 1 << 6,
  kSmallPortion = 1 << 7,
  kMeal = 1 << 8,
  kProteinRich = 1 << 9,
  kComboTag = 1 << 10,
};

uint16_t parseTag(const std::string& name);  // 0 if unknown

using Ingredients = std::vector<std::pair<std::string, float>>;  // food_id, servings

struct FoodDef {
  std::string id;
  std::string name;
  std::string serving;  // e.g. "1 bar"
  float kcal = 0;       // per serving (combos: computed from ingredients)
  float protein = 0;
  uint16_t tags = 0;
  bool archived = false;
  bool custom = false;  // created on the device
  Ingredients ingredients;
  bool isCombo() const { return !ingredients.empty(); }
};

enum class Category : uint8_t { Snacks = 0, Drinks = 1, Meals = 2 };
Category categoryOf(const FoodDef& f);

struct Amount {
  float kcal = 0;
  float protein = 0;
};

class FoodDb {
 public:
  void clear() { foods_.clear(); }
  void add(const FoodDef& f) { foods_.push_back(f); }
  const FoodDef* find(const std::string& id) const;
  const std::vector<FoodDef>& all() const { return foods_; }
  // Fills each combo's kcal/protein from its ingredients. Returns false and
  // lists problems (missing, archived, or nested ingredients) if any.
  bool computeCombos(std::vector<std::string>* problems = nullptr);
  // Visible (non-archived) foods in a category, sorted by name.
  std::vector<const FoodDef*> inCategory(Category c) const;

 private:
  std::vector<FoodDef> foods_;
};

// Rounds to the nearest 0.25 and clamps to 0.25..10.
float snapServings(float s);
// "1 serving", "0.5 servings", "1.25 servings"
std::string servingsLabel(float s);
// Nutrition for an amount of a plain food or a combo (per-serving values scaled).
Amount scale(const FoodDef& f, float servings);
// Nutrition for a combo with per-entry ingredient amounts.
Amount ingredientsTotal(const FoodDb& db, const Ingredients& ingr);

// Most recently logged distinct food ids (newest first), skipping water,
// unsaved custom entries, and ids no longer in the database or archived.
std::vector<std::string> recentFoodIds(const Ledger& ledger, const FoodDb& db, size_t max);

}  // namespace ft
