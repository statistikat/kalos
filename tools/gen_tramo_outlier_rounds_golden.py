#!/usr/bin/env python3
"""Generate tests/jvm_tramo_outlier_rounds.h (item 23-F F.2 golden).

Parses the multi-round witness capture
validation/evidence/native/tramo-outlier-rounds/mr-witness.out - the
Scratch23g dual-machine dump (the real OutliersDetectionModule run plus the
manual FastOutliersDetector.process transcription over the same JVM, both
machines byte-identical per the committed capture pair) - and emits the
expected per-case request seeds (raw IEEE-754 series bits, the make()-time
driver fields from the mrd line) plus the full trace expectation: every
probe state dump (FastOutliersDetector fields), every sod dump
(SingleOutlierDetector fields), the add records, the manret note line and
the manouts final set.

Regeneration (from the repository root):

    python3 tools/gen_tramo_outlier_rounds_golden.py \
        > tests/jvm_tramo_outlier_rounds.h

Cases whose block carries the `make null` marker (comatip rejection) expect
a Failed result with no trace.
"""

import re
import sys

STATE_RE = re.compile(r"^st (\w+) ")
MANOUTS_PAIR_RE = re.compile(r"\[(-?\d+),\s*(-?\d+)\]")


def fail(msg):
    raise SystemExit("golden generator: " + msg)


def dbl(tok):
    """'%.17g#hex' -> uint64 bits."""
    if "#" not in tok:
        fail("malformed double token " + tok)
    return int(tok.split("#", 1)[1], 16)


def vec(tokens, i):
    """'null' | N v... -> (null, n, bits) and the next index."""
    if tokens[i] == "null":
        return True, 0, [], i + 1
    n = int(tokens[i])
    bits = [dbl(t) for t in tokens[i + 1:i + 1 + n]]
    return False, n, bits, i + 1 + n


def parse_state(line):
    """state dump line (without the 'st TAG' prefix) -> dict."""
    t = line.split()
    d = {}
    i = 0
    while i < len(t):
        k = t[i]
        if k in ("round", "rflag", "estep", "bstep", "exit", "xcount", "mean"):
            d[k] = int(t[i + 1])
            i += 2
        elif k == "outs":
            n = int(t[i + 1])
            pairs = []
            j = i + 2
            while len(pairs) < n:
                pairs.append((int(t[j].strip("[,")), int(t[j + 1].strip("],"))))
                j += 2
            d["outs"] = pairs
            i = j
        elif k == "lastrem":
            d["lastrem"] = -1 if t[i + 1] == "null" else int(t[i + 1])
            i += 2
        elif k in ("coeff", "tstats", "res", "pars"):
            null, n, bits, i = vec(t, i + 1)
            d[k] = (null, bits)
        elif k == "orders":
            d["orders"] = [int(x) for x in t[i + 1:i + 7]]
            i += 7
        else:
            fail("state token " + k)
    return d


def parse_sod(line):
    t = line.split()
    d = {}
    i = 0
    while i < len(t):
        k = t[i]
        if k == "sod" or k == "scan":
            i += 1
            continue
        if k == "el":
            d["el_null"] = t[i + 1] == "null"
            d["el_n"] = 0 if d["el_null"] else int(t[i + 1])
            i += 2
        elif k == "mad":
            d["mad"] = dbl(t[i + 1])
            i += 2
        elif k in ("lb", "ub"):
            d[k] = int(t[i + 1])
            i += 2
        elif k == "T":
            d["t_null"] = 1 if t[i + 1] == "null" else 0
            i += 2
        elif k == "alw":
            if t[i + 1] == "null":
                d["alw_null"] = 1
                d["alw_rows"] = 0
                d["alw_cols"] = 0
                d["alw_counts"] = []
                i += 2
            else:
                d["alw_null"] = 0
                rc = t[i + 1].split("/")
                d["alw_rows"], d["alw_cols"] = int(rc[0]), int(rc[1])
                d["alw_counts"] = [
                    int(x) for x in t[i + 2:i + 2 + d["alw_cols"]]
                ]
                i += 2 + d["alw_cols"]
        elif k == "maxT":
            d["maxt_threw"] = t[i + 1].startswith("threw:")
            d["maxt"] = 0 if d["maxt_threw"] else dbl(t[i + 1])
            i += 2
        elif k == "pick":
            if t[i + 1].startswith("threw:"):
                d["pick_pos"], d["pick_type"] = -1, -1
                i += 2
            else:
                p, ty = t[i + 1].split(",")
                d["pick_pos"], d["pick_type"] = int(p), int(ty)
                i += 2
        elif k == "cf":
            d["has_cf"] = 1
            d["cf"] = dbl(t[i + 1])
            i += 2
        else:
            fail("sod token " + k)
    return d


