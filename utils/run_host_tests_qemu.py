#!/usr/bin/env python3
"""On-target Espressif QEMU firmware smoke for ESP32-S3 (LCC Control Panel).

Matches the GrokBot-CI-Espressif EMULATOR gate: merge an IDF flash image and
boot it under qemu-system-xtensa -machine esp32s3. Does NOT flash hardware.

Native Linux host unit tests (clang++ / wrap selftests) stay in
utils/run_host_tests.sh — those binaries are host-native and are not wrapped
into QEMU here.

Usage (from repo root, after an IDF build or with --build):
  python -u utils/run_host_tests_qemu.py
  python -u utils/run_host_tests_qemu.py --build
  python -u utils/run_host_tests_qemu.py --flash build/qemu_flash.bin
  TIMEOUT_SEC=30 python -u utils/run_host_tests_qemu.py

Discover QEMU via QEMU_ESP / ESPRESSIF_QEMU / QEMU_SYSTEM_XTENSA, PATH, or
/workspace/tools (source /workspace/env/emulators.sh when present).
"""
from __future__ import print_function

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
MACHINE = "esp32s3"
TARGET_LABEL = "ESP32-S3 (Waveshare 4.3 Inch LCC Control Panel)"
DEFAULT_FLASH = os.path.join(ROOT, "build", "qemu_flash.bin")
DEFAULT_TIMEOUT = 25
BOOT_OK = re.compile(
    r"esp_image|boot:|I \(|app_main|hello_world|cpu_start|Loaded app",
    re.I,
)
BOOT_PANIC = re.compile(
    r"Guru Meditation|panic|LoadProhibited|abort\(\)|StoreProhibited|IllegalInstruction",
    re.I,
)


def eprint(*args):
    print(*args, file=sys.stderr)


def maybe_source_workspace_env():
    """Best-effort: prepend workspace emulator/IDF paths when on the GrokBot box."""
    emu = "/workspace/env/emulators.sh"
    if os.path.isfile(emu) and os.name != "nt":
        # Pull exports without requiring bash in this process: set known paths.
        bin_dir = "/workspace/tools/bin"
        qemu_esp = "/workspace/tools/qemu-esp/qemu/bin/qemu-system-xtensa"
        if os.path.isdir(bin_dir):
            os.environ["PATH"] = bin_dir + os.pathsep + os.environ.get("PATH", "")
        if os.path.isfile(qemu_esp):
            os.environ.setdefault("QEMU_ESP", qemu_esp)
            os.environ.setdefault("ESPRESSIF_QEMU", qemu_esp)
            os.environ.setdefault("QEMU_SYSTEM_XTENSA", qemu_esp)


def find_qemu():
    """Prefer PATH / tools/bin wrappers (set LD_LIBRARY_PATH) over raw qemu-esp binary."""
    # 1) Explicit wrapper-friendly env that already points at a working binary
    for key in ("QEMU_ESP", "ESPRESSIF_QEMU", "QEMU_SYSTEM_XTENSA"):
        val = os.environ.get(key)
        if not val:
            continue
        # If env points at the extract tree binary, prefer the workspace wrapper instead
        wrapper = "/workspace/tools/bin/qemu-system-xtensa"
        if "qemu-esp/qemu/bin" in val.replace("\\", "/") and os.path.isfile(wrapper):
            return wrapper
        if os.path.isfile(val) and os.access(val, os.X_OK):
            return val
    # 2) PATH (tools/bin wrapper after sourcing emulators.sh)
    for name in ("qemu-system-xtensa", "qemu-esp"):
        found = shutil.which(name)
        if found:
            return found
    # 3) Well-known workspace locations (wrapper first)
    candidates = [
        "/workspace/tools/bin/qemu-system-xtensa",
        "/workspace/tools/bin/qemu-esp",
        os.path.join(ROOT, "tools", "bin", "qemu-system-xtensa"),
        "/workspace/tools/qemu-esp/qemu/bin/qemu-system-xtensa",
    ]
    for path in candidates:
        if os.path.isfile(path) and os.access(path, os.X_OK):
            return path
    return None


def prepare_qemu_env():
    """Ensure shared libs resolve when invoking the raw Espressif qemu binary."""
    lib_dirs = [
        "/workspace/tools/qemu/usr/lib/x86_64-linux-gnu",
        "/workspace/tools/lib",
    ]
    parts = [d for d in lib_dirs if os.path.isdir(d)]
    if not parts:
        return
    cur = os.environ.get("LD_LIBRARY_PATH", "")
    os.environ["LD_LIBRARY_PATH"] = os.pathsep.join(parts + ([cur] if cur else []))


