# Milestone 1 hardware checklist

Flash the firmware (docs/SETUP.md), set the clock, then work through this list. Mark each line and send me the
results, including anything that felt off. Numbers in brackets are what the guide expects.

The Home screen has test buttons in M1: **+100 kcal**, **+5 g**, **Undo**, **Test** (opens the hardware test
screen). They're replaced by Today · Ideas · Trends · Settings later.

## A. Display and touch

| # | Do this | Expect | Result |
| --- | --- | --- | --- |
| A1 | Look at Home | Status bar (time, battery %), three bars, test buttons, zone labels along the bottom | |
| A2 | Hold the device at arm's length | Numbers are readable | |
| A3 | Tap each test button | Correct toast; totals update | |

## B. Touch zones (the three circles under the screen)

| # | Do this | Expect | Result |
| --- | --- | --- | --- |
| B1 | Tap left circle (unlocked) | Toast "Food logging arrives in Milestone 2" | |
| B2 | Tap right circle (unlocked) | +8 oz water, toast with Undo | |
| B3 | Tap middle circle from the Test screen | Returns to Home | |
| B4 | Hold middle ~1 s (unlocked) | Toast "Today's Log arrives in Milestone 3" | |
| B5 | Test screen → "Last zone" line | Shows the last zone event correctly every time | |
| B6 | Tap each zone 10 times | Count any missed or wrong-zone taps | |

## C. Vibration (Test screen)

| # | Do this | Expect | Result |
| --- | --- | --- | --- |
| C1 | Buzz: eat | One long buzz [~600 ms] | |
| C2 | Buzz: water | Two short pulses | |
| C3 | Buzz: both | Long, pause, two short | |
| C4 | Can you tell C1 and C2 apart with the device in a pocket or bag? | Yes | |

## D. Sleep, wake, and lock

| # | Do this | Expect | Result |
| --- | --- | --- | --- |
| D1 | Leave it alone | Screen turns off after [30 s] | |
| D2 | Tap the screen | Wakes to Home, lock icon shown, the waking tap does nothing else | |
| D3 | Tap +100 kcal or the right circle while locked | "Locked. Hold the middle to unlock." Nothing logged | |
| D4 | Hold middle ~1 s | Unlocks with a tick | |
| D5 | From sleep, press-and-hold middle in one motion | Wakes and unlocks | |

## E. Reminder while asleep

| # | Do this | Expect | Result |
| --- | --- | --- | --- |
| E1 | Test → Reminder 30s, then let the screen sleep | At 30 s the screen wakes with "Quick water check." and two pulses | |
| E2 | While it's showing (and locked), tap **Sipped +2 oz** | Works without unlocking; water +2 oz | |
| E3 | Repeat E1 and leave it | Screen sleeps again after 30 s | |

## F. Storage and power loss

| # | Do this | Expect | Result |
| --- | --- | --- | --- |
| F1 | Log a few entries, note the totals, press the side power button to restart | Same totals after reboot | |
| F2 | Log one entry, then **immediately** hold the power button ~6 s to force off. Power on | Entry is there (or absent), never doubled; device boots normally | |
| F3 | Test → **Save test** a few times, restart | "Saves:" count kept | |
| F4 | Undo right after a log, restart | Undone entry stays gone | |
| F5 | `python sync/m1_tool.py stats` | `bad_lines` is 0 (a 1 after F2 is fine) | |

## G. Clock and tracking day

| # | Do this | Expect | Result |
| --- | --- | --- | --- |
| G1 | `python sync/m1_tool.py settime` | OK; status bar shows correct time | |
| G2 | Power off for a few minutes, power on | Time still correct | |
| G3 | Check after 3:00 AM (or next morning) | Totals reset to zero; yesterday's entries still counted in `stats` | |

## H. USB

| # | Do this | Expect | Result |
| --- | --- | --- | --- |
| H1 | `python sync/m1_tool.py hello` | Firmware 0.1.0-m1 | |
| H2 | Opening the port doesn't reboot the device | Uptime in `stats` keeps counting | |

## I. Battery (one real day)

| # | Do this | Expect | Result |
| --- | --- | --- | --- |
| I1 | Charge to 100%, unplug ~9 AM, use normally with sleep | Battery % at noon, 6 PM, midnight: ___ / ___ / ___ | |
| I2 | Battery % on Home vs. Test screen voltage | Plausible (not jumping around) | |
