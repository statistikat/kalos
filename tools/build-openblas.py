#!/usr/bin/env python3
"""Build the pinned external provider for CI/development (never at package install)."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import subprocess
import tarfile
import tempfile
import urllib.request

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--prefix", type=Path, required=True)
parser.add_argument("--jobs", type=int, default=2)
args = parser.parse_args()
if args.jobs < 1:
    parser.error("--jobs must be positive")
pin = json.loads((Path(__file__).resolve().parents[1] / "dependencies/openblas.lock.json").read_text())
with tempfile.TemporaryDirectory(prefix="kalos-openblas-") as work:
    work = Path(work)
    archive = work / "source.tar.gz"
    urllib.request.urlretrieve(pin["url"], archive)
    if hashlib.sha256(archive.read_bytes()).hexdigest() != pin["sha256"]:
        raise ValueError("OpenBLAS archive checksum mismatch")
    with tarfile.open(archive) as tar:
        tar.extractall(work, filter="data")
    source = work / ("OpenBLAS-" + pin["version"])
    target = "ARMV8" if platform.machine().lower() in ("arm64", "aarch64") else "CORE2"
    settings = ["USE_THREAD=0", "USE_LOCKING=1", "DYNAMIC_ARCH=1", "TARGET="+target,
                "INTERFACE64=0", "NO_CBLAS=0", "NO_LAPACK=0", "NO_LAPACKE=0"]
    subprocess.run(["make", "-j", str(args.jobs), *settings], cwd=source, check=True)
    subprocess.run(["make", "install", "PREFIX="+str(args.prefix.resolve()), *settings], cwd=source, check=True)
