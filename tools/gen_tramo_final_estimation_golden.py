#!/usr/bin/env python3
"""Generate tests/jvm_tramo_final_estimation.h from the item 23-F F.1
FinalEstimator decision-layer corpus (validation/evidence/native/
tramo-final-estimation, captured through the split-package observer
Scratch23f.java against the pinned jdplus 3.9.0 jars; double capture,
byte-identical across JVM processes, plus a class-load witness run).
The golden is the captured capture-line facts: every double is carried as
its raw IEEE-754 bits (the probe prints value#hex16 through %.17g) and
every decision as the JVM's own answer. Row layouts (documented for the
replay layer tests/test_tramo_final_estimation.cpp):

  SR   in = [ur, (re, im)*]                      exp = [count...]
  MR   in = [ascending coefficients...]          exp = [K, (re, im)...]
  RU   in = [ur, p, d, q, bp, bd, bq, np, phi..., nbp, bphi...]
       exp = [unchanged, p, d, q, bp, bd, bq, npar, par...]
  CUR  in = [cancel, p, d, q, bp, bd, bq, np, phi..., nbp, bphi...,
             nq, theta..., nbq, btheta...]
       exp = [ok, p, d, q, bp, bd, bq]
  TC   in = [tsig, sample_length, p, d, q, bp, bd, bq, npar, par...,
             nvd, vd...]                          exp = [total, p, d, q, bp, bd, bq]
       text = the "arima spec changed" info detail, empty when absent
  EST  in = [n, mean, eps-bits, ur-bits, pass, ami, outliers,
             orders(p,d,q,bp,bd,bq; p = -1 flags airline), fam-kind]
       exp = [return, nnsig, mean_out, p, d, q, bp, bd, bq, np, par-bits...,
              cl_state (0 estimation-null, 1 full, 2 cov-null),
              cl_dim, ssq-bits, nc, coef-bits..., nv, vd-bits...,
              n_events, (kind, depth)*...]
       event kind codes: 1 push, 2 pop, 3 remark, 4 info, 5 error, 6 warn.
       fam-kind: 0 none, 1 ls, 2 out2 (replay rebuilds the SECTION-1
       family with detail::fdlibm_sin/fdlibm_cos - the probe recipe is
       StrictMath-only, i.e. the bytecode FdLibm path that the landed
       transcription mirrors bit-exactly, so the series is reproducible
       from the case facts alone). `text` stays empty for EST rows.

Integer slots carry their value; double slots carry raw bits. Regenerate:
  python3 tools/gen_tramo_final_estimation_golden.py \
      validation/evidence/native/tramo-final-estimation tests/jvm_tramo_final_estimation.h
"""
import os
import struct
import sys

NAN = float("nan")


def parse_token(tok):
    if tok == "nan":
        return NAN
    if tok == "inf":
        return float("inf")
    if tok == "-inf":
        return float("-inf")
    if tok.startswith("0x"):
        return struct.unpack("<d", struct.pack("<Q", int(tok[2:], 16)))[0]
    return float(tok)


def bits(value):
    if value != value:  # NaN: canonical quiet bits, matching Double.doubleToRawLongBits of the captured NaN
        return 0x7FF8000000000000
    return struct.unpack("<Q", struct.pack("<d", value))[0]


def bits_of_fact(fact):
    """fact: 'value#hex16' from the probe gb() printer."""
    return int(fact.split("#")[1], 16)


def read_cases(path):
    rows = {}
    order = []
    with open(path) as handle:
        for line in handle:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            toks = line.split()
            rows[toks[1]] = toks
            order.append(toks[1])
    return rows, order


def read_capture(path):
    blocks = {}
    current = None
    with open(path) as handle:
        for line in handle:
            line = line.rstrip("\n")
            if line.startswith("case ") and len(line.split()) >= 2:
                current = line.split()[1]
                blocks[current] = []
                continue
            if line.startswith("end "):
                current = None
                continue
            if current is not None:
                blocks[current].append(line)
    return blocks


def orders_from(line):
    parts = line.split()
    o = parts.index("orders")
    ints = [int(x) for x in parts[o + 1:o + 7]]
    pars = []
    if "pars" in parts:
        pars = [bits_of_fact(x) for x in parts[parts.index("pars") + 1:]]
    return ints, pars


