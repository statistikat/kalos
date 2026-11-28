#!/usr/bin/env python3
"""Generate tests/jvm_x13_automatic_td.h from the frozen calendar-detection
evidence corpus (roadmap 21-I stage 1). Sources of truth: the double-captured
JVM TD-only battery (captures/), the native Tier-1 replays (replay/), and the
frozen corpora (drivers/td1-corpora.txt plus the kAir array in
drivers/td1_replay.cpp). Every pinned bit carries its raw AICC margin; the
generator refuses to emit any case whose pinned margins fall below 0.1.
Do not edit the header by hand."""
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EVID = ROOT / "validation" / "evidence" / "native" / "calendar-detection"
OUT = ROOT / "tests" / "jvm_x13_automatic_td.h"

TPLS = ["td2noe", "td7noe", "td2noadj"]
KINDS = ["clean", "tdeff", "air"]
TPL_PARAMS = {"td2noe": (0, "true"), "td7noe": (8, "true"),
              "td2noadj": (0, "false")}
# Final-state JVM agreement, established by the m17-td1 replay qualification
# (see the corpus README): td7noe/air and td2noadj/tdeff are the documented
# ODM-boundary divergences, the clean corpus is the pre-existing gap.
AGREE = {("td2noe", "tdeff"), ("td2noe", "air"), ("td7noe", "tdeff"),
         ("td2noadj", "air")}


def bits(x: float) -> int:
    return struct.unpack("<Q", struct.pack("<d", x))[0]


def series_from_corpora():
    out = {}
    active = None
    for line in (EVID / "drivers" / "td1-corpora.txt").read_text().splitlines():
        head = line.split()
        if head and head[0] == "series":
            active = head[1]
            out[active] = []
            continue
        if active:
            out[active].append(line)
    return out


def air_array():
    txt = (EVID / "drivers" / "td1_replay.cpp").read_text()
    m = re.search(r"const double kAir\[144\] = \{(.*?)\};", txt, re.S)
    return [v.strip().rstrip(",") for v in m.group(1).split() if v.strip()]


def jvm_parse(path):
    txt = path.read_text()

    def one(pat):
        m = re.search(pat, txt)
        return m.group(1) if m else ""

    firings = []
    for line in txt.splitlines():
        if "ATD names" in line:
            aiccs = re.findall(r"aicc=([0-9.eE+-]+)", line)
            firings.append((int(re.match(r"evt (\d+)", line).group(1)),
                            re.search(r"best=(\d+)", line).group(1),
                            float(aiccs[0]), float(aiccs[1])))
    return {
        "log": 1 if one(r"fact \S+ log (\S+)") == "true" else 0,
        "lop": one(r"lopTransform (\S+)"),
        "orders": one(r"fact \S+ orders (\d \d \d \d \d \d)"),
        "variables": int(one(r"fact \S+ variables (\d+)")),
        "ll": float(one(r"ll (\S+) adjll")),
        "aicc": float(one(r"adjll \S+ aicc (\S+) ssq")),
        "firings": firings,
    }


def native_parse(path):
    txt = path.read_text()
    m = re.search(r"^fact log=(\d) mean=(\d) orders (\d \d \d \d \d \d)$",
                  txt, re.M)
    firings = []
    for line in txt.splitlines():
        if line.startswith("evt ") and "atd" in line.split()[1]:
            firings.append((line.split()[1],
                            int(re.search(r"best=(\d+)", line).group(1))))
    best = firings[0][1]
    pre = re.search(r"^auto_calendar preadjust=(\d)", txt, re.M)
    return {
        "log": int(m.group(1)), "mean": int(m.group(2)), "orders": m.group(3),
        "preadjust": int(pre.group(1)) if pre else -1,
        "bests": [b for _, b in firings],
        "ll": float(re.search(r"^fact ll=(\S+)", txt, re.M).group(1)),
        "aicc": float(re.search(r"aicc=(\S+) eff=", txt).group(1)),
        "n_outliers": int(re.search(
            r"^chain final .*?n_outliers=(\d+)", txt, re.M).group(1)),
        "firings": firings, "best_first": int(best),
        "log_margin": abs(float(re.search(
            r"chain transformation .*?aicc_level=([0-9.eE+-]+) "
            r"aicc_log=([0-9.eE+-]+)", txt).group(1)) - float(re.search(
            r"chain transformation .*?aicc_level=([0-9.eE+-]+) "
            r"aicc_log=([0-9.eE+-]+)", txt).group(2))),
    }


