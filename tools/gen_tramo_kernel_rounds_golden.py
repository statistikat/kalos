#!/usr/bin/env python3
"""Generate tests/jvm_tramo_kernel_rounds.h (item 23-F.3 golden).

Parses the double-captured TramoKernel round-driver corpus
validation/evidence/native/tramo-kernel-rounds/kr-capture-1.out - the
Scratch23r dual-machine dump (the jar TramoKernel run plus the manual
bytecode transcription over the same JVM, byte-identical per the
committed identity lines) - and emits, per case, the
kalos::TramoKernelRoundsRequest (resolved spec, prestage facts, the
ordered seam facts with their module log blocks) plus the expected
streams: the kernel ProcessingLog event stream (from the jar-side run),
the per-iterate state snapshots (rsnap == msnap), and the init-line
bits (the calcCv / pcr initialisation the unit computes).

The unit RECOMPUTES the kernel decisions (estimate taken, differencing
drop, round-0 accept, the refStats gate operands and verdict, the gate
return and pcr, reduceVa, the pass3 reset, the iterate end, the whole
log structure); the heavy-stage outcomes and module log texts ride as
request facts. The replay (tests/test_tramo_kernel_rounds.cpp) compares
the recomputed decisions against the JVM-echoed fields of the facts and
the snapshot/event streams against the captured jar-side streams.

Regeneration (from the repository root):

    python3 tools/gen_tramo_kernel_rounds_golden.py \
        validation/evidence/native/tramo-kernel-rounds/kr-capture-1.out \
        > tests/jvm_tramo_kernel_rounds.h

kr_q_air (quarterly) is emitted as the D11 refusal witness: its request
is built faithfully but only a throw is expected (no stream replay).
"""

import re
import struct
import sys

EVENT_KINDS = {"push": 0, "pop": 1, "info": 2, "remark": 3, "warn": 4,
               "step": 5}
RESULT_ENUM = {
    "Changed": "TramoKernelStageResult::changed",
    "Unchanged": "TramoKernelStageResult::unchanged",
    "Unprocessed": "TramoKernelStageResult::unprocessed",
    "Failed": "TramoKernelStageResult::failed",
}
SEAM_ENUM = {
    "estimate": "TramoKernelSeamKind::estimation",
    "diff": "TramoKernelSeamKind::differencing",
    "diffdrop": "TramoKernelSeamKind::differencing_drop",
    "arma": "TramoKernelSeamKind::arma",
    "armaq1": "TramoKernelSeamKind::arma_q1",
    "scan": "TramoKernelSeamKind::scan",
    "estmodel": "TramoKernelSeamKind::estimation_model",
    "estmodelreduceva": "TramoKernelSeamKind::estimation_reduce",
    "regtest": "TramoKernelSeamKind::regression_test",
    "round0": "TramoKernelSeamKind::round0",
    "pass2gate": "TramoKernelSeamKind::pass2_gate",
    "pass2": "TramoKernelSeamKind::pass2_return",
    "scontroller": "TramoKernelSeamKind::seasonality_controller",
    "controller": "TramoKernelSeamKind::model_controller",
    "iterate": "TramoKernelSeamKind::iterate_end",
}
END_ENUM = {
    "done": "TramoKernelIterateEnd::done",
    "pass3reset": "TramoKernelIterateEnd::pass3_reset",
    "estmodelfail": "TramoKernelIterateEnd::estimation_fail",
    "regfail": "TramoKernelIterateEnd::regression_fail",
}
LOGKIND = {0: "TramoKernelLogKind::push", 1: "TramoKernelLogKind::pop",
           2: "TramoKernelLogKind::info", 3: "TramoKernelLogKind::remark",
           4: "TramoKernelLogKind::warning", 5: "TramoKernelLogKind::step"}
MODULE_BLOCK_SEAMS = {
    "differencing selection": "diff",
    "arma selection": "arma",
    "outliers selection": "scan",
    "full estimation": "estmodel",
}


def fail(msg):
    raise SystemExit("golden generator: " + msg)


def bits(tok):
    """raw 16-hex IEEE-754 bit string -> uint64."""
    return int(tok, 16)


def g17(tok):
    """%.17g text -> uint64 IEEE-754 bits (17 digits round-trip)."""
    return struct.unpack("<Q", struct.pack("<d", float(tok)))[0]


