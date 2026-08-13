#!/usr/bin/env bash
# Interactive Wi-Fi PSK provisioner. Run in YOUR terminal, never from Grok.
#
# Prerequisites:
#   1. Flash a wrap-free image (utils/build_idf5.sh flash)
#   2. Collect IDs: ./utils/collect_hw_ids.py --port /dev/ttyACM0
#   3. This script (hidden prompt) host-encrypts and bakes ciphertext only
#
# The PSK is not accepted as a command-line argument, is not written to a
# project file, and is not appended to bash history.

set -euo pipefail

if [[ "${BASH_SOURCE[0]}" != "$0" ]]; then
    echo "Do not source ${BASH_SOURCE[0]}. Execute it." >&2
    return 1 2>/dev/null || exit 1
fi

set +o history 2>/dev/null || true
unset HISTFILE

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

IDS="${WIFI_IDS_FILE:-$ROOT/local/hw_ids.env}"
WRAP_OUT="${WIFI_WRAP_OUT:-$ROOT/main/wifi/wifi_psk_wrap.inc}"

if [[ ! -t 0 || ! -t 1 || ! -t 2 ]]; then
    echo "Must be run in an interactive terminal (keyboard + screen)." >&2
    echo "Do not run this from Grok, a pipe, or CI." >&2
    exit 1
fi

if [[ ! -f "$IDS" ]]; then
    echo "Missing $IDS" >&2
    echo "Flash the wrap-free image, then: ./utils/collect_hw_ids.py --port /dev/ttyACM0" >&2
    exit 1
fi

if [[ -n "${WIFI_PASSWORD:-}" ]]; then
    echo "WIFI_PASSWORD is already set in the environment." >&2
    echo "Unset it and type the password at the hidden prompt." >&2
    echo "    unset WIFI_PASSWORD" >&2
    exit 1
fi

for arg in "$@"; do
    case "$arg" in
        --password|--password=*|--psk|--psk=*|--wifi-password|--wifi-password=*)
            echo "Do not pass the Wi-Fi password as a command-line argument." >&2
            exit 1
            ;;
    esac
done

DEFAULT_SSID="${WIFI_SSID:-SRIF2333}"
read -r -p "Wi-Fi SSID [${DEFAULT_SSID}]: " ssid_in
WIFI_SSID="${ssid_in:-$DEFAULT_SSID}"
if [[ -z "$WIFI_SSID" ]]; then
    echo "SSID must not be empty."
    exit 1
fi

prompt_hidden() {
    local dest_var="$1"
    local prompt="$2"
    local value=""
    read -r -s -p "$prompt" value
    echo
    printf -v "$dest_var" '%s' "$value"
}

prompt_hidden WIFI_PASSWORD "Wi-Fi password (hidden, not written as plaintext): "
prompt_hidden WIFI_PASSWORD2 "Again to confirm: "

if [[ "$WIFI_PASSWORD" != "$WIFI_PASSWORD2" ]]; then
    WIFI_PASSWORD="x"
    WIFI_PASSWORD2="y"
    unset WIFI_PASSWORD WIFI_PASSWORD2
    echo "Passwords did not match."
    exit 1
fi
unset WIFI_PASSWORD2

if [[ -z "$WIFI_PASSWORD" ]]; then
    unset WIFI_PASSWORD
    echo "Password must not be empty."
    exit 1
fi

wipe() {
    if [[ -n "${WIFI_PASSWORD+x}" ]]; then
        WIFI_PASSWORD="$(dd if=/dev/urandom bs=64 count=1 status=none | base64 2>/dev/null || printf 'wiped')"
    fi
    unset WIFI_PASSWORD WIFI_PASSWORD2
}

trap wipe EXIT INT TERM HUP

echo "SSID in wrap blob: ${WIFI_SSID}"
echo "Encrypting on the host. Only ciphertext will be compiled in."

printf '%s' "$WIFI_PASSWORD" | python3 "$ROOT/utils/wifi_wrap.py" encrypt \
    --ids "$IDS" \
    --ssid "$WIFI_SSID" \
    --out "$WRAP_OUT"

wipe
trap - EXIT INT TERM HUP

if grep -Eiq 'WIFI_PASSWORD|PSK=|password=' "$WRAP_OUT"; then
    echo "Refusing to build: wrap file looks like it contains a password field."
    exit 1
fi

echo "Host wrap written. Building firmware (no PSK in the environment)."
if [[ $# -eq 0 ]]; then
    set -- build
fi
"${ROOT}/utils/build_idf5.sh" "$@"
