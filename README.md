# Core2 Food Tracker

A pocket M5Stack Core2 (v1.3) that tracks calories, protein, and water, with fast logging and gentle
reminders. The implementation guide (Claude Docs) is the single source of truth for behavior.

**Status:** Milestone 1, hardware proof of concept.

| Folder | What's in it |
| --- | --- |
| `firmware/` | PlatformIO project. `src/core/` is hardware-free logic (tested on the laptop); the rest talks to the device. |
| `sync/` | `foods.py` validates and compiles the food database; `m1_tool.py` sets the clock and reads stats over USB. |
| `data/foods.csv` | The food database you edit in a spreadsheet (48 starter foods). |
| `tests/` | Python tests (`python -m pytest tests`). C++ tests live in `firmware/test/`. |
| `docs/` | `SETUP.md` (Windows + VS Code), `M1_CHECKLIST.md` (what to test on the device). |

Start with `docs/SETUP.md`.

## Milestone 1 scope

Display and touch zones, vibration patterns, screen sleep/wake with auto-lock, a test reminder that wakes the
screen, an append-only journal on flash with checksums, atomic settings writes, battery display and low-battery
shutdown, RTC clock and the 3 AM tracking day, and USB commands `hello`, `time_get`, `time_set`, `stats`.
