// Pinned-reference observer for item 23-C: the round-1 differencing decision
// of the pinned jdplus 3.9.0 stack - the live tramo.internal.DifferencingModule
// (exactly as TramoKernel.differencingModule() wires it: cancel/ub1/ub2 from
// the AutoModelSpec, seasonal from TramoContext, precision at the
// intermediate precision, initial at (round == 1)), the under-differencing
// battery the live iterate() runs in order after the SeasonalityController
// (RegularUnderDifferencingTest, SeasonalUnderDifferencingTest,
// RegularUnderDifferencingTest2, SeasonalUnderDifferencingTest2, estimator =
// ModelEstimator(precision, curva, outliersModule())), and the
// SeasonalOverDifferencingTest over-side the SeasonalityController loads from
// compareReferenceModels when the specification changed since the reference
// model was built. The observer computes no decision rule and transcribes no
// threshold; it drives the live components and echoes raw getter values only
// (%.17g doubles, enum names; identity-hash toStrings never echoed - pinning
// notes 6.1/6.2). Private DifferencingModule fields have no getters and are
// read reflectively for observation only (DifferencingDecisionProbe/
// OutlierScoringProbe precedent).
//
// The battery, SeasonalityController, SeasonalOverDifferencingTest and
// ModelEstimator classes are package-private in jdplus.tramoseats.base.core.
// tramo; this observer is compiled into that package so the classpath
// (unnamed-module) runtime resolves the same runtime package - no reflection
// is used for access, and the pinned jars stay untouched.
//
// Case grammar (one case per run() call; lines[0] is the header):
//   DIFF <id> <freq> <seasonal y|n> <log y|n> <d> <bd> <p> <q> <bp> <bq>
//        <mean y|n> <cancel> <ub1> <ub2> <eps> <initial y|n>
//   values <tok...>
// DIFF drives DifferencingModule.process over the live-built, live-estimated
// ModelDescription (no variables) and echoes the module result, the resulting
// orders/mean, the IDifferencingModule$Info quadruple, the private field
// panel and the final SarimaModel parameter blocks.
//
//   BAT <id> <freq> <seasonal y|n> <ost n|1|2> <log y|n> <d> <bd> <p> <q>
//       <bp> <bq> <mean y|n> <cancel> <ub1> <ub2> <eps> <ip> <initial y|n>
//       <io y|n> <ao> <ls> <tc> <so> <tcrate> <ml y|n> <curva tok|calc>
//       <sweep y|n>
//   values <tok...>
// BAT runs the driver segment: the differencing module, the driver's
// outlier-variable cleanup when the difference order changed, the live
// ModelEstimator(precision, curva, odm) estimation (odm built as
// TramoKernel.outliersModule() when io=y), the SeasonalityController pass,
// the battery loop (reference = a copy of the post-estimation model, exactly
// as iterate() passes refAuto), and a second SeasonalityController pass so a
// battery spec change forces the compareReferenceModels/SeasonalOver-
// DifferencingTest path. <sweep> additionally echoes, before any mutation,
// the residual-ladder stage facts (SeasonalityTests.residualSeasonalityTest
// score, Qs/Friedman p-values, Tukey/AR peak-test and SpectralPeaks nullness)
// on the estimation residuals and the direct over-side code of a fresh
// SeasonalOverDifferencingTest on the live model.
//
//   FULL <id> <specname> <freq>
//   values <tok...>
// FULL runs the whole live kernel (TramoKernel.of(spec, DEFAULT).process) as
// an end-to-end cross-check and class-load witness; the capture script pairs
// it with a -Xlog:class+load side run.
//
// Monthly or quarterly calendar from 1990. Values tokens are %.17g decimals
// or "nan". Inputs are intrinsic-independent; the capture pins the JVM with
// the G2 recipe anyway.
package jdplus.tramoseats.base.core.tramo;

