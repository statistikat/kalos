#!/usr/bin/env python3
"""Generate tests/jvm_final_information.h from the item 21-H.0
final-information corpus (validation/evidence/native/final-information,
captured by validation/reference/capture-final-information.R through the
observer FinalInformationProbe.java against the pinned jdplus 3.9.0 jars;
double capture, byte-identical csv files across JVM processes). Every number
is emitted as the raw IEEE-754 bits of the JVM double so the C++ replay is
bit-exact without decimal round-trips. Nodes are deduplicated by the
capture (the stencil evaluates some keys repeatedly); a THROW value marks a
node whose deviance evaluation failed and that NumericalDerivatives.newval
replaced by fx. Regenerate with:
  python3 tools/gen_final_information_golden.py \
      validation/evidence/native/final-information tests/jvm_final_information.h
"""
import csv
import os
import sys


def bits(tok):
    v = int(tok, 16)
    assert 0 <= v <= 0xFFFFFFFFFFFFFFFF, tok
    return v


def dec_bits(tok):
    # %.17g decimal (exact round trip); Java String.format spellings for the
    # non-finite values.
    import struct
    if tok == "NaN":
        return 0x7FF8000000000000
    if tok in ("Infinity", "+Infinity"):
        return 0x7FF0000000000000
    if tok in ("-Infinity",):
        return 0xFFF0000000000000
    d = float(tok)
    return struct.unpack(">Q", struct.pack(">d", d))[0]


def dec_d(field, sep):
    return [dec_bits(t) for t in (x for x in field.split(sep) if x)]


def bits_d(field, sep):
    return [bits(t) for t in (x for x in field.split(sep) if x)]