def emit_array(out, name, values):
    out.append("static const uint64_t %s[] = {%s};" % (name,
        ", ".join("0x%016xULL" % (v & 0xFFFFFFFFFFFFFFFF) for v in values)
        if values else "0"))
    return len(values)


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    evidence, target = sys.argv[1], sys.argv[2]
    cases, order = read_cases(os.path.join(evidence, "cases-fe1.txt"))
    capture = read_capture(os.path.join(evidence, "fe1-capture-1.out"))

    rows = []
    arrays = []
    counts = {"fe_sr": 0, "fe_mr": 0, "fe_ru": 0, "fe_cur": 0, "fe_tc": 0, "fe_est": 0}
    for cid in order:
        toks = cases[cid]
        ctype = toks[0]
        counts[ctype] += 1
        block = capture[cid]
        text = ""
        if ctype == "fe_sr":
            tin = [bits(parse_token(toks[2]))]
            texp = []
            for line in block:
                if line.startswith("sr "):
                    parts = line.split()
                    tin.append(bits_of_fact(parts[3]))
                    tin.append(bits_of_fact(parts[5]))
                    texp.append(int(parts[7]))
        elif ctype == "fe_mr":
            tin = [bits(parse_token(t)) for t in toks[3:]]
            texp = []
            for line in block:
                if line.startswith("mr roots"):
                    parts = line.split()
                    k = int(parts[2])
                    texp.append(k)
                    pairs = parts[3::3]
                    assert len(pairs) == k, cid
                    for pair in pairs:
                        re_s, im_s = pair.split(",")
                        texp.append(int(re_s.split("#")[1], 16))
                        texp.append(int(im_s.split("#")[1], 16))
        elif ctype == "fe_ru":
            src = toks[3:]
            k = 0
            orders = [int(x) for x in src[0:6]]
            k = 6
            tin = [bits(parse_token(toks[2]))] + orders
            np_ = int(src[k]); k += 1
            phi = [bits(parse_token(t)) for t in src[k:k + np_]]; k += np_
            nbp = int(src[k]); k += 1
            bphi = [bits(parse_token(t)) for t in src[k:k + nbp]]; k += nbp
            nq = int(src[k]); k += 1
            k += nq
            nbq = int(src[k]); k += 1
            k += nbq
            assert k == len(src), cid
            tin += [np_] + phi + [nbp] + bphi
            unchanged = int(next(l.split()[1] for l in block if l.startswith("ru_unchanged")))
            oline = next(l for l in block if l.startswith("ru_out"))
            oints, opars = orders_from(oline)
            texp = [unchanged] + oints + [len(opars)] + opars
        elif ctype == "fe_cur":
            src = toks[3:]
            cancel = bits(parse_token(toks[2]))
            orders = [int(x) for x in src[0:6]]
            k = 6
            tin = [cancel] + orders
            for prefix in ("np", "nbp", "nq", "nbq"):
                n = int(src[k]); k += 1
                tin.append(n)
                tin += [bits(parse_token(t)) for t in src[k:k + n]]
                k += n
            assert k == len(src), cid
            ok = int(next(l.split()[1] for l in block if l.startswith("cur_ok")))
            oints, _ = orders_from(next(l for l in block if l.startswith("cur_out")))
            texp = [ok] + oints
        elif ctype == "fe_tc":
            tsig = bits(parse_token(toks[3]))
            lline = next(l for l in block if l.startswith("tc_landing"))
            lints, lpars = orders_from(lline)
            vd = [bits_of_fact(l.split()[2])
                  for l in block if l.startswith("tc_vd ")]
            total = int(next(l.split()[1] for l in block if l.startswith("tc_total")))
            oints, _ = orders_from(next(l for l in block if l.startswith("tc_out")))
            tin = [tsig, 96] + lints + [len(lpars)] + lpars + [len(vd)] + vd
            texp = [total] + oints
            for line in block:
                if "obj arima spec changed det" in line:
                    text = line.split("det ")[1]
        elif ctype == "fe_est":
            # `in` facts come from the cases file (doubles via the capture's
            # exact gb() bits); everything else is the observed final state.
            cap_in = next(l for l in block if l.startswith("in "))
            ctoks = cap_in.split()
            f_eps = bits_of_fact(ctoks[ctoks.index("eps") + 1])
            f_ur = bits_of_fact(ctoks[ctoks.index("ur") + 1])
            f_mean = int(ctoks[ctoks.index("mean") + 1])
            f_n = int(ctoks[ctoks.index("n") + 1])
            f_pass = int(ctoks[ctoks.index("pass") + 1])
            f_ami = int(ctoks[ctoks.index("ami") + 1])
            f_out = int(ctoks[ctoks.index("outliers") + 1])
            fam = toks[2]
            assert int(toks[3]) == f_n and (toks[4] == "y") == (f_mean == 1)
            if int(toks[7]) != f_pass or (toks[8] == "y") != (f_ami == 1) \
                    or (toks[9] == "y") != (f_out == 1):
                raise SystemExit("est row %s: cases/capture mismatch" % cid)
            if fam not in ("none", "ls", "out2"):
                raise SystemExit("est row %s: family %s not replayable" % (cid, fam))
            if toks[10] == "air":
                ovals = [-1, 0, 0, 0, 0, 0]
            else:
                ovals = [int(x) for x in toks[10].split(",")]
                assert len(ovals) == 6
            tin = [f_n, f_mean, f_eps, f_ur, f_pass, f_ami, f_out] + ovals \
                + [{"none": 0, "ls": 1, "out2": 2}[fam]]
            if any(l.startswith("threw ") for l in block):
                raise SystemExit("est row %s: exception path not replayed" % cid)
            ret = next(l.split() for l in block if l.startswith("est_return"))
            oline = next(l for l in block if l.startswith("est_out"))
            oints, opars = orders_from(oline)
            mean_out = int(next(l.split()[1] for l in block
                                if l.startswith("mean_out")))
            # Cl/observation block: three states (no estimation, full,
            # covariance-null - distinguished by where the probe's dump died).
            if any(l.startswith("cl_unavailable") for l in block):
                ua = next(l for l in block if l.startswith("cl_unavailable"))
                if any(l.startswith("cl_dim ") for l in block):
                    state = 2  # estimation present, asymptoticCovariance null
                elif ua.split()[1] != "java.lang.NullPointerException":
                    raise SystemExit("est row %s: unexpected cl failure" % cid)
                else:
                    state = 0  # getEstimation() null (setSpecification after cancel)
            else:
                state = 1
            cl_dim, ssq, coefs, vds = 0, 0, [], []
            if state in (1, 2):
                cline = next(l.split() for l in block if l.startswith("cl_dim "))
                cl_dim, ssq = int(cline[1]), bits_of_fact(cline[3])
                coefs = [bits_of_fact(l.split()[1])
                         for l in block if l.startswith("coef ")]
            if state == 1:
                vds = [bits_of_fact(l.split()[2])
                       for l in block if l.startswith("vd ")]
            events = []
            kind_codes = {"push": 1, "pop": 2, "remark": 3, "info": 4,
                          "error": 5, "warn": 6}
            for l in block:
                if l.startswith("evt "):
                    et = l.split()
                    events += [kind_codes[et[2]], int(et[3])]
            texp = ([int(ret[1]), int(ret[3]), mean_out] + oints + [len(opars)]
                    + opars + [state, cl_dim, ssq, len(coefs)] + coefs
                    + [len(vds)] + vds + [len(events) // 2] + events)
            text = ""
        else:
            raise SystemExit("unknown case type " + ctype)
        name = cid.replace("-", "_")
        n_in = emit_array(arrays, "tfe_%s_in" % name, tin)
        n_exp = emit_array(arrays, "tfe_%s_exp" % name, texp)
        type_name = {"fe_sr": "TFE_SR", "fe_mr": "TFE_MR", "fe_ru": "TFE_RU",
                     "fe_cur": "TFE_CUR", "fe_tc": "TFE_TC", "fe_est": "TFE_EST"}[ctype]
        rows.append('    {"%s", %s, tfe_%s_in, %d, tfe_%s_exp, %d, "%s"},'
                    % (cid, type_name, name, n_in, name, n_exp, text))

    with open(target, "w") as out:
        out.write("// Generated by tools/gen_tramo_final_estimation_golden.py from\n"
                  "// validation/evidence/native/tramo-final-estimation (item 23-F F.1).\n"
                  "// JVM-decision goldens for kalos::tramo_final_estimation; the replay\n"
                  "// contract (row layouts, bit slots) is documented in the generator.\n"
                  "#ifndef KALOS_TESTS_JVM_TRAMO_FINAL_ESTIMATION_H\n"
                  "#define KALOS_TESTS_JVM_TRAMO_FINAL_ESTIMATION_H\n\n"
                  "#include <cstdint>\n#include <cstring>\n\n"
                  "namespace golden_tfe {\n\n"
                  "enum Kind { TFE_SR, TFE_MR, TFE_RU, TFE_CUR, TFE_TC, TFE_EST };\n\n"
                  "struct Row {\n"
                  "    const char* id;\n"
                  "    Kind kind;\n"
                  "    const uint64_t* in;  int in_n;\n"
                  "    const uint64_t* exp; int exp_n;\n"
                  "    const char* text;\n"
                  "};\n\n"
                  "inline uint64_t dbl(double v) {\n"
                  "    uint64_t u;\n"
                  "    std::memcpy(&u, &v, 8);\n"
                  "    return u;\n"
                  "}\n\n"
                  + "\n".join(arrays) + "\n\n"
                  "inline const Row* table(int& count) {\n"
                  "    static const Row rows[] = {\n"
                  + "\n".join(rows) + "\n    };\n"
                  "    count = static_cast<int>(sizeof(rows) / sizeof(rows[0]));\n"
                  "    return rows;\n}\n\n"
                  "} // namespace golden_tfe\n\n"
                  "#endif\n")
    print("wrote %s (%d rows: sr %d mr %d ru %d cur %d tc %d est %d)"
          % (target, sum(counts.values()), counts["fe_sr"], counts["fe_mr"],
             counts["fe_ru"], counts["fe_cur"], counts["fe_tc"], counts["fe_est"]))


if __name__ == "__main__":
    main()
