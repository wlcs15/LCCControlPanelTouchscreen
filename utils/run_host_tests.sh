#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
echo "== wrap tests =="
"$ROOT/utils/test_wifi_wrap.sh"
echo "== CDI Configure =="
clang++ -std=c++11 -I"$ROOT/tests" "$ROOT/tests/test_cdi_configure.cpp" -o /tmp/test_s3_cdi_configure
/tmp/test_s3_cdi_configure
echo "host tests OK"
