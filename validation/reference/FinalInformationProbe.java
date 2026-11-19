// FinalInformationProbe.java — 21-H.0 observer over the real JVM producer path.
//
// Reads case records from stdin (one `C` line per case, followed by one `Y`
// line with the response series and an optional `X` line with the single
// regressor column, all in round-trippable decimal), writes one result block
// per case to stdout. The protocol is documented in
// docs/contracts/final-information-contract.md.
//
// The probe OBSERVES — it re-implements no numeric except the producer glue
// (the nobs convention and the score / information scalings, transcribed
// operation for operation from the pinned bytecode of
// RegArmaProcessor.compute, and cross-checked on est rows against the
// estimator's own values):
//
//   * mode "est": the case runs the real estimator entry point
//     RegSarimaComputer.optimize(model, mapping) (real LM, real final block
//     with computeExactFinalDerivatives(true)); the converged point, score,
//     information and asymptotic covariance are captured from
//     RegArimaEstimation.getMax() — the LogLikelihoodFunction.Point the
//     final-estimation driver hands to the specification test. The probe then
//     REPLAYS the pinned tail (differenced model, allCoefficients,
//     calcResiduals, RegArmaFunction, NumericalDerivatives, scalings,
//     SymmetricMatrix.inverse) at the converged point and aborts (exit 2) on
//     any bitwise mismatch — so an est row can only be captured when the
//     documented tail reproduces the estimator's own numbers exactly.
//
//   * mode "inj": the case injects the given point as the final point (the
//     estimator cannot reach boundary / eps-zero / NaN / singular-information
//     points through the optimiser on the pinned jars — 21-A precedent) and
//     runs the same pinned tail over the real classes.
//
// The deviance nodes the stencil touches are recorded through a logging
// wrapper around the real RegArmaFunction: every coordinate vector the real
// NumericalDerivatives evaluates, and the exact value it got (or THROW, which
// newval turns into fx), goes into the output table keyed by raw-bit hex. The
// C++ unit rebuilds the nodes from its transcribed epsilon / checkepsilon and
// looks them up — a transcription error shows up as a missing key, not a
// silent tolerance pass.

package jdplus.toolkit.base.core.regarima.estimation;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

import jdplus.toolkit.base.api.arima.SarimaOrders;
import jdplus.toolkit.base.api.data.DoubleSeq;
import jdplus.toolkit.base.core.data.DataBlock;
import jdplus.toolkit.base.core.math.functions.IFunction;
import jdplus.toolkit.base.core.math.functions.IFunctionDerivatives;
import jdplus.toolkit.base.core.math.functions.IFunctionPoint;
import jdplus.toolkit.base.core.math.functions.NumericalDerivatives;
import jdplus.toolkit.base.core.math.functions.levmar.LevenbergMarquardtMinimizer;
import jdplus.toolkit.base.core.math.matrices.FastMatrix;
import jdplus.toolkit.base.core.math.matrices.SymmetricMatrix;
import jdplus.toolkit.base.core.regarima.RegArimaEstimation;
import jdplus.toolkit.base.core.regarima.RegArimaModel;
import jdplus.toolkit.base.core.regarima.RegArmaModel;
import jdplus.toolkit.base.core.regarima.estimation.RegArmaFunction;
import jdplus.toolkit.base.core.regarima.estimation.RegArmaSsqFunction;
import jdplus.toolkit.base.core.regsarima.RegSarimaComputer;
import jdplus.toolkit.base.core.sarima.SarimaModel;
import jdplus.toolkit.base.core.sarima.estimation.SarimaMapping2;
import jdplus.toolkit.base.core.stats.likelihood.DefaultLikelihoodEvaluation;
import jdplus.toolkit.base.core.stats.likelihood.LogLikelihoodFunction;

public final class FinalInformationProbe {

    // Same value the real SarimaMapping2 static init computes for STEP.
    static final double STEP = Math.sqrt(2.220446E-16);

    static boolean verbose = false;

