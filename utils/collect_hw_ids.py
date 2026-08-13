#!/usr/bin/env python3
"""Collect MAC, OpenLCB node ID, and SPI flash UID from DEBUG serial.

Does not handle the Wi-Fi password. Writes local/hw_ids.env only.

  ./utils/collect_hw_ids.py --from-log path/to/serial.log
  ./utils/collect_hw_ids.py --port /dev/ttyACM0
"""

from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from wifi_wrap import parse_debug_log, write_hw_ids  # noqa: E402

DEFAULT_OUT = "local/hw_ids.env"


def collect_from_log(log_path: Path, out_path: Path) -> dict[str, str]:
    ids = parse_debug_log(log_path.read_text(encoding="utf-8", errors="replace"))
    write_hw_ids(out_path, ids)
    return ids


def _reset_esp32(ser) -> None:
    ser.setDTR(False)
    ser.setRTS(True)
    time.sleep(0.1)
    ser.setRTS(False)
    time.sleep(0.1)


def collect_from_port(port: str, baud: int, timeout_s: float, out_path: Path) -> dict[str, str]:
    try:
        import serial
    except ImportError:
        print("pyserial is required for --port", file=sys.stderr)
        raise SystemExit(1)

    chunks: list[str] = []
    deadline = time.time() + timeout_s
    with serial.Serial(port, baudrate=baud, timeout=0.2) as ser:
        _reset_esp32(ser)
        ser.reset_input_buffer()
        while time.time() < deadline:
            raw = ser.read(1024)
            if raw:
                chunks.append(raw.decode("utf-8", errors="replace"))
                text = "".join(chunks)
                if (
                    "MAC Address:" in text
                    and "OpenLCB Node ID:" in text
                    and "SPI flash unique ID:" in text
                ):
                    break
    text = "".join(chunks)
    if not text.strip():
        print(f"No serial data from {port}. Is the board connected?", file=sys.stderr)
        raise SystemExit(1)
    ids = parse_debug_log(text)
    write_hw_ids(out_path, ids)
    return ids


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="Serial device, e.g. /dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=25.0)
    parser.add_argument("--from-log", dest="from_log", help="Parse a saved log instead of opening serial")
    parser.add_argument("--out", default=DEFAULT_OUT)
    args = parser.parse_args()

    out_path = Path(args.out)
    if args.from_log:
        ids = collect_from_log(Path(args.from_log), out_path)
    elif args.port:
        ids = collect_from_port(args.port, args.baud, args.timeout, out_path)
    else:
        parser.error("provide --port or --from-log")

    print(f"Wrote {out_path}")
    print(f"MAC Address: {ids['WIFI_MAC']}")
    print(f"OpenLCB Node ID: {ids['WIFI_NODE_ID']}")
    print(f"SPI flash unique ID: {ids['WIFI_FLASH_UID']}")


if __name__ == "__main__":
    main()
