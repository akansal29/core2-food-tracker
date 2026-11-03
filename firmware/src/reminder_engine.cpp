#include "reminder_engine.h"

#include <algorithm>
#include <vector>

#include "app.h"
#include "core/format.h"
#include "core/timeutil.h"
#include "food_store.h"
#include "serial_cmd.h"
#include "storage.h"

using app::S;

namespace remind {

namespace {

ft::ReminderScheduler sched;
bool started = false;
uint32_t messageCounter = 0;
uint32_t suggestionCounter = 0;
std::string currentSuggestion;

ft::ReminderConfig config() {
  ft::ReminderConfig c;
  const app::Settings& st = S.settings;
  c.eatingOn = st.eatingReminders;
  c.waterOn = st.waterReminders;
  c.eat = {st.eatStartMin, st.eatEndMin};
  c.eatIntervalMin = st.eatIntervalMin;
  c.water = {st.waterStartMin, st.waterEndMin};
  c.waterIntervalMin = st.waterIntervalMin;
  return c;
}

bool now(int64_t& t) { return app::nowLocal(t); }

int64_t lastFoodTs() {
  int64_t last = 0;
  for (const ft::Entry& e : S.ledger.all())
    if (e.kind != ft::EntryKind::Water && e.ts > last) last = e.ts;
  return last;
}

bool ensureStarted() {
  if (started) return true;
  int64_t t;
  if (!now(t)) return false;
  sched.reset(config(), t, lastFoodTs());
  sched.setDnd(S.settings.dndUntil);
  started = true;
  return true;
}

void reschedule() {
  int64_t t;
  if (!now(t)) return;
  sched.reset(config(), t, lastFoodTs());
  sched.setDnd(S.settings.dndUntil);
  started = true;
}

std::string timeText(int64_t ts) {
  ft::CivilTime c = ft::fromLocalSeconds(ts);
  return ft::clock12(c.hour, c.minute);
}

// Favorites first; otherwise easy, calorie-dense snacks. Behind pace prefers calorie-dense.
void pickSuggestion(bool behind) {
  std::vector<const ft::FoodDef*> pool;
  for (const ft::FoodDef* f : foods::favorites())
    if (!f->archived) pool.push_back(f);
  if (pool.empty()) {
    for (const ft::FoodDef& f : foods::db().all()) {
      if (f.archived || (f.tags & ft::kMeal)) continue;
      if (f.tags & (ft::kCalorieDense | ft::kPortable | ft::kNoPrep)) pool.push_back(&f);
    }
  }
  if (behind) {
    std::vector<const ft::FoodDef*> dense;
    for (const ft::FoodDef* f : pool)
      if (f->tags & ft::kCalorieDense) dense.push_back(f);
    if (!dense.empty()) pool = dense;
  }
  if (pool.empty()) {
    currentSuggestion.clear();
    return;
  }
  const ft::FoodDef* pick = pool[suggestionCounter++ % pool.size()];
  if (pool.size() > 1 && pick->id == currentSuggestion) pick = pool[suggestionCounter++ % pool.size()];
  currentSuggestion = pick->id;
}

}  // namespace

ft::Due poll() {
  if (!ensureStarted()) return ft::Due::None;
  if (S.screen == app::Screen::Reminder || serial_cmd::isSyncing()) return ft::Due::None;
  // Don't interrupt someone mid-task: wait until they pause or return Home.
  if (S.screenOn && S.screen != app::Screen::Home && millis() - S.lastInteractionMs < 20000) return ft::Due::None;

  int64_t t;
  if (!now(t)) return ft::Due::None;
  ft::Totals tot = S.ledger.totalsFor(ft::trackingDay(t));
  ft::ReminderInputs in;
  in.now = t;
  in.kcal = tot.kcal;
  in.kcalTarget = S.settings.kcalTarget;
  in.waterOz = tot.waterOz;
  in.waterTargetOz = S.settings.waterTargetOz;
  ft::Due d = sched.check(config(), in);
  if (d == ft::Due::Eat || d == ft::Due::Both) pickSuggestion(sched.lastWasBehindNudge());
  if (d != ft::Due::None) ++messageCounter;
  // DND that has run out is cleared so it doesn't show after a restart.
  if (S.settings.dndUntil && !sched.dndActive(t)) {
    S.settings.dndUntil = 0;
    storage::saveSettings(S.settings);
  }
  return d;
}

void foodLogged(int64_t ts) {
  if (ensureStarted()) sched.foodLogged(config(), ts);
}

void waterLogged() {
  int64_t t;
  if (ensureStarted() && now(t)) sched.waterLogged(config(), t);
}

void eatSnooze() {
  int64_t t;
  if (sched.eatPending() && now(t)) sched.snoozeEat(config(), t);
}

void eatSkip() {
  int64_t t;
  if (sched.eatPending() && now(t)) sched.skipEat(config(), t);
}

void waterSnooze() {
  int64_t t;
  if (sched.waterPending() && now(t)) sched.snoozeWater(config(), t);
}

void silenceHour() {
  int64_t t;
  if (!now(t)) return;
  if (sched.waterPending()) sched.ignoreWater(config(), t);
  if (sched.eatPending()) sched.snoozeEat(config(), t);
  setDnd(t + 3600);
}

void leaveOverlay() {
  int64_t t;
  if (!now(t)) return;
  if (sched.eatPending()) sched.ignoreEat(config(), t);
  if (sched.waterPending()) sched.ignoreWater(config(), t);
}

void setDnd(int64_t until) {
  S.settings.dndUntil = until;
  sched.setDnd(until);
  storage::saveSettings(S.settings);
}

bool dndActive() {
  int64_t t;
  return now(t) && S.settings.dndUntil > t;
}

std::string dndText() {
  if (!dndActive()) return "";
  if (S.settings.dndUntil >= ft::kForever) return "Do not disturb: on until you turn it off";
  return "Do not disturb until " + timeText(S.settings.dndUntil);
}

void setEatingOn(bool on) {
  S.settings.eatingReminders = on;
  storage::saveSettings(S.settings);
  reschedule();
}

void setWaterOn(bool on) {
  S.settings.waterReminders = on;
  storage::saveSettings(S.settings);
  reschedule();
}

std::string nextText() {
  if (!ensureStarted()) return "";
  std::string d = dndText();
  if (!d.empty()) return d;
  std::string out;
  if (S.settings.eatingReminders) out = "Next: snack " + timeText(sched.nextEat());
  if (S.settings.waterReminders) {
    int64_t t;
    bool goalMet = now(t) && S.ledger.totalsFor(ft::trackingDay(t)).waterOz >= S.settings.waterTargetOz;
    std::string w = goalMet ? "water goal met" : "water " + timeText(sched.nextWater());
    out = out.empty() ? "Next: " + w : out + " | " + w;
  }
  return out.empty() ? "Reminders are off" : out;
}

const char* message(ft::Due d) {
  static const char* kEat[] = {"Quick snack break?", "A little fuel goes a long way.",
                               "Got a minute to eat something?", "Time for a small bite.",
                               "Want to grab a snack?"};
  static const char* kWater[] = {"Take a little sip.", "Quick water check.", "Sip break?"};
  if (d == ft::Due::Water) return kWater[messageCounter % 3];
  if (d == ft::Due::Both) return "Snack and sip time?";
  return kEat[messageCounter % 5];
}

const std::string& suggestion() { return currentSuggestion; }
void nextSuggestion() { pickSuggestion(false); }

void clockChanged() { reschedule(); }

bool eatPending() { return sched.eatPending(); }
bool waterPending() { return sched.waterPending(); }

}  // namespace remind
