#!/usr/bin/env python3
"""Generate tests/jvm_tramo_regression_tests.h from the item 23-E tramo
regression-tests corpus (validation/evidence/native/tramo-regression-tests,
captured through the observer TramoRegressionTestsProbe.java against the
pinned jdplus 3.9.0 jars; double capture, byte-identical csv files across
JVM processes; class-load witnesses under loadlog-*.txt pin the
FastRegressionTest / DefaultRegressionTest / ModelStatistics machines and
show the 21-I.3-owned controller classes loading ONLY on the seam cases).

Every double rides as its raw IEEE-754 bit pattern; the replay layer
(tests/tramo_regression_replay.h) rebuilds the unit requests from the
hand-over echoes (msc + the mse residual block, clc/clcb, drb/drbb/drbc,
the pgin ladder inputs) and compares every consumed ModelStatistics row,
both cl.tstats arms, every mean-test run decision, the full pass2 ladder
after-state (pg/pgm) and the AmiOptions witnesses bit-for-bit. Refusal rows:
the quarterly MS case (D11), the Fast ami-td/ami-lp/ami-easter cases and the
Default calendar-field cases (21-I.3 seam) pin the unit's fail-closed
throws. Regenerate with:
  python3 tools/gen_tramo_regression_tests_golden.py \
      validation/evidence/native/tramo-regression-tests \
      tests/jvm_tramo_regression_tests.h
"""
import csv
import os
import struct
import sys

NAN_BITS = 0x7FF8000000000000


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


def flt(tok):
    t = tok.strip()
    if t.lower() == "nan":
        return float("nan")
    return float(t)


def B(x):
    return struct.unpack(">Q", struct.pack(">d", x))[0]


def hx(x):
    return "0x%016xULL" % B(x)


def bits_of(tok):
    return hx(flt(tok))


def yes(tok):
    return 1 if tok in ("y", "true", "1") else 0


def spec_of(tok):
    # "<p>,<d>,<q>,<bp>,<bd>,<bq>@<period>" (the probe's specOf grammar)
    core = tok.split("@")[0]
    p, d, q, bp, bd, bq = [int(v) for v in core.split(",")]
    return (p, d, q, bp, bd, bq)


KEYWORDS = {"coef", "covdiag", "ssq", "dim", "dof", "sigma2", "nx", "k",
            "tstats", "tstatsd"}


def parse_cl(tokens):
    """Parse an echoCl line body (tokens after the id)."""
    def span(word):
        i = tokens.index(word) + 1
        out = []
        while i < len(tokens) and tokens[i] not in KEYWORDS:
            out.append(tokens[i])
            i += 1
        return out

    def at(word):
        return tokens.index(word)

    return dict(
        coef=[flt(t) for t in span("coef")],
        cov=[flt(t) for t in span("covdiag")],
        ssq=flt(tokens[at("ssq") + 1]),
        dim=int(tokens[at("dim") + 1]),
        dof=int(tokens[at("dof") + 1]),
        sigma2=flt(tokens[at("sigma2") + 1]),
        nx=int(tokens[at("nx") + 1]),
        k=int(tokens[at("k") + 1]),
        ts=[flt(t) for t in span("tstats")],
        tsd=[flt(t) for t in span("tstatsd")],
    )


def parse_msr(tokens):
    at = tokens.index
    return dict(lb=flt(tokens[at("lb") + 1]), lbp=flt(tokens[at("lbp") + 1]),
                se=flt(tokens[at("se") + 1]), bic=flt(tokens[at("bic") + 1]),
                slb=flt(tokens[at("slb") + 1]),
                slbp=flt(tokens[at("slbp") + 1]),
                outliers=int(tokens[at("outliers") + 1]),
                obs=int(tokens[at("obs") + 1]),
                effobs=int(tokens[at("effobs") + 1]))


RESULT = {"Changed": 0, "Unchanged": 1, "Unprocessed": 2}  # enum ordinals