def emit_uint(name, bits, out):
    if not bits:
        out.append("inline const std::uint64_t %s[] = {0};" % name)
        return
    out.append("inline const std::uint64_t %s[] = {" % name)
    row = ["    "]
    for b in bits:
        row.append("0x%016xull," % b)
        if len(row[-1]) > 96:
            out.append("".join(row[:-1]) + "\n    " + row[-1])
            row = ["    "]
    out.append("".join(row))
    out.append("};")


def emit_ints(name, xs, out):
    if not xs:
        out.append("inline const int %s[] = {0};" % name)
        return
    out.append("inline const int %s[] = {%s};" % (
        name, ",".join(str(x) for x in xs)))


def parse_case(block, out):
    """block: witness lines of one case; appends the Case emission."""
    name = block[0].split()[1]
    line = None
    vals = None
    mrd = None
    real_status = None
    make_null = False
    events = []          # list of dicts
    for raw in block:
        ln = raw.rstrip("\n")
        if ln.startswith("req "):
            line = ln[4:]
        elif ln.startswith("vals "):
            vals = [dbl(t) for t in ln[5:].split()]
        elif ln.startswith("mrd "):
            mrd = ln
        elif ln.startswith("real_status "):
            real_status = ln.split(" ", 1)[1].strip()
        elif ln == "make null":
            make_null = True
        elif STATE_RE.match(ln):
            tag = ln.split()[1]
            st = parse_state(ln.split(None, 2)[2])
            for e in reversed(events):
                if e["tag"] == "add" and "st" not in e:
                    e["st"] = st
                    break
            else:
                events.append({"tag": tag, "st": st})
        elif ln.startswith("sod scan"):
            sod = parse_sod(ln)
            for e in reversed(events):
                if e["tag"] == "scan":
                    e["sod"] = sod
                    break
            else:
                fail(name + ": sod line without a scan event")
        elif ln.startswith("add pos "):
            t = ln.split()
            events.append({"tag": "add", "add_pos": int(t[2]),
                           "add_type": int(t[4]), "add_cf": dbl(t[6])})
        elif ln == "tailest 1":
            continue  # the paired "st tailest" dump carries the event
        elif ln.startswith("manret "):
            t = ln.split()
            events.append({"tag": "manret", "manret": int(t[1]),
                           "note": t[3], "round": int(t[5])})
        elif ln.startswith("manouts"):
            pairs = [(int(a), int(b)) for a, b in MANOUTS_PAIR_RE.findall(ln[8:])]
            events.append({"tag": "manouts", "outs": pairs})
    if line is None or vals is None or real_status is None:
        fail(name + ": missing req/vals/real_status")
    if make_null:
        emit_uint("k_val_" + name, vals, out)
        out.append("inline const Event k_ev_%s[] = {{\"\",0,nullptr,0,nullptr,0,"
                   "0,0,0,0,0,0,\"\",0,nullptr,nullptr}};" % name)
        out.append("")
        out.append("inline const Case k_case_%s = {" % name)
        out.append('      "%s", 1, "%s", %d, k_val_%s, 0, 0, 0, 0, 0, 0, "", "%s",'
                   " 1, k_ev_%s};" % (name, line.replace('"', '\\"'),
                                      len(vals), name, real_status, name))
        out.append("")
        return None
    if mrd is None:
        fail(name + ": missing mrd")
    if not events:
        fail(name + ": no events")
    t = mrd.split()
    cv = dbl(t[2])
    mvx = int(t[4])
    maxround = int(t[6])
    maxout = int(t[8])
    lb = int(t[10])
    ub = int(t[12])
    types = mrd.split("types[")[1].rstrip("]\n")

    emit_uint("k_val_" + name, vals, out)
    n = len(events)
    ev_em = []
    for i, e in enumerate(events):
        pre = "k_%s_%d_" % (name, i)
        if "st" in e:
            st = e["st"]
            emit_ints(pre + "op", [p for p, _ in st["outs"]], out)
            emit_ints(pre + "ot", [ty for _, ty in st["outs"]], out)
            for key in ("coeff", "tstats", "res", "pars"):
                emit_uint(pre + key, st[key][1], out)
            emit_ints(pre + "orders", st["orders"], out)
            st_expr = ("ExpState{%d, %d, %d, %d, %d, %d, %s, %s, %d, %d, %s, "
                       "%d, %d, %s, %d, %d, %s, %d, %d, %s, %d, %d, %s}" % (
                           st["round"], st["rflag"], st["estep"], st["bstep"],
                           st["exit"], len(st["outs"]), pre + "op", pre + "ot",
                           st["lastrem"],
                           1 if st["coeff"][0] else 0,
                           "nullptr" if st["coeff"][0] else pre + "coeff",
                           len(st["coeff"][1]),
                           1 if st["tstats"][0] else 0,
                           "nullptr" if st["tstats"][0] else pre + "tstats",
                           len(st["tstats"][1]),
                           1 if st["res"][0] else 0,
                           "nullptr" if st["res"][0] else pre + "res",
                           len(st["res"][1]),
                           len(st["pars"][1]), pre + "pars",
                           st["xcount"], st["mean"], pre + "orders"))
            st_name = pre + "st"
            out.append("inline const ExpState %s = %s;" % (st_name, st_expr))
            st_ref = "&" + st_name
        else:
            st_ref = "nullptr"
        if "sod" in e:
            sod = e["sod"]
            emit_ints(pre + "alw", sod["alw_counts"], out)
            sod_expr = ("%s{%d, %d, 0x%016xull, %d, %d, %d, %d, %d, %d, %s, "
                        "%d, 0x%016xull, %d, %d, %d, 0x%016xull}" % (
                            "ExpSod", 1 if sod["el_null"] else 0, sod["el_n"],
                            sod["mad"], sod["lb"], sod["ub"], sod["t_null"],
                            sod["alw_null"], sod["alw_rows"], sod["alw_cols"],
                            (pre + "alw") + ("" if sod["alw_counts"] else " /*none*/"),
                            1 if sod["maxt_threw"] else 0, sod["maxt"],
                            sod["pick_pos"], sod["pick_type"],
                            1 if "cf" in sod else 0, sod.get("cf", 0)))
            sod_name = pre + "sod"
            out.append("inline const ExpSod %s = %s;" % (sod_name, sod_expr))
            sod_ref = "&" + sod_name
        else:
            sod_ref = "nullptr"
        add = ("1, %d, %d, 0x%016xull" % (e["add_pos"], e["add_type"], e["add_cf"])
               if e["tag"] == "add" else "0, 0, 0, 0")
        mr = ("1, %d, %d, \"%s\"" % (e["manret"], e["round"], e["note"])
              if e["tag"] == "manret" else "0, 0, 0, \"\"")
        op, ot, nouts = "nullptr", "nullptr", 0
        if e["tag"] == "manouts":
            emit_ints(pre + "mop", [p for p, _ in e["outs"]], out)
            emit_ints(pre + "mot", [ty for _, ty in e["outs"]], out)
            op, ot, nouts = pre + "mop", pre + "mot", len(e["outs"])
        ev_em.append('{"%s", %d, %s, %d, %s, %s, %s, %d, %s, %s}' % (
            e["tag"], 1 if "st" in e else 0, st_ref,
            1 if "sod" in e else 0, sod_ref, add, mr, nouts, op, ot))
    out.append("inline const Event k_ev_%s[] = {" % name)
    for em in ev_em:
        out.append("    " + em + ",")
    out.append("};")
    out.append("inline const Case k_case_%s = {" % name)
    out.append('      "%s", 0, "%s", %d, k_val_%s, 0x%016xull, %d, %d, %d, %d, '
               '%d, "%s", "%s", %d, k_ev_%s};' % (
                   name, line.replace('"', '\\"'), len(vals), name, cv, mvx,
                   maxround, maxout, lb, ub, types, real_status, n, name))
    out.append("")
    return name