    static void log(String s) {
        if (verbose) {
            System.err.println("[probe] " + s);
        }
    }

    static String hex(double v) {
        return Long.toHexString(Double.doubleToRawLongBits(v));
    }

    static double unhex(String s) {
        return Double.longBitsToDouble(Long.parseUnsignedLong(s, 16));
    }

    static String hexAll(double[] v) {
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < v.length; ++i) {
            if (i != 0) {
                sb.append(' ');
            }
            sb.append(hex(v[i]));
        }
        return sb.toString();
    }

    static String decAll(double[] v) {
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < v.length; ++i) {
            if (i != 0) {
                sb.append(' ');
            }
            sb.append(String.format(java.util.Locale.ROOT, "%.17g", v[i]));
        }
        return sb.toString();
    }

    static double[] decList(String s) {
        String t = s.trim();
        if (t.isEmpty()) {
            return new double[0];
        }
        String[] w = t.split("[ ;]+");
        double[] v = new double[w.length];
        for (int i = 0; i < w.length; ++i) {
            v[i] = Double.parseDouble(w[i]);
        }
        return v;
    }

    // ------------------------------------------------------------------ //

    static class Case {

        String id, mode;
        int p, bp, q, bq, d, bd, freq;
        boolean nobsAuto;
        int nobsOvr;
        boolean checkAll;
        double arLimit, maLimit, epsilon;
        double[] point; // raw convention (est: start for the optimiser)
        double[] y;
        double[] xcol; // null when no regressor
    }

    static Case parseC(String[] f) {
        Case c = new Case();
        c.id = f[1];
        c.mode = f[2];
        c.p = Integer.parseInt(f[3]);
        c.bp = Integer.parseInt(f[4]);
        c.q = Integer.parseInt(f[5]);
        c.bq = Integer.parseInt(f[6]);
        c.d = Integer.parseInt(f[7]);
        c.bd = Integer.parseInt(f[8]);
        c.freq = Integer.parseInt(f[9]);
        if (f[10].equals("a")) {
            c.nobsAuto = true;
        } else {
            c.nobsAuto = false;
            c.nobsOvr = Integer.parseInt(f[10]);
        }
        c.checkAll = f[11].equals("1");
        c.arLimit = Double.parseDouble(f[12].trim());
        c.maLimit = Double.parseDouble(f[13].trim());
        c.epsilon = Double.parseDouble(f[14].trim());
        String t = f[15].trim();
        String[] pt = t.isEmpty() ? new String[0] : t.split(" +");
        c.point = new double[pt.length];
        for (int k = 0; k < pt.length; ++k) {
            c.point[k] = Double.parseDouble(pt[k]);
        }
        return c;
    }

    // Logging IFunction wrapper: records every node the stencil evaluates.
    static class Registry implements IFunction {

        final IFunction real;
        final Map<String, double[]> values = new LinkedHashMap<>(); // null = THROW
        final Map<String, double[]> coords = new LinkedHashMap<>();

        Registry(IFunction real) {
            this.real = real;
        }

        @Override
        public IFunctionPoint evaluate(DoubleSeq x) {
            double[] v = x.toArray();
            String key = hexAll(v);
            coords.put(key, v);
            try {
                IFunctionPoint p = real.evaluate(x);
                double r = p.getValue();
                values.put(key, new double[]{r});
                return p;
            } catch (Exception e) {
                values.put(key, null);
                throw e;
            }
        }

        @Override
        public jdplus.toolkit.base.core.math.functions.IParametersDomain getDomain() {
            return real.getDomain();
        }
    }

    // Output sink: null streams to System.out, the capture() entry point
    // installs a StringBuilder so an R driver can read the block in-process.
    static StringBuilder sink;

    static void emit(String s) {
        if (sink == null) {
            System.out.println(s);
        } else {
            sink.append(s).append('\n');
        }
    }

    static void fatal(String id, String why) {
        throw new RuntimeException("FATAL " + id + ": " + why);
    }

    static boolean bitsEq(double[] a, double[] b) {
        if (a.length != b.length) {
            return false;
        }
        for (int i = 0; i < a.length; ++i) {
            if (Double.doubleToRawLongBits(a[i]) != Double.doubleToRawLongBits(b[i])) {
                return false;
            }
        }
        return true;
    }

    static double[] flat(FastMatrix m) {
        double[] v = new double[m.getRowsCount() * m.getColumnsCount()];
        int k = 0;
        for (int i = 0; i < m.getRowsCount(); ++i) {
            for (int j = 0; j < m.getColumnsCount(); ++j) {
                v[k++] = m.get(i, j);
            }
        }
        return v;
    }

    public static void main(String[] args) throws Exception {
        for (String a : args) {
            if (a.equals("-v")) {
                verbose = true;
            }
        }
        StringBuilder in = new StringBuilder();
        try (BufferedReader r = new BufferedReader(
                new InputStreamReader(System.in, StandardCharsets.UTF_8))) {
            String l;
            while ((l = r.readLine()) != null) {
                in.append(l).append('\n');
            }
        }
        System.out.print(capture(in.toString()));
    }

    // In-process entry point for the R capture driver: consumes the same
    // stdin protocol text and returns the result block text.
    public static String capture(String input) {
        StringBuilder sb = new StringBuilder();
        StringBuilder old = sink;
        sink = sb;
        try {
            run(input);
        } finally {
            sink = old;
        }
        return sb.toString();
    }

    static void run(String input) {
        List<String> lines = new ArrayList<>();
        for (String l : input.split("\n", -1)) {
            if (!l.trim().isEmpty() && !l.startsWith("#")) {
                lines.add(l);
            }
        }
        int nc = 0;
        int i = 0;
        while (i < lines.size()) {
            String[] f = lines.get(i).split(";", -1);
            if (!f[0].equals("C")) {
                throw new RuntimeException("expected C line, got: " + lines.get(i));
            }
            Case c = parseC(f);
            i++;
            if (i >= lines.size() || !lines.get(i).startsWith("Y;")) {
                throw new RuntimeException("expected Y line for case " + c.id);
            }
            c.y = decList(lines.get(i).substring(2));
            i++;
            c.xcol = null;
            if (i < lines.size() && lines.get(i).startsWith("X;")) {
                c.xcol = decList(lines.get(i).substring(2));
                if (c.xcol.length != c.y.length) {
                    throw new RuntimeException("regressor length mismatch for case " + c.id);
                }
                i++;
            }
            try {
                runCase(c);
            } catch (RuntimeException t) {
                if (t.getMessage() != null && t.getMessage().startsWith("FATAL")) {
                    throw t;
                }
                throw new RuntimeException("case " + c.id + ": " + t, t);
            }
            nc++;
        }
        log("cases=" + nc);
    }

    static SarimaOrders orders(Case c) {
        SarimaOrders o = new SarimaOrders(c.freq);
        o.setRegular(c.p, c.d, c.q);
        o.setSeasonal(c.bp, c.bd, c.bq);
        return o;
    }

    static SarimaModel sarimaModel(Case c, double[] start) {
        SarimaModel.Builder mb = SarimaModel.builder(orders(c));
        if (start != null) {
            mb.parameters(DoubleSeq.of(start));
        } else {
            mb.setDefault();
        }
        return mb.build();
    }

    static SarimaMapping2 mapping(Case c) {
        return SarimaMapping2.builder(orders(c))
                .arLimit(c.arLimit).maLimit(c.maLimit)
                .checkAll(c.checkAll).epsilon(c.epsilon)
                .build();
    }

    static void runCase(Case c) {
        int dim = c.p + c.bp + c.q + c.bq;
        if (dim == 0) {
            fatal(c.id, "zero-dimensional mapping out of scope");
        }
        if (c.point.length != dim) {
            fatal(c.id, "point length " + c.point.length + " != dim " + dim);
        }
        SarimaMapping2 map = mapping(c);
        SarimaMapping2 smap = (SarimaMapping2) map.stationaryMapping();
        // The knobs the JVM evaluates with (read back, not assumed).
        emit("N;" + c.id + ";" + c.mode + ";" + dim
                + ";" + (smap.isCheckAll() ? 1 : 0)
                + ";" + hex(smap.getArLimit()) + ";" + hex(smap.getMaLimit())
                + ";" + hex(smap.getEpsilon()));

        SarimaModel model = sarimaModel(c, c.mode.equals("est") ? c.point : null);
        RegArimaModel.Builder<SarimaModel> ab = RegArimaModel.<SarimaModel>builder()
                .y(DoubleSeq.of(c.y)).arima(model).meanCorrection(false);
        if (c.xcol != null) {
            ab.addX(DoubleSeq.of(c.xcol));
        }
        RegArimaModel<SarimaModel> am = ab.build();
        RegArmaModel<SarimaModel> dmodel = am.differencedModel();
        double[] yd = dmodel.getY().toArray();
        int xcols = dmodel.getX().getColumnsCount();
        emit("P;" + c.id + ";" + decAll(yd));
        if (xcols > 1) {
            fatal(c.id, "multi-column designs out of scope");
        }
        if (xcols == 1) {
            emit("Q;" + c.id + ";" + decAll(dmodel.getX().column(0).toArray()));
        }
        int nobs = c.nobsAuto ? yd.length - xcols - smap.getDim() : c.nobsOvr;
        emit("O;" + c.id + ";" + nobs);

        // --- est rows: run the real estimator, capture its Point ----------
        double[] capPoint = null, capScore = null, capInfo = null, capAsy = null;
        boolean capAsyThrew = false;
        if (c.mode.equals("est")) {
            RegSarimaComputer comp = RegSarimaComputer.builder()
                    .minimizer(LevenbergMarquardtMinimizer.builder())
                    .computeExactFinalDerivatives(true)
                    .build();
            RegArimaEstimation<SarimaModel> res = comp.optimize(am, map);
            LogLikelihoodFunction.Point<?, ?> max = res.getMax();
            capPoint = max.getParameters().toArray();
            capScore = max.getScore().toArray();
            capInfo = flat(max.getInformation());
            log(c.id + ": capPoint=" + capPoint.length + " capScore="
                    + capScore.length + " capInfo=" + capInfo.length
                    + " mappingDim=" + smap.getDim());
            try {
                capAsy = flat(max.asymptoticCovariance());
            } catch (Exception e) {
                capAsyThrew = true;
            }
        }

        // --- the pinned producer tail, replayed over the real classes -----
        // The producer's validate/stabilize step only rewrites the OPTIMISER
        // start point (RegSarimaComputer.optimize, discarded result); the
        // final block evaluates at the converged parameters, which is what
        // the point below is (est: captured; inj: injected).
        double[] point = c.mode.equals("est") ? capPoint : c.point;
        emit("U;" + c.id + ";" + hexAll(point));
        DoubleSeq p0 = DoubleSeq.of(point);
        DataBlock res13;
        try {
            RegArmaSsqFunction<SarimaModel> ssqfn = RegArmaSsqFunction
                    .<SarimaModel>builder(dmodel.getY())
                    .variables(dmodel.getX())
                    .missingCount(dmodel.getMissingCount())
                    .mapping(smap)
                    .maximumLikelihood(true)
                    .parallelProcessing(false)
                    .build();
            DoubleSeq allc = ssqfn.ssqEvaluate(p0).allCoefficients();
            res13 = dmodel.asLinearModel().calcResiduals(allc);
        } catch (Throwable t) {
            emit("K;" + c.id + ";REJECTED;residuals;" + t.getClass().getSimpleName());
            return;
        }
        RegArmaFunction<SarimaModel> fn;
        double fx;
        try {
            fn = RegArmaFunction.<SarimaModel>builder(res13)
                    .variables(FastMatrix.EMPTY)
                    .missingCount(0)
                    .mapping(smap)
                    .likelihoodEvaluation(DefaultLikelihoodEvaluation.deviance())
                    .build();
            fx = fn.evaluate(p0).getValue();
        } catch (Throwable t) {
            emit("K;" + c.id + ";REJECTED;fx;" + t.getClass().getSimpleName());
            return;
        }
        emit("K;" + c.id + ";" + hex(fx));

        Registry reg = new Registry(fn);
        final double fx0 = fx;
        final double[] pt0 = point;
        IFunctionPoint wfp = new IFunctionPoint() {
            @Override
            public IFunction getFunction() {
                return reg;
            }

            @Override
            public DoubleSeq getParameters() {
                return DoubleSeq.of(pt0);
            }

            @Override
            public double getValue() {
                return fx0;
            }
        };
        IFunctionDerivatives d = new NumericalDerivatives(wfp, false);
        // The stencil's per-coordinate epsilons after checkepsilon (private
        // state, read reflectively; the corpus pins them so the C++ replay
        // need not infer eps from node keys).
        try {
            java.lang.reflect.Field epsField
                    = NumericalDerivatives.class.getDeclaredField("eps");
            epsField.setAccessible(true);
            emit("E;" + c.id + ";" + hexAll((double[]) epsField.get(d)));
        } catch (ReflectiveOperationException t) {
            fatal(c.id, "eps reflection: " + t);
        }
        double[] grad = d.gradient().toArray();
        double[] hflat = flat(d.hessian());

        // Pinned scalings (RegArmaProcessor.compute, fast==false tail):
        //   H *= (0.5 * nobs) / fx ;  g[i] = g[i] * ((-0.5 * nobs) / fx)
        double hk = (0.5 * (double) nobs) / fx;
        double gk = (-0.5 * (double) nobs) / fx;
        double[] score = new double[dim];
        double[] info = new double[dim * dim];
        for (int k = 0; k < dim; ++k) {
            score[k] = grad[k] * gk;
        }
        for (int k = 0; k < dim * dim; ++k) {
            info[k] = hflat[k] * hk;
        }
        FastMatrix im = FastMatrix.square(dim);
        for (int r = 0; r < dim; ++r) {
            for (int j = 0; j < dim; ++j) {
                im.set(r, j, info[r * dim + j]);
            }
        }
        double[] asy = null;
        boolean asyThrew = false;
        try {
            asy = flat(SymmetricMatrix.inverse(im));
        } catch (Throwable t) {
            asyThrew = true;
        }

        if (c.mode.equals("est")) {
            if (!bitsEq(point, capPoint)) {
                fatal(c.id, "replay point drifted");
            }
            if (!bitsEq(score, capScore)) {
                fatal(c.id, "score mismatch vs estimator Point");
            }
            if (!bitsEq(info, capInfo)) {
                fatal(c.id, "information mismatch vs estimator Point");
            }
            if (asyThrew != capAsyThrew) {
                fatal(c.id, "inverse behaviour differs");
            }
            if (!asyThrew && !bitsEq(asy, capAsy)) {
                fatal(c.id, "asymptotic covariance mismatch");
            }
        }

        emit("G;" + c.id + ";" + hexAll(grad));
        emit("H;" + c.id + ";" + hexAll(hflat));
        emit("S;" + c.id + ";" + hexAll(score));
        emit("I;" + c.id + ";" + hexAll(info));
        if (asyThrew) {
            emit("A;" + c.id + ";INVFAIL");
        } else {
            emit("A;" + c.id + ";" + hexAll(asy));
            double[] diag = new double[dim];
            for (int k = 0; k < dim; ++k) {
                diag[k] = asy[k * dim + k];
            }
            emit("D;" + c.id + ";" + hexAll(diag));
        }
        for (Map.Entry<String, double[]> e : reg.values.entrySet()) {
            String v = e.getValue() == null ? "THROW" : hex(e.getValue()[0]);
            emit("V;" + c.id + ";" + e.getKey() + ";" + v);
        }
    }
}
