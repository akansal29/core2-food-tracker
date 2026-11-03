// Hardware-free reminder scheduler (guide Section 2). All times are local seconds.
//
// Windows are local times of day and may cross midnight (11:00 AM to 1:00 AM).
// They are measured from the 3 AM tracking-day start, so a window must not span 3 AM.
#pragma once
#include <stdint.h>

namespace ft {

constexpr int64_t kForever = INT64_C(4000000000000);  // "until I turn it off"

struct Window {
  int startMin;  // minutes after local midnight
  int endMin;
};

struct ReminderConfig {
  Window eat{11 * 60, 30};      // 11:00 AM - 12:30 AM
  int eatIntervalMin = 150;     // about every 2.5 h
  Window water{11 * 60, 60};    // 11:00 AM - 1:00 AM
  int waterIntervalMin = 20;
  bool eatingOn = true;
  bool waterOn = true;
  int eatSnoozeMin = 15;
  int waterSnoozeMin = 20;
  int eatRepeatMin = 15;        // an ignored eating reminder repeats once
  int combineMin = 5;           // due within 5 min of each other -> one combined alert
};

enum class Due : uint8_t { None = 0, Eat = 1, Water = 2, Both = 3 };

struct ReminderInputs {
  int64_t now = 0;
  float kcal = 0;
  float kcalTarget = 3000;
  float waterOz = 0;
  float waterTargetOz = 40;
};

bool inWindow(const Window& w, int64_t ts);
// Earliest time >= ts that is inside the window.
int64_t clampToWindow(const Window& w, int64_t ts);
// 0..1 progress through today's window (0 before it starts, 1 after it ends).
float windowFraction(const Window& w, int64_t ts);
// Calories meaningfully behind an even pace across the window (guide Section 6).
bool behindPace(const Window& w, int64_t ts, float kcal, float target);

class ReminderScheduler {
 public:
  // Called at boot and whenever the config changes. lastFoodTs = 0 if nothing logged.
  void reset(const ReminderConfig& cfg, int64_t now, int64_t lastFoodTs);

  // Returns what to show right now. After a non-None result the scheduler waits
  // for one of the action calls below before anything else becomes due.
  Due check(const ReminderConfig& cfg, const ReminderInputs& in);

  // Eating reminder actions
  void foodLogged(const ReminderConfig& cfg, int64_t ts);  // any food log, from anywhere
  void snoozeEat(const ReminderConfig& cfg, int64_t now);
  void skipEat(const ReminderConfig& cfg, int64_t now);
  void ignoreEat(const ReminderConfig& cfg, int64_t now);  // overlay closed without an answer

  // Water reminder actions (water reminders never repeat)
  void waterLogged(const ReminderConfig& cfg, int64_t now);  // any water log, from anywhere
  void snoozeWater(const ReminderConfig& cfg, int64_t now);
  void ignoreWater(const ReminderConfig& cfg, int64_t now);

  // Do Not Disturb: nothing fires until `until` (kForever = until turned off; 0 = off).
  void setDnd(int64_t until) { dndUntil_ = until; }
  int64_t dndUntil() const { return dndUntil_; }
  bool dndActive(int64_t now) const { return dndUntil_ > now; }

  bool eatPending() const { return eatPending_; }
  bool waterPending() const { return waterPending_; }
  int64_t nextEat() const { return nextEat_; }
  int64_t nextWater() const { return nextWater_; }
  bool lastWasBehindNudge() const { return behindNudge_; }

 private:
  int64_t nextEat_ = 0;
  int64_t nextWater_ = 0;
  bool eatPending_ = false;
  bool waterPending_ = false;
  bool repeatUsed_ = false;
  bool behindNudge_ = false;
  uint32_t nudgeDay_ = 0;
  int64_t dndUntil_ = 0;
};

}  // namespace ft
