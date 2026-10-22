#include "serial_cmd.h"

#include <ArduinoJson.h>
#include <M5Unified.h>

#include "app.h"
#include "core/timeutil.h"
#include "rtc.h"
#include "storage.h"

using app::S;

namespace serial_cmd {

namespace {

char buf[512];
size_t len = 0;
bool overflow = false;

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
    out["battery"] = M5.Power.getBatteryLevel();
    out["battery_mv"] = M5.Power.getBatteryVoltage();
    out["uptime_s"] = (millis() - S.bootMs) / 1000;
    out["saves"] = S.settings.saveCounter;
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
}

}  // namespace serial_cmd