def cstr(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def hexvec(name, values):
    """emit a `static const double name[] = { B(0x..), ... };` block."""
    out = ["    static const double %s[] = {" % name]
    line = "        "
    for v in values:
        piece = "B(0x%016xULL)," % v
        if len(line) + len(piece) > 92:
            out.append(line.rstrip())
            line = "        "
        line += piece
    if line.strip():
        out.append(line.rstrip())
    out.append("    };")
    return out


def parse_orders(tok):
    m = re.match(r"^(\d+),(\d+),(\d+),(\d+),(\d+),(\d+)@(\d+)$", tok)
    if not m:
        fail("bad orders token " + tok)
    return [int(x) for x in m.groups()]


def parse_outs(line, start_key=" outs ", end_key=" pars "):
    """outlier names are '<TYPE> (<date>)' with an inner space: slice the
    raw line between the sentinels, split on ';'."""
    i = line.find(start_key)
    if i < 0:
        return []
    j = line.find(end_key, i) if end_key else -1
    seg = line[i + len(start_key):j if j >= 0 else len(line)]
    seg = seg.strip()
    if seg == "-":
        return []
    return [x.strip() for x in seg.split(";") if x.strip()]


class Snap:
    def __init__(self, line):
        kv = dict(re.findall(r"(\w+)=([^\s]+)", line))
        for k in ("pass", "round", "pass3", "needOut", "needAuto",
                  "refAuto", "refStats", "seasonal", "ost"):
            setattr(self, k, int(kv[k]))
        self.curva = g17(kv["curva"])
        self.pcr = g17(kv["pcr"])

    def key(self):
        return (self.pass_, self.round, self.pass3, self.needOut,
                self.needAuto, self.curva, self.pcr, self.refAuto,
                self.refStats, self.seasonal, self.ost)

    @property
    def pass_(self):
        return getattr(self, "pass")


def parse_capture(path):
    """path -> list of case dicts."""
    cases = []
    cur = None
    run = 0
    for raw in open(path):
        line = raw.rstrip("\n")
        t = line.split()
        if not t:
            continue
        if t[0] == "KER" and len(t) > 3 and t[2] == "specname":
            if cur is not None:
                fail("case %s ended without its identity line" % cur["id"])
            cur = {"id": t[1], "freq": int(t[5]), "n": int(t[9]),
                   "events": [[], []], "snaps": [[], []],
                   "record": [], "finals": [[], []], "run": 0,
                   "resolved": None, "prestate": [], "init": None,
                   "identity": None}
            continue
        if cur is None:
            continue
        if t[0] == "KER" and len(t) > 2 and t[2] == "resolved":
            cur["resolved"] = t[3:]
            continue
        if t[0] == "evt":
            if t[2] == "push" and t[3] == "0" and t[4:] == ["tramo"] and \
                    t[1] == "1" and cur["events"][0]:
                cur["run"] = 1
            kind = t[2]
            level = int(t[3])
            parts = line.split(None, 4)
            text = parts[4] if len(parts) > 4 else ""
            if kind not in EVENT_KINDS:
                fail("unknown evt kind " + kind)
            cur["events"][cur["run"]].append(
                (EVENT_KINDS[kind], level, text))
            continue
        if t[0] in ("rsnap", "msnap"):
            cur["snaps"][0 if t[0] == "rsnap" else 1].append(
                Snap(line))
            continue
        if t[0] in ("rfinal", "mfinal"):
            cur["finals"][0 if t[0] == "rfinal" else 1].append(
                line.split(None, 1)[1])
            continue
        if t[0] in ("prestate", "init", "seam", "stats", "p2dbg",
                    "statdbg"):
            if t[0] == "init":
                cur["init"] = t
            elif t[0] == "prestate":
                cur["prestate"].append(line)
            else:
                cur["record"].append(line)
            continue
        if t[0] == "identity":
            cur["identity"] = t
            if t[1] != cur["id"]:
                fail("identity line for " + t[1])
            if t[2:] != ["events", "1", "final", "1", "all", "1"]:
                fail("case %s is not identity all-1" % cur["id"])
            if cur["events"][0] != cur["events"][1]:
                fail("case %s: jar and manual event streams differ"
                     % cur["id"])
            if [s.key() for s in cur["snaps"][0]] != \
                    [s.key() for s in cur["snaps"][1]]:
                fail("case %s: jar and manual snapshots differ" % cur["id"])
            if cur["finals"][0] != cur["finals"][1]:
                fail("case %s: jar and manual final blocks differ"
                     % cur["id"])
            cases.append(cur)
            cur = None
            continue
    if cur is not None:
        fail("trailing incomplete case " + cur["id"])
    return cases


def record_walk(case):
    """Parse the record section into (spec extras, prestage, seams) and
    cross-check the record against the manual-run event stream."""
    resolved = {}
    tok = case["resolved"]
    i = 0
    keys = ("autoModel outliers ao ls tc so tcrate ml oc fct accept pcr "
            "tsig pc cancel ub1 ub2 va tol ubp meanTest tdAuto tdTest "
            "easter").split()
    while i < len(tok):
        k = tok[i]
        if k in keys:
            resolved[k] = tok[i + 1]
            i += 2
        elif k == "tfunc":
            resolved[k] = tok[i + 1]
            i += 2
        else:
            fail("unparsed resolved token " + k)
    events = case["events"][1]
    # prestage = the module blocks before the first step; events[0..1]
    # are the "tramo"/"automatic model identification" pushes the unit
    # emits itself.
    first_step = next(idx for idx, e in enumerate(events) if e[0] == 5)
    prestage_events = events[2:first_step]

    def kernel_owned(idx_ev):
        kind, level, text = idx_ev
        if kind == 5:
            return True  # "round N" step
        if (kind, level, text) in ((0, 0, "tramo"), (0, 1, "automatic model"
                                                    " identification"),
                                   (1, 1, "-"), (1, 0, "-"),
                                   (3, 2, "reduction of the critical value"
                                          " for outliers detection"),
                                   (4, 2, "last chance model")):
            return True
        return False

    # collect module blocks (push name .. pop) with their positions
    blocks = []  # (start, end, name, [inner events incl. wrapper])
    spans = []   # index ranges consumed by blocks
    j = first_step
    while j < len(events):
        kind, level, text = events[j]
        if kind == 0 and level == 2 and text in MODULE_BLOCK_SEAMS:
            inner = [(kind, level, text)]
            start = j
            j += 1
            while j < len(events):
                k2, l2, t2 = events[j]
                if k2 == 1 and l2 == 2:
                    inner.append((k2, l2, t2))
                    break
                inner.append((k2, l2, t2))
                j += 1
            if j >= len(events):
                fail("unterminated module block " + text)
            spans.append((start, j + 1))
            blocks.append((start, j, text, inner))
            j += 1
        else:
            j += 1
    used = [False] * len(blocks)

    # leftover events (controller-sweep prints, no module block): per
    # step region, not inside a block, not kernel-owned.
    def in_block(idx):
        return any(a <= idx < b for a, b in spans)

    region_leftovers = []  # one list per step region
    region = -1
    for idx in range(first_step, len(events)):
        ev = events[idx]
        if ev[0] == 5:
            region += 1
            region_leftovers.append([])
            continue
        if in_block(idx) or kernel_owned(ev):
            continue
        region_leftovers[region].append(ev)

    def take_block(kind_word):
        if kind_word not in MODULE_BLOCK_SEAMS_KEY:
            return None
        want = MODULE_BLOCK_SEAMS_KEY[kind_word]
        for b in range(len(blocks)):
            if not used[b] and blocks[b][2] == want:
                used[b] = True
                return blocks[b][3]
        return None

    prestate_regtest = None
    prestate_orders = None
    prestate_mean = None
    prestate_log = None
    prestate_outs = []
    for line in case["prestate"]:
        t = line.split()
        if t[1] == "regtest":
            prestate_regtest = t[2]
        elif t[1] == "orders":
            prestate_orders = parse_orders(t[2])
            prestate_mean = t[t.index("mean") + 1] == "1"
            prestate_log = t[t.index("log") + 1] == "1"
            prestate_outs = parse_outs(line, " outliers ", "")
    if prestate_orders is None or prestate_regtest is None:
        fail("missing prestate lines")

    snaps = case["snaps"][1]
    seams = []
    pending_ref = None
    pending_pass = None
    pending_dbg = None
    snap_at = []  # snapshot index consumed as scontroller follow-up
    snap_iter = 0
    # snapshot positions relative to the record: snapshots interleave the
    # record by iterate; track how many snapshots precede each scontroller
    # by walking iterate_end / round0 / pass2-return boundaries.
    for line in case["record"]:
        t = line.split()
        if t[0] == "stats" and t[1] in ("refstats", "pass2") and t[2] != "e":
            kv = {}
            k = 2
            while k < len(t):
                kv[t[k]] = t[k + 1]
                k += 2
            row = dict(freq=int(kv["freq"]), k=int(kv["k"]),
                       dim=int(kv["dim"]), obs=int(kv["obs"]),
                       lbp=g17(kv["lbp"]), se=g17(kv["se"]),
                       lb=g17(kv["lb"]), bic=g17(kv["bic"]),
                       oc=int(kv["oc"]), eff=int(kv["eff"]), e=None)
            if t[1] == "refstats":
                pending_ref = row
            else:
                pending_pass = row
            continue
        if t[0] == "stats" and t[2] == "e":
            e = [bits(x) for x in t[3:]]
            row = pending_ref if (pending_ref and
                                  pending_ref["e"] is None) else pending_pass
            if row is None or row["e"] is not None:
                fail("orphan stats e line")
            row["e"] = e
            continue
        if t[0] == "p2dbg":
            k = t.index("arima")
            arima = parse_orders(t[k + 1])
            pk = t.index("pars")
            pars = [bits(x) for x in t[pk + 1:]]
            pending_dbg = (arima, pars)
            continue
        if t[0] == "statdbg":
            if pending_pass is None:
                fail("statdbg without a pass2 row")
            if (g17(t[3]) != pending_pass["lbp"] or
                    g17(t[5]) != pending_pass["se"] or
                    g17(t[7]) != pending_pass["lb"]):
                fail("statdbg disagrees with the pass2 row")
            continue
        if t[0] != "seam":
            fail("unparsed record line " + line)
        kind = t[2]
        s = {"kind": kind}

        def pars_from():
            p = line.find(" pars ")
            return [bits(x) for x in line[p + 6:].split()] if p >= 0 else []

        if kind == "estimate":
            if t[3] != "taken":
                fail("bad estimate line")
            s["taken"] = int(t[4])
            s["ip"] = bits(t[6]) if t[6] != "-" else None
        elif kind in ("diff", "scan", "scontroller", "controller"):
            s["res"] = t[4]
            s["orders"] = parse_orders(t[t.index("orders") + 1])
            s["mean"] = t[t.index("mean") + 1] == "1"
            s["outs"] = parse_outs(line)
            s["pars"] = pars_from()
            if kind == "scan":
                s["cv"] = g17(t[t.index("cv") + 1])
        elif kind == "arma":
            s["res"] = t[4]
            s["orders"] = parse_orders(t[t.index("orders") + 1])
            s["mean"] = t[t.index("mean") + 1] == "1"
            s["outs"] = []
            s["pars"] = []
        elif kind == "armaq1":
            s["orders"] = parse_orders(t[t.index("orders") + 1])
        elif kind == "diffdrop":
            pass
        elif kind == "estmodel":
            s["ret"] = int(t[4])
            s["pass"] = int(t[6])
            s["nnsig"] = int(t[8])
            s["orders"] = parse_orders(t[t.index("orders") + 1])
            s["outs"] = parse_outs(line)
            s["pars"] = pars_from()
            s["mean"] = None
        elif kind == "estmodelreduceva":
            s["curva"] = g17(t[4])
        elif kind == "regtest":
            site = t[4]
            if site.startswith("retry"):
                s["retry"] = int(site[5:])
            else:
                s["retry"] = 0
            s["tsig"] = g17(t[6])
            s["res"] = t[8]
            s["mean"] = t[10] == "1"
            if len(t) > 11 and t[11] == "t":
                s["t"] = [bits(x) for x in t[12:] if x != "-"]
            else:
                fail("regtest line without the t field")
        elif kind == "pass2gate":
            if t[3] == "first":
                s["first"] = True
            else:
                s["first"] = False
                s["lbp"] = g17(t[4])
                s["se"] = g17(t[6])
                s["rlbp"] = g17(t[8])
                s["rse"] = g17(t[10])
                s["nb"] = int(t[12])
                s["nr"] = int(t[14])
                s["revert"] = int(t[16])
                s["why"] = t[18]
            if pending_pass is None or pending_pass["e"] is None:
                fail("pass2gate without its stats row")
            if pending_dbg is None:
                fail("pass2gate without p2dbg")
            s["stats"] = pending_pass
            s["arima"] = pending_dbg[0]
            s["apars"] = pending_dbg[1]
            pending_pass = None
            pending_dbg = None
        elif kind == "pass2":
            s["ret"] = int(t[4])
            s["pcr"] = g17(t[6])
        elif kind == "iterate":
            s["end"] = t[3]
        elif kind == "round0":
            s["lbp"] = g17(t[4])
            s["accept"] = int(t[6])
            if pending_ref is None or pending_ref["e"] is None:
                fail("round0 without its refstats row")
            if pending_ref["lbp"] != s["lbp"]:
                fail("round0 lbp disagrees with the refstats row")
            s["stats"] = pending_ref
            pending_ref = None
        else:
            fail("unknown seam kind " + kind)
        # attach the module block if this kind owns one
        s["events"] = take_block(kind) or []
        seams.append(s)
    for b in range(len(blocks)):
        if not used[b]:
            fail("unmatched module block " + blocks[b][2])
    # Attach the leftover events (controller-sweep prints outside any
    # module block) to the first controller seam of their iterate region
    # (the sweep prints in controller order; all leftover events of a
    # region ride as one fact block, preserving stream order).
    ends_seen = 0
    first_ctrl_in_region = {}
    for s in seams:
        if s["kind"] == "controller":
            first_ctrl_in_region.setdefault(ends_seen, s)
        if (s["kind"] == "iterate" or s["kind"] == "round0"
                or (s["kind"] == "pass2" and s.get("ret") == 0)):
            ends_seen += 1
    for r, evs in enumerate(region_leftovers):
        if not evs:
            continue
        host = first_ctrl_in_region.get(r)
        if host is None:
            fail("events outside any block in region %d of %s without a "
                 "controller seam: %s" % (r, case["id"], evs))
        host["events"] = host["events"] + evs
    if pending_ref or pending_pass:
        fail("unattached stats row")
    return resolved, prestage_events, prestate_regtest, prestate_orders, \
        prestate_mean, prestate_log, prestate_outs, seams


MODULE_BLOCK_SEAMS_KEY = {v: k for k, v in MODULE_BLOCK_SEAMS.items()}


def scontroller_context(case, seams):
    """Fill seasonal_after / original_seasonality_after for the
    seasonality_controller seams from the following snapshot (every
    Changed answer in the corpus ends in a pass3 reset, so a following
    iterate entry - and snapshot - exists); Unchanged answers keep the
    running context state."""
    snaps = case["snaps"][1]
    n_est = sum(1 for s in seams if s["kind"] == "estimate")
    if len(snaps) != n_est:
        fail("snapshot/estimate-site count mismatch")
    seasonal = snaps[0].seasonal
    ost = snaps[0].ost
    idx = 0
    for s in seams:
        if s["kind"] == "estimate":
            snap = snaps[idx]
            idx += 1
            seasonal, ost = snap.seasonal, snap.ost
        if s["kind"] == "scontroller":
            if s["res"] == "Changed":
                if idx >= len(snaps):
                    fail("a Changed seasonality controller without a "
                         "following snapshot (state not observable)")
                s["seasonal_after"] = snaps[idx].seasonal
                s["ost_after"] = snaps[idx].ost
            else:
                s["seasonal_after"] = seasonal
                s["ost_after"] = ost
    return


def emit_case(out, case):
    cid = case["id"]
    resolved, pre_events, preres, pro, pmean, plog, pots, seams = \
        record_walk(case)
    scontroller_context(case, seams)
    snaps = case["snaps"][0]
    events = case["events"][0]
    init = case["init"]
    fn = "build_" + cid
    out.append("inline void %s(kalos::TramoKernelRoundsRequest& rq) {" % fn)
    out.append("  rq.spec.annual_frequency = %d;" % case["freq"])
    out.append("  rq.spec.using_auto_model = %s;" %
               ("true" if resolved["autoModel"] == "1" else "false"))
    out.append("  rq.spec.outliers_detection = %s;" %
               ("true" if resolved["outliers"] == "1" else "false"))
    out.append("  rq.spec.accept_airline = %s;" %
               ("true" if resolved["accept"] == "1" else "false"))
    out.append("  rq.spec.mean_is_test = %s;" %
               ("true" if resolved["meanTest"] == "1" else "false"))
    out.append("  rq.spec.pcr = B(0x%016xULL);" % g17(resolved["pcr"]))
    out.append("  rq.spec.reduce_va = B(0x%016xULL);" % g17(resolved["pc"]))
    out.append("  rq.spec.tsig = B(0x%016xULL);" % g17(resolved["tsig"]))
    out.append("  rq.spec.critical_value = B(0x%016xULL);" %
               g17(resolved["va"]))
    ip = next((s["ip"] for s in seams
               if s["kind"] == "estimate" and s["ip"] is not None),
              struct.unpack("<Q", struct.pack("<d", 1e-7))[0])
    out.append("  rq.spec.intermediate_precision = B(0x%016xULL);" % ip)
    # initProcessing feeds calcCv the ACTUAL (non-missing) observation
    # count. The probe dumped the init bits, not the count: for the
    # calc branch invert the (strictly monotone) linear branch of
    # TramoUtility.calcCv and verify the inversion reproduces the captured
    # curva bit-exactly; any other branch keeps the series length and the
    # scan-cv cross-check adjudicates.
    obs = case["n"]
    if g17(resolved["va"]) == 0.0:
        inv = int(round((float(init[2]) - 3.3) / 0.0025)) + 50
        if (struct.pack("<Q", g17(init[2]))
                == struct.pack("<d", 3.3 + 0.0025 * (inv - 50))):
            obs = inv
    out.append("  rq.spec.observations = %d;" % obs)
    ctrl = 0
    run = 0
    for s in seams:
        if s["kind"] == "controller":
            run += 1
            ctrl = max(ctrl, run)
        else:
            run = 0
    out.append("  rq.spec.controller_count = %d;" % ctrl)
    # prestage
    out.append("  rq.prestage.regression_test = %s;" % cstr(preres))
    o = pro
    out.append("  rq.prestage.orders = kalos::TramoOrders{%d, %d, %d, %d, "
               "%d, %d};" % (o[0], o[1], o[2], o[3], o[4], o[5]))
    out.append("  rq.prestage.mean = %s;" % ("true" if pmean else "false"))
    out.append("  rq.prestage.log = %s;" % ("true" if plog else "false"))
    if pots:
        out.append("  rq.prestage.outliers = {%s};" %
                   ", ".join(cstr(x) for x in pots))
    out.append("  rq.prestage.seasonal = %s;" %
               ("true" if snaps[0].seasonal else "false"))
    out.append("  rq.prestage.original_seasonality_test = %d;" % snaps[0].ost)
    if pre_events:
        out.append("  rq.prestage.events = {")
        for k, lvl, text in pre_events:
            out.append("      {%s, %d, %s}," % (LOGKIND[k], lvl, cstr(text)))
        out.append("  };")
    # seams
    vecn = 0
    for s in seams:
        out.append("  {")
        out.append("    TramoKernelSeam s;")
        out.append("    s.kind = %s;" % SEAM_ENUM[s["kind"]])
        if s["events"]:
            out.append("    s.events = {")
            for k, lvl, text in s["events"]:
                out.append("        {%s, %d, %s}," %
                           (LOGKIND[k], lvl, cstr(text)))
            out.append("    };")
        kind = s["kind"]
        if kind == "estimate":
            out.append("    s.estimation_taken = %s;" %
                       ("true" if s["taken"] else "false"))
            if s["ip"] is not None:
                out.append("    s.intermediate_precision = "
                           "B(0x%016xULL);" % s["ip"])
        elif kind in ("diff", "arma", "scan", "scontroller", "controller"):
            out.append("    s.result = %s;" % RESULT_ENUM[s["res"]])
            o = s["orders"]
            out.append("    s.orders = TramoOrders{%d, %d, %d, %d, %d, %d};"
                       % (o[0], o[1], o[2], o[3], o[4], o[5]))
            out.append("    s.mean = %s;" % ("true" if s["mean"] else "false"))
            if s["outs"]:
                out.append("    s.outliers = {%s};" %
                           ", ".join(cstr(x) for x in s["outs"]))
            if s["pars"]:
                nm = "v%d_%d" % (vecn, len(out))
                vecn += 1
                out.extend(hexvec(nm, s["pars"]))
                out.append("    s.parameters = "
                           "std::vector<double>(%s, %s + %d);" %
                           (nm, nm, len(s["pars"])))
            if kind == "scan":
                out.append("    s.critical_value = B(0x%016xULL);" % s["cv"])
            if kind == "scontroller":
                out.append("    s.seasonal_after = %s;" %
                           ("true" if s["seasonal_after"] else "false"))
                out.append("    s.original_seasonality_after = %d;" %
                           s["ost_after"])
        elif kind == "armaq1":
            o = s["orders"]
            out.append("    s.orders = TramoOrders{%d, %d, %d, %d, %d, %d};"
                       % (o[0], o[1], o[2], o[3], o[4], o[5]))
        elif kind == "estmodel":
            out.append("    s.returned = %d;" % s["ret"])
            out.append("    s.pass_at_call = %d;" % s["pass"])
            out.append("    s.nnsig = %d;" % s["nnsig"])
            o = s["orders"]
            out.append("    s.orders = TramoOrders{%d, %d, %d, %d, %d, %d};"
                       % (o[0], o[1], o[2], o[3], o[4], o[5]))
            if s["outs"]:
                out.append("    s.outliers = {%s};" %
                           ", ".join(cstr(x) for x in s["outs"]))
            if s["pars"]:
                nm = "v%d_%d" % (vecn, len(out))
                vecn += 1
                out.extend(hexvec(nm, s["pars"]))
                out.append("    s.parameters = "
                           "std::vector<double>(%s, %s + %d);" %
                           (nm, nm, len(s["pars"])))
        elif kind == "estmodelreduceva":
            out.append("    s.critical_value = B(0x%016xULL);" % s["curva"])
        elif kind == "regtest":
            out.append("    s.result = %s;" % RESULT_ENUM[s["res"]])
            if s["retry"]:
                out.append("    s.retry_site = true;")
                out.append("    s.retry_index = %d;" % s["retry"])
            out.append("    s.tsig = B(0x%016xULL);" % s["tsig"])
            out.append("    s.mean = %s;" % ("true" if s["mean"] else "false"))
            if s["t"]:
                nm = "v%d_%d" % (vecn, len(out))
                vecn += 1
                out.extend(hexvec(nm, s["t"]))
                out.append("    s.tstats = std::vector<double>(%s, %s + %d);"
                           % (nm, nm, len(s["t"])))
        elif kind in ("round0", "pass2gate"):
            if kind == "round0":
                out.append("    s.accept = %s;" %
                           ("true" if s["accept"] else "false"))
            else:
                if not s["first"]:
                    out.append("    s.outlier_regressors = %d;" % s["nb"])
                    out.append("    s.reference_regressors = %d;" % s["nr"])
                    out.append("    s.reverted = %s;" %
                               ("true" if s["revert"] else "false"))
                    out.append("    s.rule = %s;" % cstr(s["why"]))
                    out.append("    s.gate_ljung_box_complement = "
                               "B(0x%016xULL);" % s["lbp"])
                    out.append("    s.gate_standard_error = "
                               "B(0x%016xULL);" % s["se"])
                    out.append("    s.gate_reference_ljung_box_complement = "
                               "B(0x%016xULL);" % s["rlbp"])
                    out.append("    s.gate_reference_standard_error = "
                               "B(0x%016xULL);" % s["rse"])
                else:
                    out.append("    s.reference_regressors = -1;")
                    out.append("    s.rule = \"first\";")
                o = s["arima"]
                out.append("    s.arima_orders = "
                           "TramoOrders{%d, %d, %d, %d, %d, %d};" %
                           (o[0], o[1], o[2], o[3], o[4], o[5]))
                nm = "v%d_%d" % (vecn, len(out))
                vecn += 1
                out.extend(hexvec(nm, s["apars"]))
                out.append("    s.parameters = "
                           "std::vector<double>(%s, %s + %d);" %
                           (nm, nm, len(s["apars"])))
            st = s["stats"]
            out.append("    s.stats.free_parameters = %d;" % st["k"])
            out.append("    s.stats.dim = %d;" % st["dim"])
            out.append("    s.stats.observations = %d;" % st["obs"])
            out.append("    s.stats.ljung_box_pvalue = B(0x%016xULL);"
                       % st["lbp"])
            out.append("    s.stats.se = B(0x%016xULL);" % st["se"])
            out.append("    s.stats.ljung_box = B(0x%016xULL);" % st["lb"])
            out.append("    s.stats.bic = B(0x%016xULL);" % st["bic"])
            out.append("    s.stats.outliers_count = %d;" % st["oc"])
            out.append("    s.stats.effective_observations = %d;" % st["eff"])
            nm = "v%d_%d" % (vecn, len(out))
            vecn += 1
            out.extend(hexvec(nm, st["e"]))
            out.append("    s.stats.residuals = "
                       "std::vector<double>(%s, %s + %d);" %
                       (nm, nm, len(st["e"])))
        elif kind == "pass2":
            out.append("    s.returned = %d;" % s["ret"])
            out.append("    s.returned_pcr = B(0x%016xULL);" % s["pcr"])
        elif kind == "iterate":
            out.append("    s.end = %s;" % END_ENUM[s["end"]])
        out.append("    rq.seams.push_back(s);")
        out.append("  }")
    out.append("}")
    out.append("")

    # expected streams
    out.append("// expected: jar-side event stream")
    out.append("inline const std::vector<KrEvent> events_" + cid + "() {")
    out.append("  return {")
    for k, lvl, text in events:
        out.append("      {%d, %d, %s}," % (k, lvl, cstr(text)))
    out.append("  };")
    out.append("}")
    out.append("inline const std::vector<KrSnap> snaps_" + cid + "() {")
    out.append("  return {")
    for sn in snaps:
        out.append("      {%d, %d, %d, %d, %d, 0x%016xULL, 0x%016xULL, %d, "
                   "%d, %d, %d}," % (sn.pass_, sn.round, sn.pass3,
                                     sn.needOut, sn.needAuto, sn.curva,
                                     sn.pcr, sn.refAuto, sn.refStats,
                                     sn.seasonal, sn.ost))
    out.append("  };")
    out.append("}")
    meta = {
        "init_curva": g17(init[2]),
        "init_pcr": g17(init[4]),
        "n_iter": len(snaps),
        "converged": int(any(s["kind"] == "iterate" and s.get("end") ==
                             "done" for s in seams) or
                         any(s["kind"] == "round0" and s.get("accept") == 1
                             for s in seams)),
        "refuses": int(case["freq"] != 12),
    }
    return meta


def main():
    if len(sys.argv) > 2:
        fail("usage: gen_tramo_kernel_rounds_golden.py [kr-capture-1.out]")
    path = sys.argv[1] if len(sys.argv) == 2 else \
        "validation/evidence/native/tramo-kernel-rounds/kr-capture-1.out"
    cases = parse_capture(path)
    if len(cases) != 35:
        fail("expected 35 cases, parsed %d" % len(cases))
    out = []
    out.append("// GENERATED by tools/gen_tramo_kernel_rounds_golden.py - "
               "DO NOT EDIT.")
    out.append("// Golden: the TramoKernel round-driver corpus of item "
               "23-F.3 (kr-capture-1.out,")
    out.append("// validation/evidence/native/tramo-kernel-rounds; double "
               "capture byte-identical,")
    out.append("// per-case identity lines events/final/all = 1, re-checked "
               "by this generator).")
    out.append("// Per case: the faithful TramoKernelRoundsRequest (resolved "
               "spec, prestage and the")
    out.append("// ordered seam facts with module log blocks), the expected "
               "jar-side event")
    out.append("// stream and per-iterate snapshots, and the init-line bits. "
               "The quarterly case")
    out.append("// kr_q_air expects the D11 refusal (no stream).")
    out.append("")
    out.append("#ifndef KALOS_TESTS_JVM_TRAMO_KERNEL_ROUNDS_H")
    out.append("#define KALOS_TESTS_JVM_TRAMO_KERNEL_ROUNDS_H")
    out.append("")
    out.append("#include <kalos/tramo_kernel_rounds.hpp>")
    out.append("")
    out.append("#include <cstdint>")
    out.append("#include <cstring>")
    out.append("#include <map>")
    out.append("#include <string>")
    out.append("#include <vector>")
    out.append("")
    out.append("using kalos::TramoKernelSeam;")
    out.append("using kalos::TramoKernelSeamKind;")
    out.append("using kalos::TramoKernelStageResult;")
    out.append("using kalos::TramoKernelIterateEnd;")
    out.append("using kalos::TramoKernelLogKind;")
    out.append("using kalos::TramoOrders;")
    out.append("")
    out.append("// Raw IEEE-754 bits -> double (bit-exact, no parsing).")
    out.append("inline double B(std::uint64_t b) {")
    out.append("  double d;")
    out.append("  std::memcpy(&d, &b, 8);")
    out.append("  return d;")
    out.append("}")
    out.append("")
    out.append("// kind: 0 push 1 pop 2 info 3 remark 4 warn 5 step")
    out.append("struct KrEvent {")
    out.append("  int kind, level;")
    out.append("  const char* text;")
    out.append("};")
    out.append("")
    out.append("struct KrSnap {")
    out.append("  int pass, round, pass3, need_out, need_auto;")
    out.append("  std::uint64_t curva, pcr;")
    out.append("  int ref_auto, ref_stats, seasonal, ost;")
    out.append("};")
    out.append("")
    out.append("struct KrExpect {")
    out.append("  const char* id;")
    out.append("  std::uint64_t init_curva, init_pcr;")
    out.append("  int iterations, converged, refuses;")
    out.append("};")
    out.append("")
    metas = []
    for case in cases:
        metas.append((case["id"], emit_case(out, case)))
    out.append("inline const std::vector<KrExpect>& expectations() {")
    out.append("  static const std::vector<KrExpect> k_all = {")
    for cid, m in metas:
        out.append("      {%s, 0x%016xULL, 0x%016xULL, %d, %d, %d}," %
                   (cstr(cid), m["init_curva"], m["init_pcr"], m["n_iter"],
                    m["converged"], m["refuses"]))
    out.append("  };")
    out.append("  return k_all;")
    out.append("}")
    out.append("")
    out.append("inline void build_case(const std::string& id,")
    out.append("                           kalos::TramoKernelRoundsRequest& "
               "rq) {")
    for cid, _ in metas:
        out.append("  if (id == %s) { build_%s(rq); return; }" %
                   (cstr(cid), cid))
    out.append("}")
    out.append("")
    out.append("inline std::vector<KrEvent> expected_events("
               "const std::string& id) {")
    out.append("  static const std::map<std::string, "
               "std::vector<KrEvent>> k_all = {")
    for cid, _ in metas:
        out.append("      {%s, events_%s()}," % (cstr(cid), cid))
    out.append("  };")
    out.append("  return k_all.at(id);")
    out.append("}")
    out.append("inline std::vector<KrSnap> expected_snaps("
               "const std::string& id) {")
    out.append("  static const std::map<std::string, "
               "std::vector<KrSnap>> k_all = {")
    for cid, _ in metas:
        out.append("      {%s, snaps_%s()}," % (cstr(cid), cid))
    out.append("  };")
    out.append("  return k_all.at(id);")
    out.append("}")
    out.append("")
    out.append("#endif  // KALOS_TESTS_JVM_TRAMO_KERNEL_ROUNDS_H")
    sys.stdout.write("\n".join(out) + "\n")


if __name__ == "__main__":
    main()
