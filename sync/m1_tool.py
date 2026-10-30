"""Milestone 1 laptop tool: talk to the Core2 over USB.

Usage (from the repo root; on Windows use `py` instead of `python`):
    pip install pyserial
    python sync/m1_tool.py hello
    python sync/m1_tool.py settime          # sets the device clock to this laptop's local time
    python sync/m1_tool.py pushfoods        # validates data/foods.csv and sends it to the device
    python sync/m1_tool.py stats
    python sync/m1_tool.py --port COM5 stats

The full sync script (Milestone 5) replaces this.
"""
from __future__ import annotations

import argparse
import base64
import json
import sys
import time
import zlib
from datetime import datetime
from pathlib import Path

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    sys.exit("pyserial is missing. Run:  pip install pyserial")

WCH_VID = 0x1A86  # CH9102 USB-serial chip on the Core2 v1.3


def find_port() -> str:
    ports = list(list_ports.comports())
    for p in ports:
        if p.vid == WCH_VID:
            return p.device
    names = ", ".join(p.device for p in ports) or "none"
    sys.exit(f"Core2 not found (ports seen: {names}). Plug it in, install the CH9102 driver, or pass --port COMx.")


def open_port(name: str) -> serial.Serial:
    s = serial.Serial()
    s.port = name
    s.baudrate = 115200
    s.timeout = 0.2
    s.dtr = False  # keep the auto-reset lines released so opening the port doesn't reboot the device
    s.rts = False
    s.open()
    return s


def request(s: serial.Serial, payload: dict, timeout: float = 3.0) -> dict:
    s.reset_input_buffer()
    s.write((json.dumps(payload) + "\n").encode())
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        raw = s.readline().decode(errors="replace").strip()
        if not raw.startswith("{"):
            continue  # ignore boot logs and other noise
        try:
            msg = json.loads(raw)
        except json.JSONDecodeError:
            continue
        if msg.get("cmd") == payload["cmd"] or msg.get("ok") is False:
            return msg
    sys.exit(f"No reply to {payload['cmd']!r} within {timeout:.0f} s. Is the latest firmware flashed and the serial monitor closed?")


CHUNK = 384  # raw bytes per message (512 base64 chars)


def compile_foods(csv_path: Path) -> bytes:
    """Validates the CSV first; nothing is sent if it has errors."""
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from foods import compile_json, load_foods

    foods, report = load_foods(csv_path)
    for w in report.warnings:
        print("warning:", w)
    if not report.ok:
        for e in report.errors:
            print("ERROR:  ", e)
        sys.exit(f"{len(report.errors)} error(s) in {csv_path}; nothing sent.")
    data = json.dumps(compile_json(foods), separators=(",", ":"), ensure_ascii=False).encode("utf-8")
    print(f"{csv_path.name}: {len(foods)} foods, {len(data):,} bytes")
    return data


def push_foods(s: serial.Serial, data: bytes) -> dict:
    r = request(s, {"cmd": "put_begin", "path": "foods.json", "size": len(data), "crc": zlib.crc32(data)})
    if not r.get("ok"):
        return r
    for i in range(0, len(data), CHUNK):
        chunk = base64.b64encode(data[i:i + CHUNK]).decode("ascii")
        r = request(s, {"cmd": "put_chunk", "d": chunk})
        if not r.get("ok"):
            return r
        print(f"\r  sent {min(i + CHUNK, len(data)):,} / {len(data):,} bytes", end="", flush=True)
    print()
    return request(s, {"cmd": "put_commit"}, timeout=15)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", choices=["hello", "settime", "time", "stats", "pushfoods"])
    ap.add_argument("--port", help="serial port, e.g. COM5 (auto-detected if omitted)")
    ap.add_argument("--csv", default=str(Path(__file__).resolve().parent.parent / "data" / "foods.csv"),
                    help="food database to push (default: data/foods.csv)")
    args = ap.parse_args()

    payload = None
    if args.command == "pushfoods":
        payload = compile_foods(Path(args.csv))

    with open_port(args.port or find_port()) as s:
        if args.command == "hello":
            r = request(s, {"cmd": "hello"})
        elif args.command == "time":
            r = request(s, {"cmd": "time_get"})
        elif args.command == "settime":
            now = datetime.now().replace(microsecond=0)
            r = request(s, {"cmd": "time_set", "t": now.isoformat()})
            if r.get("ok"):
                check = request(s, {"cmd": "time_get"})
                if not check.get("valid"):
                    print("Clock write did not stick:", check)
                    return 1
                r = check
        elif args.command == "pushfoods":
            r = push_foods(s, payload)
        else:
            r = request(s, {"cmd": "stats"})

    if not r.get("ok"):
        print("Device error:", r.get("error"))
        return 1
    for k, v in r.items():
        if k not in ("ok", "cmd"):
            print(f"{k:>14}: {v}")
    print("OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
