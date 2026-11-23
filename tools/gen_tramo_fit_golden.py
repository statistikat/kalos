#!/usr/bin/env python3
"""Generate tests/jvm_tramo_fit.h from the 23-F F.4 fit capture.

Reuses the 23-F.3 kr golden pipeline (tools/gen_tramo_kernel_rounds_golden.py
imported as a module): the fit capture embeds the full kr case record, so the
kernel requests are emitted by kr.emit_case verbatim (the fit_* ids live in
their own namespace inside jvm_tramo_fit.h, which includes
jvm_tramo_kernel_rounds.h for the B()/KrEvent/KrSnap helpers). This generator
adds the F.4 surface: the fitbuild/fitdec/fitseam/fitresid/fitcl/fituc/
fitcoef/fitdy/rfinal lines become the TramoFitRequest builders (resolved spec
with the refusal-state fields, series, module ENTRY seam records, the final-
model record) plus the expectations (fail-closed refusal status, jar
seasonality verdict, measured live/fact seam tiers, D9 covariance entry
count).

Pre-filters before delegating to the kr parser: the "amth <NAME>" witness
token is stripped from the resolved lines (the kr record_walk fails on
unknown tokens; the value is read here first) and the fit_const crash-witness
block is dropped (identity all-0, no golden).

Usage (from the repository root):
  python3 tools/gen_tramo_fit_golden.py \
      [validation/evidence/native/tramo-fit/fit-capture-1.out] \
      > tests/jvm_tramo_fit.h
"""
import importlib.util
import math
import os
import struct
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
_spec = importlib.util.spec_from_file_location(
    "kr_gen", os.path.join(HERE, "gen_tramo_kernel_rounds_golden.py"))
kr = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(kr)

fail = kr.fail
bits = kr.bits        # hex token -> raw bit int
g17 = kr.g17          # %.17g token -> raw bit int
cstr = kr.cstr
CASES = os.path.join(HERE, "..", "validation", "evidence", "native",
                     "tramo-fit", "cases.txt")

OUTLIER_KIND = {"AO": "Ao", "LS": "Ls", "TC": "Tc", "SO": "So"}


def parse_cases(path):
    """cases.txt -> {id: (alias, freq, year, month, [floats])}."""
    out = {}
    hdr = None
    for line in open(path):
        t = line.split()
        if not t:
            continue
        if t[0] == "KER":
            hdr = (t[1], t[2], int(t[-3]), int(t[-2]), int(t[-1]))
        elif t[0] == "values":
            cid, alias, freq, y0, m0 = hdr
            out[cid] = (alias, freq, y0, m0, [float(x) for x in t[1:]])
    return out


def split_capture(path):
    """-> (kr-filtered text, {id: [aux lines]})."""
    texts, aux = [], {}
    cur = None
    drop = False
    for raw in open(path):
        line = raw.rstrip("\n")
        t = line.split()
        if len(t) > 3 and t[0] == "KER" and t[2] == "specname":
            drop = t[1] == "fit_const"
            cur = t[1]
            aux[cur] = []
        if drop:
            continue
        if len(t) > 3 and t[0] == "KER" and t[2] == "resolved":
            if cur is not None:
                aux[cur].append("RESOLVED " + " ".join(t[3:]))
            if "amth" in t:
                i = t.index("amth")
                t = t[:i] + t[i + 2:]
            line = " ".join(t)
        elif cur is not None and t and t[0] in (
                "fitbuild", "fitdec", "fitll", "fitseam", "fitresid",
                "fitcl", "fitcoef", "fituc", "fitdy", "rfinal"):
            aux[cur].append(line)
        texts.append(line)
    return "\n".join(texts) + "\n", aux


def keyval(toks, key):
    return toks[toks.index(key) + 1]


def bitvec(hexvals):
    if not hexvals:
        return "{}"
    return "{" + ", ".join("B(0x%016xULL)" % v for v in hexvals) + "}"