import jdplus.toolkit.base.api.arima.SarimaOrders;
import jdplus.toolkit.base.api.data.DoubleSeq;
import jdplus.toolkit.base.api.processing.ProcessingLog;
import jdplus.toolkit.base.api.stats.StatisticalTest;
import jdplus.toolkit.base.api.timeseries.TsData;
import jdplus.toolkit.base.api.timeseries.TsPeriod;
import jdplus.toolkit.base.core.regsarima.regular.IDifferencingModule;
import jdplus.toolkit.base.core.regsarima.regular.ModelDescription;
import jdplus.toolkit.base.core.regsarima.regular.ProcessingResult;
import jdplus.toolkit.base.core.regsarima.regular.RegSarimaModelling;
import jdplus.toolkit.base.core.sarima.SarimaModel;
import jdplus.tramoseats.base.api.tramo.TramoSpec;
import jdplus.tramoseats.base.core.tramo.internal.DifferencingModule;
import jdplus.tramoseats.base.core.tramo.internal.OutliersDetectionModule;
import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

public final class TramoDifferencingDecisionProbe {

  TramoDifferencingDecisionProbe() {
  }

  static String g(double v) {
    return String.format(Locale.ROOT, "%.17g", v);
  }

  static double tok(String s) {
    return s.equals("nan") ? Double.NaN : Double.parseDouble(s);
  }

  static boolean yes(String s) {
    return s.equals("y");
  }

  static final class Observer implements ProcessingLog {
    final List<String> evts = new ArrayList<>();
    int depth = 0;
    long seq = 0;

    static String detail(Object d) {
      if (d == null) {
        return "-";
      }
      if (d instanceof Double) {
        return "num " + g((Double) d);
      }
      if (d instanceof IDifferencingModule.Info) {
        IDifferencingModule.Info i = (IDifferencingModule.Info) d;
        return "diffInfo d=" + i.getD() + " bd=" + i.getBd() + " mean="
            + (i.isMean() ? 1 : 0) + " tmean=" + g(i.getTmean());
      }
      if (d instanceof Object[]) {
        StringBuilder s = new StringBuilder("arr[");
        Object[] a = (Object[]) d;
        for (int i = 0; i < a.length; ++i) {
          if (i > 0) {
            s.append(',');
          }
          s.append(String.valueOf(a[i]));
        }
        return s.append(']').toString();
      }
      if (d instanceof String || d instanceof Integer || d instanceof Boolean) {
        return "lit " + String.valueOf(d);
      }
      return "obj " + d.getClass().getSimpleName();
    }

    void emit(String kind, String msg, Object details) {
      ++seq;
      evts.add("evt " + seq + " " + kind + " " + depth + " "
          + (msg == null ? "-" : msg.replace('\n', ' ')) + " det " + detail(details));
    }

    public void push(String s) {
      emit("push", s, null);
      ++depth;
    }

    public void pop() {
      if (depth > 0) {
        --depth;
      }
      emit("pop", null, null);
    }

    public void error(String m, String o, Object d) {
      emit("error", m, d);
    }

    public void warning(String m, String o, Object d) {
      emit("warn", m, d);
    }

    public void remark(String m, String o, Object d) {
      emit("remark", m, d);
    }

    public void info(String m, String o, Object d) {
      emit("info", m, d);
    }

    public void step(String m, Object d) {
      emit("step", m, d);
    }

    public boolean isVerbose() {
      return true;
    }
  }

  static TsData calendar(double[] v, int freq) {
    TsPeriod p = freq == 4 ? TsPeriod.quarterly(1990, 1) : TsPeriod.monthly(1990, 1);
    return TsData.of(p, DoubleSeq.of(v));
  }

  static SarimaOrders orders(int freq, int d, int bd, int p, int q, int bp, int bq) {
    SarimaOrders o = new SarimaOrders(freq);
    o.setD(d);
    o.setBd(bd);
    o.setP(p);
    o.setQ(q);
    o.setBp(bp);
    o.setBq(bq);
    return o;
  }

  static String specOf(ModelDescription desc) {
    SarimaOrders o = desc.specification();
    return o.getP() + "," + o.getD() + "," + o.getQ() + "," + o.getBp() + ","
        + o.getBd() + "," + o.getBq() + "@" + o.getPeriod();
  }

  static double[] block(SarimaModel m, int k, int sel) {
    double[] v = new double[k];
    for (int i = 0; i < k; ++i) {
      v[i] = switch (sel) {
        case 0 -> m.phi(i + 1);
        case 1 -> m.bphi(i + 1);
        case 2 -> m.theta(i + 1);
        default -> m.btheta(i + 1);
      };
    }
    return v;
  }

