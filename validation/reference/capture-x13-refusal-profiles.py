"""Recompute the nine X13 demand records and verify their complete byte digests."""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
EVIDENCE = HERE.parent / "evidence/native/x13-refusal-jvm-profiles"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--jdk", type=Path, required=True)
    parser.add_argument("--jar-dir", type=Path, action="append", required=True)
    parser.add_argument("--portable-x86", action="store_true")
    panels = parser.add_mutually_exclusive_group()
    panels.add_argument("--estimated-templates", action="store_true",
                        help="replay the stock fixed-structure product panel")
    panels.add_argument("--estimated-selection", action="store_true",
                        help="replay the fixed-structure Henderson and MSR decisions")
    args = parser.parse_args()
    ref = json.loads((EVIDENCE / "reference.json").read_text())
    java = args.jdk / "bin/java"
    properties = subprocess.check_output([java, "-XshowSettings:properties", "-version"],
                                         stderr=subprocess.STDOUT, text=True)
    props = dict(line.lstrip().split(" = ", 1) for line in properties.splitlines() if " = " in line)
    if props.get("java.version") != ref["java_version"] or props.get("java.vendor") != "Eclipse Adoptium":
        raise SystemExit("Replay requires the pinned Temurin JVM")
    arch = props["os.arch"]
    x86 = arch in {"amd64", "x86_64"}
    if (not x86 and arch not in {"arm64", "aarch64"}) or (not x86 and args.portable_x86):
        raise SystemExit(f"Unsupported profile for {arch}")
    options = (["-XX:+UnlockDiagnosticVMOptions", "-XX:DisableIntrinsic=_dexp,_dlog,_dpow,_dcos,_dsin,_dtan"]
               if args.portable_x86 else [])
    profile = "x86_64-default" if x86 and not args.portable_x86 else "portable"
    for name, expected in ref["inputs"].items():
        if digest(EVIDENCE / name) != expected:
            raise SystemExit(f"Input hash mismatch: {name}")
    jars = []
    for name, expected in ref["jars"].items():
        path = next((d / name for d in args.jar_dir if (d / name).is_file()), None)
        if not path or digest(path) != expected:
            raise SystemExit(f"Missing pinned jar or digest mismatch: {name}")
        jars.append(str(path))
    if args.estimated_templates or args.estimated_selection:
        if not x86 or not args.portable_x86:
            raise SystemExit("The fixed-structure panel uses the portable x86-64 profile")
        probe = "X13FixedTemplateProbe"
        panel = "selection" if args.estimated_selection else "templates"
        expected = (EVIDENCE.parent / "x13-estimated-templates" / (panel + "-x86_64.txt")).read_bytes()
        with tempfile.TemporaryDirectory() as directory:
            subprocess.run([args.jdk / "bin/javac", "--release", "21", "-cp", os.pathsep.join(jars),
                            "-d", directory, HERE / (probe + ".java")], check=True)
            command = [java, "-Dline.separator=\n", *options, "-cp", os.pathsep.join([directory, *jars]),
                       probe, EVIDENCE]
            if args.estimated_selection:
                command.append("selection")
            for _ in range(2):
                if subprocess.check_output(command) != expected:
                    raise SystemExit("Stock fixed-structure output differs from the frozen panel")
        print(f"Fixed-structure {panel}: 9 cases, two JVMs, SHA-256 {hashlib.sha256(expected).hexdigest()}")
        return
    with (EVIDENCE / "demand.tsv").open() as source:
        expected_rows = [r for r in csv.DictReader(source, delimiter="\t") if r["profile"] == profile]
    if len(expected_rows) != 9:
        raise SystemExit("Unexpected demand panel size")
    with tempfile.TemporaryDirectory() as directory:
        subprocess.run([args.jdk / "bin/javac", "--release", "21", "-cp", os.pathsep.join(jars),
                        "-d", directory, HERE / "X13RefusalDemandProbe.java"], check=True)
        subprocess.run([java, *options, "-cp", os.pathsep.join([directory, *jars]),
                        "X13RefusalDemandProbe", EVIDENCE, directory], check=True)
        subprocess.run(["Rscript", "--vanilla", HERE / "project-x13-refusal-demand.R", directory], check=True)
        for row in expected_rows:
            path = Path(directory) / f'demand-{row["template"]}-{row["series"]}.txt'
            data = dict(line.split("=", 1) for line in path.read_text().splitlines())
            if digest(path) != row["record_sha256"] or any(data[k] != row[k] for k in
                                                           ("template", "series", "theta", "btheta", "lsr_coef")):
                raise SystemExit(f"Demand record mismatch: {path.name}")
    print(f"{profile} ({arch}): 9/9 complete demand records byte-exact")


if __name__ == "__main__":
    main()
