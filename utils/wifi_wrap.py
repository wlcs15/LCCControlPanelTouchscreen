#!/usr/bin/env python3
"""Host-side AES-256-GCM wrap for the house Wi-Fi PSK.

Matches main/wifi/wifi_cred.cpp exactly:
  IKM  = flash_uid[8] || MAC[6] || node[6]
  info = 05.01.01.01.A5 || MAC[6]
  salt = owlthree-ws43b-wifi-wrap-v1
  HKDF-SHA256 -> 32-byte key
  AES-256-GCM, 12-byte nonce, 16-byte tag, no AAD

The PSK is never written to a project file. Only the wrap blob (ciphertext)
is emitted, as main/wifi/wifi_psk_wrap.inc.
"""

from __future__ import annotations

import argparse
import hmac
import os
import re
import struct
import sys
import tempfile
from hashlib import sha256
from pathlib import Path

SALT = b"owlthree-ws43b-wifi-wrap-v1"
OWL_PREFIX = bytes([0x05, 0x01, 0x01, 0x01, 0xA5])
WRAP_VER = 1
NONCE_LEN = 12
TAG_LEN = 16
CIPHER_LEN = 64
# Packed layout must match struct wrap_blob in wifi_cred.cpp.
WRAP_STRUCT = struct.Struct("=B12s16sB64s")
assert WRAP_STRUCT.size == 94

MAC_RE = re.compile(
    r"MAC Address:\s*([0-9A-Fa-f]{2}(?::[0-9A-Fa-f]{2}){5})"
)
NODE_RE = re.compile(
    r"OpenLCB Node ID:\s*([0-9A-Fa-f]{2}(?:\.[0-9A-Fa-f]{2}){5})"
)
UID_RE = re.compile(
    r"SPI flash unique ID:\s*([0-9A-Fa-f]{16}|UNAVAILABLE)"
)


def _die(msg: str, code: int = 1) -> None:
    print(msg, file=sys.stderr)
    raise SystemExit(code)


def parse_mac(text: str) -> bytes:
    parts = text.strip().replace("-", ":").split(":")
    if len(parts) != 6:
        _die(f"bad MAC: {text!r}")
    return bytes(int(p, 16) for p in parts)


def parse_node(text: str) -> bytes:
    parts = text.strip().split(".")
    if len(parts) != 6:
        _die(f"bad node ID: {text!r}")
    return bytes(int(p, 16) for p in parts)


def parse_uid(text: str) -> bytes:
    text = text.strip().upper()
    if text == "UNAVAILABLE":
        return bytes(8)
    if len(text) != 16 or any(c not in "0123456789ABCDEF" for c in text):
        _die(f"bad flash UID: {text!r}")
    return bytes.fromhex(text)


def parse_debug_log(text: str) -> dict[str, str]:
    mac_m = MAC_RE.search(text)
    node_m = NODE_RE.search(text)
    uid_m = UID_RE.search(text)
    missing = [
        name
        for name, m in (
            ("MAC Address", mac_m),
            ("OpenLCB Node ID", node_m),
            ("SPI flash unique ID", uid_m),
        )
        if not m
    ]
    if missing:
        _die("serial/log missing: " + ", ".join(missing))
    return {
        "WIFI_MAC": mac_m.group(1).upper(),
        "WIFI_NODE_ID": node_m.group(1).upper(),
        "WIFI_FLASH_UID": uid_m.group(1).upper(),
    }


def load_hw_ids(path: Path) -> dict[str, str]:
    data: dict[str, str] = {}
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, val = line.split("=", 1)
        data[key.strip()] = val.strip()
    for key in ("WIFI_MAC", "WIFI_NODE_ID", "WIFI_FLASH_UID"):
        if key not in data or not data[key]:
            _die(f"{path} missing {key}")
    return data


