#!/usr/bin/env python3
"""Check relocated historical evidence against the original split manifest."""
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
original = json.loads((root / "validation/migration/pre-split-manifest.json").read_text())["files"]
checked = 0
groups = [
    (root / "validation/fixtures", "inst/tinytest/fixtures/"),
    (root / "validation/migration/source-snapshot", ""),
    (root / "validation/reference/information", "tools/reference/information/"),
    (root / "validation/reference/timings", "tools/reference/timings/"),
]
for directory, prefix in groups:
    for path in directory.rglob("*"):
        if not path.is_file():
            continue
        name = prefix + path.relative_to(directory).as_posix()
        if name not in original:
            # New post-split evidence belongs outside these immutable directories.
            raise ValueError(f"Unrecorded historical artifact: {name}")
        if hashlib.sha256(path.read_bytes()).hexdigest() != original[name]:
            raise ValueError(f"Historical artifact changed: {name}")
        checked += 1
print(f"Verified {checked} historical source, fixture and report files without rebaselining")
