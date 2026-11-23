"""Replay the five pinned bridge observations in two fresh JVMs."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile


HERE = Path(__file__).resolve().parent
EVIDENCE = HERE.parent / "evidence/native/tramo-seats-bridge"
CASES = EVIDENCE.parent / "tramo-fit/cases.txt"


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--jdk", type=Path, required=True, help="Java 21.0.11 JDK home")
    parser.add_argument("--jar-dir", type=Path, action="append", required=True,
                        help="directory containing pinned jars (repeatable)")
    parser.add_argument("--stage", choices=["bridge", "merge", "rsa0", "rsa0-forecast-errors", "rsa0-residuals"], default="bridge")
    parser.add_argument("--output", type=Path, help="write a new capture instead of verifying the frozen file")
    args = parser.parse_args()
    reference = json.loads((EVIDENCE / "reference.json").read_text())
    if sha256(CASES) != reference["inputs_sha256"]:
        raise SystemExit("23-F source cases digest differs")
    java = args.jdk / "bin/java"
    version = subprocess.check_output([java, "-XshowSettings:properties", "-version"], stderr=subprocess.STDOUT, text=True)
    if f'version "{reference["java_version"]}"' not in version:
        raise SystemExit(f"unexpected JVM: {version.splitlines()[0]}")

    arch = next((line.split("=", 1)[1].strip() for line in version.splitlines()
                 if line.strip().startswith("os.arch =")), "")
    output_name = "bridge.txt"
    if args.stage == "merge":
        if arch in {"aarch64", "arm64"}:
            output_name = "merge.txt"
        elif arch in {"x86_64", "amd64"}:
            output_name = "merge-x86_64.txt"
        else:
            raise SystemExit(f"no frozen merge reference for JVM architecture {arch!r}")
    if args.stage.startswith("rsa0"):
        if arch not in {"x86_64", "amd64"}:
            raise SystemExit("RSA0 uses the pinned x86-64 numerical profile")
        output_name = args.stage + "-x86_64.txt"
    jars = []
    for name, digest in reference["jars"].items():
        jar = next((d / name for d in args.jar_dir if (d / name).is_file()), None)
        if jar is None or sha256(jar) != digest:
            raise SystemExit(f"missing pinned jar or checksum mismatch: {name}")
        jars.append(str(jar))
    with tempfile.TemporaryDirectory() as directory:
        subprocess.run([args.jdk / "bin/javac", "--release", "21", "-cp", os.pathsep.join(jars),
                        "-d", directory, HERE / "TramoSeatsBridgeProbe.java"], check=True)
        command = [java, "-Dline.separator=\n", "-XX:+UnlockDiagnosticVMOptions", "-XX:DisableIntrinsic=_dexp,_dlog",
                   "-cp", os.pathsep.join([directory, *jars]), "TramoSeatsBridgeProbe", CASES, args.stage]
        first = subprocess.check_output(command)
        if first != subprocess.check_output(command):
            raise SystemExit("two independent JVM captures differ")
    if args.output:
        args.output.write_bytes(first)
    elif first != (EVIDENCE / output_name).read_bytes():
        raise SystemExit("live capture differs from the frozen bridge evidence")
    print(f"{args.stage}: two independent JVMs, SHA-256 {hashlib.sha256(first).hexdigest()}")


if __name__ == "__main__":
    main()
