// Food database on the device (guide Sections 3 and 9).
//   /data/foods.json        laptop-owned definitions, pushed over USB
//   /data/foods.prev.json   previous valid copy, kept on every push
//   /data/custom_foods.json custom foods saved on the device
//   /data/food_state.json   device-owned state: favorites, availability
#pragma once
#include <Arduino.h>

#include <string>
#include <vector>

#include "core/nutrition.h"

namespace foods {

bool load();                 // (re)loads everything; false if foods.json is missing or invalid
bool hasFoods();
const String& loadMessage(); // why there are no foods, for the Log Food screen
const ft::FoodDb& db();

bool isFavorite(const std::string& id);
bool toggleFavorite(const std::string& id);  // returns the new state; persists immediately
std::vector<const ft::FoodDef*> favorites(); // in the order they were starred

// Saves a custom food; returns its new id ("" on failure).
std::string saveCustom(const String& name, float kcal, float protein, int64_t nowLocal);

// USB push of foods.json: staged, CRC-checked, validated, then swapped in atomically.
bool stageBegin(size_t size, uint32_t crc, String& err);
bool stageChunk(const uint8_t* data, size_t len, size_t offset, uint32_t chunkCrc, String& err);
bool stageCommit(String& err, int& foodCount);

}  // namespace foods
