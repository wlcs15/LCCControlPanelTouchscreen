#!/usr/bin/env bash
# On-target Espressif QEMU firmware smoke (esp32s3 / LCC Control Panel).
# Launcher for utils/run_host_tests_qemu.py — same behavior as .ps1.
#
#   ./utils/run_host_tests_qemu.sh
#   ./utils/run_host_tests_qemu.sh --build
#   ./utils/run_host_tests_qemu.sh --flash build/qemu_flash.bin
#
# Native host unit tests: utils/run_host_tests.sh (Linux clang++; not QEMU).
# Does not flash hardware. Prefers Espressif qemu-system-xtensa (-machine esp32s3).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck disable=SC1091
[[ -f /workspace/env/emulators.sh ]] && source /workspace/env/emulators.sh
if [[ -z "${IDF_PATH:-}" && -f /workspace/env/esp-idf-v5.1.6.sh ]]; then
  # shellcheck disable=SC1091
  source /workspace/env/esp-idf-v5.1.6.sh
fi
export PATH="/workspace/tools/bin:${IDF_PYTHON_ENV_PATH:-}/bin:${PATH:-}"
if command -v python3 >/dev/null 2>&1; then
  py=python3
elif command -v python >/dev/null 2>&1; then
  py=python
else
  echo "python3 not found" >&2
  exit 1
fi
exec "$py" -u "$ROOT/utils/run_host_tests_qemu.py" "$@"
