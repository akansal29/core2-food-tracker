// Things that change data: logging, undo, water. Each one writes the journal
// line before updating memory and never reports success otherwise.
#pragma once
#include <Arduino.h>

#include <string>

#include "core/journal.h"
#include "core/vibe.h"

namespace actions {

void buzz(ft::VibePattern p);

// Logs `servings` of a database food (plain or combo, saved amounts).
bool logFood(const std::string& foodId, float servings);
// Logs a combo with per-entry ingredient amounts.
bool logAdjustedCombo(const std::string& comboId);
// Logs a custom entry; if saveAsFood, also adds a reusable food.
bool logCustom(float kcal, float protein, bool saveAsFood);
bool addWater(float oz);
void undoLast();

// Today's Log / Past Days corrections. Never trigger reminders (guide Section 4).
bool saveEdit(const ft::Entry& edited);
bool deleteEntry(const std::string& id);

}  // namespace actions
