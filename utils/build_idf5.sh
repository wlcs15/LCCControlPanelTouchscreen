#!/usr/bin/env bash
# Build with ESP-IDF 5.1.6. No secret file is read.
#
#   ./utils/build_idf5.sh build
#   ./utils/build_idf5.sh -p /dev/ttyACM0 flash
#
# Collect board IDs after a wrap-free flash, then provision in YOUR terminal:
#   ./utils/collect_hw_ids.py --port /dev/ttyACM0
#   ./utils/provision_wifi_build.sh
#   ./utils/provision_wifi_build.sh -p /dev/ttyACM0 flash
#
# Do not pass the Wi-Fi password on this command line.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

if [[ -z "${IDF_PATH:-}" ]]; then
    export IDF_PATH="${HOME}/esp/esp-idf-v5.1.6"
fi
# shellcheck disable=SC1091
source "${IDF_PATH}/export.sh"

idf_ver="$(idf.py --version 2>/dev/null || true)"
if [[ "${idf_ver}" != *v5.1* ]]; then
    echo "This tree must be built with ESP-IDF v5.1.6 (got: ${idf_ver:-unknown})" >&2
    echo "export IDF_PATH=~/esp/esp-idf-v5.1.6 && . \"\$IDF_PATH/export.sh\"" >&2
    exit 1
fi

if [[ $# -eq 0 ]]; then
    set -- build
fi

if [[ -f "$ROOT/main/wifi/wifi_psk_wrap.inc" ]]; then
    echo "Building with host-encrypted wrap blob (ciphertext only)."
else
    echo "Building without a wrap blob (DEBUG collect / NVS-only)."
fi

exec idf.py "$@"
