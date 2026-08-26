#!/usr/bin/env python3
"""Golden wrap vectors. Fake PSK only. Node ID must not change the key."""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from wifi_wrap import (  # noqa: E402
    derive_key,
    encrypt_psk,
    decrypt_psk,
    parse_mac,
    parse_uid,
    parse_node,
)

# Not a house password. Used only in this test.
FAKE_PSK = b"Password!"
MAC = parse_mac("1C:DB:D4:42:EF:D0")
UID = parse_uid("051D0D1918250DCC")
NONCE = bytes(range(12))
# HKDF v2 (UID||MAC, salt owlthree-ws43b-wifi-wrap-v2), AES-GCM, nonce 00..0b
GOLDEN_KEY = bytes.fromhex(
    "ae96c67c0956b4439d21c2b922f817fc5642d90d75361baa9b6baa81ec294eb4"
)
GOLDEN_BLOB = bytes.fromhex(
    "01000102030405060708090a0bed19a2bbca70d0d454a1f3ea989205b3"
    "09ef0a8f049df2c6e44b"
    + ("00" * 55)
)


def main() -> None:
    key = derive_key(MAC, UID)
    if key != GOLDEN_KEY:
        raise SystemExit(f"key mismatch: {key.hex()}")

    # Former bug: IKM included OpenLCB node ID. These two must match.
    n31 = parse_node("05.01.01.01.A5.31")
    n04 = parse_node("05.01.01.01.A5.04")
    if derive_key(MAC, UID, n31) != derive_key(MAC, UID, n04):
        raise SystemExit("node ID must not change wrap key")
    if derive_key(MAC, UID, n31) != key:
        raise SystemExit("optional node arg must be ignored")

    other_mac = parse_mac("1C:DB:D4:42:EF:D1")
    if derive_key(other_mac, UID) == key:
        raise SystemExit("MAC change must change wrap key")
    other_uid = parse_uid("051D0D1918250DCD")
    if derive_key(MAC, other_uid) == key:
        raise SystemExit("flash UID change must change wrap key")

    blob = encrypt_psk(key, FAKE_PSK, nonce=NONCE)
    if blob != GOLDEN_BLOB:
        raise SystemExit(f"blob mismatch:\n {blob.hex()}\n {GOLDEN_BLOB.hex()}")
    if decrypt_psk(key, blob) != FAKE_PSK:
        raise SystemExit("decrypt mismatch")
    print("wifi_wrap vector tests passed (Password! / node ID ignored)")


if __name__ == "__main__":
    main()
