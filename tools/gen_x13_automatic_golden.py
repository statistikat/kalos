#!/usr/bin/env python3
"""Generate tests/jvm_x13_automatic.h from the item 21-H.2 JVM oracle
corpus (validation/evidence/native/x13-automatic, captured by
validation/reference/capture-x13-automatic.R through the X13AutomaticProbe
observer - the decoder-shaped automatic pipeline run on the pinned jdplus
3.9.0 jars; double capture, byte-identical data csv files across JVM
processes).

Every JVM double is emitted as its raw IEEE-754 bit pattern so the C++
replay is bit-exact without decimal round-trips. The request block mirrors
the probe's spec echo (req fact): ub1/ub2 are stored in MODULE space, the
exact IEEE reciprocals 1/spec the decoder hands to DifferencingModule. The
decision block projects the observer event stream (AICC display pair,
differencing quadruple, ARMA selection and merged panel, ODM selections
with the cv, cascade final spec); the expected block carries the final
description facts plus the landing coefficients/statistics and three
representative downstream tables (out, D11final, E11 - the full table
sweep belongs to tools/verify_x13_automatic.cpp).

Regenerate with:
  python3 tools/gen_x13_automatic_golden.py \
      validation/evidence/native/x13-automatic tests/jvm_x13_automatic.h
"""
import csv
import os
import re
import struct
import sys


def dec_bits(tok):
    if tok == "NaN":
        return 0x7FF8000000000000
    if tok in ("Infinity", "+Infinity"):
        return 0x7FF0000000000000
    if tok == "-Infinity":
        return 0xFFF0000000000000
    return struct.unpack(">Q", struct.pack(">d", float(tok)))[0]


def reals(field, sep=";"):
    return [dec_bits(t) for t in field.split(sep) if t]


MODE = {"Undefined": 0, "Multiplicative": 1, "LogAdditive": 2,
        "PseudoAdditive": 3, "Additive": 0}
TYPE = {"AO": 0, "LS": 1, "TC": 2, "SO": 3}


def parse_pair4(s, at):
    m = re.match(r"\((-?\d+),(-?\d+)\)\((-?\d+),(-?\d+)\)", s[at:])
    assert m, s
    return tuple(int(g) for g in m.groups()), at + m.end()


def parse_events(rows):
    """Project the observer stream onto the decision kinds."""
    evs = []
    in_full = False
    full_depth = None
    for r in rows:
        d = r["detail"]
        kind = r["kind"]
        msg = r["msg"]
        if kind == "push" and msg == "full estimation":
            in_full, full_depth = True, int(r["depth"])
            continue
        if kind == "pop" and in_full and int(r["depth"]) == full_depth:
            in_full = False
        if d.startswith("obj AICC on logs="):
            logs = re.search(r"logs=([-+0-9.eE]+)", d).group(1)
            levels = re.search(r"levels=([-+0-9.eE]+)", d).group(1)
            pref = re.search(r"preference\)=([-+0-9.eE]+)", d).group(1)
            evs.append(dict(kind=1, i=(0, 0, 0, 0),
                            d=(dec_bits(logs), dec_bits(levels), dec_bits(pref)),
                            panel=[], picks=[]))
        elif d.startswith("diff d="):
            m = re.match(r"diff d=(\d+) bd=(\d+) mean=(true|false) tmean=(\S+)", d)
            assert m, d
            evs.append(dict(kind=2, i=(int(m.group(1)), int(m.group(2)),
                                       1 if m.group(3) == "true" else 0, 0),
                            d=(dec_bits(m.group(4)), 0, 0), panel=[], picks=[]))
        elif d.startswith("arma sel="):
            sel, at = parse_pair4(d, d.index("sel=") + 4)
            panel = []
            while True:
                k = d.find("[", at)
                if k < 0:
                    break
                j = d.find("]", k)
                spec, col = parse_pair4(d, k + 1)
                panel.append((spec, dec_bits(d[col + 1:j])))
                at = j
            evs.append(dict(kind=3, i=sel, d=(0, 0, 0), panel=panel, picks=[]))
        elif d.startswith("odm "):
            head = d[4:]
            types = head[:head.index(" ")]  # "AO,LS,TC,SO" or "" (no picks)
            # The Info picks carry the FACTORY index into this case's type
            # list (IOutliersDetectionModule.Info.getOutliers), not the
            # global AO/LS/TC/SO code the driver and the gate speak; the
            # full AO,LS,TC,SO list hides the difference, an "SO"-only case
            # does not (21-H.3 find, gate extension).
            names = [t for t in types.split(",") if t]
            picks = [(int(a), TYPE[names[int(b)]]) for a, b in
                     re.findall(r"\[(\d+);(\d+)\]", d)]
            va = d.split()[-1]
            tt = tuple(sorted(TYPE[t] for t in types.split(",") if t))
            tt = (tt + (0, 0, 0, 0))[:4]
            evs.append(dict(kind=4, i=tt,
                            d=(dec_bits(va), 0, 0), panel=[], picks=picks))
        elif in_full and kind == "info" and d.startswith("obj ("):
            m = re.match(r"obj \((-?\d+),(-?\d+),(-?\d+)\)\((-?\d+),(-?\d+),(-?\d+)\)", d)
            assert m, d
            o = [int(g) for g in m.groups()]
            evs.append(dict(kind=5, i=(o[0], o[1], o[2], o[3]), d=(o[4], o[5], 0),
                            panel=[], picks=[]))
    return evs


