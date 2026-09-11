#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
clang++ -std=c++11 -fprofile-instr-generate -fcoverage-mapping \
    -I"$ROOT/main/wifi" "$ROOT/tests/test_svc_reach.cpp" -o /tmp/test_s3_svc_reach_cov
LLVM_PROFILE_FILE=/tmp/test_s3_svc_reach.profraw /tmp/test_s3_svc_reach_cov
clang++ -std=c++11 -fprofile-instr-generate -fcoverage-mapping \
    -I"$ROOT/tests" "$ROOT/tests/test_cdi_configure.cpp" -o /tmp/test_s3_cdi_cov
LLVM_PROFILE_FILE=/tmp/test_s3_cdi.profraw /tmp/test_s3_cdi_cov
llvm-profdata merge -sparse \
    /tmp/test_s3_svc_reach.profraw /tmp/test_s3_cdi.profraw \
    -o /tmp/test_s3_host.profdata
llvm-cov report /tmp/test_s3_svc_reach_cov -instr-profile=/tmp/test_s3_host.profdata \
    "$ROOT/main/wifi/SvcReachPick.h"
llvm-cov report /tmp/test_s3_cdi_cov -instr-profile=/tmp/test_s3_host.profdata \
    "$ROOT/tests/CdiWellFormed.h"
echo "coverage report above (host headers only; IDF main is not host-instrumented)"
