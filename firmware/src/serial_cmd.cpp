#include "serial_cmd.h"

#include <ArduinoJson.h>
#include <M5Unified.h>
#include <mbedtls/base64.h>

#include "app.h"
#include "core/timeutil.h"
#include "food_store.h"
#include "rtc.h"
#include "storage.h"
#include "ui.h"

using app::S;

namespace serial_cmd {

namespace {

char buf[1200];
size_t len = 0;
bool overflow = false;
bool syncing = false;
uint32_t lastSyncMsgMs = 0;

void reply(JsonDocument& doc) {
  serializeJson(doc, Serial);
  Serial.print('\n');
}

void error(const char* msg) {
  JsonDocument doc;
  doc["ok"] = false;
  doc["error"] = msg;
  reply(doc);
}

void handle(const char* line) {
  JsonDocument in;
  if (deserializeJson(in, line)) return error("bad json");
  const char* cmd = in["cmd"] | "";
  JsonDocument out;
  out["ok"] = true;
  out["cmd"] = cmd;

  if (!strcmp(cmd, "hello")) {
    out["fw"] = app::kFirmwareVersion;
    out["schema"] = app::kSchemaVersion;
    out["board"] = "m5stack-core2";
  } else if (!strcmp(cmd, "time_get")) {
    int64_t now;
    bool valid = app::nowLocal(now);
    if (valid) {
      ft::CivilTime t = ft::fromLocalSeconds(now);
      char ts[24];
      snprintf(ts, sizeof(ts), "%04ld-%02u-%02uT%02u:%02u:%02u", (long)t.year, t.month, t.day, t.hour, t.minute,
               t.second);
      out["t"] = ts;
    }
    out["valid"] = valid;
  } else if (!strcmp(cmd, "time_set")) {
    const char* ts = in["t"] | "";
    int y, mo, d, h, mi, s;
    if (sscanf(ts, "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &s) != 6) return error("t must be YYYY-MM-DDTHH:MM:SS");
    ft::CivilTime t{y, (uint8_t)mo, (uint8_t)d, (uint8_t)h, (uint8_t)mi, (uint8_t)s};
    if (!ft::isPlausible(t)) return error("implausible time");
    if (!rtc::set(t)) return error("rtc write failed");
    out["t"] = ts;
  } else if (!strcmp(cmd, "stats")) {
    out["entries"] = (unsigned)S.ledger.size();
    out["bad_lines"] = S.journalBadLines;
    out["journal_bytes"] = (unsigned)storage::journalBytes();
    out["flash_used"] = (unsigned)storage::usedBytes();
    out["flash_total"] = (unsigned)storage::totalBytes();
    out["foods"] = (unsigned)foods::db().all().size();
    out["battery"] = M5.Power.getBatteryLevel();
    out["battery_mv"] = M5.Power.getBatteryVoltage();
    out["uptime_s"] = (millis() - S.bootMs) / 1000;
    out["saves"] = S.settings.saveCounter;
  } else if (!strcmp(cmd, "put_begin")) {
    const char* path = in["path"] | "";
    if (strcmp(path, "foods.json") != 0) return error("only foods.json can be pushed");
    String err;
    if (!foods::stageBegin(in["size"] | 0u, in["crc"] | 0u, err)) return error(err.c_str());
    syncing = true;
    lastSyncMsgMs = millis();
    app::showToast("Syncing food list...", 60000);
  } else if (!strcmp(cmd, "put_chunk")) {
    const char* b64 = in["d"] | "";
    uint8_t data[768];
    size_t n = 0;
    if (mbedtls_base64_decode(data, sizeof(data), &n, reinterpret_cast<const unsigned char*>(b64), strlen(b64)) != 0)
      return error("bad base64");
    String err;
    if (!foods::stageChunk(data, n, in["o"] | 0u, in["c"] | 0u, err)) return error(err.c_str());
    lastSyncMsgMs = millis();
    out["n"] = (unsigned)n;
  } else if (!strcmp(cmd, "put_commit")) {
    String err;
    int count = 0;
    syncing = false;
    if (!foods::stageCommit(err, count)) {
      app::showToast("Food list rejected; old list kept", 4000);
      return error(err.c_str());
    }
    out["foods"] = count;
    app::showToast(("Food list updated: " + std::to_string(count) + " foods").c_str(), 4000);
    ui::requestRedraw();
  } else {
    return error("unknown cmd");
  }
  reply(out);
}

}  // namespace

void poll() {
  while (Serial.available()) {
    char c = static_cast<char>(Serial.read());
    if (c == '\r') continue;
    if (c == '\n') {
      buf[len] = '\0';
      if (overflow) error("line too long");
      else if (len > 0) handle(buf);
      len = 0;
      overflow = false;
    } else if (len < sizeof(buf) - 1) {
      buf[len++] = c;
    } else {
      overflow = true;
    }
  }
  // An abandoned upload (cable pulled) stops showing "Syncing" after 10 s.
  if (syncing && millis() - lastSyncMsgMs > 10000) {
    syncing = false;
    app::showToast("Sync interrupted; old list kept", 3000);
  }
}

bool isSyncing() { return syncing; }

}  // namespace serial_cmd
