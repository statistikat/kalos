#!/usr/bin/env python3
"""Generate tests/jvm_tramo_arma.h from the item 23-D tramo arma-selection
corpus (validation/evidence/native/tramo-arma-selection, captured through the
observer TramoArmaSelectionProbe.java against the pinned jdplus 3.9.0 jars;
double capture, byte-identical csv files across JVM processes, class-load
witnesses under loadlog-*.txt pinning the HannanRissanen + FastKalmanFilter
candidate machine against zero ML-side loads).

The golden is the captured LINE GRAMMAR itself: the JVM-echoed module facts
(direct_sel, the five dpanel rows of gePreferredModels, arm_result, after)
and the raw IEEE-754 bits of the olsResiduals hand-over plus the protocol
header fields carrying the request (orders d,bd,p,q,bp,bq; seasonal; the
acceptWhiteNoise flag; nobs). The replay layer (tests/test_tramo_arma.cpp)
drives kalos::select_tramo_arma from the header tokens and compares every
fact line and ProcessingLog event line bit-pattern-for-bit-pattern. The
pcnt/maxspec gate lines and the resvals hand-over are pinned inert (the gate
is observable through the panel and result); the quarterly case (D11 refusal)
and the constant-series case (olsResiduals fails upstream, no residual block
exists) are record-only. Regenerate with:
  python3 tools/gen_tramo_arma_golden.py \
      validation/evidence/native/tramo-arma-selection tests/jvm_tramo_arma.h
"""
import csv
import os
import struct
import sys

NAN_BITS = 0x7FF8000000000000

# Fact keys the replay asserts; pcnt/maxspec/resvals/in/req stay inert.
MODULE_KEYS = ("direct_sel", "dpanel", "arm_result", "after")


def cstr(s):
    out = ['"']
    for ch in s:
        o = ord(ch)
        if ch == '"':
            out.append('\\"')
        elif ch == "\\":
            out.append("\\\\")
        elif 32 <= o < 127:
            out.append(ch)
        else:
            out.append("\\x%02x" % o)
    out.append('"')
    return "".join(out)


def value_bits(tok):
    if tok == "nan":
        return NAN_BITS
    return struct.unpack(">Q", struct.pack(">d", float(tok)))[0]


def yes(tok):
    return 1 if tok in ("y", "true", "1") else 0