class Corpus:
    def __init__(self, corpus_dir):
        self.facts = {}
        with open(os.path.join(corpus_dir, "rt-facts.csv"), newline="") as fh:
            for row in csv.DictReader(fh):
                self.facts.setdefault(row["id"], []).append(
                    (row["key"], row["value"]))
        self.events = {}
        with open(os.path.join(corpus_dir, "rt-events.csv"), newline="") as fh:
            for row in csv.DictReader(fh):
                self.events.setdefault(row["id"], []).append(row["kind"])
        self.headers = {}
        self.order = []
        with open(os.path.join(corpus_dir, "inputs.csv"), newline="") as fh:
            for row in csv.DictReader(fh):
                tok = row["line"].split(" ")
                if tok[0] in ("MS", "FR", "DR", "PG", "AM"):
                    if row["id"] not in self.headers:
                        self.order.append(row["id"])
                    self.headers[row["id"]] = tok

    def find(self, cid, key):
        for k, v in self.facts.get(cid, []):
            if k == key:
                return v
        return None

    def all(self, cid, key):
        return [v for k, v in self.facts.get(cid, []) if k == key]

    def msr_for(self, cid, suffix):
        want = cid + suffix
        for k, v in self.facts.get(cid, []):
            if k == "msr":
                t = v.split(" ")
                if len(t) > 1 and t[0] == want:
                    return parse_msr(t[1:])
        return None


class Arrays:
    def __init__(self):
        self.lines = []
        self.seq = 0

    def emit(self, prefix, values):
        self.seq += 1
        name = "%s_%d" % (prefix, self.seq)
        if not values:
            self.lines.append("static const std::uint64_t %s[] = {0};" % name)
        else:
            chunks = []
            for i in range(0, len(values), 5):
                chunks.append("    " + ", ".join(
                    "0x%016xULL" % B(b) for b in values[i:i + 5]))
            self.lines.append("static const std::uint64_t %s[] = {\n%s\n};"
                              % (name, ",\n".join(chunks)))
        return (name, len(values))


def V(pair):
    return "{%s, %d}" % (pair[0], pair[1])