HEADER = """// Generated by tools/gen_tramo_outlier_rounds_golden.py - do not edit.
// Item 23-F F.2 multi-round outlier golden: the Scratch23g dual-machine
// witness capture (validation/evidence/native/tramo-outlier-rounds) as
// per-case request seeds (raw series bits, make()-time driver fields from
// the mrd line) plus the full expected trace: probe state dumps, sod dumps,
// add records, the manret note and the final manouts set. All doubles are
// raw IEEE-754 bit patterns. See the generator docstring for the command.
#ifndef KALOS_TESTS_JVM_TRAMO_OUTLIER_ROUNDS_H
#define KALOS_TESTS_JVM_TRAMO_OUTLIER_ROUNDS_H

#include <cstddef>
#include <cstdint>

namespace golden_tor {

struct ExpState {
    int round, rflag, estep, bstep, exit_flag;
    int outs; const int* outs_pos; const int* outs_type;
    int lastrem_len; // -1: null; otherwise the witness istat length (2)
    int coeff_null; const std::uint64_t* coeff; std::size_t coeff_n;
    int tstats_null; const std::uint64_t* tstats; std::size_t tstats_n;
    int res_null; const std::uint64_t* res; std::size_t res_n;
    std::size_t pars_n; const std::uint64_t* pars;
    int xcount, mean; const int* orders; // p d q bp bd bq
};

struct ExpSod {
    int el_null; std::size_t el_n;
    std::uint64_t mad; int lb, ub; int t_null;
    int alw_null; std::size_t alw_rows, alw_cols; const int* alw_counts;
    int maxt_threw; std::uint64_t maxt;
    int pick_pos, pick_type; int has_cf; std::uint64_t cf;
};

struct Event {
    const char* tag;
    int has_state; const ExpState* st;
    int has_sod; const ExpSod* sod;
    int has_add, add_pos, add_type; std::uint64_t add_cf;
    int has_manret, manret, manret_round; const char* note;
    int nouts; const int* outs_pos; const int* outs_type; // manouts list
};

struct Case {
    const char* id;
    int make_null;      // 1: comatip rejected the series (Failed, no trace)
    const char* line;   // the captured SCAN protocol line
    std::size_t nvalues; const std::uint64_t* values;
    std::uint64_t cv; int mvx, maxround, maxout, lb, ub; // the mrd fields
    const char* types;         // the factory echo, "AO,LS,TC,SO"
    const char* real_status;   // the live module result (D3 flip table)
    std::size_t nevents; const Event* events;
};

"""


