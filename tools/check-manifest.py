#!/usr/bin/env python3
"""Fail when the export manifest misses tracked core sources or fixtures."""
import json
from pathlib import Path, PurePosixPath
import re
import subprocess

root = Path(__file__).resolve().parents[1]


def tracked():
    listing = subprocess.check_output(
        ["git", "-C", str(root), "ls-files"], text=True)
    return set(listing.split())


def add_library_sources():
    cmake = (root / "CMakeLists.txt").read_text()
    match = re.search(r"add_library\(kalos STATIC(?P<sources>.*?)\)", cmake, re.S)
    if not match:
        raise ValueError("No add_library(kalos STATIC ...) block in CMakeLists.txt")
    return {token for token in match.group("sources").split() if token != "STATIC"}


def report(kind, entries):
    raise ValueError(
        f"tools/source-files.json [{kind}] misses {len(entries)} tracked files: "
        + ", ".join(sorted(entries)))


def main():
    inventory = json.loads((root / "tools/source-files.json").read_text())
    if inventory["schema_version"] != 1:
        raise ValueError("Unsupported manifest schema")
    core, fixtures = inventory["core"], inventory["fixtures"]
    for name in core + fixtures:
        path = PurePosixPath(name)
        if path.is_absolute() or ".." in path.parts or str(path) != name:
            raise ValueError(f"Unsafe manifest path: {name}")
    for label, entries in (("core", core), ("fixtures", fixtures)):
        if len(set(entries)) != len(entries):
            raise ValueError(f"Duplicate [{label}] manifest entries")
        if entries != sorted(entries):
            raise ValueError(f"[{label}] manifest entries are not sorted")
    files = tracked()
    # The generated revision header exists only inside export snapshots.
    required_core = (add_library_sources()
                     | {f for f in files if f.startswith(
                         ("src/", "include/", "backends/", "cmake/"))}
                     | {"CMakeLists.txt", "cmake/FindKalosOpenBLAS.cmake"}
                     - {"include/kalos/revision.hpp"})
    missing = required_core - set(core)
    if missing:
        report("core", missing)
    stale = set(core) - files
    if stale:
        raise ValueError("tools/source-files.json [core] lists untracked files: "
                         + ", ".join(sorted(stale)))
    required_fixtures = {f for f in files if f.startswith("validation/fixtures/")}
    missing = required_fixtures - set(fixtures)
    if missing:
        report("fixtures", missing)
    stale = set(fixtures) - files
    if stale:
        raise ValueError("tools/source-files.json [fixtures] lists untracked files: "
                         + ", ".join(sorted(stale)))
    print(f"Export manifest covers {len(core)} core and {len(fixtures)} fixture "
          f"files; every add_library source, public header and fixture is listed")


if __name__ == "__main__":
    main()
