#!/usr/bin/env python3
"""Generate tests/jvm_calendar_detection.h from the item 21-I.3
calendar-detection corpus (validation/evidence/native/calendar-detection,
captured through the observer CalendarDetectionProbe.java against the
pinned jdplus 3.9.0 jars; per-case JVMs with the G2 intrinsic-disable
recipe; double capture byte-identical, capA/capB; class-load witnesses
under loadlog-*.txt prove the machine classes load in the pipeline/step
runs).

EVERY DOUBLE RIDES ITS RAW IEEE-754 BIT PATTERN: Java's String.format
%.17g is not a faithful 17-digit renderer (the implementation pinned a
same-double/different-print divergence), so the probe dumps
Double.doubleToRawLongBits hex and this generator parses hex - no decimal
round-trip ever touches a golden value. The per-case thresholds the probe
never echoes (pmodel/pconstraint/tmean/tvar/fpvalue, the arm switches)
come from the corpus inputs.csv (the capture script records every case
header); decimal thresholds cross through float()'s correctly-rounded
double, which is the exact literal the probe's Double.parseDouble read.
The replay layer (tests/calendar_detection_replay.h) rebuilds the machine
requests from the hand-over echoes (cmr ladder rows, the cand
createTestModel replicas, clc/dtc + ucov/dtu, the fssl/dt/saw witnesses)
and compares the machine decisions plus the jar's post-state (res lines,
post vars, the det FT / det ATD journal bits, the jt quadratic form)
bit-for-bit. Regenerate with:
  python3 tools/gen_calendar_detection_golden.py \
      validation/evidence/native/calendar-detection \
      tests/jvm_calendar_detection.h
"""
import csv
import os
import struct
import sys

RESULT = {"Unchanged": 0, "Changed": 1, "Failed": 2, "Unprocessed": 3}
SELECTOR = {"aicc": 0, "bic": 1, "wald": 2}


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


def hxd(tok):
    """Probe hex token (raw doubleToRawLongBits) -> int."""
    return int(tok, 16)


def lit(bits):
    return "0x%016xULL" % bits


def decbits(tok):
    """Decimal threshold -> the bits of the double it parses to."""
    return struct.unpack(">Q", struct.pack(">d", float(tok)))[0]


def yes(tok):
    return 1 if tok in ("y", "true", "1") else 0


def posdim(tok):
    if tok == "-:-":
        return (-1, 0)
    p, d = tok.split(":")
    return (int(p), int(d))


