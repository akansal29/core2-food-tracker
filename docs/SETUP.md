# Setup (Windows + VS Code)

## One-time

1. **PlatformIO:** in VS Code, open Extensions, install **PlatformIO IDE**, then restart VS Code.
2. **USB driver:** plug in the Core2 with the included USB-C cable. Open Device Manager → *Ports (COM & LPT)*.
   If no "USB-SERIAL CH9102" / "USB-Enhanced-SERIAL CH343" COM port appears, install the WCH **CH343SER** driver
   from wch-ic.com, then replug.
3. **Python:** Python 3.11 or newer, then in a terminal:
   ```
   pip install pyserial
   ```

## Flash the firmware

1. VS Code → File → Open Folder → `core2-food-tracker/firmware`.
2. PlatformIO sidebar (alien icon) → **core2** → **Upload**. The first build downloads the ESP32 toolchain and
   libraries; give it several minutes.
3. Optional: **Monitor** shows the device's serial output. Close the monitor before running the Python tool
   (only one program can hold the COM port).

## Set the clock

From the repo root:
```
python sync/m1_tool.py settime
python sync/m1_tool.py stats
```
The tool finds the Core2 automatically; if it can't, pass `--port COM5` (use the number Device Manager shows).

## Check the food database

```
python sync/foods.py data/foods.csv
```
Prints `OK` or lists the rows to fix. Edit `data/foods.csv` in Excel or Google Sheets (save as CSV, keep the
header row and column order).

## Laptop unit tests (optional)

- Python: `pip install pytest`, then `python -m pytest tests`.
- C++ core: `pio test -e native` from the `firmware` folder. This needs a C++ compiler on Windows
  (e.g. MSYS2 MinGW-w64 with `g++` on PATH). Skip it if that's a hassle; the same tests were run during the build.

## Troubleshooting

| Symptom | Fix |
| --- | --- |
| Upload fails "could not open port" | Close the serial monitor and any running Python tool. |
| Upload fails to connect | Hold nothing; just retry. If it persists, press the side power button once to reset and retry. |
| Screen stays black after upload | Press the side power button once. |
| `m1_tool` says "Core2 not found" | Driver missing or charge-only cable. Try the included cable. |
| Home says "Set the clock" | Run `python sync/m1_tool.py settime`. |