  static void echoParams(StringBuilder out, String key, SarimaModel m) {
    SarimaOrders o = m.orders();
    out.append(key).append(" phi");
    for (double v : block(m, o.getP(), 0)) {
      out.append(' ').append(g(v));
    }
    out.append(" bphi");
    for (double v : block(m, o.getBp(), 1)) {
      out.append(' ').append(g(v));
    }
    out.append(" theta");
    for (double v : block(m, o.getQ(), 2)) {
      out.append(' ').append(g(v));
    }
    out.append(" btheta");
    for (double v : block(m, o.getBq(), 3)) {
      out.append(' ').append(g(v));
    }
    out.append('\n');
  }

  static Object rf(Object target, String name) {
    try {
      Field f = target.getClass().getDeclaredField(name);
      f.setAccessible(true);
      return f.get(target);
    } catch (ReflectiveOperationException e) {
      return "unavailable";
    }
  }

  static String rfD(Object target, String name) {
    Object v = rf(target, name);
    return v instanceof Double ? g((Double) v) : String.valueOf(v);
  }

  static String rfI(Object target, String name) {
    Object v = rf(target, name);
    return v instanceof Integer ? String.valueOf(v) : String.valueOf(v);
  }

  static String rfB(Object target, String name) {
    Object v = rf(target, name);
    return v instanceof Boolean ? (((Boolean) v) ? "1" : "0") : String.valueOf(v);
  }

  // The private field panel of the live module, in declaration order.
  static void modulePanel(StringBuilder out, String id, DifferencingModule mod) {
    out.append("panel ").append(id)
        .append(" ylen ").append(String.valueOf(rf(mod, "y") == null ? "null" : "set"))
        .append(" lastModel ").append(String.valueOf(rf(mod, "lastModel") == null ? "null" : "set"))
        .append(" rmax ").append(rfD(mod, "rmax"))
        .append(" rsmax ").append(rfD(mod, "rsmax"))
        .append(" c ").append(rfD(mod, "c"))
        .append(" din ").append(rfD(mod, "din"))
        .append(" tmean ").append(rfD(mod, "tmean"))
        .append(" iter ").append(rfI(mod, "iter"))
        .append(" ml ").append(rfB(mod, "ml"))
        .append(" useml ").append(rfB(mod, "useml"))
        .append(" mlused ").append(rfB(mod, "mlused"))
        .append(" maxd ").append(rfI(mod, "maxd"))
        .append(" maxbd ").append(rfI(mod, "maxbd"))
        .append(" ub1 ").append(rfD(mod, "ub1"))
        .append(" ub2 ").append(rfD(mod, "ub2"))
        .append(" cancel ").append(rfD(mod, "cancel"))
        .append(" eps ").append(rfD(mod, "eps"))
        .append(" seasonal ").append(rfB(mod, "seasonal"))
        .append(" initial ").append(rfB(mod, "initial"))
        .append('\n');
    Object sp = rf(mod, "spec");
    out.append("mspec ").append(id).append(' ').append(String.valueOf(sp)).append('\n');
    Object lm = rf(mod, "lastModel");
    if (lm instanceof SarimaModel) {
      echoParams(out, "lastmodel " + id, (SarimaModel) lm);
    }
  }

  static ModelDescription describe(double[] y, int freq, boolean log, boolean mean,
      SarimaOrders o) {
    TsData ts = calendar(y, freq);
    ModelDescription desc = new ModelDescription(ts, ts.getDomain());
    desc.setSpecification(o);
    desc.setMean(mean);
    desc.setLogTransformation(log);
    return desc;
  }