def main(corpus_dir, dst):
    with open(os.path.join(corpus_dir, "cases.csv")) as f:
        cases = {r["id"]: r for r in csv.DictReader(f)}
    with open(os.path.join(corpus_dir, "capture.csv")) as f:
        caps = list(csv.DictReader(f))
    for col in ("fx_hex", "eps_hex", "checkall_eff", "arlimit_hex",
                "malimit_hex", "epsilon_hex"):
        if col not in caps[0]:
            sys.exit("capture.csv lacks column %s" % col)
    nodes = {}
    with open(os.path.join(corpus_dir, "nodes.csv")) as f:
        for r in csv.DictReader(f):
            nodes.setdefault(r["id"], []).append(r)

    recs = []
    for cap in caps:
        cid = cap["id"]
        case = cases[cid]
        st = cap["status"]
        status = 2 if st.startswith("rej") else (0 if st == "ok" else 1)
        rec = {
            "id": cid,
            "status": status,
            "est": 1 if case["mode"] == "est" else 0,
            "p": int(case["p"]), "bp": int(case["bp"]),
            "q": int(case["q"]), "bq": int(case["bq"]),
            "d": int(case["d"]), "bd": int(case["bd"]),
            "freq": int(case["freq"]),
            "dim": int(cap["dim"]),
            "nobs": int(cap["nobs"]),
            "check_all": int(cap["checkall_eff"]),
            "ar_limit": bits(cap["arlimit_hex"]),
            "ma_limit": bits(cap["malimit_hex"]),
            "epsilon": bits(cap["epsilon_hex"]),
            "point": bits_d(cap["point_hex"], " "),
            "yd": dec_d(cap["yd"], ";") if cap["yd"] else [],
            "eps": bits_d(cap["eps_hex"], " ") if cap["eps_hex"] else [],
            "fx": bits(cap["fx_hex"]) if cap["fx_hex"] else 0,
            "grad": bits_d(cap["grad_hex"], " ") if cap["grad_hex"] else [],
            "hess": bits_d(cap["hess_hex"], " ") if cap["hess_hex"] else [],
            "score": bits_d(cap["score_hex"], " ") if cap["score_hex"] else [],
            "info": bits_d(cap["info_hex"], " ") if cap["info_hex"] else [],
            "asy": bits_d(cap["asy_hex"], " ") if cap["asy_hex"] else [],
            "diag": bits_d(cap["diag_hex"], " ") if cap["diag_hex"] else [],
            "nodes": [
                ([bits(t) for t in r["node"].split(";")],
                 0 if r["value"] == "THROW" else bits(r["value"]),
                 1 if r["value"] == "THROW" else 0)
                for r in nodes.get(cid, [])
            ],
        }
        dim = rec["dim"]
        assert len(rec["point"]) == dim, cid
        if status != 2:
            assert rec["nodes"], cid       # the stencil always evaluates fp[i]
            assert len(rec["eps"]) == dim, cid
            assert not rec["nodes"] or all(len(k) == dim for k, _, _ in rec["nodes"]), cid
        if status == 0:
            assert len(rec["grad"]) == dim and len(rec["score"]) == dim, cid
            assert len(rec["hess"]) == dim * dim, cid
            assert len(rec["info"]) == dim * dim, cid
            assert len(rec["asy"]) == dim * dim, cid
            assert len(rec["diag"]) == dim, cid
        elif status == 1:
            assert not rec["asy"] and not rec["diag"], cid
        else:
            assert not rec["grad"] and not rec["nodes"], cid
        # The base node: the tail evaluates the deviance at the point before
        # the stencil (capture.csv fx_hex; a rejected point threw there, so
        # the probe recorded no stencil nodes at all). Inject it unless the
        # captured table already carries the key - an all-zero eps makes the
        # eager forward nodes newval(i, 0) coincide with the base key.
        key = list(rec["point"])
        same = [n for n in rec["nodes"] if list(n[0]) == key]
        if status == 2:
            rec["nodes"].append((key, 0, 1))
        elif same:
            assert len(same) == 1 and same[0][2] == 0 and same[0][1] == rec["fx"], cid
        else:
            rec["nodes"].append((key, rec["fx"], 0))
        recs.append(rec)

    with open(dst, "w") as f:
        f.write("""// Item 21-H.0 golden replay table for the final-information producer,
// generated from validation/evidence/native/final-information (double JVM
// capture through the FinalInformationProbe observer; see
// capture-final-information.R). All doubles are raw IEEE bit patterns.
// Do not edit by hand; regenerate with tools/gen_final_information_golden.py.
//
// status: 0 available, 1 inverse-failed (both SymmetricMatrix.inverse
// branches threw), 2 final-point-rejected. Nodes are the deduplicated
// deviance evaluations of the stencil plus the base node (the point with
// fx_hex, or its THROW for rejected cases, injected here because the probe
// observes the stencil, not the tail's own base evaluation); is_throw marks
// the newval catch (value replaced by fx). est cases additionally satisfy
// the probe's self-check: a replay of the pinned tail reproduced the
// estimator's own Point/score/information/covariance bit for bit.
#ifndef KALOS_TESTS_JVM_FINAL_INFORMATION_H
#define KALOS_TESTS_JVM_FINAL_INFORMATION_H

#include <cstddef>
#include <cstdint>

namespace golden_fi {

struct Node {
    const std::uint64_t* key;  // dim raw-bit coordinates
    std::uint64_t value;       // raw bits, valid unless is_throw
    std::uint8_t is_throw;     // newval caught the failure and used fx
};

struct Case {
    const char* id;
    int status, est;
    int p, bp, q, bq, d, bd, freq;
    int dim, nobs, check_all;
    std::uint64_t ar_limit, ma_limit, epsilon;
    const std::uint64_t* point;
    const std::uint64_t* yd;   // differenced response (sweep tier)
    const std::uint64_t* eps;
    std::uint64_t fx;
    const std::uint64_t* grad;
    const std::uint64_t* hess;
    const std::uint64_t* score;
    const std::uint64_t* info;
    const std::uint64_t* asy;
    const std::uint64_t* diag;
    const Node* nodes;
    int nodes_len;
};

""")
        for r in recs:
            cid = r["id"]
            w = lambda vals: "static const std::uint64_t k_%s_%s[%d] = {%s};\n" % (
                vals[0], cid, max(len(vals[1]), 1),
                ", ".join("0x%016xULL" % v for v in vals[1]) or "0")
            f.write("static const std::uint64_t k_point_%s[%d] = {%s};\n"
                    % (cid, max(len(r["point"]), 1),
                       ", ".join("0x%016xULL" % v for v in r["point"]) or "0"))
            f.write("static const std::uint64_t k_yd_%s[%d] = {%s};\n"
                    % (cid, max(len(r["yd"]), 1),
                       ", ".join("0x%016xULL" % v for v in r["yd"]) or "0"))
            f.write("static const std::uint64_t k_eps_%s[%d] = {%s};\n"
                    % (cid, max(len(r["eps"]), 1),
                       ", ".join("0x%016xULL" % v for v in r["eps"]) or "0"))
            for name in ("grad", "hess", "score", "info", "asy", "diag"):
                f.write(w((name, r[name])))
            for j, (key, val, thr) in enumerate(r["nodes"]):
                f.write("static const std::uint64_t k_nk_%s_%d[%d] = {%s};\n"
                        % (cid, j, max(len(key), 1),
                           ", ".join("0x%016xULL" % v for v in key) or "0"))
            f.write("static const Node k_nodes_%s[%d] = {\n"
                    % (cid, max(len(r["nodes"]), 1)))
            if r["nodes"]:
                for j, (key, val, thr) in enumerate(r["nodes"]):
                    f.write("    {k_nk_%s_%d, 0x%016xULL, %d},\n"
                            % (cid, j, val, thr))
            else:
                f.write("    {nullptr, 0, 0},\n")
            f.write("};\n")
        f.write("\ninline const Case* table(std::size_t& count) {\n"
                "    static const Case kCases[] = {\n")
        for r in recs:
            cid = r["id"]
            nul = lambda name: ("nullptr" if not r[name]
                                else "k_%s_%s" % (name, cid))
            f.write(
                "        {\"%s\", %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, "
                "%d, 0x%016xULL, 0x%016xULL, 0x%016xULL, "
                "k_point_%s, k_yd_%s, k_eps_%s, 0x%016xULL, "
                "%s, %s, %s, %s, %s, %s, k_nodes_%s, %d},\n"
                % (cid, r["status"], r["est"], r["p"], r["bp"], r["q"],
                   r["bq"], r["d"], r["bd"], r["freq"], r["dim"], r["nobs"],
                   r["check_all"], r["ar_limit"], r["ma_limit"], r["epsilon"],
                   cid, cid, cid, r["fx"],
                   nul("grad"), nul("hess"), nul("score"),
                   nul("info"), nul("asy"), nul("diag"),
                   cid, len(r["nodes"])))
        f.write("    };\n"
                "    count = sizeof(kCases) / sizeof(kCases[0]);\n"
                "    return kCases;\n"
                "}\n\n"
                "}  // namespace golden_fi\n\n"
                "#endif\n")
    print("wrote %s (%d cases, %d nodes)"
          % (dst, len(recs), sum(len(r["nodes"]) for r in recs)))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
