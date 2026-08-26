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
from wifi_wrap import overlay_nodeid, parse_debug_log, write_hw_ids  # noqa: E402

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


def _open_serial(serial_mod, port: str, baud: int, wait_s: float):
    deadline = time.time() + wait_s
    last_err = None
    while time.time() < deadline:
        try:
            return serial_mod.Serial(port, baudrate=baud, timeout=0.2)
        except Exception as exc:  # port may vanish during USB-JTAG reset
            last_err = exc
            time.sleep(0.2)
    print(f"Could not open {port}: {last_err}", file=sys.stderr)
    raise SystemExit(1)


def collect_from_port(port: str, baud: int, timeout_s: float, out_path: Path, no_reset: bool = False) -> dict[str, str]:
    try:
        import serial
    except ImportError:
        print("pyserial is required for --port", file=sys.stderr)
        raise SystemExit(1)

    chunks: list[str] = []
    deadline = time.time() + timeout_s
    ser = _open_serial(serial, port, baud, 5.0)
    try:
        if not no_reset:
            try:
                _reset_esp32(ser)
            except Exception:
                pass
            ser.close()
            time.sleep(0.8)
            ser = _open_serial(serial, port, baud, 8.0)
        ser.reset_input_buffer()
        while time.time() < deadline:
            try:
                raw = ser.read(1024)
            except Exception:
                ser.close()
                time.sleep(0.5)
                ser = _open_serial(serial, port, baud, 8.0)
                continue
            if raw:
                chunks.append(raw.decode("utf-8", errors="replace"))
                text = "".join(chunks)
                if (
                    "MAC Address:" in text
                    and "OpenLCB Node ID:" in text
                    and "SPI flash unique ID:" in text
                ):
                    break
    finally:
        try:
            ser.close()
        except Exception:
            pass
    text = "".join(chunks)
    raw_log = Path(out_path).parent / "last_collect.log"
    try:
        raw_log.parent.mkdir(parents=True, exist_ok=True)
        raw_log.write_text(text, encoding="utf-8", errors="replace")
    except Exception:
        raw_log = None
    if not text.strip():
        print(f"No serial data from {port}. Is the board connected?", file=sys.stderr)
        print("On USB-JTAG use --no-reset, then tap the board RESET while this runs.", file=sys.stderr)
        raise SystemExit(1)
    try:
        ids = parse_debug_log(text)
    except SystemExit:
        print(f"Got {len(text)} bytes but they did not contain the DEBUG ID lines.", file=sys.stderr)
        if raw_log:
            print(f"Raw capture: {raw_log}", file=sys.stderr)
        print("Tap RESET once while collect is running (USB-JTAG: use --no-reset).", file=sys.stderr)
        raise
    write_hw_ids(out_path, ids)
    return ids


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="Serial device, e.g. /dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=25.0)
    parser.add_argument("--from-log", dest="from_log", help="Parse a saved log instead of opening serial")
    parser.add_argument("--out", default=DEFAULT_OUT)
    parser.add_argument(
        "--no-reset",
        action="store_true",
        help="Do not pulse RTS (required for ESP32-S3 USB-JTAG; tap the board RESET instead)",
    )
    args = parser.parse_args()

    out_path = Path(args.out)
    if args.from_log:
        ids = collect_from_log(Path(args.from_log), out_path)
    elif args.port:
        ids = collect_from_port(args.port, args.baud, args.timeout, out_path, no_reset=args.no_reset)
    else:
        parser.error("provide --port or --from-log")

    root = Path(__file__).resolve().parent.parent
    ids = overlay_nodeid(ids, root)
    write_hw_ids(out_path, ids)

    print(f"Wrote {out_path}")
    print(f"MAC Address: {ids['WIFI_MAC']}")
    print(f"OpenLCB Node ID: {ids['WIFI_NODE_ID']}")
    print(f"SPI flash unique ID: {ids['WIFI_FLASH_UID']}")
    print("Wrap key uses flash UID + MAC only; node ID is for LCC, not the wrap.")


if __name__ == "__main__":
    main()