  // -- DIFF -------------------------------------------------------------------
  static void diff(StringBuilder out, String[] h, double[] y) {
    String id = h[1];
    int freq = Integer.parseInt(h[2]);
    boolean seasonal = yes(h[3]);
    boolean log = yes(h[4]);
    int d = Integer.parseInt(h[5]);
    int bd = Integer.parseInt(h[6]);
    int p = Integer.parseInt(h[7]);
    int q = Integer.parseInt(h[8]);
    int bp = Integer.parseInt(h[9]);
    int bq = Integer.parseInt(h[10]);
    boolean mean = yes(h[11]);
    double cancel = tok(h[12]);
    double ub1 = tok(h[13]);
    double ub2 = tok(h[14]);
    double eps = tok(h[15]);
    boolean initial = yes(h[16]);
    out.append("case ").append(id).append('\n');
    out.append("req diff freq ").append(freq).append('\n');
    Observer obs = new Observer();
    ModelDescription desc = describe(y, freq, log, mean, orders(freq, d, bd, p, q, bp, bq));
    RegSarimaModelling m = RegSarimaModelling.of(desc, obs);
    out.append("in ").append(id).append(" n ").append(y.length)
        .append(" spec ").append(specOf(desc))
        .append(" mean ").append(desc.isMean() ? 1 : 0)
        .append(" log ").append(desc.isLogTransformation() ? 1 : 0)
        .append('\n');
    DifferencingModule mod = DifferencingModule.builder()
        .cancel(cancel).ub1(ub1).ub2(ub2).seasonal(seasonal)
        .precision(eps).initial(initial).build();
    ProcessingResult r = mod.process(m);
    out.append("diff_result ").append(id).append(' ').append(r).append('\n');
    IDifferencingModule.Info info = IDifferencingModule.Info.of(mod);
    out.append("diff_info ").append(id)
        .append(" d ").append(info.getD())
        .append(" bd ").append(info.getBd())
        .append(" mean ").append(info.isMean() ? 1 : 0)
        .append(" tmean ").append(g(info.getTmean())).append('\n');
    out.append("after ").append(id).append(" spec ").append(specOf(desc))
        .append(" mean ").append(desc.isMean() ? 1 : 0).append('\n');
    modulePanel(out, id, mod);
    for (String e : obs.evts) {
      out.append(e).append('\n');
    }
  }

  // -- BAT --------------------------------------------------------------------
  static OutliersDetectionModule odm(String[] h, double precision) {
    return OutliersDetectionModule.builder()
        .ao(yes(h[20])).ls(yes(h[21])).tc(yes(h[22])).so(yes(h[23]))
        .span(null).tcrate(tok(h[24])).maximumLikelihood(yes(h[25]))
        .precision(precision).build();
  }

  static void ladderFacts(StringBuilder out, String id, RegSarimaModelling m, int freq) {
    try {
      DoubleSeq res = m.getEstimation().getConcentratedLikelihood().e();
      double[] e = new double[res.length()];
      for (int i = 0; i < e.length; ++i) {
        e[i] = res.get(i);
      }
      SeasonalityTests st = SeasonalityTests.residualSeasonalityTest(DoubleSeq.of(e), freq);
      StatisticalTest qs = st.getQs();
      StatisticalTest np = st.getNonParametricTest();
      out.append("ladder ").append(id)
          .append(" score ").append(st.getScore())
          .append(" qs ").append(qs == null ? "null" : (g(qs.getValue()) + " " + g(qs.getPvalue())))
          .append(" np ").append(np == null ? "null" : (g(np.getValue()) + " " + g(np.getPvalue())))
          .append('\n');
      out.append("peaks ").append(id)
          .append(" tukey ").append(st.getTukeyPeaks() == null ? "null" : "set")
          .append(" ar ").append(st.getArPeaks() == null ? "null" : "set")
          .append(" arr ").append(st.getSpectralPeaks() == null ? "null" : "set")
          .append(" has ").append(jdplus.sa.base.core.tests.SpectralPeaks.hasSeasonalPeaks(
              st.getSpectralPeaks()) ? 1 : 0)
          .append('\n');
    } catch (RuntimeException ex) {
      out.append("ladder ").append(id).append(" error ").append(ex.getClass().getName())
          .append(' ').append(String.valueOf(ex.getMessage()).replace('\n', ' ')).append('\n');
    }
  }

  static void overFacts(StringBuilder out, String id, RegSarimaModelling m) {
    try {
      int code = new SeasonalOverDifferencingTest().test(m);
      out.append("over ").append(id).append(" code ").append(code).append('\n');
    } catch (RuntimeException ex) {
      out.append("over ").append(id).append(" error ").append(ex.getClass().getName())
          .append(' ').append(String.valueOf(ex.getMessage()).replace('\n', ' ')).append('\n');
    }
  }

