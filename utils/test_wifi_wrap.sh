#!/usr/bin/env bash
# Fake-data tests for collect + host wrap. Never uses a real house PSK.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
PY="${PYTHON:-python3}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

echo "== wifi_wrap selftest =="
"$PY" "$ROOT/utils/wifi_wrap.py" selftest

echo "== collect_hw_ids from fake log =="
"$PY" "$ROOT/utils/collect_hw_ids.py" \
    --from-log "$ROOT/utils/testdata/fake_debug_ids.txt" \
    --out "$TMP/hw_ids.env"

grep -q 'WIFI_MAC=DE:AD:BE:EF:00:01' "$TMP/hw_ids.env"
grep -q 'WIFI_NODE_ID=05.01.01.01.A5.31' "$TMP/hw_ids.env"
grep -q 'WIFI_FLASH_UID=0123456789ABCDEF' "$TMP/hw_ids.env"

echo "== host encrypt fake PSK to wrap include =="
FAKE_PSK='fake-psk-not-a-house-password'
printf '%s' "$FAKE_PSK" | "$PY" "$ROOT/utils/wifi_wrap.py" encrypt \
    --ids "$TMP/hw_ids.env" \
    --ssid SRIF2333 \
    --out "$TMP/wifi_psk_wrap.inc"

if grep -Fq "$FAKE_PSK" "$TMP/wifi_psk_wrap.inc"; then
    echo "FAIL: plaintext PSK leaked into wrap include"
    exit 1
fi
grep -q 'kWifiWrapBlob\[94\]' "$TMP/wifi_psk_wrap.inc"
grep -q 'kWifiWrapSsid' "$TMP/wifi_psk_wrap.inc"

echo "== provision script refuses non-TTY =="
if "$ROOT/utils/provision_wifi_build.sh" </dev/null >/dev/null 2>"$TMP/prov.err"; then
    echo "FAIL: provision_wifi_build.sh should refuse a pipe"
    exit 1
fi
grep -q 'interactive terminal' "$TMP/prov.err"

echo "All fake-data script tests passed."
