#!/usr/bin/env python3
"""Host clang-tidy fail gate for A5.04 (S3 turnout panel).

Python core; launchers are run_clang_tidy.sh and run_clang_tidy.ps1.
Scans main/wifi host headers + tests/. Not LVGL/OpenMRN/UI/app_main.

  python -u utils/run_clang_tidy.py
"""
from __future__ import print_function

import os
import shutil
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
WAE = "clang-diagnostic-error,clang-analyzer-*,google-*,cert-*"
CONFIG = os.path.join(ROOT, ".clang-tidy")
JOBS = (
    (os.path.join("tests", "test_svc_reach.cpp"), ["-std=c++11", "-I", os.path.join(ROOT, "main", "wifi")]),
    (os.path.join("tests", "test_cdi_configure.cpp"), ["-std=c++11", "-I", os.path.join(ROOT, "tests")]),
)


def find_clang_tidy():
    for name in ("clang-tidy", "clang-tidy.exe"):
        found = shutil.which(name)
        if found:
            return found
    if os.name == "nt":
        pf = os.environ.get("ProgramFiles", r"C:\Program Files")
        pf86 = os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
        extras = [
            os.path.join(pf, "LLVM", "bin", "clang-tidy.exe"),
            os.path.join(pf86, "LLVM", "bin", "clang-tidy.exe"),
        ]
    else:
        extras = ["/usr/bin/clang-tidy", "/usr/lib/llvm-18/bin/clang-tidy"]
    for path in extras:
        if os.path.isfile(path):
            return path
    return None


def main():
    tidy = find_clang_tidy()
    if not tidy:
        print("clang-tidy not installed")
        return 1
    os.chdir(ROOT)
    for rel, extra in JOBS:
        src = os.path.join(ROOT, rel)
        cmd = [
            tidy,
            "--config-file=" + CONFIG,
            "--warnings-as-errors=" + WAE,
            src,
            "--",
        ] + extra
        rc = subprocess.call(cmd)
        if rc != 0:
            return rc
    print("OK: clang-tidy host gate (A5.04 main/wifi + tests/; no LVGL/OpenMRN)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
