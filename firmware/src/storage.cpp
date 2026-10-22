#include "storage.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

namespace storage {

namespace {
constexpr const char* kJournal = "/data/journal.log";
constexpr const char* kSettings = "/data/settings.json";
}  // namespace

bool begin() {
  if (!LittleFS.begin(true /* format if mount fails (first boot) */)) return false;
  if (!LittleFS.exists("/data")) LittleFS.mkdir("/data");
  return LittleFS.exists("/data");
}

bool appendJournal(const std::string& line) {
  File f = LittleFS.open(kJournal, FILE_APPEND);
  if (!f) return false;
  size_t n = f.write(reinterpret_cast<const uint8_t*>(line.data()), line.size());
  n += f.write(static_cast<uint8_t>('\n'));
  f.flush();
  f.close();  // LittleFS commits metadata on close
  return n == line.size() + 1;
}

bool loadJournal(ft::Ledger& ledger, uint32_t& badLines) {
  badLines = 0;
  if (!LittleFS.exists(kJournal)) return true;  // nothing logged yet
  File f = LittleFS.open(kJournal, FILE_READ);
  if (!f) return false;
  std::string contents;
  contents.reserve(f.size());
  uint8_t buf[512];
  while (f.available()) {
    size_t n = f.read(buf, sizeof(buf));
    if (n == 0) break;
    contents.append(reinterpret_cast<const char*>(buf), n);
  }
  f.close();
  size_t bad = 0;
  ledger.replay(contents, &bad);
  badLines = static_cast<uint32_t>(bad);
  return true;
}

bool writeFileAtomic(const char* path, const std::string& contents) {
  String tmp = String(path) + ".tmp";
  File f = LittleFS.open(tmp, FILE_WRITE);
  if (!f) return false;
  size_t n = f.write(reinterpret_cast<const uint8_t*>(contents.data()), contents.size());
  f.flush();
  f.close();
  if (n != contents.size()) {
    LittleFS.remove(tmp);
    return false;
  }
  // LittleFS rename replaces the destination atomically.
  return LittleFS.rename(tmp, path);
}

bool saveSettings(const app::Settings& s) {
  JsonDocument doc;
  doc["v"] = app::kSchemaVersion;
  doc["kcal"] = s.kcalTarget;
  doc["protein"] = s.proteinTarget;
  doc["water"] = s.waterTargetOz;
  doc["water_long"] = s.waterLongTermOz;
  doc["bright"] = s.brightness;
  doc["timeout"] = s.screenTimeoutSec;
  doc["vibe"] = s.vibration;
  doc["saves"] = s.saveCounter;
  std::string out;
  serializeJson(doc, out);
  return writeFileAtomic(kSettings, out);
}

bool loadSettings(app::Settings& s) {
  // A leftover .tmp means a write was interrupted before rename: ignore it.
  if (LittleFS.exists(String(kSettings) + ".tmp")) LittleFS.remove(String(kSettings) + ".tmp");
  if (!LittleFS.exists(kSettings)) return saveSettings(s);  // first boot: write defaults
  File f = LittleFS.open(kSettings, FILE_READ);
  if (!f) return false;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;  // keep defaults; caller shows a warning
  s.kcalTarget = doc["kcal"] | s.kcalTarget;
  s.proteinTarget = doc["protein"] | s.proteinTarget;
  s.waterTargetOz = doc["water"] | s.waterTargetOz;
  s.waterLongTermOz = doc["water_long"] | s.waterLongTermOz;
  s.brightness = doc["bright"] | s.brightness;
  s.screenTimeoutSec = doc["timeout"] | s.screenTimeoutSec;
  s.vibration = doc["vibe"] | s.vibration;
  s.saveCounter = doc["saves"] | s.saveCounter;
  return true;
}

size_t usedBytes() { return LittleFS.usedBytes(); }
size_t totalBytes() { return LittleFS.totalBytes(); }

size_t journalBytes() {
  if (!LittleFS.exists(kJournal)) return 0;
  File f = LittleFS.open(kJournal, FILE_READ);
  size_t n = f ? f.size() : 0;
  if (f) f.close();
  return n;
}

}  // namespace storage