def parse_var(row):
    toks = row.split()
    # "0 const - -" or "1 AO (1994-01-01) x13 Irregular"
    ami, regeff = toks[-2], toks[-1]
    name = " ".join(toks[1:-2])
    return name, ami, regeff


def main(corpus_dir, dst):
    with open(os.path.join(corpus_dir, "cases.csv")) as f:
        cases = list(csv.DictReader(f))
    inputs = {}
    with open(os.path.join(corpus_dir, "inputs.csv")) as f:
        for r in csv.DictReader(f):
            line = r["line"]
            inputs.setdefault(r["id"], {})[line.split(" ", 1)[0]] = line.split(" ", 1)[1]
    events = {}
    with open(os.path.join(corpus_dir, "oracle-events.csv")) as f:
        for r in csv.DictReader(f):
            events.setdefault(r["id"], []).append(r)
    facts = {}
    with open(os.path.join(corpus_dir, "oracle-facts.csv")) as f:
        for r in csv.DictReader(f):
            facts.setdefault(r["id"], []).append((r["key"], r["value"]))

    def fact(cid, key):
        for k, v in facts[cid]:
            if k == key:
                return v
        return ""

    def facts_of(cid, key):
        return [v for k, v in facts[cid] if k == key]

    recs = []
    for case in cases:
        cid = case["id"]
        assert case["status"] == "ok", cid
        req_line = fact(cid, "req").split()
        req = dict(zip(req_line[0::2], req_line[1::2]))
        assert "start" in req and req["start"].count(":") == 1, cid
        year, period = (int(x) for x in req["start"].split(":"))
        inp = inputs[cid]
        assert inp.get("id", cid) == cid
        values = reals(inp["values"])
        assert len(values) == int(req["n"]), cid
        evs = parse_events(events[cid])
        oracle_vars = [parse_var(row) for row in facts_of(cid, "var")]
        ami = [(n, re_) for n, a, re_ in oracle_vars if a == "x13"]
        st = re.match(r"nobs=(\d+) neff=(\d+) npar=(\d+) loglik=(\S+) aicc=(\S+)",
                      fact(cid, "stats"))
        assert st, cid
        orders = [int(x) for x in fact(cid, "orders").split()]
        sigma = req["sigma"].split(";")
        recs.append({
            "id": cid,
            "n": len(values),
            "first_year": year, "first_period": period,
            "mode": MODE[req["mode"]],
            "mode_requested": req["mode"], "mode_effective": case["mode_effective"],
            "outliers_used": 1 if req["outliers"] == "on" else 0,
            "otypes": sorted(TYPE[t] for t in req["otypes"].split(",") if t),
            "maxiter": int(req["maxiter"]),
            "mixed": 1 if req["mixed"] == "true" else 0,
            "balanced": 1 if req["balanced"] == "true" else 0,
            "aicc_log_correction": dec_bits(req["aicdiff"]),
            "ub1": dec_bits(repr(1.0 / float(req["ub1"]))),
            "ub2": dec_bits(repr(1.0 / float(req["ub2"]))),
            "cancel": dec_bits(req["cancel"]),
            "tcrate": dec_bits(req["tcrate"]),
            "reduce_va": dec_bits(req["predcv"]),
            "ljung_box_limit": dec_bits(req["lblimit"]),
            "lower_sigma": dec_bits(sigma[0]), "upper_sigma": dec_bits(sigma[1]),
            "values": values,
            "exp": dict(log=1 if fact(cid, "log") == "true" else 0,
                        mean=1 if fact(cid, "mean") == "true" else 0,
                        orders=orders,
                        mode=MODE[case["mode_effective"]],
                        nvars=len(ami)),
            "evs": evs,
            "var_names": [n for n, _ in ami],
            "var_regeffects": [r for _, r in ami],
            "var_positions": [
                (int(n.split("(")[1].split(")")[0][:4]) - year) * 12
                + int(n.split("(")[1].split(")")[0][5:7]) - period
                for n, _ in ami],
            "var_rows": [int(row.split()[0]) for row in facts_of(cid, "var")
                         if parse_var(row)[1] == "x13"],
            "regcoef": reals(fact(cid, "regcoef")),
            "arma": reals(fact(cid, "arma")),
            "neff": int(st.group(2)),
            "loglik": dec_bits(st.group(4)), "aicc": dec_bits(st.group(5)),
            "out": reals(fact(cid, "out")),
            "d11final": reals(fact(cid, "d11final")),
            "e11": reals(fact(cid, "e11")),
        })

    with open(dst, "w") as f:
        f.write("""// Item 21-H.2 golden replay table for the automatic-spec pipeline oracle,
// generated from validation/evidence/native/x13-automatic (double JVM
// capture through the X13AutomaticProbe observer on the decoder-shaped
// automatic pipeline; see capture-x13-automatic.R). All doubles are raw
// IEEE bit patterns; ub1/ub2 are the module-space reciprocals the decoder
// hands to DifferencingModule. Do not edit by hand; regenerate with
// tools/gen_x13_automatic_golden.py.
//
// Event kinds: 1 log-level test (d: aicc logs/levels at observer display
// precision, preference scalar; i unused), 2 differencing (i: d, bd, mean;
// d0: tmean), 3 arma selection (i: sel p, q, bp, bq; panel: merged
// preference list), 4 ODM round (i: type codes sorted; picks: selection
// pairs; d0: the cv), 5 cascade final spec (i: p, d, q, bp; d0: bd, d1: bq
// as raw ints). The request block mirrors the probe's req echo; the fixed
// decoder scalars (max_d 2, max_bd 1, models 5, grid 2/2/1/1, epsilons
// 1e-7, mean eps 1e-5, max outliers 30, ur_limit 0.95, mean tests
// 1.96/1.0, tsig 1.0) are constants of the test and pinned to the
// contract.
#ifndef KALOS_TESTS_JVM_X13_AUTOMATIC_H
#define KALOS_TESTS_JVM_X13_AUTOMATIC_H

#include <cstddef>
#include <cstdint>

namespace golden_x13auto {

struct PanelEnt { int p, q, bp, bq; std::uint64_t bic; };
struct Pick { int position, type; };
struct Ev {
    int kind;
    int i0, i1, i2, i3;
    std::uint64_t d0, d1, d2;
    const PanelEnt* panel; int n_panel;
    const Pick* picks; int n_picks;
};

struct Case {
    const char* id;
    int n, first_year, first_period;
    int mode, outliers_used, maxiter, mixed, balanced, n_types;
    const int* otypes;
    std::uint64_t aicc_log_correction, ub1, ub2, cancel, tcrate, reduce_va,
        ljung_box_limit, lower_sigma, upper_sigma;
    const std::uint64_t* values;
    int exp_log, exp_mean, exp_p, exp_d, exp_q, exp_bp, exp_bd, exp_bq,
        exp_mode, exp_nvars;
    const Ev* evs; int n_evs;
    const char* const* var_names;
    const char* const* var_regeffects;
    const int* var_positions;
    const int* var_rows;
    const std::uint64_t* regcoef; int n_regcoef;
    const std::uint64_t* arma; int n_arma;
    long neff;
    std::uint64_t loglik, aicc;
    const std::uint64_t* out; const std::uint64_t* d11final;
    const std::uint64_t* e11;
};

""")
        for r in recs:
            cid = r["id"]
            arr = lambda name, vals, cast="std::uint64_t": \
                "static const %s k_%s_%s[%d] = {%s};\n" % (
                    cast, name, cid, max(len(vals), 1),
                    ", ".join("0x%016xULL" % v if cast == "std::uint64_t"
                              else str(v) for v in vals) or "0")
            f.write(arr("values", r["values"]))
            f.write("static const int k_otypes_%s[%d] = {%s};\n"
                    % (cid, max(len(r["otypes"]), 1),
                       ", ".join(str(v) for v in r["otypes"]) or "0"))
            for j, ev in enumerate(r["evs"]):
                if ev["panel"]:
                    f.write("static const PanelEnt k_panel_%s_%d[%d] = {\n"
                            % (cid, j, len(ev["panel"])))
                    for p, bic in ev["panel"]:
                        f.write("    {%d, %d, %d, %d, 0x%016xULL},\n"
                                % (p[0], p[1], p[2], p[3], bic))
                    f.write("};\n")
                if ev["picks"]:
                    f.write("static const Pick k_picks_%s_%d[%d] = {\n"
                            % (cid, j, len(ev["picks"])))
                    for pos, t in ev["picks"]:
                        f.write("    {%d, %d},\n" % (pos, t))
                    f.write("};\n")
            f.write("static const Ev k_evs_%s[%d] = {\n" % (cid, max(len(r["evs"]), 1)))
            if r["evs"]:
                for j, ev in enumerate(r["evs"]):
                    f.write("    {%d, %d, %d, %d, %d, 0x%016xULL, 0x%016xULL, "
                            "0x%016xULL, %s, %d, %s, %d},\n"
                            % (ev["kind"], ev["i"][0], ev["i"][1], ev["i"][2],
                               ev["i"][3], ev["d"][0], ev["d"][1], ev["d"][2],
                               ("k_panel_%s_%d" % (cid, j)) if ev["panel"] else "nullptr",
                               len(ev["panel"]),
                               ("k_picks_%s_%d" % (cid, j)) if ev["picks"] else "nullptr",
                               len(ev["picks"])))
            else:
                f.write("    {0, 0, 0, 0, 0, 0, 0, 0, nullptr, 0, nullptr, 0},\n")
            f.write("};\n")
            f.write("static const char* k_vnames_%s[%d] = {%s};\n"
                    % (cid, max(len(r["var_names"]), 1),
                       ", ".join('"%s"' % n for n in r["var_names"]) or '""'))
            f.write("static const char* k_vrege_%s[%d] = {%s};\n"
                    % (cid, max(len(r["var_regeffects"]), 1),
                       ", ".join('"%s"' % n for n in r["var_regeffects"]) or '""'))
            f.write("static const int k_vpos_%s[%d] = {%s};\n"
                    % (cid, max(len(r["var_positions"]), 1),
                       ", ".join(str(v) for v in r["var_positions"]) or "0"))
            f.write("static const int k_vrow_%s[%d] = {%s};\n"
                    % (cid, max(len(r["var_rows"]), 1),
                       ", ".join(str(v) for v in r["var_rows"]) or "0"))
            for name in ("regcoef", "arma", "out", "d11final", "e11"):
                f.write(arr(name, r[name]))
        f.write("\ninline const Case* table(std::size_t& count) {\n"
                "    static const Case kCases[] = {\n")
        for r in recs:
            cid = r["id"]
            e = r["exp"]
            f.write(
                "        {\"%s\", %d, %d, %d, %d, %d, %d, %d, %d, %d, k_otypes_%s,\n"
                "         0x%016xULL, 0x%016xULL, 0x%016xULL, 0x%016xULL,\n"
                "         0x%016xULL, 0x%016xULL, 0x%016xULL, 0x%016xULL,\n"
                "         0x%016xULL, k_values_%s,\n"
                "         %d, %d, %d, %d, %d, %d, %d, %d, %d, %d,\n"
                "         k_evs_%s, %d, k_vnames_%s, k_vrege_%s, k_vpos_%s,\n"
                "         k_vrow_%s,\n"
                "         k_regcoef_%s, %d, k_arma_%s, %d,\n"
                "         %ld, 0x%016xULL, 0x%016xULL,\n"
                "         k_out_%s, k_d11final_%s, k_e11_%s},\n"
                % (cid, r["n"], r["first_year"], r["first_period"], r["mode"],
                   r["outliers_used"], r["maxiter"], r["mixed"], r["balanced"],
                   len(r["otypes"]), cid,
                   r["aicc_log_correction"], r["ub1"], r["ub2"], r["cancel"],
                   r["tcrate"], r["reduce_va"], r["ljung_box_limit"],
                   r["lower_sigma"], r["upper_sigma"], cid,
                   e["log"], e["mean"], e["orders"][0], e["orders"][1],
                   e["orders"][2], e["orders"][3], e["orders"][4], e["orders"][5],
                   e["mode"], e["nvars"],
                   cid, len(r["evs"]), cid, cid, cid, cid,
                   cid, len(r["regcoef"]), cid, len(r["arma"]),
                   r["neff"], r["loglik"], r["aicc"], cid, cid, cid))
        f.write("    };\n"
                "    count = sizeof(kCases) / sizeof(kCases[0]);\n"
                "    return kCases;\n"
                "}\n\n"
                "}  // namespace golden_x13auto\n\n"
                "#endif\n")
    print("wrote %s (%d cases, %d events)"
          % (dst, len(recs), sum(len(r["evs"]) for r in recs)))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
