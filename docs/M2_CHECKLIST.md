# Milestone 2 checklist: food logging

## Setup

1. Upload the new firmware (→ arrow in VS Code's bottom bar). Your Milestone 1 entries and settings are kept.
2. Close the serial monitor if it's open, then push your food list. From the `firmware` folder's terminal:
   ```
   py ..\sync\m1_tool.py pushfoods
   ```
   It validates `data/foods.csv` first, then sends it. Expect `foods: 48` and the toast
   "Food list updated: 48 foods" on the Core2.
3. Whenever you edit `foods.csv`, run `pushfoods` again. No firmware upload needed.

The Home test buttons are gone. The bottom row is now **Today · Ideas · Trends · Settings**; only Settings
works yet (it opens the hardware test screen from M1).

## A. Getting in

| # | Do this | Expect | Result |
| --- | --- | --- | --- |
| A1 | Tap the **left circle** | Log Food screen: Browse all · Custom · Favorites buttons, Recent list below | |
| A2 | Tap **< Back** or the **middle circle** | Home | |

## B. Browse and serving picker

| # | Do this | Expect | Result |
| --- | --- | --- | --- |
| B1 | Browse all → Snacks / Drinks / Meals tabs | Foods sorted A–Z; each row shows kcal, protein, serving | |
| B2 | Drag the list up and down | Scrolls smoothly; a drag never opens a food | |
| B3 | Tap a food | Serving picker: 1 serving, preview "+190 kcal, +8.0 g protein" and "New total" | |
| B4 | Try **- / +** and the 1/4, 1/2, 3/4, 1, 2 chips | Servings and preview update; numbers scale correctly | |
| B5 | Tap **Log** | Back to Home; "Logged! +… kcal" with **Undo** and **Log another**; bars go up; one tick | |
| B6 | Tap **Undo** within 5 s | Entry removed, totals drop back | |
| B7 | Tap **Cancel** in a picker | Nothing logged; back to the list you came from | |

## C. Favorites and recents

| # | Do this | Expect | Result |
| --- | --- | --- | --- |
| C1 | In a picker, tap the **star** (top right) | Star fills; "Added to favorites" | |
| C2 | Log Food → **Favorites** | That food is listed, with a small star | |
| C3 | Log 2–3 different foods, open Log Food | They appear under Recent, newest first | |
| C4 | **Press and hold** a food in Recent or Favorites ~0.6 s | Logs 1 serving instantly, Home with Undo | |
| C5 | Count taps: left circle → hold a recent food | Logged in **2 actions** (3 via Favorites) | |

## D. Combos

| # | Do this | Expect | Result |
| --- | --- | --- | --- |
| D1 | Meals → **Chicken and rice bowl** | 390 kcal, 39.0 g per serving | |
| D2 | Tap **Adjust**, set rice to 0.5, **Done** | Picker says "Adjusted amounts"; preview ~288 kcal | |
| D3 | **Log** | Logged as "Chicken and rice bowl (adjusted)" | |
| D4 | Open the same combo again | Back to the saved recipe (390 kcal); the edit was this entry only | |
| D5 | In Adjust, press **-** until an ingredient reads 0 | That ingredient is left out | |

## E. Custom entry

| # | Do this | Expect | Result |
| --- | --- | --- | --- |
| E1 | Custom entry → type 350 calories | Log button turns on | |
| E2 | Tap the Protein box, type 12.5 | Only one decimal allowed | |
| E3 | **Log** | Home shows "+350 kcal, +12.5 g" | |
| E4 | Custom again with **Save as food** ticked | Logged; the food appears in Browse → Snacks as "Custom 4:12 PM" (your time) | |
| E5 | Try to log with calories empty | Not allowed | |

## F. Lock, sleep, and safety

| # | Do this | Expect | Result |
| --- | --- | --- | --- |
| F1 | Let the screen sleep while a serving picker is open, then wake and unlock | Home shows "Tap to resume: <food>"; tapping it brings the picker back | |
| F2 | While locked, try the left circle and holding a list row | Nothing logs; "Locked" hint | |
| F3 | Log a few foods, restart (side button) | Totals, favorites, recents, and custom foods all come back | |
| F4 | Note Home's totals, edit a logged food's calories in `foods.csv`, run `pushfoods` | Picker shows the new value; Home's totals don't change (old entries keep their numbers) | |
| F5 | Put a typo in `foods.csv` (e.g. calories `abc`), run `pushfoods` | Laptop refuses before sending; device keeps the old list | |
| F6 | Unplug the cable halfway through a `pushfoods` | Device shows "Sync interrupted; old list kept" and still works | |

## G. Feel

Anything awkward? Text too small, rows too short to tap, scrolling sticky, long-press too slow or too easy to
trigger? Note it. These are quick to tune.