def main(corpus_dir, dst):
    facts = {}      # id -> ordered [(key, value)]
    with open(os.path.join(corpus_dir, "arma-facts.csv"), newline="") as fh:
        for row in csv.DictReader(fh):
            facts.setdefault(row["id"], []).append((row["key"], row["value"]))

    events = {}     # id -> ordered [line]
    with open(os.path.join(corpus_dir, "arma-events.csv"), newline="") as fh:
        for row in csv.DictReader(fh):
            events.setdefault(row["id"], []).append(
                "evt %s %s %s %s det %s" % (row["seq"], row["kind"], row["depth"],
                    row["msg"], row["detail"]))

    # Request fields from the protocol header line; residuals from resvals.
    cases = {}      # id -> dict
    order = []
    with open(os.path.join(corpus_dir, "inputs.csv"), newline="") as fh:
        for row in csv.DictReader(fh):
            cid, line = row["id"], row["line"]
            tok = line.split(" ")
            if cid not in cases:
                order.append(cid)
                cases[cid] = {}
            if tok[0] == "in":
                d = cases[cid]
                d["n"] = int(tok[3])
                d["nobs"] = int(tok[5])
                d["period"] = int(tok[7])
                spec = tok[9].replace("(", " ").replace(")", " ").replace(",", " ")
                inner = spec.split()
                d["p0"], d["d0"], d["q0"] = int(inner[0]), int(inner[1]), int(inner[2])
                d["bp0"], d["bd0"], d["bq0"] = int(inner[3]), int(inner[4]), int(inner[5])
            elif tok[0] == "resvals":
                d = cases[cid]
                if tok[2] == "error":
                    d["res_error"] = tok[3]
                else:
                    d["res"] = [value_bits(t) for t in tok[3:]]
        # seasonal/wn come from the probe header echoed in the req line
    with open(os.path.join(corpus_dir, "inputs.csv"), newline="") as fh:
        for row in csv.DictReader(fh):
            tok = row["line"].split(" ")
            if tok[0] == "req":
                d = cases[row["id"]]
                d["seasonal"] = yes(tok[5])
                d["wn"] = yes(tok[7])

    lines = []
    arrays = []
    structs = []
    for cid in order:
        d = cases[cid]
        want = []
        for key, value in facts.get(cid, []):
            if key in MODULE_KEYS:
                want.append("%s %s" % (key, value))
        want.extend(events.get(cid, []))
        res = d.get("res")
        replay = 1 if d["period"] == 12 and res is not None else 0
        rname = "r_%s" % cid
        wname = "w_%s" % cid
        if res:
            arrays.append("static const std::uint64_t %s[] = {\n    %s\n};" % (
                rname, ", ".join("0x%016xULL" % b for b in res)))
        else:
            arrays.append("static const std::uint64_t %s[] = {0};" % rname)
        arrays.append("static const char* const %s[] = {\n%s\n};" % (
            wname, "\n".join("    %s," % cstr(w) for w in want)))
        structs.append(
            "    {%s, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %s, %s, "
            "%d, %s}," % (
                cstr(cid), d["period"], d["seasonal"], d["wn"], d["nobs"],
                d["d0"], d["bd0"], d["p0"], d["q0"], d["bp0"], d["bq0"],
                replay, (len(res) if res else 0), rname, cstr(d.get("res_error", "")),
                len(want), wname))

    out = []
    out.append("// Generated by tools/gen_tramo_arma_golden.py - do not edit.")
    out.append("// Item 23-D tramo arma-selection golden: the live tramo.internal.")
    out.append("// ArmaModule facts captured through validation/reference/")
    out.append("// TramoArmaSelectionProbe.java from the pinned jdplus 3.9.0 jars")
    out.append("// (corpus validation/evidence/native/tramo-arma-selection, double")
    out.append("// capture byte-identical; load witnesses pin HannanRissanen +")
    out.append("// FastKalmanFilter candidate fits with zero ML-side class loads).")
    with open(os.path.join(corpus_dir, "metadata.R")) as fh:
        for line in fh:
            if line.startswith("probe_sha256"):
                out.append("// " + line.strip().replace('"', ""))
    out.append("// jvm = 21 (OpenJDK, G2 intrinsic-disable recipe)")
    out.append("// MODULE_KEYS lines are replayed by tests/test_tramo_arma.cpp; the")
    out.append("// gate lines (pcnt/maxspec), the resvals hand-over and the non-monthly")
    out.append("// and upstream-failure cases are pinned inert.")
    out.append("#ifndef KALOS_TESTS_JVM_TRAMO_ARMA_H")
    out.append("#define KALOS_TESTS_JVM_TRAMO_ARMA_H")
    out.append("")
    out.append("#include <cstddef>")
    out.append("#include <cstdint>")
    out.append("")
    out.append("namespace kalos::test {")
    out.append("")
    out.append("struct TArmaCase {")
    out.append("    const char* id;")
    out.append("    int period, seasonal, wn, nobs;")
    out.append("    int d0, bd0, p0, q0, bp0, bq0;")
    out.append("    int replay;            // 1: monthly case with a residual hand-over")
    out.append("    std::size_t nres;")
    out.append("    const std::uint64_t* res;")
    out.append("    const char* res_error; // olsResiduals exception (record only)")
    out.append("    std::size_t nwant;")
    out.append("    const char* const* want;")
    out.append("};")
    out.append("")
    out.extend(arrays)
    out.append("")
    out.append("static const TArmaCase kArmaCases[] = {")
    out.extend(structs)
    out.append("};")
    out.append("constexpr std::size_t kArmaCasesN = sizeof(kArmaCases) / sizeof(kArmaCases[0]);")
    out.append("")
    out.append("} // namespace kalos::test")
    out.append("")
    out.append("#endif // KALOS_TESTS_JVM_TRAMO_ARMA_H")
    with open(dst, "w") as fh:
        fh.write("\n".join(out) + "\n")
    print("wrote %s: %d cases, %d replay" % (
        dst, len(order), sum(1 for c in order
            if cases[c]["period"] == 12 and "res" in cases[c])))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