def write_hw_ids(path: Path, ids: dict[str, str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        "# Board IDs from DEBUG serial. Not the Wi-Fi password.\n"
        f"WIFI_MAC={ids['WIFI_MAC']}\n"
        f"WIFI_NODE_ID={ids['WIFI_NODE_ID']}\n"
        f"WIFI_FLASH_UID={ids['WIFI_FLASH_UID']}\n",
        encoding="utf-8",
    )


def derive_key(mac: bytes, uid: bytes, node: bytes) -> bytes:
    if len(mac) != 6 or len(uid) != 8 or len(node) != 6:
        _die("ID lengths must be MAC=6 UID=8 node=6")
    ikm = uid + mac + node
    info = OWL_PREFIX + mac
    prk = hmac.new(SALT, ikm, sha256).digest()
    t = b""
    okm = b""
    block = 1
    while len(okm) < 32:
        t = hmac.new(prk, t + info + bytes([block]), sha256).digest()
        okm += t
        block += 1
    return okm[:32]


def _aesgcm():
    try:
        from cryptography.hazmat.primitives.ciphers.aead import AESGCM
    except ImportError:
        _die("Python cryptography package is required (AES-GCM)")
    return AESGCM


def encrypt_psk(key: bytes, psk: bytes, nonce: bytes | None = None) -> bytes:
    if not psk or len(psk) > CIPHER_LEN:
        _die("PSK must be 1..64 bytes")
    if nonce is None:
        nonce = os.urandom(NONCE_LEN)
    if len(nonce) != NONCE_LEN:
        _die("nonce must be 12 bytes")
    AESGCM = _aesgcm()
    packed = AESGCM(key).encrypt(nonce, psk, None)
    cipher, tag = packed[:-TAG_LEN], packed[-TAG_LEN:]
    blob = WRAP_STRUCT.pack(WRAP_VER, nonce, tag, len(psk), cipher.ljust(CIPHER_LEN, b"\x00"))
    return blob


def decrypt_psk(key: bytes, blob: bytes) -> bytes:
    if len(blob) != WRAP_STRUCT.size:
        _die(f"wrap blob must be {WRAP_STRUCT.size} bytes")
    ver, nonce, tag, clen, cipher = WRAP_STRUCT.unpack(blob)
    if ver != WRAP_VER or clen == 0 or clen > CIPHER_LEN:
        _die("wrap blob header invalid")
    AESGCM = _aesgcm()
    return AESGCM(key).decrypt(nonce, cipher[:clen] + tag, None)


def render_inc(ssid: str, blob: bytes) -> str:
    if not ssid or len(ssid) > 32:
        _die("SSID must be 1..32 characters")
    if any(c in ssid for c in '"\\\n\r'):
        _die("SSID contains unsupported characters")
    hex_bytes = ", ".join(f"0x{b:02X}" for b in blob)
    return (
        "/* Generated wrap blob. Ciphertext only. Do not commit. */\n"
        f'static const char kWifiWrapSsid[] = "{ssid}";\n'
        f"static const uint8_t kWifiWrapBlob[{len(blob)}] = {{\n    {hex_bytes}\n}};\n"
    )


def write_inc(path: Path, ssid: str, blob: bytes) -> None:
    text = render_inc(ssid, blob)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def _read_psk_stdin() -> bytes:
    data = sys.stdin.buffer.read()
    if data.endswith(b"\n"):
        data = data[:-1]
    if data.endswith(b"\r"):
        data = data[:-1]
    if not data:
        _die("PSK on stdin is empty")
    return data


def cmd_encrypt(args: argparse.Namespace) -> None:
    ids = load_hw_ids(Path(args.ids))
    mac = parse_mac(ids["WIFI_MAC"])
    node = parse_node(ids["WIFI_NODE_ID"])
    uid = parse_uid(ids["WIFI_FLASH_UID"])
    psk = _read_psk_stdin()
    key = derive_key(mac, uid, node)
    blob = encrypt_psk(key, psk)
    if decrypt_psk(key, blob) != psk:
        _die("internal wrap check failed")
    write_inc(Path(args.out), args.ssid, blob)
    print(f"Wrote ciphertext wrap ({len(blob)} bytes) to {args.out}")
    print("PSK was not written to any file.")


def cmd_parse_log(args: argparse.Namespace) -> None:
    text = Path(args.log).read_text(encoding="utf-8", errors="replace")
    ids = parse_debug_log(text)
    write_hw_ids(Path(args.out), ids)
    print(f"Wrote {args.out}")
    for key in ("WIFI_MAC", "WIFI_NODE_ID", "WIFI_FLASH_UID"):
        print(f"{key}={ids[key]}")


def cmd_selftest(_args: argparse.Namespace) -> None:
    fake_log = (
        "I (123) main: boot\n"
        "I (200) debug_ids: ==== DEBUG IDs (not the WiFi PSK) ====\n"
        "I (201) debug_ids: MAC Address: DE:AD:BE:EF:00:01\n"
        "I (202) debug_ids: OpenLCB Node ID: 05.01.01.01.A5.31\n"
        "I (203) debug_ids: SPI flash unique ID: 0123456789ABCDEF\n"
    )
    ids = parse_debug_log(fake_log)
    if ids["WIFI_MAC"] != "DE:AD:BE:EF:00:01":
        _die("selftest MAC parse failed")
    if ids["WIFI_NODE_ID"] != "05.01.01.01.A5.31":
        _die("selftest node parse failed")
    if ids["WIFI_FLASH_UID"] != "0123456789ABCDEF":
        _die("selftest UID parse failed")

    unavail = fake_log.replace("0123456789ABCDEF", "UNAVAILABLE")
    uid0 = parse_uid(parse_debug_log(unavail)["WIFI_FLASH_UID"])
    if uid0 != bytes(8):
        _die("selftest UNAVAILABLE UID must be 8 zero bytes")

    mac = parse_mac(ids["WIFI_MAC"])
    node = parse_node(ids["WIFI_NODE_ID"])
    uid = parse_uid(ids["WIFI_FLASH_UID"])
    key = derive_key(mac, uid, node)
    psk = b"fake-psk-not-a-house-password"
    nonce = bytes(range(12))
    blob = encrypt_psk(key, psk, nonce=nonce)
    if decrypt_psk(key, blob) != psk:
        _die("selftest decrypt mismatch")

    bad_key = derive_key(bytes([0x00]) * 6, uid, node)
    try:
        decrypt_psk(bad_key, blob)
    except Exception:
        pass
    else:
        _die("selftest expected GCM failure on wrong MAC")

    with tempfile.TemporaryDirectory() as tmp:
        inc = Path(tmp) / "wifi_psk_wrap.inc"
        ids_path = Path(tmp) / "hw_ids.env"
        write_hw_ids(ids_path, ids)
        write_inc(inc, "SRIF2333", blob)
        text = inc.read_text(encoding="utf-8")
        if psk.decode("ascii") in text:
            _die("selftest leaked PSK into wrap include")
        if "fake-psk" in text:
            _die("selftest leaked PSK fragment into wrap include")
        if "kWifiWrapBlob[94]" not in text:
            _die("selftest wrap include size mismatch")

    print("wifi_wrap selftest passed")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="cmd", required=True)

    p_enc = sub.add_parser("encrypt", help="Read PSK from stdin; write ciphertext .inc")
    p_enc.add_argument("--ids", required=True, help="local/hw_ids.env")
    p_enc.add_argument("--ssid", default="SRIF2333")
    p_enc.add_argument("--out", required=True, help="main/wifi/wifi_psk_wrap.inc")
    p_enc.set_defaults(func=cmd_encrypt)

    p_log = sub.add_parser("parse-log", help="Parse a saved DEBUG serial log")
    p_log.add_argument("--log", required=True)
    p_log.add_argument("--out", required=True)
    p_log.set_defaults(func=cmd_parse_log)

    p_t = sub.add_parser("selftest", help="Round-trip crypto and parser with fake data")
    p_t.set_defaults(func=cmd_selftest)

    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
