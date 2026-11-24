"""Replay the seas.csv WK factorizer captures in two fresh JVMs.

Frozen stage for the M6 SEATS Wiener-Kolmogorov factorizer slice. The three
stages replay the same refused profile (the x13-refusal-jvm-profiles seas.csv
series) through the pinned jdplus 3.9.0 TRAMO-SEATS stack:

  wk               SeatsWkFactorizerProbe    - fit facts, ucarima models with
                                               per-component tier results and
                                               the full WK component surface
  factorize-calls  SeatsWkFactorizeCallsProbe - the ledger of every
                                               SymmetricFilter.factorize()
                                               call of the RSA3 run
  robust-stages    SeatsWkRobustStagesProbe   - the robust tier stage walk on
                                               the refused total error
                                               variance filter

Every capture is produced twice, in two independent JVM invocations, and the
two byte streams must be identical. The JVM math flags match the raw TRAMO-
SEATS preset stages of capture-tramo-seats-bridge.py; these captures use the
pinned x86-64 numerical profile.
"""
import argparse
import hashlib
import json
import os
import subprocess
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
EVIDENCE = HERE.parent / "evidence/native/seats-wk-factorizer-tiers"
SEAS = HERE.parent / "evidence/native/x13-refusal-jvm-profiles/seas.csv"

STAGES = {
    "wk": ("SeatsWkFactorizerProbe", "wk-x86_64.txt"),
    "factorize-calls": ("SeatsWkFactorizeCallsProbe", "factorize-calls-x86_64.txt"),
    "robust-stages": ("SeatsWkRobustStagesProbe", "robust-stages-x86_64.txt"),
}


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--jdk", type=Path, required=True, help="Java 21.0.11 JDK home")
    parser.add_argument("--jar-dir", type=Path, action="append", required=True,
                        help="directory containing pinned jars (repeatable)")
    parser.add_argument("--stage", choices=[*STAGES, "all"], default="all")
    parser.add_argument("--output", type=Path,
                        help="write new captures into this directory instead of verifying")
    args = parser.parse_args()
    reference = json.loads((EVIDENCE / "reference.json").read_text())
    if sha256(SEAS) != reference["inputs"]["seas.csv"]:
        raise SystemExit("seas.csv digest differs from the frozen input pin")
    java = args.jdk / "bin/java"
    version = subprocess.check_output([java, "-XshowSettings:properties", "-version"],
                                      stderr=subprocess.STDOUT, text=True)
    if f'version "{reference["java_version"]}"' not in version:
        raise SystemExit(f"unexpected JVM: {version.splitlines()[0]}")
    arch = next((line.split("=", 1)[1].strip() for line in version.splitlines()
                 if line.strip().startswith("os.arch =")), "")
    if arch not in {"x86_64", "amd64"}:
        raise SystemExit("WK factorizer captures use the pinned x86-64 numerical profile")
    jars = []
    for name, digest in reference["jars"].items():
        jar = next((d / name for d in args.jar_dir if (d / name).is_file()), None)
        if jar is None or sha256(jar) != digest:
            raise SystemExit(f"missing pinned jar or checksum mismatch: {name}")
        jars.append(str(jar))
    sources = [HERE / (probe + ".java") for probe, _ in STAGES.values()]
    with tempfile.TemporaryDirectory() as directory:
        subprocess.run([args.jdk / "bin/javac", "--release", "21", "-cp", os.pathsep.join(jars),
                        "-d", directory, *sources], check=True)
        command = {
            "wk": ["SeatsWkFactorizerProbe", SEAS],
            "factorize-calls": ["SeatsWkFactorizeCallsProbe", SEAS],
            "robust-stages": ["SeatsWkRobustStagesProbe", reference["tevar_filter"]],
        }
        for stage, (probe, output_name) in STAGES.items():
            if args.stage not in {stage, "all"}:
                continue
            run = [java, "-Dline.separator=\n", "-XX:+UnlockDiagnosticVMOptions",
                   "-XX:DisableIntrinsic=_dexp,_dlog",
                   "-cp", os.pathsep.join([directory, *jars]), *command[stage]]
            first = subprocess.check_output(run)
            if first != subprocess.check_output(run):
                raise SystemExit(f"{stage}: two independent JVM captures differ")
            digest = hashlib.sha256(first).hexdigest()
            if digest != reference["captures"][stage]:
                raise SystemExit(f"{stage}: capture digest {digest} differs from the frozen pin")
            if args.output:
                (args.output / output_name).write_bytes(first)
            elif first != (EVIDENCE / output_name).read_bytes():
                raise SystemExit(f"{stage}: live capture differs from the frozen evidence")
            print(f"{stage}: two independent JVMs, SHA-256 {digest}")


if __name__ == "__main__":
    main()