def main():
    corpora = series_from_corpora()
    kair = air_array()
    cases = []
    for tpl in TPLS:
        for kind in KINDS:
            j = jvm_parse(EVID / "captures" / f"{tpl}-{kind}.out")
            n = native_parse(EVID / "replay" / f"replay-{tpl}-{kind}-native.out")
            a0, a1 = j["firings"][0][2], j["firings"][0][3]
            td_margin = abs(a0 - a1)
            agree = (tpl, kind) in AGREE
            for name, val in (("log", n["log_margin"]), ("td", td_margin)):
                if val < 0.1:
                    sys.exit(f"margin gate: {tpl}/{kind} {name} margin {val}")
            if agree and n["orders"] != j["orders"]:
                sys.exit(f"agreement set mismatch orders {tpl}/{kind}")
            if int(j["firings"][0][1]) != n["best_first"]:
                sys.exit(f"atd0 best mismatch {tpl}/{kind}")
            jbest = [int(f[1]) for f in j["firings"]]
            for i in range(min(len(jbest), len(n["bests"]))):
                if jbest[i] != n["bests"][i]:
                    sys.exit(f"paired best mismatch {tpl}/{kind} #{i}")
            cases.append(dict(tpl=tpl, kind=kind, series=kind,
                              cl=TPL_PARAMS[tpl][0], adj=TPL_PARAMS[tpl][1],
                              j=j, n=n, jbest=jbest, td_margin=td_margin,
                              agree=agree))

    def arr(name, values):
        out = [f"inline const double {name}[{len(values)}] = {{"]
        for i in range(0, len(values), 8):
            out.append("    " + ", ".join(values[i:i + 8]) + ",")
        out.append("};")
        return "\n".join(out)

    lines = [
        "// Item 21-I stage-1 golden replay table for the automatic trading",
        "// days (ATD) pipeline, generated from",
        "// validation/evidence/native/calendar-detection (frozen JVM TD-only",
        "// battery run1/ + native Tier-1 replays; see the corpus README).",
        "// Doubles are stored as raw IEEE bit patterns. Pins:",
        "//   - the ATD arm selection of every firing, paired by ordinal with",
        "//     the JVM capture (jvm_firings vs native_firings records the",
        "//     documented td7noe/air count difference);",
        "//   - the transformation bit, both pinned against the JVM;",
        "//   - the final orders and the ll/aicc bit patterns (<= 64 ulp): JVM-",
        "//     pinned where the replay qualified (agree=1), native baseline-",
        "//     pinned otherwise (the pre-existing clean-corpus gap and the two",
        "//     ODM-boundary divergences documented in the corpus README).",
        "// Every pinned decision carries its raw AICC margin; all are >= 0.1.",
        "// Do not edit by hand; regenerate with tools/gen_x13_automatic_td_golden.py.",
        "#ifndef KALOS_TESTS_JVM_X13_AUTOMATIC_TD_H",
        "#define KALOS_TESTS_JVM_X13_AUTOMATIC_TD_H",
        "",
        "#include <cstddef>",
        "#include <cstdint>",
        "",
        "namespace golden_x13auto_td {",
        "",
        arr("kClean", corpora["clean"]),
        arr("kTdeff", corpora["tdeff"]),
        arr("kAir", kair),
        "",
        "struct Case {",
        "    const char* tpl;      // td2noe | td7noe | td2noadj",
        "    const char* corpus;   // clean | tdeff | air",
        "    const double* series;",
        "    std::size_t n;",
        "    int start_year;",
        "    int td_clustering;    // request field (0 = td2, 8 = td7)",
        "    bool td_auto_adjust;",
        "    // JVM capture facts (run1/).",
        "    int jvm_log;",
        "    const char* jvm_orders;",
        "    int jvm_variables;    // regression-variable count incl. td columns",
        "    const char* jvm_lop;  // lopTransform: preadjust fork (LeapYear =",
        "                          // td preadjustment; None = column or no td)",
        "    int jvm_firings;",
        "    int jvm_best_first;   // arm selection of the JVM atd0 firing",
        "    // Native replay facts (replay/): the pinned current behaviour.",
        "    int native_log;",
        "    int native_mean;",
        "    const char* native_orders;",
        "    int native_n_outliers;",
        "    int native_preadjust; // auto_calendar preadjust bit (-1: absent)",
        "    int native_firings;",
        "    int atd_best_first;",
        "    int native_bests[4];  // arm selection per firing (-1 past the end)",
        "    int jvm_bests[4];     // JVM arm selection per firing ordinal",
        "    std::uint64_t native_ll;",
        "    std::uint64_t native_aicc;",
        "    // Raw AICC margins behind the pinned decisions.",
        "    double log_margin;",
        "    double td_margin;",
        "    bool agree;           // final state JVM-pinned (not baseline)",
        "};",
        "",
        "inline const Case kCases[] = {",
    ]
    for c in cases:
        j, n = c["j"], c["n"]
        jb = ", ".join(str(v) for v in c["jbest"] + [-1] * (4 - len(c["jbest"])))
        nb = ", ".join(str(v) for v in n["bests"] + [-1] * (4 - len(n["bests"])))
        lines.append(
            f'    {{"{c["tpl"]}", "{c["kind"]}", k{c["series"].capitalize()},'
            f" {len(kair) if c['kind'] == 'air' else len(corpora[c['kind']])},"
            f" {1949 if c['kind'] == 'air' else 1990},"
            f" {c['cl']}, {c['adj']},"
            f" {j['log']}, \"{j['orders']}\", {j['variables']}, \"{j['lop']}\","
            f" {len(j['firings'])}, {j['firings'][0][1]},"
            f" {n['log']}, {n['mean']}, \"{n['orders']}\", {n['n_outliers']},"
            f" {n['preadjust']},"
            f" {len(n['firings'])}, {n['best_first']}, {{{jb}}}, {{{nb}}},"
            f" 0x{bits(n['ll']):016X}, 0x{bits(n['aicc']):016X},"
            f" {n['log_margin']:.10f}, {c['td_margin']:.10f}, "
            f"{'true' if c['agree'] else 'false'}}},")
    lines += [
        "};",
        "inline constexpr std::size_t kCaseCount = sizeof(kCases) / sizeof(kCases[0]);",
        "",
        "}  // namespace golden_x13auto_td",
        "",
        "#endif  // KALOS_TESTS_JVM_X13_AUTOMATIC_TD_H",
        "",
    ]
    OUT.write_text("\n".join(lines))
    print(f"wrote {OUT} ({len(cases)} cases)")


if __name__ == "__main__":
    main()
