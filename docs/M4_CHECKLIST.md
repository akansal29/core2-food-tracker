# Milestone 4 checklist: reminders and Do Not Disturb

## Setup

1. Upload the new firmware (✓ Build, → Upload). Entries, foods, and favorites are kept.
2. **Set the clock** (daylight saving time ended Nov 1; the Core2 doesn't adjust itself):
   ```
   py ..\sync\m1_tool.py settime
   ```
   Do this again after every daylight-saving change.

Defaults: eating reminders **11:00 AM–12:30 AM, about every 2.5 h**; water sips **11:00 AM–1:00 AM, every 20 min**
until you reach 40 oz. Reminders never fire between 1 AM and 11 AM.

## A. Quick test without waiting

| # | Do this | Expect | Result |
| --- | --- | --- | --- |
| A1 | Settings → Hardware test → **Reminder 30s**, let the screen sleep | Wakes with "Snack and sip time?", long buzz then two short pulses | |
| A2 | On that screen, check the layout | Snack idea + Next, Log food / Snooze 15 / Skip, Sipped +2 oz / +8 oz, Silence 1 h / Dismiss | |
| A3 | Tap **Sipped +2 oz** (while still locked) | Water +2 oz; the screen stays for the snack half | |
| A4 | Tap **Skip** | Back to Home | |

## B. Home and the Reminders screen

| # | Do this | Expect | Result |
| --- | --- | --- | --- |
| B1 | Look under the Water bar on Home | "Next: snack 4:30 PM \| water 3:50 PM" (real times) | |
| B2 | Tap that line (or Settings → Reminders & Do not disturb) | Reminders screen | |
| B3 | Turn **Water reminders** off, then on | Home's next line changes to match | |

## C. Real reminders (over an afternoon)

| # | Do this | Expect | Result |
| --- | --- | --- | --- |
| C1 | Wait for a water reminder | "Take a little sip." style message, two short pulses; Sipped +2 / +4 / +8 / +16, Snooze 20, Silence 1 h | |
| C2 | Ignore one (let the screen sleep) | It does **not** come back early; the next one is ~20 min later | |
| C3 | Wait for an eating reminder | One long buzz; friendly message; a snack idea from your favorites | |
| C4 | Ignore it | It repeats **once** ~15 min later, then waits the full 2.5 h | |
| C5 | Log a food normally (not from a reminder) | Next snack time on Home moves to ~2.5 h after that log | |
| C6 | Tap the snack idea in a reminder | Opens that food's serving picker (unlocked) | |
| C7 | Reach 40 oz of water | Water reminders stop; Home says "water goal met" | |
| C8 | When a reminder is due while you're in the middle of logging | It waits until you've paused ~20 s or gone back Home | |

## D. Do Not Disturb (exams)

| # | Do this | Expect | Result |
| --- | --- | --- | --- |
| D1 | Reminders → **1 h** | "Do not disturb until …" in amber; "DND" in the status bar | |
| D2 | Wait through a time a reminder would have fired | Nothing buzzes or lights up | |
| D3 | Restart the Core2 during DND | Still on afterward | |
| D4 | **Turn off Do not disturb** | Normal reminders resume without a pile of missed ones | |
| D5 | **Until off** | Stays on until you turn it off, even past 1 AM | |
| D6 | In any reminder, **Silence 1 h** | Same as 1 h DND | |

## E. Battery

Water reminders wake the screen up to 3 times an hour. Note the battery % at noon, 6 PM, and midnight on a
normal day, so we can tell whether reminders are costing too much.
