# Core2 Food Tracker

A pocket M5Stack Core2 (v1.3) that tracks calories, protein, and water, with fast logging and gentle
reminders. The implementation guide (Claude Docs) is the single source of truth for behavior.

**Status:** Milestone 4, reminders and Do Not Disturb. (Milestones 1–3 passed on the device.)

| Folder | What's in it |
| --- | --- |
| `firmware/` | PlatformIO project. `src/core/` is hardware-free logic (tested on the laptop); the rest talks to the device. |
| `sync/` | `foods.py` validates and compiles the food database; `m1_tool.py` sets the clock, pushes the food list (`pushfoods`), and reads stats over USB. |
| `data/foods.csv` | The food database you edit in a spreadsheet (48 starter foods). |
| `tests/` | Python tests (`python -m pytest tests`). C++ tests live in `firmware/test/`. |
| `docs/` | `SETUP.md` (Windows + VS Code), `M1`–`M4_CHECKLIST.md` (what to test on the device). |

Start with `docs/SETUP.md`.

## Milestone 1 scope

Display and touch zones, vibration patterns, screen sleep/wake with auto-lock, a test reminder that wakes the
screen, an append-only journal on flash with checksums, atomic settings writes, battery display and low-battery
shutdown, RTC clock and the 3 AM tracking day, and USB commands `hello`, `time_get`, `time_set`, `stats`.

## Milestone 2 scope

Food list pushed over USB (validated, checksummed, swapped in atomically with the previous copy kept), Log Food
screen with recents and a Favorites screen, Browse by Snacks / Drinks / Meals, serving picker (1/4 steps, live preview),
combos with per-entry ingredient adjustments, long-press instant logging, Undo / Log another banner, custom entry
with optional save as food, "Resume?" after the screen sleeps mid-entry, and scrolling lists.

## Milestone 3 scope

Today's Log (Today button or hold the middle circle) with totals, remaining amounts, and last-food time; tap any
entry to change servings or water amount, shift its time (moving it across the 3 AM boundary when needed, never into
the future), or delete it with a two-tap confirm. Past Days lists every logged day with totals and opens each day's
editable log. Edits rescale the entry's own logged snapshot, so food-list changes never rewrite history.

## Milestone 4 scope

Eating reminders (11 AM–12:30 AM, about every 2.5 h, pushed back by any food log, repeat once if ignored, one gentle
extra nudge per day when behind pace) and water sip reminders (11 AM–1 AM, every 20 min, never repeated, stop at the
daily goal). Alerts due within 5 minutes merge into one, each type has its own vibration pattern, and reminders wait
while you're mid-task. Do Not Disturb (1 h / 2 h / 3 h / until off) survives restarts and never replays missed
reminders. The scheduler is hardware-free and unit-tested, including windows that cross midnight.
