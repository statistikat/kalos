"""Replay fitted-model, public RSA0, and calendar references in two fresh JVMs."""
import argparse
import csv
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
    parser.add_argument("--stage", choices=["bridge", "merge", "rsa0", "rsa0-forecast-errors",
                        "rsa0-residuals", "rsa0-ljung-box", "rsa0-doornik-hansen", "rsa1", "rsa1-level-effects", "rsa1-level-tc", "rsa1-log-tc", "rsa1-residual-diagnostics", "rsa1-forecast-errors", "rsa3", "rsa3-additional", "rsa3-mean-retry", "rsa3-mean-retry-diagnostics", "rsa3-ref-retry", "rsa3-ref-retry-wk", "rsa3-ref-retry-diagnostics", "rsa3-ref-retry-forecast-errors", "rsa3-residual-diagnostics", "rsa3-forecast-errors", "rsafull", "rsafull-calendar-effect", "rsafull-calendar-wk", "rsafull-residual-diagnostics", "rsafull-forecast-errors", "rsafull-reference", "rsafull-late-calendar", "rsafull-late-calendar-effect", "rsafull-late-forecast-errors", "rsafull-late-residual-diagnostics", "rsafull-late-wk",
                        "adjusted-seasonality", "combined-seasonality", "combined-seasonality-fixed", "combined-seasonality-automatic", "combined-seasonality-stock", "calendar-ftest", "trfull-calendar", "residual-td", "td-controller", "fast-calendar", "spectrum-cos", "seasonal-peaks", "seasonal-f", "calendar-outliers", "td-controller-outliers"], default="bridge")
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
    calendar_outputs = {"calendar-ftest": "panel.txt", "trfull-calendar": "full.txt", "residual-td": "residual-td.txt",
                        "td-controller": "td-controller.txt", "fast-calendar": "fast-calendar.txt",
                        "calendar-outliers": "calendar-outliers.txt", "td-controller-outliers": "td-controller-outliers.txt"}
    if args.stage in calendar_outputs:
        if arch not in {"x86_64", "amd64"}:
            raise SystemExit("TRAMO calendar F-test uses the pinned x86-64 numerical profile")
        output_name = "../tramo-calendar-ftest/" + calendar_outputs[args.stage]
    if args.stage in {"seasonal-peaks", "seasonal-f"}:
        if arch not in {"x86_64", "amd64"}:
            raise SystemExit("monthly seasonality tests use the pinned x86-64 numerical profile")
        name = "monthly-spectral-peaks.txt" if args.stage == "seasonal-peaks" else "seasonal-f.txt"
        output_name = "../tramo-differencing-battery/" + name
    if args.stage.startswith("rsa") or args.stage == "spectrum-cos":
        if arch not in {"x86_64", "amd64"}:
            raise SystemExit("raw TRAMO-SEATS presets use the pinned x86-64 numerical profile")
        output_name = args.stage + "-x86_64.txt"
    pins = dict(reference["jars"])
    if args.stage.startswith("combined-seasonality") or args.stage == "adjusted-seasonality":
        if arch not in {"x86_64", "amd64"}:
            raise SystemExit("combined seasonality uses the pinned x86-64 numerical profile")
        names = {"combined-seasonality": "public", "combined-seasonality-fixed": "fixed-x13",
                 "combined-seasonality-automatic": "automatic-x13", "combined-seasonality-stock": "stock"}
        output_name = ("../adjusted-seasonality/public-x86_64.txt" if args.stage == "adjusted-seasonality" else
                       "../combined-seasonality/" + names[args.stage] + "-x86_64.txt")
        x13 = json.loads((EVIDENCE.parent / "x13-refusal-jvm-profiles/reference.json").read_text())
        pins.update({name: digest for name, digest in x13["jars"].items()
                     if name.startswith(("jdplus-x13-base-api-", "jdplus-x13-base-core-"))})
    jars = []
    for name, digest in pins.items():
        jar = next((d / name for d in args.jar_dir if (d / name).is_file()), None)
        if jar is None or sha256(jar) != digest:
            raise SystemExit(f"missing pinned jar or checksum mismatch: {name}")
        jars.append(str(jar))
    calendar_probes = {"calendar-ftest": "CalendarFTestProbe", "trfull-calendar": "TramoFullCalendarProbe",
                       "residual-td": "TramoResidualTradingDaysProbe",
                       "td-controller": "TramoTradingDaysControllerProbe", "fast-calendar": "TramoFastCalendarProbe",
                       "seasonal-f": "TramoSeasonalFProbe", "calendar-outliers": "TramoCalendarOutliersProbe",
                       "td-controller-outliers": "TramoTradingDaysControllerProbe"}
    probe = calendar_probes.get(args.stage, "TramoSeatsBridgeProbe")
    arguments = [] if args.stage in calendar_probes else [CASES, args.stage]
    if args.stage == "td-controller-outliers":
        arguments = ["outliers"]
    if args.stage.startswith("rsafull"):
        calendar_inputs = EVIDENCE.parent / "tramo-calendar-ftest/full.txt"
        if sha256(calendar_inputs) != reference["calendar_inputs_sha256"]:
            raise SystemExit("TRfull calendar source cases digest differs")
        arguments.append(calendar_inputs)
    if args.stage == "seasonal-peaks":
        probe = "TramoSeasonalPeaksProbe"
        arguments = []
    if args.stage.startswith("rsa3-ref-retry"):
        probe = "TramoRefStatsRetryProbe"
        arguments = [args.stage]
    if args.stage.startswith("combined-seasonality"):
        probe = "CombinedSeasonalityProbe"
        arguments = [EVIDENCE / "rsa3-x86_64.txt", EVIDENCE / "rsa3-ref-retry-x86_64.txt"]
        if args.stage == "combined-seasonality-stock":
            arguments = ["stock", EVIDENCE / "rsafull-x86_64.txt", EVIDENCE / "rsafull-late-calendar-x86_64.txt",
                         EVIDENCE.parent / "x13-refusal-jvm-profiles"]
        if args.stage.endswith("-fixed"):
            arguments.append("fixed")
    sources = [HERE / (probe + ".java")]
    if args.stage.startswith("rsa3-ref-retry"):
        sources.append(HERE / "TramoSeatsBridgeProbe.java")
    if args.stage in {"trfull-calendar", "residual-td", "td-controller", "fast-calendar", "seasonal-f", "calendar-outliers", "td-controller-outliers"}:
        sources.append(HERE / "CalendarFTestProbe.java")
    if args.stage == "adjusted-seasonality":
        sources = [HERE / "AdjustedSeasonalityProbe.java"]
        probe = "AdjustedSeasonalityProbe"
        arguments = [EVIDENCE.parent / "x13-refusal-jvm-profiles"]
    if args.stage == "calendar-outliers":
        sources.append(EVIDENCE.parent / "tramo-outlier-rounds/Scratch23g.java")
    with tempfile.TemporaryDirectory() as directory:
        if args.stage == "combined-seasonality-automatic":
            sources.extend([HERE / "X13AutomaticProbe.java", HERE / "AutomaticCombinedSeasonalityProbe.java"])
            probe = "AutomaticCombinedSeasonalityProbe"
            inputs = {}
            with (EVIDENCE.parent / "x13-automatic/inputs.csv").open(newline="") as stream:
                for row in csv.DictReader(stream):
                    inputs.setdefault(row["id"], []).append(row["line"])
            arguments = []
            for cid, mode, nb, nf in [("quick", "Undefined", 0, 0), ("ao", "Undefined", 0, 0),
                    ("mode_logadd", "LogAdditive", 3, 6), ("mode_pseudo", "PseudoAdditive", 3, 6),
                    ("quick", "PseudoAdditive", 0, 0), ("noout", "PseudoAdditive", 3, 6)]:
                request = Path(directory) / f"request-{len(arguments)}.txt"
                request.write_text("\n".join([f"{cid} {mode} {nb} {nf}", *inputs[cid]]) + "\n")
                arguments.append(request)
        subprocess.run([args.jdk / "bin/javac", "--release", "21", "-cp", os.pathsep.join(jars),
                        "-d", directory, *sources], check=True)
        command = [java, "-Dline.separator=\n", "-XX:+UnlockDiagnosticVMOptions", "-XX:DisableIntrinsic=_dexp,_dlog",
                   "-cp", os.pathsep.join([directory, *jars]), probe, *arguments]
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