def main(corpus_dir, dst):
    c = Corpus(corpus_dir)
    arrays = Arrays()

    # The pass2 kernel's reduceVa field for the TRfull spec (every gate case
    # runs the TRfull kernel): from the am_TRfull witness.
    am_full = c.find("am_TRfull", "am").split(" ")
    reduce_va_full = flt(am_full[am_full.index("reduceVa") + 1])

    ms_rows, machine_rows, pg_rows, am_rows = [], [], [], []

    for cid in c.order:
        h = c.headers[cid]
        kind = h[0]

        if kind == "MS":
            freq = int(h[2])
            ms = c.find(cid, "ms").split(" ")
            k = int(ms[ms.index("k") + 1])
            m = int(ms[ms.index("m") + 1])
            cl = parse_cl(c.find(cid, "msc").split(" ")[1:])
            resid = [flt(t) for t in c.find(cid, "mse").split(" ")[1:]]
            built = 0
            for k2, v in c.facts[cid]:
                if k2 == "ms" and "built" in v:
                    built = 1 if "built 1" in v else 0
            msr = c.msr_for(cid, "")
            nan = float("nan")
            if msr is not None:
                exp = dict(built=built, lb=msr["lb"], lbp=msr["lbp"],
                           se=msr["se"], bic=msr["bic"], slb=msr["slb"],
                           slbp=msr["slbp"], outl=msr["outliers"],
                           obs=msr["obs"], effobs=msr["effobs"])
            else:
                exp = dict(built=0, lb=nan, lbp=nan, se=nan, bic=nan,
                           slb=nan, slbp=nan, outl=-1, obs=-1, effobs=-1)
            extra = c.find(cid, "msrx")
            fields = extra.split()[1:] if extra else []
            values = {name: flt(value) for name, value in zip(fields[::2], fields[1::2])}
            stability = ", ".join(hx(values.get(name, nan)) for name in
                                  ("skew", "skewp", "stabM", "stabMp", "stabV", "stabVp"))
            a_res = arrays.emit("mres_" + cid, resid)
            a_co = arrays.emit("mco_" + cid, cl["coef"])
            a_cv = arrays.emit("mcv_" + cid, cl["cov"])
            a_ta = arrays.emit("mta_" + cid, cl["ts"])
            a_tp = arrays.emit("mtp_" + cid, cl["tsd"])
            ms_rows.append("    {%s, %d, %d, %d, %d, %s, %s, %s, %s, %d, "
                           "%d, %s, %d, %d, %s, %s, %s, %s, %s, %s, %d, "
                           "%d, %d, %d, %s, %s, {%s}}," % (
                               cstr(cid), freq, k, max(exp["outl"], 0),
                               max(exp["obs"], 0),
                               V(a_res), V(a_co), V(a_cv),
                               hx(cl["ssq"]), cl["dim"], cl["dof"],
                               hx(cl["sigma2"]),
                               1 if freq != 12 else 0, exp["built"],
                               hx(exp["lb"]), hx(exp["lbp"]), hx(exp["se"]),
                               hx(exp["bic"]), hx(exp["slb"]), hx(exp["slbp"]),
                               m, exp["outl"], exp["obs"], exp["effobs"],
                               V(a_ta), V(a_tp), stability))

        elif kind in ("FR", "DR"):
            is_dr = kind == "DR"
            tags = ("drb", "drbb", "drbc") if is_dr else ("clc", "clcb", "clc")
            runs = []
            for v in c.all(cid, kind.lower()):
                t = v.split(" ")
                if "run" not in t:  # the echoVars companion line
                    continue
                idx = {"a": 0, "b": 1, "c": 2}[t[t.index("run") + 1]]
                est = 0
                if "est" in t:
                    est = 1 if t[t.index("est") + 1] == "set" else 2
                runs.append((idx, t[t.index("tmean") + 1],
                             RESULT[t[t.index("result") + 1]],
                             int(t[t.index("mean") + 1]), est))
            runs.sort()
            if is_dr:
                mean_f = yes(h[4])
                # run a replays the header builder; the exact-boundary runs
                # b/c rebuild the builder with testMean(true) and without
                # adjust/joinTest (the probe's own call sequence).
                run_opts = [(yes(h[12]), yes(h[15]), yes(h[16])),
                            (1, 0, 0), (1, 0, 0)]
                plain = (0, 0, 0)
                seam = [1 if h[17] != "n" else 0, 1 if h[18] != "n" else 0,
                        1 if h[19] != "n" else 0]
                auto = [0, 0, 0]
                refuse = 1 if any(seam) else 0
            else:
                mean_f = yes(h[4])
                # Fast: every run replays the one builder (testMean from the
                # header, no adjust/join knobs on this machine).
                run_opts = [(yes(h[13]), 0, 0)] * 3
                plain = [0, 0, 0]
                seam = [0, 0, 0]
                auto = [0, 0, 0]
                refuse = 0
                for vs in ([] if h[14] == "-" else h[14].split(",")):
                    if vs == "tdn":
                        plain[0] = 1
                    elif vs == "lpn":
                        plain[1] = 1
                    elif vs == "ean":
                        plain[2] = 1
                    elif vs == "tda":
                        auto[0] = refuse = 1
                    elif vs == "lpa":
                        auto[1] = refuse = 1
                    elif vs == "eaa":
                        auto[2] = refuse = 1
            run_rows = []
            for i in range(3):
                tm_i, adj_i, join_i = run_opts[i]
                matching = [r for r in runs if r[0] == i]
                if not matching:
                    run_rows.append(
                        "{0ULL, -1, 0, 0, %d, %d, %d, {nullptr, 0}, "
                        "{nullptr, 0}, 0ULL, 0, 0, 0, {nullptr, 0}, "
                        "{nullptr, 0}}" % (tm_i, adj_i, join_i))
                    continue
                _, tmean, res, mean_after, est = matching[0]
                cl_line = c.find(cid, tags[i])
                if cl_line is None:
                    run_rows.append(
                        "{%s, %d, %d, %d, %d, %d, %d, {nullptr, 0}, "
                        "{nullptr, 0}, 0ULL, 0, 0, 0, {nullptr, 0}, "
                        "{nullptr, 0}}" % (
                            bits_of(tmean), res, mean_after, est,
                            tm_i, adj_i, join_i))
                    continue
                cl = parse_cl(cl_line.split(" ")[1:])
                a_co = arrays.emit("rco_%s" % cid, cl["coef"])
                a_cv = arrays.emit("rcv_%s" % cid, cl["cov"])
                a_ta = arrays.emit("rta_%s" % cid, cl["ts"])
                a_tp = arrays.emit("rtp_%s" % cid, cl["tsd"])
                run_rows.append(
                    "{%s, %d, %d, %d, %d, %d, %d, %s, %s, %s, %d, %d, %d, "
                    "%s, %s}" % (
                        bits_of(tmean), res, mean_after, est,
                        tm_i, adj_i, join_i,
                        V(a_co), V(a_cv), hx(cl["ssq"]), cl["dim"], cl["dof"],
                        cl["k"], V(a_ta), V(a_tp)))
            machine_rows.append(
                "    {%s, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, "
                "%d, {%s, %s, %s}}," % (
                    cstr(cid), 1 if is_dr else 0, mean_f,
                    plain[0], plain[1], plain[2], refuse,
                    auto[0], auto[1], auto[2],
                    seam[0], seam[1], seam[2], len(runs),
                    run_rows[0], run_rows[1], run_rows[2]))

        elif kind == "PG":
            p1 = c.find(cid, "pgin").split(" ")
            p2 = c.all(cid, "pgin")[1].split(" ")
            at1 = p1.index
            cur = spec_of(p1[at1("spec") + 1])
            cur_mean = int(p1[at1("mean") + 1])
            ro6 = [int(x) for x in h[18:24]]  # d bd p q bp bq
            ref_mean = yes(h[15])
            pg = c.find(cid, "pg").split(" ")
            pgm = c.find(cid, "pgm").split(" ")
            after = spec_of(pgm[pgm.index("spec") + 1])
            evs = c.events.get(cid, [])
            cur_msr = c.msr_for(cid, "-cur")
            ref_msr = c.msr_for(cid, "-ref")
            nan = float("nan")
            atg = pg.index
            pg_rows.append(
                "    {%s, %d, %d, %s, %d, %d, %s, 1, %s, "
                "%d, %d, %d, %d, %d, %d, %d, %s, %s, %d, "
                "%d, %d, %d, %d, %d, %d, %d, %d, %s, %s, %s, %s, "
                "%d, %d, %d, %s, %s, %d, %d, %d, "
                "%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, "
                "%s, %s, %s, %s, %s, %s}," % (
                    cstr(cid),
                    int(p1[at1("pass") + 1]), int(p1[at1("round") + 1]),
                    hx(flt(p1[at1("pcr") + 1])),
                    int(p1[at1("changed") + 1]), int(p1[at1("seasonal") + 1]),
                    hx(flt(p1[at1("curva") + 1])), hx(reduce_va_full),
                    cur[0], cur[1], cur[2], cur[3], cur[4], cur[5], cur_mean,
                    bits_of(p2[p2.index("phi1") + 1]),
                    bits_of(p2[p2.index("bphi1") + 1]),
                    int(p1[at1("nb") + 1]),
                    int(p1[at1("nr") + 1]),
                    ro6[2], ro6[0], ro6[3], ro6[4], ro6[1], ro6[5], ref_mean,
                    hx(flt(p1[at1("lbp") + 1])), hx(flt(p1[at1("se") + 1])),
                    hx(flt(p1[at1("reflbp") + 1])), hx(flt(p1[at1("refse") + 1])),
                    int(pg[atg("ret") + 1]),
                    int(pg[atg("pass") + 1]), int(pg[atg("round") + 1]),
                    hx(flt(pg[atg("pcr") + 1])), hx(flt(pg[atg("curva") + 1])),
                    int(pg[atg("needAuto") + 1]),
                    int(pg[atg("needOutliers") + 1]),
                    int(pg[atg("refNull") + 1]),
                    after[0], after[1], after[2], after[3], after[4], after[5],
                    int(pgm[pgm.index("mean") + 1]),
                    int(pgm[pgm.index("nb") + 1]),
                    1 if "remark" in evs else 0, 1 if "warn" in evs else 0,
                    hx(cur_msr["lbp"] if cur_msr else nan),
                    hx(cur_msr["se"] if cur_msr else nan),
                    hx(ref_msr["lbp"] if ref_msr else nan),
                    hx(ref_msr["se"] if ref_msr else nan),
                    hx(flt(pg[atg("refLbp") + 1])),
                    hx(flt(pg[atg("refSe") + 1]))))

        elif kind == "AM":
            am = c.find(cid, "am").split(" ")
            at = am.index
            am_rows.append(
                "    {%s, %s, %s, %s, %s, %d, %s, %d, %d, %d}," % (
                    cstr(cid[3:]),
                    hx(flt(am[at("precision") + 1])),
                    hx(flt(am[at("ur") + 1])),
                    hx(flt(am[at("va") + 1])),
                    hx(flt(am[at("reduceVa") + 1])),
                    int(am[at("checkMu") + 1]),
                    hx(flt(am[at("ljungBoxLimit") + 1])),
                    int(am[at("acceptAirline") + 1]),
                    int(am[at("outliers") + 1]),
                    int(am[at("seasonal") + 1])))

    out = []
    out.append("// Generated by tools/gen_tramo_regression_tests_golden.py "
               "- do not edit.")
    out.append("// Item 23-E tramo regression-tests golden: the live "
               "ModelStatistics /")
    out.append("// FastRegressionTest / DefaultRegressionTest / "
               "TramoKernel.pass2 / readAmiOptions")
    out.append("// facts captured through validation/reference/")
    out.append("// TramoRegressionTestsProbe.java from the pinned jdplus "
               "3.9.0 jars (corpus")
    out.append("// validation/evidence/native/tramo-regression-tests, "
               "double capture byte-")
    out.append("// identical; load witnesses pin the two machines and "
               "ModelStatistics with the")
    out.append("// 21-I.3-owned controller classes loading ONLY on the seam "
               "cases).")
    with open(os.path.join(corpus_dir, "metadata.R")) as fh:
        for line in fh:
            if line.startswith("probe_sha256"):
                out.append("// " + line.strip().replace('"', ""))
    out.append("// jvm = 21 (OpenJDK, G2 intrinsic-disable recipe)")
    out.append("#ifndef KALOS_TESTS_JVM_TRAMO_REGRESSION_TESTS_H")
    out.append("#define KALOS_TESTS_JVM_TRAMO_REGRESSION_TESTS_H")
    out.append("")
    out.append("#include <cstddef>")
    out.append("#include <cstdint>")
    out.append("")
    out.append("namespace kalos::test {")
    out.append("")
    out.append("struct JvmRtVec {")
    out.append("    const std::uint64_t* bits;")
    out.append("    int n;")
    out.append("};")
    out.append("")
    out.append("struct JvmRtMsCase {")
    out.append("    const char* id;")
    out.append("    int freq, free_parameters, ami_outliers, "
               "estimation_domain;")
    out.append("    JvmRtVec residuals, coefficients, cov_diagonal;")
    out.append("    std::uint64_t ssq;")
    out.append("    int dim, dof;")
    out.append("    std::uint64_t sigma2;")
    out.append("    int refuse;  // 1: non-monthly (D11), the unit must "
               "throw")
    out.append("    int built;")
    out.append("    std::uint64_t lb, lbp, se, bic, slb, slbp;  // expected "
               "rows")
    out.append("    int lb_length, outliers_count, observations_count,")
    out.append("        effective_observations_count;  // counts < 0: "
               "unchecked (throw row)")
    out.append("    JvmRtVec tstats_addmu, tstats_plain;  // msc echo, both "
               "arms")
    out.append("    std::uint64_t stability[6];  // skew, mean and variance tests and probabilities")
    out.append("};")
    out.append("")
    out.append("struct JvmRtRun {")
    out.append("    std::uint64_t tmean;  // mean-test threshold this run")
    out.append("                          // replayed with (echoed)")
    out.append("    int result;  // 0 Unchanged 1 Changed 2 Unprocessed; -1 "
               "absent")
    out.append("    int mean_after, est_after;  // est_after 0 n/a 1 set 2 "
               "cleared")
    out.append("    int test_mean, adjust, join_test;  // this run's builder "
               "flags")
    out.append("    JvmRtVec coefficients, cov_diagonal;")
    out.append("    std::uint64_t ssq;")
    out.append("    int dim, dof, free_parameters;")
    out.append("    JvmRtVec tstats_addmu, tstats_plain;  // hand-over echo "
               "of this run")
    out.append("};")
    out.append("")
    out.append("struct JvmRtMachineCase {  // kind 0: Fast, 1: Default")
    out.append("    const char* id;")
    out.append("    int kind, mean;")
    out.append("    int plain_td, plain_lp, plain_easter;")
    out.append("    int refuse;  // 1: 21-I.3 seam (ami auto variable / "
               "calendar field)")
    out.append("    int auto_td, auto_lp, auto_easter;        // Fast seam "
               "flags")
    out.append("    int has_td, has_lp, has_easter;           // Default "
               "seam flags")
    out.append("    int nruns;")
    out.append("    JvmRtRun runs[3];")
    out.append("};")
    out.append("")
    out.append("struct JvmRtPgCase {")
    out.append("    const char* id;")
    out.append("    int pass, round;")
    out.append("    std::uint64_t pcr;")
    out.append("    int changed, seasonal;")
    out.append("    std::uint64_t curva;")
    out.append("    int outliers_detection;")
    out.append("    std::uint64_t reduce_va;")
    out.append("    int p, d, q, bp, bd, bq, mean;          // current model")
    out.append("    std::uint64_t phi1, bphi1;")
    out.append("    int nb;")
    out.append("    int nr;  // -1: refAuto null")
    out.append("    int rp, rd, rq, rbp, rbd, rbq, rmean;   // reference "
               "model")
    out.append("    std::uint64_t lbp, se, rlbp, rse;")
    out.append("    int ret, pass_after, round_after;")
    out.append("    std::uint64_t pcr_after, curva_after;")
    out.append("    int need_auto, need_outliers, ref_null;")
    out.append("    int ap, ad, aq, abp, abd, abq, mean_after, nb_after;")
    out.append("    int remark_event, warn_event;")
    out.append("    std::uint64_t cur_pvalue, cur_se, ref_pvalue, ref_se,")
    out.append("        ref_lbp_after, ref_se_after;")
    out.append("};")
    out.append("")
    out.append("struct JvmRtAmCase {")
    out.append("    const char* spec;")
    out.append("    std::uint64_t precision, ur, va, reduce_va;")
    out.append("    int check_mu;")
    out.append("    std::uint64_t ljung_box_limit;")
    out.append("    int accept_airline, outliers, seasonal;")
    out.append("};")
    out.append("")
    out.extend(arrays.lines)
    out.append("")
    out.append("static const JvmRtMsCase kRtMs[] = {")
    out.extend(ms_rows)
    out.append("};")
    out.append("constexpr std::size_t kRtMsN = sizeof(kRtMs) / "
               "sizeof(kRtMs[0]);")
    out.append("")
    out.append("static const JvmRtMachineCase kRtMachines[] = {")
    out.extend(machine_rows)
    out.append("};")
    out.append("constexpr std::size_t kRtMachinesN = sizeof(kRtMachines) / "
               "sizeof(kRtMachines[0]);")
    out.append("")
    out.append("static const JvmRtPgCase kRtPg[] = {")
    out.extend(pg_rows)
    out.append("};")
    out.append("constexpr std::size_t kRtPgN = sizeof(kRtPg) / "
               "sizeof(kRtPg[0]);")
    out.append("")
    out.append("static const JvmRtAmCase kRtAm[] = {")
    out.extend(am_rows)
    out.append("};")
    out.append("constexpr std::size_t kRtAmN = sizeof(kRtAm) / "
               "sizeof(kRtAm[0]);")
    out.append("")
    out.append("}  // namespace kalos::test")
    out.append("")
    out.append("#endif  // KALOS_TESTS_JVM_TRAMO_REGRESSION_TESTS_H")
    with open(dst, "w") as fh:
        fh.write("\n".join(out) + "\n")
    print("wrote %s: %d ms, %d machine, %d pg, %d am rows" % (
        dst, len(ms_rows), len(machine_rows), len(pg_rows), len(am_rows)))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