def main():
    paths = sys.argv[1:] or [
        "validation/evidence/native/tramo-outlier-rounds/mr-witness.out",
        "validation/evidence/native/tramo-outlier-rounds/stationary-witness.out",
    ]
    lines = []
    for path in paths:
        with open(path) as handle:
            lines.extend(handle.readlines())
    blocks, cur = [], None
    for ln in lines:
        if ln.startswith("case "):
            cur = [ln]
            blocks.append(cur)
        elif cur is not None:
            cur.append(ln)
            if ln.startswith("end "):
                cur = None
    out = [HEADER]
    names = []
    for block in blocks:
        nm = parse_case(block, out)
        if nm:
            names.append(nm)
        if block[0].split()[1].startswith("make_null"):
            names.append(block[0].split()[1])
    # make-null cases carry the empty trace placeholder above but still enter
    # the table.
    all_names = [b[0].split()[1] for b in blocks]
    out.append("inline const Case* const* table(std::size_t& count) {")
    out.append("    static const Case* const kCases[] = {")
    for nm in all_names:
        out.append("        &k_case_%s," % nm)
    out.append("    };")
    out.append("    count = sizeof(kCases) / sizeof(kCases[0]);")
    out.append("    return kCases;")
    out.append("}")
    out.append("")
    out.append("} // namespace golden_tor")
    out.append("#endif")
    sys.stdout.write("\n".join(out) + "\n")


if __name__ == "__main__":
    main()
