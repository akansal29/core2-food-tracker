# Milestone 3 checklist: Today's Log and history

Upload the new firmware (✓ Build, → Upload). Entries, foods, and favorites are kept. No `pushfoods` needed.

## A. Getting to Today's Log

| # | Do this | Expect | Result |
| --- | --- | --- | --- |
| A1 | Home → **Today** | Today's Log: kcal (orange), protein (green), water (blue), "Last food …", "… to go" line, entries newest first | |
| A2 | From any screen, **hold the middle circle** ~1 s (unlocked) | Opens Today's Log | |
| A3 | Scroll the entry list | Smooth; drag never opens an entry | |
| A4 | Compare totals with Home | Identical | |

## B. Editing an entry

| # | Do this | Expect | Result |
| --- | --- | --- | --- |
| B1 | Tap a food entry | Edit screen: name, time, "counts for <day>", servings, kcal/protein | |
| B2 | Press **-** to 0.5 serving | kcal and protein halve; **Save** turns orange | |
| B3 | **Save** | Back to the list; entry shows the new numbers and "edited"; totals update | |
| B4 | Open the same entry, **+** back to 1, **Cancel** | Nothing changes | |
| B5 | Tap a water entry | Shows oz; **- / +** change it by 2 oz | |
| B6 | Edit a food that you changed in `foods.csv` since logging | Scaling uses the numbers you logged, not the new list | |

## C. Changing the time

| # | Do this | Expect | Result |
| --- | --- | --- | --- |
| C1 | **-1 h** a few times | Time moves back; "counts for" stays today until before 3 AM | |
| C2 | Move an entry to before 3:00 AM today, **Save** | It disappears from Today and appears under yesterday in Past days | |
| C3 | Try **+1 h** on an entry from a few minutes ago | Stops at the current time (no future entries) | |

## D. Deleting

| # | Do this | Expect | Result |
| --- | --- | --- | --- |
| D1 | **Delete** once | Button turns amber: "Tap again" | |
| D2 | Wait 3 s | Goes back to "Delete"; nothing deleted | |
| D3 | **Delete**, then **Tap again** | Entry gone; totals drop | |

## E. Past days

| # | Do this | Expect | Result |
| --- | --- | --- | --- |
| E1 | Today's Log → **Past days** | Every day with entries, newest first, with totals; today marked "(today)" | |
| E2 | Tap an earlier day | That day's log with its own totals and entries | |
| E3 | Edit or delete an entry there | Only that day's totals change; Home is unaffected | |
| E4 | **< Back** | Returns to Past days, then to Today | |

## F. Safety

| # | Do this | Expect | Result |
| --- | --- | --- | --- |
| F1 | Let the screen sleep on the edit screen, wake, unlock | Home offers "Tap to resume: editing …" | |
| F2 | Make a few edits and deletions, restart | Everything is exactly as you left it | |
| F3 | After 3 AM tomorrow | Today's Log starts empty; yesterday is in Past days | |
