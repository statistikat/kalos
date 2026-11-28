"""Replay the thirteen-case X-13 irregular-forecast (i_f) capture in two fresh JVMs."""
import argparse
import hashlib
import json
from pathlib import Path
import os
import subprocess
import tempfile


HERE = Path(__file__).resolve().parent
EVIDENCE = HERE.parent / "evidence/native/x13-automatic-iforecast"
INPUTS = HERE.parent / "evidence/native/x13-refusal-jvm-profiles"
PROBE = HERE / "X13IrregularForecastProbe.java"
FROZEN = "iforecast-x86_64.txt"


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--jdk", type=Path, required=True, help="Java 21.0.11 JDK home")
    parser.add_argument("--jar-dir", type=Path, action="append", required=True,
                        help="directory containing pinned jars (repeatable)")
    parser.add_argument("--output", type=Path, help="write a new capture instead of "
                        "verifying the frozen battery")
    args = parser.parse_args()
    reference = json.loads((EVIDENCE / "reference.json").read_text())
    for name, digest in reference["inputs"].items():
        if sha256(INPUTS / name) != digest:
            raise SystemExit(f"{name} digest differs from the frozen input")
    if sha256(PROBE) != reference["probe_sha256"]:
        raise SystemExit("X13IrregularForecastProbe.java digest differs from the frozen probe")
    java = args.jdk / "bin/java"
    version = subprocess.check_output([java, "-XshowSettings:properties", "-version"],
                                      stderr=subprocess.STDOUT, text=True)
    if f'version "{reference["java_version"]}"' not in version:
        raise SystemExit(f"unexpected JVM: {version.splitlines()[0]}")
    arch = next((line.split("=", 1)[1].strip() for line in version.splitlines()
                 if line.strip().startswith("os.arch =")), "")
    if arch not in {"x86_64", "amd64"}:
        raise SystemExit("the i_f battery uses the pinned x86-64 numerical profile")
    jars = []
    for name, digest in reference["jars"].items():
        jar = next((d / name for d in args.jar_dir if (d / name).is_file()), None)
        if jar is None or sha256(jar) != digest:
            raise SystemExit(f"missing pinned jar or checksum mismatch: {name}")
        jars.append(str(jar))
    with tempfile.TemporaryDirectory() as directory:
        subprocess.run([args.jdk / "bin/javac", "--release", "21", "-cp", os.pathsep.join(jars),
                        "-d", directory, PROBE], check=True)
        command = [java, "-XX:+UnlockDiagnosticVMOptions", "-XX:DisableIntrinsic=_dexp,_dlog",
                   "-cp", os.pathsep.join([directory, *jars]), "X13IrregularForecastProbe", INPUTS]
        first = subprocess.check_output(command)
        if first != subprocess.check_output(command):
            raise SystemExit("two independent JVM captures differ")
    if args.output:
        args.output.write_bytes(first)
    elif first != (EVIDENCE / FROZEN).read_bytes():
        raise SystemExit("live capture differs from the frozen i_f battery evidence")
    print(f"x13-automatic-iforecast battery: two independent JVMs, SHA-256 {hashlib.sha256(first).hexdigest()}")


if __name__ == "__main__":
    main()