  static void bat(StringBuilder out, String[] h, double[] y) {
    String id = h[1];
    int freq = Integer.parseInt(h[2]);
    boolean seasonal = yes(h[3]);
    int ost = Integer.parseInt(h[4]);
    boolean log = yes(h[5]);
    SarimaOrders incoming = orders(freq, Integer.parseInt(h[6]), Integer.parseInt(h[7]),
        Integer.parseInt(h[8]), Integer.parseInt(h[9]), Integer.parseInt(h[10]),
        Integer.parseInt(h[11]));
    boolean mean = yes(h[12]);
    double cancel = tok(h[13]);
    double ub1 = tok(h[14]);
    double ub2 = tok(h[15]);
    double eps = tok(h[16]);
    double ip = tok(h[17]);
    // Grammar: 0 BAT, 1 id, 2 freq, 3 seasonal, 4 ost, 5 log, 6..11 d bd p q
    // bp bq, 12 mean, 13 cancel, 14 ub1, 15 ub2, 16 eps, 17 ip, 18 initial,
    // 19 io, 20 ao, 21 ls, 22 tc, 23 so, 24 tcrate, 25 ml, 26 curva,
    // 27 sweep.
    boolean initial = yes(h[18]);
    boolean io = yes(h[19]);
    boolean sweep = yes(h[27]);
    double curva = h[26].equals("calc") ? jdplus.tramoseats.base.core.tramo.internal.TramoUtility
        .calcCv(y.length) : tok(h[26]);
    out.append("case ").append(id).append('\n');
    out.append("req bat freq ").append(freq).append('\n');
    Observer obs = new Observer();
    ModelDescription desc = describe(y, freq, log, mean, incoming);
    RegSarimaModelling m = RegSarimaModelling.of(desc, obs);
    out.append("in ").append(id).append(" n ").append(y.length)
        .append(" spec ").append(specOf(desc))
        .append(" mean ").append(desc.isMean() ? 1 : 0)
        .append(" log ").append(desc.isLogTransformation() ? 1 : 0)
        .append(" curva ").append(g(curva)).append('\n');
    if (m.needEstimation()) {
      m.estimate(ip);
    }
    out.append("init ").append(id).append(" spec ").append(specOf(m.getDescription()))
        .append(" mean ").append(m.getDescription().isMean() ? 1 : 0).append('\n');
    SarimaOrders pre = m.getDescription().specification();
    int preDiffOrder = pre.getDifferenceOrder();
    DifferencingModule mod = DifferencingModule.builder()
        .cancel(cancel).ub1(ub1).ub2(ub2).seasonal(seasonal)
        .precision(ip).initial(initial).build();
    ProcessingResult r = mod.process(m);
    out.append("diff_result ").append(id).append(' ').append(r).append('\n');
    IDifferencingModule.Info info = IDifferencingModule.Info.of(mod);
    out.append("diff_info ").append(id)
        .append(" d ").append(info.getD())
        .append(" bd ").append(info.getBd())
        .append(" mean ").append(info.isMean() ? 1 : 0)
        .append(" tmean ").append(g(info.getTmean())).append('\n');
    modulePanel(out, id, mod);
    ModelDescription dsc = m.getDescription();
    if (r == ProcessingResult.Changed
        && dsc.specification().getDifferenceOrder() != preDiffOrder) {
      boolean removed = dsc.removeVariable(
          v -> jdplus.toolkit.base.api.timeseries.regression.ModellingUtility
              .isOutlier(v, true));
      if (removed) {
        m.clearEstimation();
      }
      out.append("cleanup ").append(id).append(" removed ").append(removed ? 1 : 0)
          .append('\n');
    }
    TramoContext ctx = new TramoContext();
    ctx.seasonal = seasonal;
    ctx.originalSeasonalityTest = ost;
    OutliersDetectionModule odm = io ? odm(h, ip) : null;
    ModelEstimator est = new ModelEstimator(eps, curva, odm);
    boolean estOk = est.estimate(m);
    out.append("estimate ").append(id).append(" ok ").append(estOk ? 1 : 0)
        .append(" spec ").append(specOf(m.getDescription()))
        .append(" mean ").append(m.getDescription().isMean() ? 1 : 0)
        .append(" nvar ").append(m.getDescription().variables().count()).append('\n');
    if (m.getEstimation() != null) {
      echoParams(out, "model " + id, m.getDescription().arima());
    }
    if (sweep) {
      ladderFacts(out, id, m, freq);
      overFacts(out, id, m);
    }
    SeasonalityController sctl = new SeasonalityController();
    sctl.setEstimator(est);
    ProcessingResult s1 = sctl.process(m, ctx);
    out.append("seas1 ").append(id).append(' ').append(s1)
        .append(" spec ").append(specOf(m.getDescription())).append('\n');
    RegSarimaModelling refAuto = RegSarimaModelling.copyOf(m);
    ModelController[] battery = new ModelController[]{
      new RegularUnderDifferencingTest(),
      new SeasonalUnderDifferencingTest(),
      new RegularUnderDifferencingTest2(),
      new SeasonalUnderDifferencingTest2(),
    };
    String[] names = new String[]{"ru1", "su1", "ru2", "su2"};
    for (int i = 0; i < battery.length; ++i) {
      battery[i].setEstimator(est);
      battery[i].setReferenceModel(refAuto);
      ProcessingResult rc = battery[i].process(m, ctx);
      out.append("step ").append(id).append(' ').append(names[i])
          .append(' ').append(rc)
          .append(" spec ").append(specOf(m.getDescription()))
          .append(" mean ").append(m.getDescription().isMean() ? 1 : 0).append('\n');
    }
    ProcessingResult s2 = sctl.process(m, ctx);
    out.append("seas2 ").append(id).append(' ').append(s2)
        .append(" spec ").append(specOf(m.getDescription())).append('\n');
    out.append("final ").append(id).append(" spec ").append(specOf(m.getDescription()))
        .append(" mean ").append(m.getDescription().isMean() ? 1 : 0).append('\n');
    for (String e : obs.evts) {
      out.append(e).append('\n');
    }
  }