def qemu_has_machine(qemu_bin, machine):
    try:
        out = subprocess.check_output(
            [qemu_bin, "-machine", "help"],
            stderr=subprocess.STDOUT,
            universal_newlines=True,
        )
    except (OSError, subprocess.CalledProcessError) as exc:
        eprint("EMU_QEMU_ESP cannot list machines: %s" % exc)
        return False
    for line in out.splitlines():
        # First token is the machine name
        tok = line.strip().split()
        if tok and tok[0] == machine:
            return True
    return False


def find_esptool():
    for name in ("esptool.py", "esptool"):
        found = shutil.which(name)
        if found:
            return found
    return None


def merge_flash(build_dir, out_path):
    fa_path = os.path.join(build_dir, "flasher_args.json")
    if not os.path.isfile(fa_path):
        eprint("EMU_QEMU_ESP missing flasher_args.json under %s (build firmware first)" % build_dir)
        return 2
    with open(fa_path) as fh:
        fa = json.load(fh)
    flash_files = fa.get("flash_files") or {}
    if not flash_files:
        eprint("EMU_QEMU_ESP flasher_args.json has no flash_files")
        return 2
    pairs = []
    for off, path in sorted(flash_files.items(), key=lambda kv: int(kv[0], 0)):
        full = path if os.path.isabs(path) else os.path.join(build_dir, path)
        if not os.path.isfile(full):
            eprint("EMU_QEMU_ESP missing flash piece: %s" % full)
            return 2
        pairs.extend([off, full])
    chip = (fa.get("extra_esptool_args") or {}).get("chip") or MACHINE
    esptool = find_esptool()
    if not esptool:
        eprint("EMU_QEMU_ESP esptool.py not on PATH (source ESP-IDF export /workspace/env/esp-idf-v5.1.6.sh)")
        return 2
    cmd = [esptool, "--chip", chip, "merge_bin", "-o", out_path, "--fill-flash-size", "4MB"] + pairs
    print("RUN:", " ".join(cmd), flush=True)
    return subprocess.call(cmd)


def maybe_build():
    build_sh = os.path.join(ROOT, "utils", "build_idf5.sh")
    if not os.path.isfile(build_sh):
        eprint("EMU_QEMU_ESP missing utils/build_idf5.sh")
        return 2
    # Ensure IDF_PATH from workspace helper if unset
    if not os.environ.get("IDF_PATH") and os.path.isdir("/workspace/tools/esp-idf-v5.1.6"):
        os.environ["IDF_PATH"] = "/workspace/tools/esp-idf-v5.1.6"
    env = os.environ.copy()
    print("=== build (set-target %s + build) ===" % MACHINE, flush=True)
    rc = subprocess.call(["bash", build_sh, "set-target", MACHINE], cwd=ROOT, env=env)
    if rc != 0:
        return rc
    return subprocess.call(["bash", build_sh, "build"], cwd=ROOT, env=env)


def ensure_flash(flash_path, do_build):
    if flash_path and os.path.isfile(flash_path):
        return flash_path, 0
    build_dir = os.path.join(ROOT, "build")
    default_out = DEFAULT_FLASH
    if os.path.isfile(default_out) and (not flash_path or flash_path == default_out):
        return default_out, 0
    if do_build or not os.path.isfile(os.path.join(build_dir, "flasher_args.json")):
        if not do_build:
            eprint(
                "EMU_QEMU_ESP no flash image at %s and no build/flasher_args.json; "
                "pass --build or build first (./utils/build_idf5.sh build)" % default_out
            )
            return None, 2
        rc = maybe_build()
        if rc != 0:
            eprint("EMU_QEMU_ESP build failed exit=%s" % rc)
            return None, rc
    out = flash_path or default_out
    os.makedirs(os.path.dirname(out), exist_ok=True)
    rc = merge_flash(build_dir, out)
    if rc != 0:
        return None, rc
    return out, 0


