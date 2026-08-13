#!/usr/bin/env bash
# Erase the entire ESP32-S3 flash (app + NVS wrap) and remove host-side secret files.
#
#   ./utils/erase_all_flash.sh
#   ./utils/erase_all_flash.sh -p /dev/ttyACM0
#
# After this the chip has no firmware. Flash a wrap-free image, then
# provision again from your terminal if you want Wi-Fi back.
# Do not pass a Wi-Fi password on this command line.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

PORT="/dev/ttyACM0"
while [[ $# -gt 0 ]]; do
    case "$1" in
        -p|--port)
            PORT="${2:-}"
            if [[ -z "$PORT" ]]; then
                echo "Missing port after $1" >&2
                exit 1
            fi
            shift 2
            ;;
        -h|--help)
            sed -n '2,14p' "$0"
            exit 0
            ;;
        *)
            echo "Unknown argument: $1" >&2
            echo "Usage: $0 [-p /dev/ttyACM0]" >&2
            exit 1
            ;;
    esac
done

if [[ -z "${IDF_PATH:-}" ]]; then
    export IDF_PATH="${HOME}/esp/esp-idf-v5.1.6"
fi
# shellcheck disable=SC1091
source "${IDF_PATH}/export.sh"

wipe_file() {
    local f="$1"
    if [[ ! -e "$f" ]]; then
        return 0
    fi
    if command -v shred >/dev/null 2>&1; then
        shred -u "$f"
    else
        dd if=/dev/urandom of="$f" bs=1024 count=2 status=none conv=notrunc 2>/dev/null || true
        rm -f "$f"
    fi
    echo "Removed host secret file: $f"
}

echo "Erasing entire flash on ${PORT} (includes NVS Wi-Fi wrap)."
idf.py -p "$PORT" erase-flash

wipe_file "$ROOT/main/wifi/wifi_psk_wrap.inc"
wipe_file "$ROOT/wifi_secrets.env"

echo "Chip is blank. Host wrap/PSK files are gone."
echo "local/hw_ids.env was left in place (board IDs, not the password)."
echo "Next: ./utils/build_idf5.sh -p ${PORT} flash"
echo "Then, to restore Wi-Fi, run ./utils/provision_wifi_build.sh in your terminal."