  // -- FULL -------------------------------------------------------------------
  static void full(StringBuilder out, String[] h, double[] y) {
    String id = h[1];
    TramoSpec spec = TramoSpec.fromString(h[2]);
    int freq = Integer.parseInt(h[3]);
    out.append("case ").append(id).append('\n');
    out.append("req full spec ").append(h[2]).append('\n');
    Observer obs = new Observer();
    TramoKernel kernel = TramoKernel.of(spec,
        new jdplus.toolkit.base.api.timeseries.regression.ModellingContext());
    jdplus.toolkit.base.core.regsarima.regular.RegSarimaModel r
        = kernel.process(calendar(y, freq), obs);
    out.append("full_result ").append(id).append(' ')
        .append(r == null ? "null" : "ok").append('\n');
    if (r != null) {
      out.append("full_spec ").append(id).append(' ')
          .append(r.specification()).append('\n');
      out.append("full_log ").append(id).append(' ')
          .append(r.getDescription().isLogTransformation() ? 1 : 0).append('\n');
      echoParams(out, "full_model " + id, r.arima());
    }
    for (String e : obs.evts) {
      out.append(e).append('\n');
    }
  }

  // Harness entry for the capture script's -Xlog:class+load side runs: reads
  // one case (header + values lines) from the file named in argv[0] and
  // writes the fact block to stdout. Computes nothing beyond run().
  public static void main(String[] argv) throws Exception {
    java.util.List<String> lines = java.nio.file.Files.readAllLines(
        java.nio.file.Paths.get(argv[0]));
    System.out.print(run(lines.toArray(new String[0])));
  }

  public static String run(String[] lines) {
    StringBuilder out = new StringBuilder();
    try {
      String[] h = lines[0].split(" +");
      String[] v = lines[1].split(" +");
      double[] y = new double[v.length - 1];
      for (int i = 1; i < v.length; ++i) {
        y[i - 1] = tok(v[i]);
      }
      switch (h[0]) {
        case "DIFF":
          diff(out, h, y);
          break;
        case "BAT":
          bat(out, h, y);
          break;
        case "FULL":
          full(out, h, y);
          break;
        default:
          out.append("fatal unknown case ").append(h[0]).append('\n');
      }
    } catch (Throwable t) {
      out.append("fatal ").append(t.getClass().getName()).append(' ')
          .append(String.valueOf(t.getMessage()).replace('\n', ' ')).append('\n');
    }
    return out.toString();
  }
}