def run_qemu(qemu_bin, flash_path, timeout_sec, log_path):
    cmd = [
        qemu_bin,
        "-nographic",
        "-machine",
        MACHINE,
        "-drive",
        "file=%s,if=mtd,format=raw" % flash_path,
    ]
    print(
        "EMU_QEMU_ESP machine=%s flash=%s timeout=%ss qemu=%s"
        % (MACHINE, flash_path, timeout_sec, qemu_bin),
        flush=True,
    )
    out_fh = open(log_path, "w")
    try:
        # Prefer GNU timeout when available (matches CI)
        if shutil.which("timeout") and os.name != "nt":
            full = [
                "timeout",
                "--signal=TERM",
                "--kill-after=10",
                "%ss" % timeout_sec,
            ] + cmd
            proc = subprocess.Popen(
                full,
                stdin=subprocess.DEVNULL,
                stdout=out_fh,
                stderr=subprocess.STDOUT,
            )
            rc = proc.wait()
        else:
            proc = subprocess.Popen(
                cmd,
                stdin=subprocess.DEVNULL,
                stdout=out_fh,
                stderr=subprocess.STDOUT,
            )
            deadline = time.time() + timeout_sec
            rc = None
            while time.time() < deadline:
                rc = proc.poll()
                if rc is not None:
                    break
                time.sleep(0.2)
            if rc is None:
                proc.terminate()
                try:
                    proc.wait(timeout=10)
                except Exception:
                    proc.kill()
                rc = 124
    finally:
        out_fh.close()
    return rc


def judge(rc, log_path):
    try:
        with open(log_path) as fh:
            text = fh.read()
    except OSError:
        text = ""
    if BOOT_OK.search(text):
        if BOOT_PANIC.search(text):
            return "FAIL", "qemu panic"
        return "PASS", "boot evidence (exit/timeout=%s)" % rc
    if rc == 0:
        return "PASS", "qemu exit 0"
    if rc == 124:
        return "FAIL", "qemu timeout no boot banner"
    return "FAIL", "qemu exit %s" % rc


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="On-target Espressif QEMU firmware smoke (%s)." % TARGET_LABEL
    )
    parser.add_argument(
        "--build",
        action="store_true",
        help="Run utils/build_idf5.sh set-target/build before merge if needed",
    )
    parser.add_argument(
        "--flash",
        default=os.environ.get("FLASH", ""),
        help="Path to merged raw flash image (default: build/qemu_flash.bin)",
    )
    parser.add_argument(
        "--timeout",
        type=int,
        default=int(os.environ.get("TIMEOUT_SEC", DEFAULT_TIMEOUT)),
        help="QEMU wall timeout seconds (default %s; CI treats 124+boot as OK)" % DEFAULT_TIMEOUT,
    )
    parser.add_argument(
        "--log",
        default="",
        help="Write QEMU console log here (default: temp file, tail printed)",
    )
    args = parser.parse_args(argv)

    maybe_source_workspace_env()

    print("On-target Espressif QEMU smoke  repo: %s" % ROOT)
    print("Target: %s  (QEMU machine: %s)" % (TARGET_LABEL, MACHINE))
    print("Note: native host unit tests remain utils/run_host_tests.sh (not run inside QEMU).")
    print("")

    qemu = find_qemu()
    if not qemu:
        eprint(
            "EMU_QEMU_ESP missing qemu-system-xtensa "
            "(source /workspace/env/emulators.sh; see /workspace/tools/EMULATORS.txt). "
            "Need Espressif QEMU fork, not stock Ubuntu qemu-system-misc."
        )
        return 2
    if not qemu_has_machine(qemu, MACHINE):
        eprint(
            "EMU_QEMU_ESP qemu at %s lacks -machine %s "
            "(Espressif QEMU required)" % (qemu, MACHINE)
        )
        return 2
    print("OK  qemu-system-xtensa  %s  (-machine %s)" % (qemu, MACHINE))

    flash_arg = args.flash.strip() or None
    flash, rc = ensure_flash(flash_arg, args.build or os.environ.get("BUILD", "") == "1")
    if rc != 0:
        return rc

    log_path = args.log.strip() or os.path.join(
        tempfile.gettempdir(), "run_host_tests_qemu_%s.log" % os.getpid()
    )
    qrc = run_qemu(qemu, flash, args.timeout, log_path)
    status, note = judge(qrc, log_path)
    print("EMU_QEMU_ESP result=%s %s" % (status, note))
    try:
        with open(log_path) as fh:
            lines = fh.readlines()
        print("--- qemu console (tail) ---")
        sys.stdout.write("".join(lines[-40:]))
        if not args.log:
            print("(full log: %s)" % log_path)
    except OSError:
        pass
    return 0 if status == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
