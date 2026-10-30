#include "food_store.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

#include <algorithm>

#include "core/journal.h"
#include "storage.h"

namespace foods {

namespace {

constexpr const char* kFoods = "/data/foods.json";
constexpr const char* kPrev = "/data/foods.prev.json";
constexpr const char* kStaging = "/data/foods.staging";
constexpr const char* kCustom = "/data/custom_foods.json";
constexpr const char* kState = "/data/food_state.json";

ft::FoodDb gDb;
bool gHasFoods = false;
String gMessage = "No foods loaded yet";
std::vector<std::string> gFavorites;
std::vector<std::string> gUnavailable;
std::vector<ft::FoodDef> gCustom;

// Staging upload
File gStage;
size_t gStageExpected = 0;
size_t gStageWritten = 0;
uint32_t gStageCrcExpected = 0;
uint32_t gStageCrc = 0;

bool parseFoodArray(JsonArrayConst arr, std::vector<ft::FoodDef>& out, bool custom) {
  for (JsonVariantConst item : arr) {
    JsonObjectConst o = item.as<JsonObjectConst>();
    ft::FoodDef f;
    f.id = (const char*)(o["id"] | "");
    f.name = (const char*)(o["n"] | "");
    if (f.id.empty() || f.name.empty()) return false;
    f.serving = (const char*)(o["s"] | "1 serving");
    f.kcal = o["c"] | 0.0f;
    f.protein = o["p"] | 0.0f;
    f.archived = (o["a"] | 0) != 0;
    f.custom = custom;
    for (JsonVariantConst t : o["t"].as<JsonArrayConst>()) f.tags |= ft::parseTag((const char*)(t | ""));
    for (JsonVariantConst ingV : o["i"].as<JsonArrayConst>()) {
      JsonArrayConst ing = ingV.as<JsonArrayConst>();
      std::string id = (const char*)(ing[0] | "");
      float q = ing[1] | 0.0f;
      if (id.empty() || q <= 0) return false;
      f.ingredients.emplace_back(id, q);
    }
    if (f.kcal < 0 || f.kcal > 5000 || f.protein < 0 || f.protein > 300) return false;
    out.push_back(f);
  }
  return true;
}

// Parses a foods file into `out`. Returns false with a reason on any problem.
bool parseFoodsFile(const char* path, std::vector<ft::FoodDef>& out, bool custom, String& err) {
  File f = LittleFS.open(path, FILE_READ);
  if (!f) {
    err = "missing";
    return false;
  }
  JsonDocument doc;
  DeserializationError e = deserializeJson(doc, f);
  f.close();
  if (e) {
    err = String("bad JSON: ") + e.c_str();
    return false;
  }
  if ((doc["v"] | 0) != 1) {
    err = "unsupported schema version";
    return false;
  }
  if (!parseFoodArray(doc["foods"].as<JsonArrayConst>(), out, custom)) {
    err = "invalid food entry";
    return false;
  }
  return true;
}

bool buildDb(const std::vector<ft::FoodDef>& base, ft::FoodDb& db, String& err) {
  db.clear();
  for (const ft::FoodDef& f : base) {
    if (db.find(f.id)) {
      err = String("duplicate id ") + f.id.c_str();
      return false;
    }
    db.add(f);
  }
  for (const ft::FoodDef& f : gCustom)
    if (!db.find(f.id)) db.add(f);  // after first sync the laptop copy wins
  std::vector<std::string> problems;
  if (!db.computeCombos(&problems)) {
    err = String("combo problem: ") + problems[0].c_str();
    return false;
  }
  return true;
}

void loadState() {
  gFavorites.clear();
  gUnavailable.clear();
  File f = LittleFS.open(kState, FILE_READ);
  if (!f) return;
  JsonDocument doc;
  if (!deserializeJson(doc, f)) {
    for (JsonVariantConst v : doc["fav"].as<JsonArrayConst>()) gFavorites.push_back((const char*)(v | ""));
    for (JsonVariantConst v : doc["na"].as<JsonArrayConst>()) gUnavailable.push_back((const char*)(v | ""));
  }
  f.close();
}

bool saveState() {
  JsonDocument doc;
  JsonArray fav = doc["fav"].to<JsonArray>();
  for (const auto& id : gFavorites) fav.add(id.c_str());
  JsonArray na = doc["na"].to<JsonArray>();
  for (const auto& id : gUnavailable) na.add(id.c_str());
  std::string out;
  serializeJson(doc, out);
  return storage::writeFileAtomic(kState, out);
}

void loadCustom() {
  gCustom.clear();
  String err;
  if (LittleFS.exists(kCustom)) parseFoodsFile(kCustom, gCustom, true, err);
}

bool saveCustomFile() {
  JsonDocument doc;
  doc["v"] = 1;
  JsonArray arr = doc["foods"].to<JsonArray>();
  for (const ft::FoodDef& f : gCustom) {
    JsonObject o = arr.add<JsonObject>();
    o["id"] = f.id.c_str();
    o["n"] = f.name.c_str();
    o["s"] = f.serving.c_str();
    o["c"] = f.kcal;
    o["p"] = f.protein;
    o["t"].to<JsonArray>();
  }
  std::string out;
  serializeJson(doc, out);
  return storage::writeFileAtomic(kCustom, out);
}

}  // namespace

bool load() {
  loadState();
  loadCustom();
  std::vector<ft::FoodDef> base;
  String err;
  gHasFoods = false;
  if (!LittleFS.exists(kFoods)) {
    gMessage = "No food list yet. Run: py ..\\sync\\m1_tool.py pushfoods";
    buildDb(base, gDb, err);  // custom foods still usable
    return false;
  }
  if (!parseFoodsFile(kFoods, base, false, err) || !buildDb(base, gDb, err)) {
    gMessage = "Food list unreadable (" + err + "). Push it again.";
    base.clear();
    String ignored;
    buildDb(base, gDb, ignored);
    return false;
  }
  gHasFoods = true;
  gMessage = "";
  return true;
}

bool hasFoods() { return gHasFoods || !gCustom.empty(); }
const String& loadMessage() { return gMessage; }
const ft::FoodDb& db() { return gDb; }

bool isFavorite(const std::string& id) {
  return std::find(gFavorites.begin(), gFavorites.end(), id) != gFavorites.end();
}

bool toggleFavorite(const std::string& id) {
  auto it = std::find(gFavorites.begin(), gFavorites.end(), id);
  bool now;
  if (it == gFavorites.end()) {
    gFavorites.push_back(id);
    now = true;
  } else {
    gFavorites.erase(it);
    now = false;
  }
  saveState();
  return now;
}

std::vector<const ft::FoodDef*> favorites() {
  std::vector<const ft::FoodDef*> out;
  for (const auto& id : gFavorites) {
    const ft::FoodDef* f = gDb.find(id);
    if (f && !f->archived) out.push_back(f);
  }
  return out;
}

std::string saveCustom(const String& name, float kcal, float protein, int64_t nowLocal) {
  ft::FoodDef f;
  f.id = "custom_" + std::to_string(static_cast<long long>(nowLocal));
  while (gDb.find(f.id)) f.id += "x";
  f.name = name.c_str();
  f.serving = "1 serving";
  f.kcal = kcal;
  f.protein = protein;
  f.custom = true;
  gCustom.push_back(f);
  if (!saveCustomFile()) {
    gCustom.pop_back();
    return "";
  }
  gDb.add(f);
  return f.id;
}

bool stageBegin(size_t size, uint32_t crc, String& err) {
  if (gStage) gStage.close();
  if (size == 0 || size > 512 * 1024) {
    err = "bad size";
    return false;
  }
  if (storage::totalBytes() - storage::usedBytes() < size * 2 + 16 * 1024) {
    err = "not enough free space";
    return false;
  }
  gStage = LittleFS.open(kStaging, FILE_WRITE);
  if (!gStage) {
    err = "cannot open staging file";
    return false;
  }
  gStageExpected = size;
  gStageWritten = 0;
  gStageCrcExpected = crc;
  gStageCrc = 0;
  return true;
}

bool stageChunk(const uint8_t* data, size_t len, size_t offset, uint32_t chunkCrc, String& err) {
  if (!gStage) {
    err = "no upload in progress";
    return false;
  }
  // A damaged or out-of-order chunk is refused before anything is written, so the laptop can resend it.
  if (ft::crc32(reinterpret_cast<const char*>(data), len) != chunkCrc) {
    err = "chunk damaged";
    return false;
  }
  if (offset != gStageWritten) {
    err = "chunk out of order";
    return false;
  }
  if (gStageWritten + len > gStageExpected) {
    err = "too much data";
    return false;
  }
  if (gStage.write(data, len) != len) {
    err = "flash write failed";
    return false;
  }
  gStageWritten += len;
  gStageCrc = ft::crc32(reinterpret_cast<const char*>(data), len, gStageCrc);
  return true;
}

bool stageCommit(String& err, int& foodCount) {
  if (!gStage) {
    err = "no upload in progress";
    return false;
  }
  gStage.flush();
  gStage.close();
  if (gStageWritten != gStageExpected || gStageCrc != gStageCrcExpected) {
    LittleFS.remove(kStaging);
    err = "checksum mismatch (got " + String((unsigned)gStageWritten) + " of " + String((unsigned)gStageExpected) +
          " bytes); nothing changed";
    return false;
  }
  // Validate before swapping anything.
  std::vector<ft::FoodDef> base;
  ft::FoodDb test;
  if (!parseFoodsFile(kStaging, base, false, err) || !buildDb(base, test, err)) {
    LittleFS.remove(kStaging);
    err = "rejected: " + err + "; nothing changed";
    return false;
  }
  if (LittleFS.exists(kFoods)) {
    if (LittleFS.exists(kPrev)) LittleFS.remove(kPrev);
    LittleFS.rename(kFoods, kPrev);
  }
  if (!LittleFS.rename(kStaging, kFoods)) {
    if (LittleFS.exists(kPrev)) LittleFS.rename(kPrev, kFoods);  // put the old one back
    err = "swap failed; previous list kept";
    return false;
  }
  load();
  foodCount = static_cast<int>(base.size());
  return true;
}

}  // namespace foods