class Case:
    """One parsed probe case file (the probe javadoc is the grammar)."""

    def __init__(self, cid, fam, lines, hdr):
        self.id = cid
        self.fam = fam
        self.hdr = hdr
        self.rows = []
        self.cmb = None
        self.cands = {}
        self.clc = None
        self.ucov = []
        self.dtc = None
        self.dtu = []
        self.fsin = None
        self.dsin = None
        self.fssl = None
        self.dt = None
        self.saw = None
        self.jt = None
        self.res = None
        self.vars_pre = None
        self.vars_after = None
        self.info_ft = None
        self.info_atd = None
        self.pipe = None
        for ln in lines:
            self._one(ln)

    def _echo_cl(self, t):
        i = t.index("covdiag")
        j = t.index("ssq")
        f = dict(zip(t[j::2], t[j + 1::2]))
        return {
            "coef": [hxd(x) for x in t[3:i]],
            "covdiag": [hxd(x) for x in t[i + 1:j]],
            "ssq": hxd(f["ssqb"]), "dim": int(f["dim"]),
            "dof": int(f["dof"]), "nx": int(f["nx"]), "k": int(f["k"]),
        }

    def _one(self, ln):
        t = ln.split(" ")
        k = t[0]
        if k == "cmr":
            if t[3] == "absent":
                self.rows.append({"present": 0})
                return
            f = dict(zip(t[4::2], t[5::2]))
            self.rows.append({
                "present": 1, "aicc": hxd(f["aiccb"]), "bic": hxd(f["bicb"]),
                "ssq": hxd(f["ssqb"]), "nx": int(f["nx"]),
                "dof": int(f["dof"]), "dim": int(f["dim"]),
            })
        elif k == "cmb":
            self.cmb = (int(t[3]), int(t[5]), int(t[7]))
        elif k == "cand":
            tag = t[1]
            c = self.cands.setdefault(tag, {"tag": tag.split(":", 1)[1]})
            if t[2] == "coef":
                c.update(self._echo_cl(t))
            else:
                f = dict(zip(t[3::2], t[4::2]))  # t[2] is the "pos" verb

                def p(tok):  # -9 marks a null machine variable
                    return -1 if tok == "-9" else int(tok)
                c.update({
                    "td_pos": p(f["td"]), "lp_pos": p(f["lp"]),
                    "ea_pos": p(f["ea"]), "nhp": int(f["nhp"]),
                    "curk": int(f["curk"]), "curmean": int(f["curmean"]),
                    "mean": int(f["mean"]), "adj": int(f["adj"]),
                    "log": int(f["log"]), "prespec": f["prespec"],
                    "spec": f["spec"],
                })
        elif k in ("clc", "dtc"):
            d = self._echo_cl(t)
            if k == "clc":
                self.clc = d
            else:
                self.dtc = d
        elif k in ("ucov", "dtu"):
            (self.ucov if k == "ucov" else self.dtu).append(
                [hxd(x) for x in t[3:]])
        elif k == "saw":
            f = dict(zip(t[2::2], t[3::2]))
            self.saw = {"td": int(f["td"]), "lp": int(f["lp"]),
                        "adj": int(f["adj"]), "log": int(f["log"]),
                        "equals": [int(x) for x in
                                   t[t.index("equals") + 1:]]}
        elif k == "fsin" and "spec" in t:
            self.fsin = {"spec": t[3], "mean": int(t[5]), "k": int(t[7])}
        elif k == "dsin" and "spec" in t:
            self.dsin = {"spec": t[3], "mean": int(t[5]), "k": int(t[7])}
        elif k == "fssl":
            f = dict(zip(t[2::2], t[3::2]))
            self.fssl = {s: posdim(f[s]) for s in ("td", "lp", "mh")}
        elif k == "dt":
            f = dict(zip(t[2::2], t[3::2]))
            self.dt = {"td": posdim(f["td"]), "lp": posdim(f["lp"]),
                       "ea": posdim(f["ea"]), "log": int(f["log"]),
                       "mean": int(f["mean"])}
        elif k == "jt":
            f = dict(zip(t[2::2], t[3::2]))
            self.jt = {kk: (hxd(v) if kk in ("value", "pvalue") else int(v))
                       for kk, v in f.items()}
        elif k == "res":
            f = dict(zip(t[4::2], t[5::2]))
            self.res = {"result": RESULT[t[3]], "spec": f["spec"],
                        "mean": int(f["mean"]),
                        "est": 0 if f.get("est") == "null" else 1,
                        "tag": t[2]}
        elif len(t) > 2 and t[1] == self.id and t[2] == "vars":
            csvvars = t[3] if len(t) > 3 else ""
            if t[0].endswith("in"):
                self.vars_pre = csvvars
            else:
                self.vars_after = csvvars
        elif " det FT " in ln:
            self.info_ft = {
                kk: (hxd(vv) if kk != "nhp" else int(vv))
                for kk, vv in (tok.split("=", 1)
                               for tok in ln.split("det FT ")[1].split())}
        elif " det ATD " in ln:
            for tok in ln.split(" det ATD ")[1].split():
                if tok.startswith("best="):
                    self.info_atd = int(tok[5:])
        elif k in ("tdspec", "x13tdspec"):
            self.pipe = {"line": ln, "run": 0}
        elif k == "run":
            assert self.pipe is not None
            self.pipe["run"] = 1 if t[1] == "OK" and t[2] == "set" else 0


