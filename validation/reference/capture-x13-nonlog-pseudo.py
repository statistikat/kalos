"""Capture public monthly non-log pseudo-additive X-13 products on the pinned JVM."""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

here = Path(__file__).resolve().parent
evidence = here.parent / "evidence/native"
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--jdk", type=Path, required=True)
parser.add_argument("--jar-dir", type=Path, action="append", required=True)
parser.add_argument("--stage", choices=["fixed", "automatic"], required=True)
parser.add_argument("--output", type=Path)
args = parser.parse_args()
pins = json.loads((evidence / "x13-refusal-jvm-profiles/reference.json").read_text())
java = args.jdk / "bin/java"
version = subprocess.check_output([java, "-XshowSettings:properties", "-version"], stderr=subprocess.STDOUT, text=True)
if 'version "21.0.11"' not in version or not any("os.arch = " + arch in version for arch in ("x86_64", "amd64")):
    raise SystemExit("the pinned 21.0.11 x86-64 JVM is required")
jars = []
for name, digest in pins["jars"].items():
    if "-base-api-" not in name and "-base-core-" not in name:
        continue
    path = next((directory / name for directory in args.jar_dir if (directory / name).exists()), None)
    if not path or hashlib.sha256(path.read_bytes()).hexdigest() != digest:
        raise SystemExit("missing pinned jar or checksum mismatch: " + name)
    jars.append(str(path))
cases = {"quick": [], ("ao" if args.stage == "fixed" else "noout"): []}
with (evidence / "x13-automatic/inputs.csv").open(newline="") as stream:
    for row in csv.DictReader(stream):
        if row["id"] in cases:
            cases[row["id"]].append(row["line"])
with tempfile.TemporaryDirectory() as directory:
    cp = os.pathsep.join([directory, *jars])
    subprocess.run([args.jdk / "bin/javac", "-cp", cp, "-d", directory, here / "X13AutomaticProbe.java",
                    here / "X13NonlogPseudoProbe.java"], check=True)
    reference = bytearray()
    for lines in cases.values():
        payload = ("\n".join(lines) + "\n").encode()
        command = [java, "-cp", cp, "X13NonlogPseudoProbe", args.stage]
        output = subprocess.check_output(command, input=payload)
        if output != subprocess.check_output(command, input=payload):
            raise SystemExit("independent JVMs disagree")
        reference.extend(output)
target = evidence / "x13-nonlog-pseudo" / (args.stage + "-x86_64.txt")
if args.output:
    args.output.write_bytes(reference)
elif reference != target.read_bytes():
    raise SystemExit("capture differs from the frozen reference")
print(args.stage, hashlib.sha256(reference).hexdigest())
