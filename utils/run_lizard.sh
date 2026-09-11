#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
LIZARD="${LIZARD:-$(command -v lizard || true)}"
if [[ -z "$LIZARD" && -x "$HOME/.local/bin/lizard" ]]; then
    LIZARD="$HOME/.local/bin/lizard"
fi
if [[ -z "$LIZARD" ]]; then
    echo "lizard not on PATH" >&2
    exit 1
fi

# Gate: OwlThree Wi-Fi / host tests (our changes). UI/OpenMRN is reported only.
gate_out="$("$LIZARD" -C 10 "$ROOT/main/wifi" "$ROOT/tests" 2>&1)" || true
printf '%s\n' "$gate_out"
if printf '%s\n' "$gate_out" | grep -F '!!!! Warnings' >/dev/null; then
    echo "FAIL: lizard CCN limit 10 on main/wifi tests/" >&2
    exit 1
fi
echo "OK: lizard CCN limit 10 on main/wifi tests/"

if [[ "${LIZARD_ALL:-0}" == "1" ]]; then
    echo "== informational (UI / app / utils; not a fail) =="
    "$LIZARD" -C 10 "$ROOT/main" "$ROOT/utils" \
        --exclude "$ROOT/main/wifi/*" \
        --exclude "$ROOT/components/*" --exclude "$ROOT/managed_components/*" \
        --exclude "$ROOT/build/*" --exclude "$ROOT/build-win11/*" || true
fi
