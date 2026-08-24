#!/usr/bin/env python3
"""Git tag for OpenLCB SNIP softwareVersion (Windows 11 and Ubuntu).

Exact annotated/lightweight tag, clean tree:  v2.04
Any later commit or a dirty/untracked tree:   v2.04+
No tag, git missing, or ill-formatted tag:    v0.01+

Well-formatted tags look like v2.04 or v1.0.0 (v + digits + dots).
OpenLCB SNIP software_version is 20 chars + NUL. Do not put this in
CANONICAL_VERSION (that is the CDI schema; changing it factory-resets).
"""
from __future__ import print_function

import argparse
import os
import re
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SNIP_MAX = 20
DEFAULT_SNIP = "v0.01+"
VERSION_TAG = "v[0-9]*"
WELL_TAG = re.compile(r"^v[0-9]+(\.[0-9]+)+$")


def clip_snip(text):
    if text is None:
        return DEFAULT_SNIP
    if len(text) <= SNIP_MAX:
        return text
    if SNIP_MAX < 1:
        return ""
    return text[: SNIP_MAX - 1] + "+"


def well_formatted_tag(tag):
    return bool(tag and WELL_TAG.match(tag))


def well_formatted_snip(text):
    if text and text.endswith("+"):
        return well_formatted_tag(text[:-1])
    return well_formatted_tag(text)


def format_snip_version(tag, exact, dirty):
    if not well_formatted_tag(tag):
        return DEFAULT_SNIP
    if exact and not dirty:
        return clip_snip(tag)
    return clip_snip(tag + "+")


def to_c_token(text):
    """Make a SNIP string safe as -DRR_GIT_VERSION=token on Win11 and Ubuntu."""
    if not text:
        return DEFAULT_SNIP
    out = []
    for ch in text:
        if ch.isalnum() or ch in "._+-":
            out.append(ch)
        else:
            out.append("_")
    token = "".join(out).strip("_")
    if not token:
        return DEFAULT_SNIP
    if token[0] in "0123456789.+-":
        token = "v" + token
    return clip_snip(token)


def sanitize_cflag(text):
    token = to_c_token(text)
    if not well_formatted_snip(token):
        return DEFAULT_SNIP
    return token


def _git(repo, args):
    cmd = ["git"] + args
    try:
        out = subprocess.check_output(
            cmd, cwd=repo, stderr=subprocess.STDOUT, universal_newlines=True
        )
    except (OSError, subprocess.CalledProcessError):
        return None
    return out.strip()


def read_repo_version(repo):
    tag = _git(repo, ["describe", "--tags", "--match", VERSION_TAG, "--abbrev=0"])
    if not well_formatted_tag(tag):
        return DEFAULT_SNIP
    exact = _git(repo, ["describe", "--tags", "--match", VERSION_TAG, "--exact-match"])
    if exact is None and tag:
        exact = _git(repo, ["describe", "--tags", "--exact-match"])
    porcelain = _git(repo, ["status", "--porcelain"])
    dirty = porcelain is not None and porcelain != ""
    is_exact = exact == tag
    return to_c_token(format_snip_version(tag, is_exact, dirty))


def selftest():
    assert format_snip_version("v2.04", True, False) == "v2.04"
    assert format_snip_version("v2.04", True, True) == "v2.04+"
    assert format_snip_version("v2.04", False, False) == "v2.04+"
    assert format_snip_version("v2.04", False, True) == "v2.04+"
    assert format_snip_version("", True, False) == DEFAULT_SNIP
    assert format_snip_version(None, False, True) == DEFAULT_SNIP
    assert format_snip_version("CLS_Works!!", True, False) == DEFAULT_SNIP
    assert format_snip_version("First_Win11", True, False) == DEFAULT_SNIP
    assert well_formatted_tag("v2.04")
    assert well_formatted_tag("v1.0.0")
    assert not well_formatted_tag("v2")
    assert not well_formatted_tag("CLS_wifi_broken")
    assert sanitize_cflag("v2.04+") == "v2.04+"
    assert sanitize_cflag("v2.04;rm") == DEFAULT_SNIP
    assert sanitize_cflag("") == DEFAULT_SNIP
    assert sanitize_cflag("CLS_wifi_broken+") == DEFAULT_SNIP
    assert clip_snip("v2.04") == "v2.04"
    assert len(clip_snip("v" + ("0" * 30))) == SNIP_MAX
    assert clip_snip("v" + ("0" * 30)).endswith("+")
    print("git_version selftest ok")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", default=ROOT)
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()
    if args.selftest:
        return selftest()
    print(sanitize_cflag(read_repo_version(args.repo)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