def load_corpus(corpus):
    inputs = {}
    with open(os.path.join(corpus, "inputs.csv"), newline="") as fh:
        for row in csv.DictReader(fh):
            inputs[row["id"]] = row["line"].split(" ; ")[0].split(" ")
    cases = []
    for name in sorted(os.listdir(corpus)):
        if not name.endswith(".txt") or name.startswith("loadlog-"):
            continue
        with open(os.path.join(corpus, name)) as fh:
            lines = [ln.rstrip("\n") for ln in fh if ln.strip()]
        head = lines[0].split(" ")
        assert head[0] == "case", name
        fam, cid = head[1], head[2]
        assert cid == name[:-4], name
        hdr = inputs.get(cid)
        if hdr is None:  # PT/PX load witnesses keep their header inline
            hdr = ["PX" if fam == "PX" else "PT", cid, lines[0].split(" ")[3],
                   "clean"]
        cases.append(Case(cid, fam, lines[1:], hdr))
    return cases


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    corpus, outpath = sys.argv[1], sys.argv[2]
    cases = load_corpus(corpus)
    fam = {}
    for c in cases:
        fam.setdefault(c.fam, []).append(c)
    for f in ("CM", "TA", "FT", "SA", "FS", "DS", "JT", "PT", "PX"):
        if f not in fam:
            raise SystemExit("corpus misses the %s family" % f)

    out = []
    pools = []

    def pool(base, vals):
        if not vals:
            return "nullptr"
        nm = "%s%d" % (base, len(pools))
        pools.append((nm, vals))
        return nm

    out.append("// Generated by tools/gen_calendar_detection_golden.py - do not edit.\n")
    out.append(
        "// Item 21-I.3 calendar-detection golden: the live TradingDaysRegressionComparator\n"
        "// ladder algebra, the TRAMO AutomaticRegressionTest / AutomaticWaldRegressionTest /\n"
        "// AutomaticFRegressionTest machines, the sa AutomaticTradingDaysRegressionTest /\n"
        "// AutomaticTradingDaysWaldTest machines, the FastRegressionTest / DefaultRegressionTest\n"
        "// calendar steps and the JointTest quadratic form, captured through validation/\n"
        "// reference/CalendarDetectionProbe.java from the pinned jdplus 3.9.0 jars (corpus\n"
        "// validation/evidence/native/calendar-detection, double capture byte-identical,\n"
        "// per-case JVMs with the G2 intrinsic-disable recipe; loadlog-*.txt class-load\n"
        "// witnesses pin the machine classes inside the pipeline/step runs).\n"
        "// EVERY double rides its raw IEEE-754 bit pattern (the probe dumps\n"
        "// Double.doubleToRawLongBits hex: Java's %.17g rendering proved NOT a faithful\n"
        "// renderer during implementation - the golden must never parse decimal witness\n"
        "// text). Per-case thresholds ride the corpus inputs.csv; the det FT / det ATD rows\n"
        "// of the processing journal carry the jar's own Info bits; PT/PX pin the\n"
        "// full-pipeline wiring witnesses (the six-machine chains firing in the kernels).\n")
    probe_sha = ""
    with open(os.path.join(corpus, "metadata.R")) as fh:
        for ln in fh:
            if ln.startswith("probe_sha256"):
                probe_sha = ln.split('"')[1]
    out.append("// probe_sha256 <- %s\n" % probe_sha)
    out.append("// jvm = 21 (OpenJDK, G2 intrinsic-disable recipe, one process per case)\n")
    out.append("#ifndef KALOS_TESTS_JVM_CALENDAR_DETECTION_H\n"
               "#define KALOS_TESTS_JVM_CALENDAR_DETECTION_H\n\n"
               "#include <cstddef>\n#include <cstdint>\n\nnamespace kalos::test {\n\n")

    out.append("struct JvmCdRow {\n"
               "    int present;\n"
               "    std::uint64_t aicc, bic, ssq;\n"
               "    int nx, dof, dim;\n"
               "};\n"
               "struct JvmCdCmpCase {\n"
               "    const char* id;\n"
               "    const JvmCdRow* rows;\n"
               "    int nrows;\n"
               "    std::uint64_t pmodel, pconstraint;   // waldTest wiring (inputs.csv)\n"
               "    int bestaicc, bestbic, wald;\n"
               "};\n\n")

    out.append("struct JvmCdCand {\n"
               "    const char* tag;\n"
               "    const std::uint64_t* coef;\n"
               "    int ncoef;\n"
               "    const std::uint64_t* covdiag;\n"
               "    int ncov;\n"
               "    std::uint64_t ssq;\n"
               "    int dim, dof, nx, k;\n"
               "    int td_pos, lp_pos, ea_pos;   // -1 = not a variable of the candidate\n"
               "    int nhp, curk, curmean, mean, adj, log;\n"
               "    const char* prespec;\n"
               "    const char* spec;\n"
               "};\n"
               "struct JvmCdLadderCase {\n"
               "    const char* id;\n"
               "    int sa;                       // 0 = TRAMO TA, 1 = sa SA\n"
               "    int selector;                 // 0 aicc 1 bic 2 wald\n"
               "    std::uint64_t pmodel, pconstraint, tlp;   // raw bits\n"
               "    int adjust, testmean, has_easter, has_lp;\n"
               "    const JvmCdRow* rows;\n"
               "    int nrows;\n"
               "    JvmCdCand cands[8];           // r0..r7; absent rows keep the echo\n"
               "    int sa_td_present, sa_lp_present, sa_adj, sa_log;\n"
               "    int sa_equals[8];\n"
               "    int sa_nequals;\n"
               "    int bestaicc, bestbic, wald;  // the jar comparator echoes\n"
               "    int det_atd_best;             // jar ATD Info; -999 = not logged\n"
               "    int result, mean_after;\n"
               "    const char* vars_pre; const char* vars_after;\n"
               "};\n\n")

    out.append("struct JvmCdFtestCase {\n"
               "    const char* id;\n"
               "    std::uint64_t fpvalue;\n"
               "    int has_easter, testmean, adjust;\n"
               "    JvmCdCand info_none, info_td, info_wd;   // the Info rows (no lp)\n"
               "    JvmCdCand upd_td, upd_wd;                // the update models (with lp)\n"
               "    int info_seen;                           // det FT journal row present\n"
               "    std::uint64_t ss0, ss1, ss6, ssmc1, ssmc6, pftd1, pftd6;\n"
               "    int info_nhp;\n"
               "    int result, mean_after;\n"
               "    const char* vars_after;\n"
               "};\n\n")

    out.append("struct JvmCdSlot {\n"
               "    int pos, dim;                 // -1/0 = the slot is empty\n"
               "};\n"
               "struct JvmCdClc {\n"
               "    const std::uint64_t* coef;\n"
               "    int ncoef;\n"
               "    const std::uint64_t* covdiag;\n"
               "    int ncov;\n"
               "    std::uint64_t ssq;\n"
               "    int dim, dof, nx, k;\n"
               "};\n"
               "struct JvmCdFastStep {\n"
               "    const char* id;\n"
               "    std::uint64_t tmean, tvar;\n"
               "    int testmean;\n"
               "    JvmCdClc clc;\n"
               "    JvmCdSlot td, lp, mh;\n"
               "    int desc_mean;\n"
               "    int result, mean_after, est_set;   // est_set 0: the jar cleared it\n"
               "    const char* vars_pre; const char* vars_after;\n"
               "};\n\n")

    out.append("struct JvmCdCovRow {\n"
               "    const std::uint64_t* bits;\n"
               "    int n;\n"
               "};\n"
               "struct JvmCdDefaultStep {\n"
               "    const char* id;\n"
               "    std::uint64_t tmean;\n"
               "    int testmean, adjust, join;\n"
               "    JvmCdClc dtc;                        // the test-model cl\n"
               "    const JvmCdCovRow* cov;              // dtu lower triangle rows\n"
               "    int ncov;\n"
               "    JvmCdSlot td, lp, ea;\n"
               "    int desc_test_log, regarima_mean, desc_mean;\n"
               "    int jt_q, jt_df;                     // jt echo; jt_q 0 = absent\n"
               "    std::uint64_t jt_value, jt_pvalue;\n"
               "    int result, mean_after, est_set;\n"
               "    const char* vars_pre; const char* vars_after;\n"
               "};\n\n")

    out.append("struct JvmCdJointCase {\n"
               "    const char* id;\n"
               "    int pos, q, k;\n"
               "    JvmCdClc clc;\n"
               "    const JvmCdCovRow* cov;\n"
               "    int ncov;\n"
               "    std::uint64_t value, pvalue;\n"
               "};\n\n")

    out.append("struct JvmCdPipeCase {\n"
               "    const char* id;\n"
               "    int px;                              // 0 PT (TRAMO), 1 PX (X13)\n"
               "    const char* method;\n"
               "    int run_ok;                          // run OK set\n"
               "    const char* tdspec;                  // the wiring witness line\n"
               "};\n\n")

    def row_arr(name, rows):
        out.append("static const JvmCdRow %s[] = {\n" % name)
        for r in rows:
            if not r["present"]:
                out.append("    {0, 0, 0, 0, 0, 0, 0},\n")
            else:
                out.append("    {1, %s, %s, %s, %d, %d, %d},\n"
                           % (lit(r["aicc"]), lit(r["bic"]), lit(r["ssq"]),
                              r["nx"], r["dof"], r["dim"]))
        out.append("};\n")

    def clc_init(d):
        if d is None:
            return "{nullptr, 0, nullptr, 0, 0, 0, 0, 0, 0}"
        cn = pool("cd", d["coef"])
        vn = pool("cv", d["covdiag"])
        return "{%s, %d, %s, %d, %s, %d, %d, %d, %d}" % (
            cn, len(d["coef"]), vn, len(d["covdiag"]), lit(d["ssq"]),
            d["dim"], d["dof"], d["nx"], d["k"])

    def cand_init(c, key):
        cc = c.cands.get(key)
        if cc is None:
            return "{nullptr, nullptr, 0, nullptr, 0, 0, 0, 0, 0, 0, -1, -1, -1, 0, 0, 0, 0, 0, 0, \"\", \"\"}"
        cn = pool("kd", cc["coef"])
        vn = pool("kv", cc["covdiag"])
        return ("{%s, %s, %d, %s, %d, %s, %d, %d, %d, %d, %d, %d, %d, %d, %d, "
                "%d, %d, %d, %d, %s, %s}" % (
                    cstr(cc["tag"]), cn, len(cc["coef"]), vn, len(cc["covdiag"]),
                    lit(cc["ssq"]), cc["dim"], cc["dof"], cc["nx"], cc["k"],
                    cc["td_pos"], cc["lp_pos"], cc["ea_pos"], cc["nhp"],
                    cc["curk"], cc["curmean"], cc["mean"], cc["adj"], cc["log"],
                    cstr(cc["prespec"]), cstr(cc["spec"])))

    # --- CM ------------------------------------------------------------------
    cmps = []
    for c in fam["CM"]:
        nm = "cdrow_" + c.id.replace("_", "")
        row_arr(nm, c.rows)
        cmps.append((c.id, nm, len(c.rows), c.cmb, decbits(c.hdr[8]),
                     decbits(c.hdr[9])))
    out.append("\nstatic const JvmCdCmpCase kCdCmp[] = {\n")
    for cid, nm, n, cmb, pm, pc in cmps:
        out.append("    {%s, %s, %d, %s, %s, %d, %d, %d},\n"
                   % (cstr(cid), nm, n, lit(pm), lit(pc), *cmb))
    out.append("};\n\n")

    # --- TA + SA -------------------------------------------------------------
    lad = []
    for c in fam["TA"] + fam["SA"]:
        is_sa = c.fam == "SA"
        nm = "cdrow_" + c.id.replace("_", "")
        row_arr(nm, c.rows)
        hdr = c.hdr
        sel = SELECTOR[hdr[6]]
        if is_sa:
            pmodel, pcon, tlp = hdr[7], hdr[8], hdr[10]
            adjust, testmean, has_ea, has_lp = hdr[11], "n", "n", "y"
        else:
            pmodel, pcon, tlp = hdr[7], hdr[8], "2.0"
            has_ea, testmean, adjust, has_lp = hdr[9], hdr[10], hdr[11], hdr[12]
        cands = [cand_init(c, "%s:r%d" % (c.id, i))
                 for i in range(len(c.rows))]
        cands += [cand_init(c, None)] * (8 - len(cands))
        saw = c.saw or {"td": 0, "lp": 0, "adj": 0, "log": 0, "equals": []}
        eq = (saw["equals"] + [0] * 8)[:8]
        lad.append(
            "{%s, %d, %d, %s, %s, %s, %d, %d, %d, %d, %s, %d, {%s}, "
            "%d, %d, %d, %d, {%s}, %d, %d, %d, %d, %d, %d, %d, %s, %s}" % (
                cstr(c.id), int(is_sa), sel,
                lit(decbits(pmodel)), lit(decbits(pcon)), lit(decbits(tlp)),
                yes(adjust), yes(testmean), yes(has_ea), yes(has_lp),
                nm, len(c.rows), ",".join(cands),
                saw["td"], saw["lp"], saw["adj"], saw["log"],
                ",".join(str(x) for x in eq), len(saw["equals"]),
                c.cmb[0], c.cmb[1], c.cmb[2],
                -999 if c.info_atd is None else c.info_atd,
                c.res["result"], c.res["mean"],
                cstr(c.vars_pre or ""), cstr(c.vars_after or "")))
    out.append("static const JvmCdLadderCase kCdLadder[] = {\n")
    for e in lad:
        out.append("    " + e + ",\n")
    out.append("};\n\n")

    # --- FT -------------------------------------------------------------------
    fts = []
    for c in fam["FT"]:
        hdr = c.hdr
        info = c.info_ft or {}
        fts.append(
            "{%s, %s, %d, %d, %d, %s, %s, %s, %s, %s, %d, %s, %s, %s, %s, %s, "
            "%s, %s, %d, %d, %d, %s}" % (
                cstr(c.id), lit(decbits(hdr[9])), yes(hdr[6]), yes(hdr[7]),
                yes(hdr[8]),
                cand_init(c, c.id + ":info-none"),
                cand_init(c, c.id + ":info-td"),
                cand_init(c, c.id + ":info-wd"),
                cand_init(c, c.id + ":upd-td"),
                cand_init(c, c.id + ":upd-wd"),
                1 if c.info_ft else 0,
                lit(info.get("ss0", 0)), lit(info.get("ss1", 0)),
                lit(info.get("ss6", 0)), lit(info.get("ssmc1", 0)),
                lit(info.get("ssmc6", 0)), lit(info.get("pftd1", 0)),
                lit(info.get("pftd6", 0)), info.get("nhp", 0),
                c.res["result"], c.res["mean"], cstr(c.vars_after or "")))
    out.append("static const JvmCdFtestCase kCdFtest[] = {\n")
    for e in fts:
        out.append("    " + e + ",\n")
    out.append("};\n\n")

    # --- FS -------------------------------------------------------------------
    fasts = []
    for c in fam["FS"]:
        hdr = c.hdr
        fssl = c.fssl
        fasts.append(
            "{%s, %s, %s, %d, %s, {%d, %d}, {%d, %d}, {%d, %d}, %d, "
            "%d, %d, %d, %s, %s}" % (
                cstr(c.id), lit(decbits(hdr[6])), lit(decbits(hdr[7])),
                yes(hdr[8]), clc_init(c.clc),
                fssl["td"][0], fssl["td"][1], fssl["lp"][0], fssl["lp"][1],
                fssl["mh"][0], fssl["mh"][1], c.fsin["mean"],
                c.res["result"], c.res["mean"], c.res["est"],
                cstr(c.vars_pre or ""), cstr(c.vars_after or "")))
    out.append("static const JvmCdFastStep kCdFastStep[] = {\n")
    for e in fasts:
        out.append("    " + e + ",\n")
    out.append("};\n\n")

    # --- DS -------------------------------------------------------------------
    def emit_cov(rows, base):
        if not rows:
            return "nullptr, 0"
        out.append("static const JvmCdCovRow %s[] = {\n" % base)
        for i, r in enumerate(rows):
            nm = pool(base.replace("cdcov_", "cc") + str(i), r)
            out.append("    {%s, %d},\n" % (nm, len(r)))
        out.append("};\n")
        return "%s, %d" % (base, len(rows))

    defs = []
    for c in fam["DS"]:
        hdr = c.hdr
        cov = emit_cov(c.dtu, "cdcov_" + c.id.replace("_", ""))
        dt = c.dt
        jt = c.jt or {}
        defs.append(
            "{%s, %s, %d, %d, %d, %s, %s, {%d, %d}, {%d, %d}, {%d, %d}, "
            "%d, %d, %d, %d, %d, %s, %s, %d, %d, %d, %s, %s}" % (
                cstr(c.id), lit(decbits(hdr[6])), yes(hdr[7]), yes(hdr[8]),
                yes(hdr[9]), clc_init(c.dtc), cov,
                dt["td"][0], dt["td"][1], dt["lp"][0], dt["lp"][1],
                dt["ea"][0], dt["ea"][1],
                dt["log"], dt["mean"], c.dsin["mean"],
                jt.get("q", 0), jt.get("df", 0),
                lit(jt.get("value", 0)), lit(jt.get("pvalue", 0)),
                c.res["result"], c.res["mean"], c.res["est"],
                cstr(c.vars_pre or ""), cstr(c.vars_after or "")))
    out.append("static const JvmCdDefaultStep kCdDefaultStep[] = {\n")
    for e in defs:
        out.append("    " + e + ",\n")
    out.append("};\n\n")

    # --- JT -------------------------------------------------------------------
    jts = []
    for c in fam["JT"]:
        cov = emit_cov(c.ucov, "cdjcov_" + c.id.replace("_", ""))
        jts.append("{%s, %d, %d, %d, %s, %s, %s, %s}" % (
            cstr(c.id), int(c.hdr[6]), c.jt["q"], c.jt["k"], clc_init(c.clc),
            cov, lit(c.jt["value"]), lit(c.jt["pvalue"])))
    out.append("static const JvmCdJointCase kCdJoint[] = {\n")
    for e in jts:
        out.append("    " + e + ",\n")
    out.append("};\n\n")

    # --- PT / PX ----------------------------------------------------------------
    pipes = []
    for c in fam["PT"] + fam["PX"]:
        pipes.append("{%s, %d, %s, %d, %s}" % (
            cstr(c.id), int(c.fam == "PX"), cstr(c.hdr[2]),
            c.pipe["run"], cstr(c.pipe["line"])))
    out.append("static const JvmCdPipeCase kCdPipe[] = {\n")
    for e in pipes:
        out.append("    " + e + ",\n")
    out.append("};\n\n")

    # --- pools last (referenced by the tables above; C++ needs them BEFORE:
    # emit pools at the top instead) ------------------------------------------
    body = "".join(out)
    head_end = body.index("struct JvmCdRow {")
    pre, post = body[:head_end], body[head_end:]
    poolsrc = []
    for nm, vals in pools:
        poolsrc.append("static const std::uint64_t %s[] = {%s};\n"
                       % (nm, ",".join(lit(b) for b in vals)))
    body = pre + "".join(poolsrc) + "\n" + post

    body += "\n}  // namespace kalos::test\n\n#endif  // KALOS_TESTS_JVM_CALENDAR_DETECTION_H\n"
    with open(outpath, "w") as fh:
        fh.write(body)
    print("cases:", {k: len(v) for k, v in sorted(fam.items())},
          "pools:", len(pools), "->", outpath)


if __name__ == "__main__":
    main()
