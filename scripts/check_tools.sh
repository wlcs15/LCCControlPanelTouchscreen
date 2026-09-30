#!/usr/bin/env bash
# List whether required ESP-IDF / OpenMRN-LCC build tools are on this Linux machine.
# Does not install anything. Does not flash hardware. Does not handle a Wi-Fi password.
#
# Writes local/check_tools-YYYYMMDD-HHMMSS-<host>.log (and check_tools-last.log)
# so a machine without the Grok CLI can still share the result.
set -u

root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"

if [[ -z "${CHECK_TOOLS_INNER:-}" ]]; then
    mkdir -p "$root/local"
    ts="$(date +%Y%m%d-%H%M%S)"
    host="$(hostname -s 2>/dev/null || hostname 2>/dev/null || echo unknown)"
    host="${host//[^A-Za-z0-9._-]/_}"
    log="$root/local/check_tools-${ts}-${host}.log"
    last="$root/local/check_tools-last.log"
    {
        echo "=== check_tools log (share this file with Grok; Grok CLI not required) ==="
        echo "file: $log"
        echo "time: $(date -Is 2>/dev/null || date)"
        echo "host: $(hostname 2>/dev/null || echo unknown)"
        echo "os: $(uname -a 2>/dev/null || echo unknown)"
        echo "user: ${USER:-unknown}"
        echo "repo: $root"
        echo "git: $(git -C "$root" describe --tags --always --dirty 2>/dev/null || echo n/a)"
        echo "python: $(command -v python3 2>/dev/null || command -v python 2>/dev/null || echo none)"
        echo
    } >"$log"
    set +e
    CHECK_TOOLS_INNER=1 "$0" "$@" 2>&1 | tee -a "$log"
    rc=${PIPESTATUS[0]}
    set -e
    cp -f "$log" "$last"
    echo
    echo "Share this file with Grok (no Grok CLI needed):"
    echo "  $log"
    echo "  $last"
    exit "$rc"
fi

missing_req=0
missing_opt=0

ok() { printf "  OK       %-22s %s\n" "$1" "$2"; }
fail() {
  printf "  MISSING  %-22s %s\n" "$1" "$2"
  missing_req=1
}
warn() {
  printf "  WARN     %-22s %s\n" "$1" "$2"
  missing_opt=1
}

have_cmd() { command -v "$1" >/dev/null 2>&1; }

py=""
if have_cmd python3; then
  py=python3
elif have_cmd python; then
  py=python
fi

echo "Required tools check (Linux / ESP-IDF)  repo: $root"
echo "Target: ESP32-S3 (Waveshare 4.3 Inch LCC Control Panel)  (QEMU machine: esp32s3)"
echo ""
echo "=== Host build tools (required) ==="

if have_cmd git; then
  ok git "$(git --version 2>/dev/null | head -n1)"
else
  fail git "git clone / submodule update --init --recursive"
fi

if [[ -n "$py" ]]; then
  ok python "$($py --version 2>&1) ($py)"
else
  fail python "Python 3 (python3)"
fi

if have_cmd cmake; then
  ok cmake "$(cmake --version 2>/dev/null | head -n1)"
else
  fail cmake "CMake 3.16+ (apt install cmake)"
fi

if have_cmd ninja; then
  ok ninja "$(ninja --version 2>/dev/null)"
elif have_cmd make; then
  ok make "$(make --version 2>/dev/null | head -n1)"
else
  fail generator "Ninja or GNU make (apt install ninja-build)"
fi

echo ""
echo "=== ESP-IDF toolchain (required to build firmware) ==="

if have_cmd idf.py; then
  ok idf.py "$(idf.py --version 2>/dev/null | head -n1 || echo present) ($(command -v idf.py))"
elif [[ -n "${IDF_PATH:-}" && -x "${IDF_PATH}/tools/idf.py" ]]; then
  ok idf.py "IDF_PATH=${IDF_PATH} (run: . \"\$IDF_PATH/export.sh\")"
else
  fail idf.py "ESP-IDF export not active. Source /workspace/env/esp-idf-v5.1.6.sh or: export IDF_PATH=... && . \$IDF_PATH/export.sh"
fi

if [[ -n "${IDF_PATH:-}" ]]; then
  ok IDF_PATH "$IDF_PATH"
else
  warn IDF_PATH "not set; source /workspace/env/esp-idf-v5.1.6.sh or your ESP-IDF export.sh"
fi

echo ""
echo "=== Repo helpers (expected in this tree) ==="

if [[ -f "$root/utils/build_idf5.sh" ]]; then
  ok build_idf5.sh "utils/build_idf5.sh"
else
  warn build_idf5.sh "utils/build_idf5.sh not found (optional if you invoke idf.py directly)"
fi

if [[ -f "$root/utils/run_host_tests.sh" ]]; then
  ok run_host_tests.sh "utils/run_host_tests.sh"
else
  warn run_host_tests.sh "utils/run_host_tests.sh not found"
fi

echo ""
echo "=== Optional host quality (WARN only) ==="

if have_cmd clang && have_cmd clang++; then
  ok clang "$(clang --version 2>/dev/null | head -n1)"
else
  warn clang "LLVM Clang optional for host tidy/coverage (apt install clang)"
fi

echo ""
echo "=== Emulator (Espressif QEMU CI smoke) ==="

# Presence only. Does not run firmware. Linux CI (GrokBot-CI-Espressif) uses:
#   qemu-system-xtensa -nographic -machine esp32s3 -drive file=<flash.bin>,if=mtd,format=raw
# Workspace helper: /workspace/tools/ci/run_qemu_esp.sh esp32s3 <flash.bin>
# Exit 124 from timeout is treated as a clean smoke PASS by the orchestrator.
# Does NOT require Renode, simavr, or ARM qemu (owned by other CI bots).
if have_cmd qemu-system-xtensa; then
  ok qemu-system-xtensa "$(command -v qemu-system-xtensa)  (-machine esp32s3 CI smoke)"
else
  warn qemu-system-xtensa "Espressif QEMU (xtensa). CI smoke only; not required to build/flash. See docs/REQUIRED_TOOLS.txt"
fi

echo ""
if [[ "$missing_req" -ne 0 ]]; then
  echo "Required tools are missing. See docs/REQUIRED_TOOLS.txt"
  exit 1
fi
if [[ "$missing_opt" -ne 0 ]]; then
  echo "Host/IDF tools OK. Optional items listed as WARN above."
  echo "Details: docs/REQUIRED_TOOLS.txt"
  exit 0
fi
echo "All required and optional tools found."
echo "Details: docs/REQUIRED_TOOLS.txt"
exit 0