def dvec(vals):
    return bitvec([struct.unpack("<Q", struct.pack("<d", v))[0]
                   for v in vals])


def parse_names(tokens):
    """outs token(s) -> [names]; "-" / empty -> []."""
    joined = " ".join(tokens).strip()
    if joined in ("-", ""):
        return []
    return [x.strip() for x in joined.split(";")]


def upos(i, j, n):
    """Row-major upper-triangle index of (i, j), i <= j (row r has n - r)."""
    return i * n - i * (i - 1) // 2 + (j - i)


def seam_outs(t):
    oi = t.index("outs")
    end = len(t)
    if "cv" in t[oi:]:
        end = t.index("cv", oi)
    return parse_names(t[oi + 1:end])


def outlier_coordinate(name, start_year, start_month):
    code, label = name.split()
    date = label.strip("()")
    position = (int(date[:4]) - start_year) * 12 + int(date[5:7]) - start_month
    return OUTLIER_KIND[code], position


def emit_trivial_case(out, case, kv, fbk):
    """The fully-specified, outlier-free templates (fit_tr0_air, fit_fsnone)
    take the ami() estimation branch: no round steps, no prestage or seam
    record in the capture. Emit the minimal kernel-request carrier the fit
    unit validates against its builder mirror."""
    a = out.append
    a("inline void build_%s(kalos::TramoKernelRoundsRequest& rq) {"
      % case["id"])
    a("  rq.spec.annual_frequency = %d;" % case["freq"])
    a("  rq.spec.observations = %d;" % case["n"])
    a("  rq.spec.using_auto_model = false;")
    a("  rq.spec.outliers_detection = false;")
    a("  rq.spec.accept_airline = %s;"
      % ("true" if kv("accept") == "1" else "false"))
    a("  rq.spec.mean_is_test = %s;"
      % ("true" if kv("meanTest") == "1" else "false"))
    for name, key in (("pcr", "pcr"), ("reduce_va", "pc"), ("tsig", "tsig"),
                      ("critical_value", "va"),
                      ("intermediate_precision", "tol")):
        a("  rq.spec.%s = B(0x%016xULL);" % (name, g17(kv(key))))
    a("  rq.spec.controller_count = 0;")
    a('  rq.prestage.regression_test = "Unprocessed";')
    a("  rq.prestage.seasonal = true;"
      " rq.prestage.original_seasonality_test = 0;")
    a("  rq.prestage.orders = {%s};"
      % ", ".join(fbk[fbk.index("orders") + 1].split("@")[0].split(",")))
    a("  rq.prestage.mean = %s;"
      % ("true" if fbk[fbk.index("mean") + 1] == "1" else "false"))
    a("  rq.prestage.log = %s;"
      % ("true" if fbk[fbk.index("log") + 1] == "1" else "false"))
    a("}")


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else \
        "validation/evidence/native/tramo-fit/fit-capture-1.out"
    text, aux = split_capture(path)
    tmp = tempfile.NamedTemporaryFile("w", suffix=".out", delete=False)
    tmp.write(text)
    tmp.close()
    cases = kr.parse_capture(tmp.name)
    os.unlink(tmp.name)
    if len(cases) != 34:
        fail("expected 34 golden cases (fit_const dropped), parsed %d"
             % len(cases))
    series = parse_cases(CASES)

    out = []
    a = out.append
    a("// GENERATED by tools/gen_tramo_fit_golden.py - DO NOT EDIT.")
    a("// Golden: the TRAMO automatic-fit corpus of item 23-F F.4")
    a("// (fit-capture-1.out, validation/evidence/native/tramo-fit; the")
    a("// triple capture fit-capture-1/2/witness is byte-identical and the")
    a("// per-case identity lines are re-checked by this generator through")
    a("// the imported 23-F.3 kr parser.")
    a("// Per case: the faithful TramoFitRequest (resolved spec incl. the")
    a("// refusal-state fields and the amth witness, the observed series,")
    a("// the kernel request emitted by the kr machinery, the module ENTRY")
    a("// seam records and the final-model record) plus the expectations:")
    a("// the fail-closed refusal status, the jar seasonality verdict, the")
    a("// measured live/fact seam tiers and the D9 covariance entry count.")
    a("")
    a("#ifndef KALOS_TESTS_JVM_TRAMO_FIT_H")
    a("#define KALOS_TESTS_JVM_TRAMO_FIT_H")
    a("")
    a('#include "jvm_tramo_kernel_rounds.h"')
    a("")
    a("#include <kalos/tramo_fit.hpp>")
    a("")
    a("#include <stdexcept>")
    a("#include <string>")
    a("#include <vector>")
    a("")
    a("using kalos::TramoFitRequest;")
    a("using kalos::TramoOutlierKind;")
    a("using kalos::tramo_auto_method;")
    a("using kalos::tramo_parameter;")
    a("using kalos::tramo_transform_function;")
    a("")

    metas = []
    for case in cases:
        cid = case["id"]
        fl = aux[cid]
        alias, freq, y0, m0, vals = series[cid]
        if freq != case["freq"] or len(vals) != case["n"]:
            fail("%s: cases.txt disagrees with the capture header" % cid)

        resolved = next(l.split(None, 1)[1].split() for l in fl
                        if l.startswith("RESOLVED "))
        kv = lambda k: keyval(resolved, k)  # noqa: E731
        fbk = next(l.split() for l in fl if l.startswith("fitbuild "))
        fd1 = next(l.split() for l in fl
                   if l.startswith("fitdec ") and " toint " in l)
        verdict, toint = fd1[1], int(keyval(fd1, "toint"))
        ck = next(l.split() for l in fl if l.startswith("fitcl dim "))
        coef_bits = [bits(x) for l in fl if l.startswith("fitcoef ")
                     for x in l.split()[1:]]
        uc_bits = [bits(x) for l in fl if l.startswith("fituc")
                   for x in l.split()[1:]]
        dy_bits = [bits(x) for l in fl if l.startswith("fitdy")
                   for x in l.split()[1:]]
        seams_in = [l.split() for l in fl if l.startswith("fitseam ")]
        resid = {}
        for l in fl:
            if l.startswith("fitresid "):
                t = l.split()
                if len(t) < 8 or t[6] != "n" or len(t[8:]) != int(t[7]):
                    fail(cid + ": invalid fitresid length: " + l)
                resid[int(t[1])] = (int(t[3]), int(t[5]),
                                    [bits(x) for x in t[8:]])

        rf = {}
        for l in fl:
            if l.startswith("rfinal "):
                t = l.split()
                rf[" ".join(t[1:3])] = t

        def rline(pref):
            for k, t in rf.items():
                if k.startswith(pref):
                    return t
            return None

        orders = keyval(rline("ok spec"), "spec")
        rlog = rline("log ")[2] == "1"
        ot = rline("outliers")
        names = parse_names(ot[2:])
        pars_bits = [bits(x) for x in rline("pars ")[2:]]
        # rq.final.coefficients is the DESIGN block (rfinal coefs): cl
        # .allCoefficients (the fitcoef row) appends the missing-value
        # estimates after it (the fit_missing row: nx + nmissing entries).
        ct = rline("coefs ")
        coef_bits = [bits(x) for x in ct[2:]] if ct else []
        cov_t = rline("cov ")
        cov_bits = [bits(x) for x in cov_t[2:]] if cov_t else []
        sk = rline("stats obs")
        fres = rline("fres")
        fres_bits = [bits(x) for x in fres[3:]]

        nx, dim, kfree = int(ck[4]), int(ck[2]), int(ck[-1])
        mean_flag = nx > len(names)
        if len(coef_bits) != nx:
            fail("%s: fitcoef length != nx" % cid)
        if len(uc_bits) != nx * (nx + 1) // 2:
            fail("%s: fituc upper triangle length != nx(nx+1)/2" % cid)
        if len(pars_bits) != kfree:
            fail("%s: rfinal pars length != k" % cid)
        if len(dy_bits) != dim and int(ck[6]) == 0:
            fail("%s: fitdy length != dim" % cid)
        # The fit_missing row (nmissing > 0) carries the missing-column
        # estimation domain in the differenced response; it is refused
        # before the stage-5 recompute and the lengths are not compared.
        if len(cov_bits) != len(uc_bits):
            fail("%s: rfinal cov bits disagree with the uc triangle" % cid)

        # live/fact tier simulation (mirrors src/tramo_fit.cpp stage 4).
        ml = kv("ml") == "1"
        n_live = n_fact = 0
        for t in seams_in:
            kind, idx, round_ = t[2], int(t[1]), int(t[6])
            if kind == "arma":
                n_live += 1 if idx in resid else 0
                n_fact += 0 if idx in resid else 1
            elif kind == "diff" and not seam_outs(t) and not ml and \
                    round_ == 1:
                n_live += 1
            else:
                n_fact += 1
        d9 = nx * (nx + 1) // 2

        # ---- the kernel request (kr machinery; trivial ami branch
        # for the fully-specified outlier-free templates) --------------------
        if any(e[0] == 5 for e in case["events"][0]):
            kr.emit_case(out, case)
        else:
            emit_trivial_case(out, case, kv, fbk)

        # ---- series + fit request ------------------------------------------
        a("inline std::vector<double> fit_series_%s() {" % cid)
        a("  return %s;" % dvec(vals))
        a("}")
        a("inline void build_fit_%s(TramoFitRequest& rq) {" % cid)
        a('  rq.id = "%s";' % cid)
        a("  rq.series = fit_series_%s();" % cid)
        a("  rq.start_year = %d; rq.start_month = %d;" % (y0, m0))
        a('  rq.spec.template_name = "%s";' % alias)
        a("  rq.spec.annual_frequency = %d;" % freq)
        a("  rq.spec.observations = %d;"
          % sum(1 for v in vals if math.isfinite(v)))
        auto = kv("autoModel") == "1"
        a("  rq.spec.using_auto_model = %s;" % ("true" if auto else "false"))
        tf = {"Auto": "automatic", "None": "none", "Log": "log"}[kv("tfunc")]
        a("  rq.spec.transform_function = tramo_transform_function::%s;" % tf)
        fct = struct.unpack("<d", struct.pack("<Q", g17(kv("fct"))))[0]
        a("  rq.spec.fct = B(0x%016xULL);" % g17(kv("fct")))
        # logPreference: Math.log(fct) under the intrinsic-disable recipe
        # (StrictMath semantics; the corpus fct values pin the glibc value
        # to the same double - the 23-B corpus qualification).
        a("  rq.spec.log_preference = B(0x%016xULL);"
          % struct.unpack("<Q", struct.pack("<d", math.log(fct)))[0])
        a("  rq.spec.outliers_correction = %s;"
          % ("true" if kv("oc") == "1" else "false"))
        for name, key in (("outliers_used", "outliers"), ("ao", "ao"),
                          ("ls", "ls"), ("tc", "tc"), ("so", "so")):
            a("  rq.spec.%s = %s;"
              % (name, "true" if kv(key) == "1" else "false"))
        for name, key in (("delta_tc", "tcrate"), ("critical_value", "va"),
                          ("pcr", "pcr"), ("tsig", "tsig"),
                          ("reduce_va", "pc"), ("cancel", "cancel"),
                          ("ub1", "ub1"), ("ub2", "ub2")):
            a("  rq.spec.%s = B(0x%016xULL);" % (name, g17(kv(key))))
        a("  rq.spec.outliers_ml = %s;" % ("true" if ml else "false"))
        a("  rq.spec.accept_airline = %s;"
          % ("true" if kv("accept") == "1" else "false"))
        a("  rq.spec.precision = B(0x%016xULL);" % g17(kv("tol")))
        a("  rq.spec.intermediate_precision = B(0x%016xULL);"
          % g17(kv("tol")))
        a("  rq.spec.mean_is_test = %s;"
          % ("true" if kv("meanTest") == "1" else "false"))
        a("  rq.spec.mean_used = %s;"
          % ("true" if fbk[fbk.index("mean") + 1] == "1" else "false"))
        a("  rq.spec.td_automatic = %s; rq.spec.td_test = %s;"
          % tuple("true" if kv(x) == "1" else "false"
                  for x in ("tdAuto", "tdTest")))
        a("  rq.spec.easter_test = %s; rq.spec.user_variables = false;"
          % ("true" if kv("easter") == "1" else "false"))
        amth_v = {"UNUSED": "unused", "FTEST": "ftest", "WALD": "wald",
                  "BIC": "bic", "AIC": "aic"}[kv("amth")]
        a("  rq.spec.auto_method = tramo_auto_method::%s;" % amth_v)
        if not auto:
            a("  rq.spec.fixed_arima.period = 12; rq.spec.fixed_arima.d = 1;"
              " rq.spec.fixed_arima.bd = 1;")
            a("  rq.spec.fixed_arima.theta = {tramo_parameter{}};"
              " rq.spec.fixed_arima.btheta = {tramo_parameter{}};")
        a("  build_%s(rq.kernel);" % cid)

        # ---- module ENTRY seam records --------------------------------------
        a("  rq.seam_inputs = {")
        for t in seams_in:
            idx, kind = int(t[1]), t[2]
            ps, rd = int(t[4]), int(t[6])
            p, d, q, bp, bd, bq = orders0 = \
                t[t.index("orders") + 1].split("@")[0].split(",")
            mean0 = t[t.index("mean") + 1] == "1"
            outs = seam_outs(t)
            if names and any(not x.endswith(")") for x in outs):
                fail(cid + ": seam outs parse " + " ".join(t))
            if kind == "diff":
                entry_nobs = int(t[t.index("nobs") + 1])
            else:
                entry_nobs = resid[idx][1] if idx in resid else 0
            spc, rnobs, res_bits = resid.get(idx, (-1, 0, []))
            if kind == "arma" and idx not in resid:
                fail(cid + ": arma seam %d without fitresid" % idx)
            variables = []
            if kind == "arma" and all(name.startswith(("AO ", "LS ")) for name in outs):
                for name in outs:
                    outlier_kind, pos = outlier_coordinate(name, y0, m0)
                    variables.append('{"%s", TramoOutlierKind::%s, %d, 0.0, {}}' % (name, outlier_kind, pos))
            a("      {%d, {%s, %s, %s, %s, %s, %s}, %s, {%s}, %d, %d, %d,"
              " {%s}, %d, %d%s}," % (
                  idx, p, d, q, bp, bd, bq, "true" if mean0 else "false",
                  ", ".join('"%s"' % x for x in outs),
                  ps, rd, entry_nobs,
                  ", ".join("B(0x%016xULL)" % x for x in res_bits),
                  rnobs if kind == "arma" else 0, spc,
                  ", {" + ", ".join(variables) + "}" if variables else ""))
        a("  };")

        # ---- final record ----------------------------------------------------
        a("  rq.final.orders = {%s};" % ", ".join(orders.split("@")[0]
                                                  .split(",")))
        a("  rq.final.log = %s; rq.final.mean = %s;"
          % ("true" if rlog else "false", "true" if mean_flag else "false"))
        a("  rq.final.outlier_names = {%s};"
          % ", ".join('"%s"' % x for x in names))
        a("  rq.final.variables = {")
        for i, nm in enumerate(names):
            outlier_kind, pos = outlier_coordinate(nm, y0, m0)
            a('      {"%s", TramoOutlierKind::%s, %d, B(0x%016xULL), {}},'
              % (nm, outlier_kind, pos,
                 coef_bits[(1 if mean_flag else 0) + i]))
        a("  };")
        a("  rq.final.parameters = %s;" % bitvec(pars_bits))
        a("  rq.final.coefficients = %s;" % bitvec(coef_bits))
        a("  rq.final.differenced_response = %s;" % bitvec(dy_bits))
        a("  rq.final.full_residuals = %s;" % bitvec(fres_bits))
        a("  rq.final.dim = %d; rq.final.nx = %d; rq.final.nmissing = %s;"
          % (dim, nx, ck[6]))
        a("  rq.final.scaling_factor = %s;"
          % ("true" if ck[-3] == "1" else "false"))
        a("  rq.final.ssq = B(0x%016xULL);"
          " rq.final.log_determinant = B(0x%016xULL);"
          % (g17(ck[8]), g17(ck[10])))
        a("  rq.final.unscaled_covariance = {")
        for i in range(nx):
            row = [uc_bits[upos(min(i, j), max(i, j), nx)]
                   for j in range(nx)]
            a("      %s," % bitvec(row)[1:-1])
        a("  };")
        a("  rq.final.free_arma_parameters = %d;" % kfree)
        a("  rq.final.captured_covariance_upper = %s;" % bitvec(cov_bits))
        a("  rq.final.ls_obs = %s; rq.final.ls_eff = %s; rq.final.ls_npar = %s;"
          % (sk[3], sk[5], sk[7]))
        for name, key in (("ls_ll", "ll"), ("ls_tadj", "tadj"),
                          ("ls_all", "all"), ("ls_ssq", "ssq"),
                          ("ls_aic", "aic"), ("ls_aicc", "aicc"),
                          ("ls_bic", "bic"), ("ls_bicc", "bicc"),
                          ("ls_bic2", "bic2"), ("ls_hq", "hq")):
            a("  rq.final.%s = B(0x%016xULL);" % (name, g17(keyval(sk, key))))
        a("}")
        a("")

        refuses = ""
        if freq != 12:
            refuses = "refused:d11-frequency"
        elif not all(math.isfinite(v) for v in vals):
            refuses = "refused:missing-values"
        elif kv("tdAuto") == "1" or kv("tdTest") == "1" or kv("easter") == "1":
            refuses = "refused:d10-calendar"
        elif amth_v != "unused":
            refuses = "refused:i10-automethod"
        elif kv("oc") == "1":
            refuses = "refused:oc-precorrection"
        metas.append((cid, refuses,
                      verdict if auto and tf == "automatic" else "-",
                      toint, n_live, n_fact, d9))

    a("struct FitExpect {")
    a("  const char* id;")
    a('  const char* refuses;      // "" = the unit must answer available')
    a('  const char* verdict;      // fitdec jar verdict ("-": no check)')
    a("  int toint, live, fact, d9_entries;")
    a("};")
    a("")
    a("inline const std::vector<FitExpect>& fit_expectations() {")
    a("  static const std::vector<FitExpect> k_all = {")
    for m in metas:
        a("      {%s, %s, %s, %d, %d, %d, %d},"
          % (cstr(m[0]), cstr(m[1]), cstr(m[2]), m[3], m[4], m[5], m[6]))
    a("  };")
    a("  return k_all;")
    a("}")
    a("")
    a("inline void build_fit(const std::string& id, TramoFitRequest& rq) {")
    for m in metas:
        a('  if (id == %s) { build_fit_%s(rq); return; }' % (cstr(m[0]), m[0]))
    a('  throw std::runtime_error("no fit case " + id);')
    a("}")
    a("")
    a("#endif  // KALOS_TESTS_JVM_TRAMO_FIT_H")
    sys.stdout.write("\n".join(out) + "\n")


if __name__ == "__main__":
    main()
