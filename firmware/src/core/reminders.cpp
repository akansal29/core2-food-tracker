#include "reminders.h"

#include "timeutil.h"

namespace ft {

namespace {

constexpr int kDayStartMin = kDayResetHour * 60;  // 3:00 AM

// Minutes since the 3 AM tracking-day start, 0..1439.
int offsetMin(int minuteOfDay) { return ((minuteOfDay - kDayStartMin) % 1440 + 1440) % 1440; }

int minuteOfDay(int64_t ts) {
  CivilTime t = fromLocalSeconds(ts);
  return t.hour * 60 + t.minute;
}

// Window as [start, end) offsets from 3 AM; an end of 3 AM means the end of the tracking day.
void windowOffsets(const Window& w, int& s, int& e) {
  s = offsetMin(w.startMin);
  e = offsetMin(w.endMin);
  if (e <= s) e = 1440;
}

// Local seconds at the 3 AM start of the tracking day containing ts.
int64_t dayStart(int64_t ts) {
  CivilTime t = fromLocalSeconds(ts);
  int secsIntoDay = offsetMin(t.hour * 60 + t.minute) * 60 + t.second;
  return ts - secsIntoDay;
}

}  // namespace

bool inWindow(const Window& w, int64_t ts) {
  int s, e;
  windowOffsets(w, s, e);
  int o = offsetMin(minuteOfDay(ts));
  return o >= s && o < e;
}

int64_t clampToWindow(const Window& w, int64_t ts) {
  if (inWindow(w, ts)) return ts;
  int s, e;
  windowOffsets(w, s, e);
  int64_t candidate = dayStart(ts) + static_cast<int64_t>(s) * 60;
  return candidate >= ts ? candidate : candidate + 86400;
}

float windowFraction(const Window& w, int64_t ts) {
  int s, e;
  windowOffsets(w, s, e);
  CivilTime t = fromLocalSeconds(ts);
  float o = offsetMin(t.hour * 60 + t.minute) + t.second / 60.0f;
  if (o <= s) return 0;
  if (o >= e) return 1;
  return (o - s) / static_cast<float>(e - s);
}

bool behindPace(const Window& w, int64_t ts, float kcal, float target) {
  float expected = target * windowFraction(w, ts);
  return expected >= 0.15f * target && kcal < 0.8f * expected;
}

void ReminderScheduler::reset(const ReminderConfig& cfg, int64_t now, int64_t lastFoodTs) {
  int64_t eat = lastFoodTs > 0 ? lastFoodTs + cfg.eatIntervalMin * 60 : now + 30 * 60;
  if (eat < now + 5 * 60) eat = now + 5 * 60;  // a reboot never fires instantly
  nextEat_ = clampToWindow(cfg.eat, eat);
  nextWater_ = clampToWindow(cfg.water, now + cfg.waterIntervalMin * 60);
  eatPending_ = waterPending_ = repeatUsed_ = behindNudge_ = false;
}

Due ReminderScheduler::check(const ReminderConfig& cfg, const ReminderInputs& in) {
  const int64_t now = in.now;
  if (eatPending_ || waterPending_) return Due::None;  // waiting for an answer

  // During DND nothing fires and nothing piles up: anything due moves to after DND.
  if (dndActive(now)) {
    int64_t after = dndUntil_ >= kForever ? now : dndUntil_;
    if (nextEat_ <= now) nextEat_ = clampToWindow(cfg.eat, after + (dndUntil_ >= kForever ? cfg.eatIntervalMin * 60 : 10 * 60));
    if (nextWater_ <= now) nextWater_ = clampToWindow(cfg.water, after + cfg.waterIntervalMin * 60);
    return Due::None;
  }

  // Times that slipped outside their window (device off, window changed) move to the next window.
  if (nextEat_ <= now && !inWindow(cfg.eat, now)) nextEat_ = clampToWindow(cfg.eat, now);
  if (nextWater_ <= now && !inWindow(cfg.water, now)) nextWater_ = clampToWindow(cfg.water, now);

  // One gentle extra eating reminder per day when behind pace (never repeated buzzing).
  behindNudge_ = false;
  uint32_t today = trackingDay(now);
  if (cfg.eatingOn && nudgeDay_ != today && inWindow(cfg.eat, now) && windowFraction(cfg.eat, now) >= 0.4f &&
      nextEat_ - now > 30 * 60 && behindPace(cfg.eat, now, in.kcal, in.kcalTarget)) {
    nudgeDay_ = today;
    nextEat_ = now;
    behindNudge_ = true;
  }

  bool waterOpen = cfg.waterOn && in.waterOz < in.waterTargetOz && inWindow(cfg.water, now);
  if (cfg.waterOn && in.waterOz >= in.waterTargetOz && nextWater_ <= now)
    nextWater_ = clampToWindow(cfg.water, now + cfg.waterIntervalMin * 60);  // goal reached: stay quiet

  bool eatDue = cfg.eatingOn && nextEat_ <= now;
  bool waterDue = waterOpen && nextWater_ <= now;
  const int64_t soon = now + cfg.combineMin * 60;
  if (eatDue && !waterDue && waterOpen && nextWater_ <= soon) waterDue = true;
  if (waterDue && !eatDue && cfg.eatingOn && inWindow(cfg.eat, now) && nextEat_ <= soon) eatDue = true;

  eatPending_ = eatDue;
  waterPending_ = waterDue;
  if (eatDue && waterDue) return Due::Both;
  if (eatDue) return Due::Eat;
  if (waterDue) return Due::Water;
  return Due::None;
}

void ReminderScheduler::foodLogged(const ReminderConfig& cfg, int64_t ts) {
  nextEat_ = clampToWindow(cfg.eat, ts + cfg.eatIntervalMin * 60);
  eatPending_ = false;
  repeatUsed_ = false;
}

void ReminderScheduler::snoozeEat(const ReminderConfig& cfg, int64_t now) {
  nextEat_ = clampToWindow(cfg.eat, now + cfg.eatSnoozeMin * 60);
  eatPending_ = false;
}

void ReminderScheduler::skipEat(const ReminderConfig& cfg, int64_t now) {
  nextEat_ = clampToWindow(cfg.eat, now + cfg.eatIntervalMin * 60);
  eatPending_ = false;
  repeatUsed_ = false;
}

void ReminderScheduler::ignoreEat(const ReminderConfig& cfg, int64_t now) {
  if (!repeatUsed_) {
    nextEat_ = clampToWindow(cfg.eat, now + cfg.eatRepeatMin * 60);
    repeatUsed_ = true;
  } else {
    nextEat_ = clampToWindow(cfg.eat, now + cfg.eatIntervalMin * 60);
    repeatUsed_ = false;
  }
  eatPending_ = false;
}

void ReminderScheduler::waterLogged(const ReminderConfig& cfg, int64_t now) {
  nextWater_ = clampToWindow(cfg.water, now + cfg.waterIntervalMin * 60);
  waterPending_ = false;
}

void ReminderScheduler::snoozeWater(const ReminderConfig& cfg, int64_t now) {
  nextWater_ = clampToWindow(cfg.water, now + cfg.waterSnoozeMin * 60);
  waterPending_ = false;
}

void ReminderScheduler::ignoreWater(const ReminderConfig& cfg, int64_t now) {
  nextWater_ = clampToWindow(cfg.water, now + cfg.waterIntervalMin * 60);
  waterPending_ = false;
}

}  // namespace ft
