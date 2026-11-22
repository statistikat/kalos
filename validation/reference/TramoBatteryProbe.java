// Pinned-reference observer for item 23-C.2: the BATTERY STAGE of the round-1
// TRAMO differencing decision of the pinned jdplus 3.9.0 stack - the live
// SeasonalityController passes, the RegularUnderDifferencingTest(+2) /
// SeasonalUnderDifferencingTest(+2) loop with the refAuto references, the
// SeasonalOverDifferencingTest over-side (forced through the
// computeReferenceModel reference replacement followed by a battery spec
// change, never through the plain probe path) and the residual-ladder /
// SpectralPeaks stage facts - over the POST-ESTIMATION state of the live
// driver segment (estimate(ip), DifferencingModule.process, the
// iterate()-style outlier cleanup and ModelEstimator(precision, curva, null)
// - the outlier-module-on estimations stay with the 23-F multi-round driver
// and are not drivable here).
//
// Like TramoDifferencingDecisionProbe this observer is compiled into the
// jdplus.tramoseats.base.core.tramo package so the package-private
// SeasonalityController / battery / SeasonalOverDifferencingTest /
// ModelEstimator / ModelStatistics types resolve as the same runtime package;
// the pinned jars stay untouched. Every echoed value is a RAW GETTER (%.17g
// doubles, enum names, ints; identity-hash toStrings never echoed - pinning
// notes 6.1/6.2). F distributions carry their degrees of freedom in private
// fields read reflectively for observation only (modulePanel precedent).
//
// The IModelEstimator wrapper (ObservingEstimator) delegates to the live
// ModelEstimator and echoes the description + estimation getters of EVERY
// model it is asked to estimate - the candidate models the battery tests
// build internally are invisible to any post-hoc observer, and this is the
// only side-effect-free observation point (all echoed getters are pure).
//
// Case grammar (one case per run() call; lines[0] is the header, same slots
// as the 23-C BAT grammar with io = n forced):
//   BAT <id> <freq> <seasonal y|n> <ost n|1|2> <log y|n> <d> <bd> <p> <q>
//       <bp> <bq> <mean y|n> <cancel> <ub1> <ub2> <eps> <ip> <initial y|n>
//       <io n> <ao> <ls> <tc> <so> <tcrate> <ml y|n> <curva tok|calc> <sweep>
//   values <tok...>
//
// Fact-line kinds (all prefixed by "id" as in the 23-C protocol):
//   init / cleanup / estimate / model     driver-segment stage facts
//   state   <tag> spec S mean M nvar V | state <tag> null
//   coef    <tag> phi ... bphi ... theta ... btheta ...
//   resid   <tag> <n> <e() bits>          ConcentratedLikelihood.e()
//   lin     <tag> <n> <linearized bits>   RegArimaEstimation.linearizedSeries()
//   lser    <tag> <ser(0,0,np+1)> <coefficient(0)>   ModelController.checkMean pair
//   mstat   <tag> lb <v> lbp <p> slb <v> slbp <p> bic <v> nobs <n> nout <n>
//           skp <p> ska <v> smp <p> svp <p>          ModelStatistics getters
//   ftest   <tag> null | <tag> v <v> p <p> k1 <k1> k2 <k2>   SeasonalFTest
//           run by the observer on the tag's description (the same live
//           machine the controller and the over-side run internally)
//   ctlst   <tag> npv <v> npp <p> dlen <n> tukey null|set ar null|set
//           arr null|<slots> <AR Tukey>* has <0|1>
//           controller-side SeasonalityTests.test(lin, freq, 1, true)
//   resl    <tag> score <s> qs <v> <p> np <v> <p> dlen <n> tukey ... has ..
//           residualSeasonalityTest(e, freq) static wrapper
//   over    <tag> code <c>                direct SeasonalOverDifferencingTest
//   cand    <seq> spec ... mean ... nvar ... + coef/mstat/lser with tag
//           "c<seq>"    ObservingEstimator snapshots (main and candidate
//           estimations in execution order)
//   step / seas1 / seas2 / final / ctx     decision lines (the replay's
//           positive assertions) with the TramoContext evolution
package jdplus.tramoseats.base.core.tramo;

import jdplus.toolkit.base.api.arima.SarimaOrders;
import jdplus.toolkit.base.api.data.DoubleSeq;
import jdplus.toolkit.base.api.processing.ProcessingLog;
import jdplus.toolkit.base.api.stats.StatisticalTest;
import jdplus.toolkit.base.api.timeseries.TsData;
import jdplus.toolkit.base.api.timeseries.TsPeriod;
import jdplus.toolkit.base.core.regsarima.regular.IModelEstimator;
import jdplus.toolkit.base.core.regsarima.regular.IDifferencingModule;
import jdplus.toolkit.base.core.regsarima.regular.ModelDescription;
import jdplus.toolkit.base.core.regsarima.regular.ProcessingResult;
import jdplus.toolkit.base.core.regsarima.regular.RegSarimaModelling;
import jdplus.toolkit.base.core.regsarima.regular.SeasonalFTest;
import jdplus.toolkit.base.core.sarima.SarimaModel;
import jdplus.tramoseats.base.core.tramo.internal.DifferencingModule;
import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

public final class TramoBatteryProbe {

  TramoBatteryProbe() {
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

  static Object rf(Object target, String name) {
    try {
      Field f = target.getClass().getDeclaredField(name);
      f.setAccessible(true);
      return f.get(target);
    } catch (ReflectiveOperationException e) {
      return "unavailable";
    }
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

  static ModelDescription describe(double[] y, int freq, boolean log, boolean mean,
      SarimaOrders o) {
    TsData ts = calendar(y, freq);
    ModelDescription desc = new ModelDescription(ts, ts.getDomain());
    desc.setSpecification(o);
    desc.setMean(mean);
    desc.setLogTransformation(log);
    return desc;
  }

  // -- state observation ------------------------------------------------------

  static void echoSeq(StringBuilder out, String key, DoubleSeq s) {
    out.append(key).append(' ').append(s.length());
    for (int i = 0; i < s.length(); ++i) {
      out.append(' ').append(g(s.get(i)));
    }
    out.append('\n');
  }

  // The ModelStatistics panel the ModelComparator consumes (every value a raw
  // getter of the live ModelStatistics.of(desc, likelihood)).
  static void echoMstat(StringBuilder out, String tag, ModelDescription desc,
      RegSarimaModelling m) {
    ModelStatistics ms = ModelStatistics.of(desc,
        m.getEstimation().getConcentratedLikelihood());
    out.append("mstat ").append(tag)
        .append(" lb ").append(g(ms.getLjungBox()))
        .append(" lbp ").append(g(ms.getLjungBoxPvalue()))
        .append(" slb ").append(g(ms.getSeasonalLjungBox()))
        .append(" slbp ").append(g(ms.getSeasonalLjungBoxPvalue()))
        .append(" bic ").append(g(ms.getBic()))
        .append(" nobs ").append(ms.getObservationsCount())
        .append(" nout ").append(ms.getOutliersCount())
        .append(" skp ").append(g(ms.getSkewnessPvalue()))
        .append(" ska ").append(g(ms.getSkewnessAbsvalue()))
        .append(" smp ").append(g(ms.getStableMeanPvalue()))
        .append(" svp ").append(g(ms.getStableVariancePvalue()))
        .append('\n');
  }

  // The ModelController.checkMean pair: ser(0, freeParametersCount, true)
  // and coefficient(0) of the concentrated likelihood (bytecode 19-43).
  static void echoLser(StringBuilder out, String tag, ModelDescription desc,
      RegSarimaModelling m) {
    try {
      int np = desc.getArimaSpec().freeParametersCount();
      var cl = m.getEstimation().getConcentratedLikelihood();
      out.append("lser ").append(tag).append(' ').append(g(cl.ser(0, np, true)))
          .append(' ').append(g(cl.coefficient(0))).append('\n');
    } catch (RuntimeException ex) {
      // The battery reaches ser() only through checkMean (d = 2, ru2); an
      // observer-side call on other states may hit the JVM's internal buffer
      // bounds (AIOOBE). The fact is simply absent there.
      out.append("lser ").append(tag).append(" error ").append(ex.getClass().getName()).append('\n');
    }
  }

  // The SeasonalFTest machine on the tag's description (the same live call
  // hasSeasonality and SeasonalOverDifferencingTest run internally; the
  // observer-side run cannot mutate the model: test() copies the
  // description before touching the specification). StatisticalTest keeps the
  // distribution only as its description string ("F with <k1> degrees of
  // freedom in the nominator and <k2> ...", Double.toString round-trip); the
  // two degrees of freedom are parsed back from that raw getter value.
  static void echoFtest(StringBuilder out, String tag, ModelDescription desc) {
    SeasonalFTest sf = new SeasonalFTest();
    sf.test(desc);
    StatisticalTest st = sf.getFTest();
    if (st == null) {
      out.append("ftest ").append(tag).append(" null\n");
      return;
    }
    java.util.regex.Matcher mat = java.util.regex.Pattern
        .compile("^F with (.*) degrees of freedom in the nominator and (.*)$")
        .matcher(st.getDescription());
    String k1 = "unavailable", k2 = "unavailable";
    if (mat.matches()) {
      String tail = mat.group(2);
      int cut = tail.indexOf(" degrees");
      try {
        k1 = g(Double.parseDouble(mat.group(1)));
        k2 = g(Double.parseDouble(cut < 0 ? tail : tail.substring(0, cut)));
      } catch (NumberFormatException e) {
        k1 = k2 = "unavailable";
      }
    }
    out.append("ftest ").append(tag)
        .append(" v ").append(g(st.getValue()))
        .append(" p ").append(g(st.getPvalue()))
        .append(" k1 ").append(k1)
        .append(" k2 ").append(k2)
        .append('\n');
  }

  static void echoPeaks(StringBuilder out, SeasonalityTests st) {
    out.append(" tukey ").append(st.getTukeyPeaks() == null ? "null" : "set")
        .append(" ar ").append(st.getArPeaks() == null ? "null" : "set");
    jdplus.sa.base.core.tests.SpectralPeaks[] pk = st.getSpectralPeaks();
    if (pk == null) {
      out.append(" arr null");
    } else {
      out.append(" arr ").append(pk.length);
      for (jdplus.sa.base.core.tests.SpectralPeaks p : pk) {
        out.append(' ').append(p.ar.name()).append('/').append(p.tu.name());
      }
    }
    out.append(" has ")
        .append(jdplus.sa.base.core.tests.SpectralPeaks.hasSeasonalPeaks(pk) ? 1 : 0);
  }

  // Controller-side SeasonalityTests.test(linearized, freq, 1, true): only
  // the non-parametric test, differenced length and spectral peaks feed
  // hasSeasonality; qs is echoed for completeness.
  static void echoCtlState(StringBuilder out, String tag, RegSarimaModelling m) {
    DoubleSeq lin = m.getEstimation().linearizedSeries();
    int freq = m.getDescription().getAnnualFrequency();
    SeasonalityTests st = new SeasonalityTests();
    st.test(lin, freq, 1, true);
    out.append("ctlst ").append(tag);
    StatisticalTest qs = st.getQs();
    out.append(" qsv ").append(qs == null ? "null" : g(qs.getValue()))
        .append(" qsp ").append(qs == null ? "null" : g(qs.getPvalue()));
    StatisticalTest np = st.getNonParametricTest();
    out.append(" npv ").append(np == null ? "null" : g(np.getValue()))
        .append(" npp ").append(np == null ? "null" : g(np.getPvalue()));
    out.append(" dlen ").append(st.getDifferencing().getDifferenced().length());
    echoPeaks(out, st);
    out.append('\n');
  }

  // The su1-side residual ladder: the residualSeasonalityTest static wrapper
  // on the concentrated-likelihood residuals.
  static void echoResLadder(StringBuilder out, String tag, RegSarimaModelling m) {
    DoubleSeq e = m.getEstimation().getConcentratedLikelihood().e();
    int freq = m.getDescription().getAnnualFrequency();
    SeasonalityTests st = SeasonalityTests.residualSeasonalityTest(e, freq);
    out.append("resl ").append(tag).append(" score ").append(st.getScore());
    StatisticalTest qs = st.getQs();
    out.append(" qsv ").append(qs == null ? "null" : g(qs.getValue()))
        .append(" qsp ").append(qs == null ? "null" : g(qs.getPvalue()));
    StatisticalTest np = st.getNonParametricTest();
    out.append(" npv ").append(np == null ? "null" : g(np.getValue()))
        .append(" npp ").append(np == null ? "null" : g(np.getPvalue()));
    out.append(" dlen ").append(st.getDifferencing().getDifferenced().length());
    echoPeaks(out, st);
    out.append('\n');
  }

  // The over-side of a fresh SeasonalOverDifferencingTest on the live model
  // (the same call compareReferenceModels makes when the specification
  // changed since the reference model was set).
  static void echoOver(StringBuilder out, String tag, RegSarimaModelling m) {
    try {
      int code = new SeasonalOverDifferencingTest().test(m);
      out.append("over ").append(tag).append(" code ").append(code).append('\n');
    } catch (RuntimeException ex) {
      out.append("over ").append(tag).append(" error ").append(ex.getClass().getName())
          .append(' ').append(String.valueOf(ex.getMessage()).replace('\n', ' ')).append('\n');
    }
  }

  static void emitState(StringBuilder out, String tag, RegSarimaModelling m) {
    ModelDescription d = m.getDescription();
    if (m.getEstimation() == null) {
      out.append("state ").append(tag).append(" null\n");
      return;
    }
    out.append("state ").append(tag).append(" spec ").append(specOf(d))
        .append(" mean ").append(d.isMean() ? 1 : 0)
        .append(" nvar ").append(d.variables().count()).append('\n');
    echoParams(out, "coef " + tag, d.arima());
    echoSeq(out, "resid " + tag, m.getEstimation().getConcentratedLikelihood().e());
    echoSeq(out, "lin " + tag, m.getEstimation().linearizedSeries());
    echoLser(out, tag, d, m);
    echoMstat(out, tag, d, m);
    echoFtest(out, tag, d);
    echoCtlState(out, tag, m);
    echoResLadder(out, tag, m);
    echoOver(out, tag, m);
  }

  // -- the observing estimator wrapper ---------------------------------------

  static final class ObservingEstimator implements IModelEstimator {
    final ModelEstimator delegate;
    final StringBuilder out;
    long seq = 0;

    ObservingEstimator(ModelEstimator delegate, StringBuilder out) {
      this.delegate = delegate;
      this.out = out;
    }

    @Override
    public boolean estimate(RegSarimaModelling m) {
      boolean ok = delegate.estimate(m);
      String tag = "c" + seq;
      ModelDescription d = m.getDescription();
      out.append("cand ").append(tag).append(" spec ").append(specOf(d))
          .append(" mean ").append(d.isMean() ? 1 : 0)
          .append(" nvar ").append(d.variables().count())
          .append(" ok ").append(ok ? 1 : 0).append('\n');
      ++seq;
      if (ok && m.getEstimation() != null) {
        echoParams(out, "coef " + tag, d.arima());
        echoLser(out, tag, d, m);
        echoMstat(out, tag, d, m);
      }
      return ok;
    }
  }

  // -- the driver segment ------------------------------------------------------

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
    boolean initial = yes(h[18]);
    if (!h[19].equals("n")) {
      out.append("fatal odm-out-of-scope ").append(id).append('\n');
      return;
    }
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
    ObservingEstimator est = new ObservingEstimator(new ModelEstimator(eps, curva, null), out);
    boolean estOk = est.estimate(m);
    out.append("estimate ").append(id).append(" ok ").append(estOk ? 1 : 0)
        .append(" spec ").append(specOf(m.getDescription()))
        .append(" mean ").append(m.getDescription().isMean() ? 1 : 0)
        .append(" nvar ").append(m.getDescription().variables().count()).append('\n');
    emitState(out, id + ":e", m);
    SeasonalityController sctl = new SeasonalityController();
    sctl.setEstimator(est);
    ProcessingResult s1 = sctl.process(m, ctx);
    out.append("seas1 ").append(id).append(' ').append(s1)
        .append(" spec ").append(specOf(m.getDescription()))
        .append(" seasonal ").append(ctx.seasonal ? 1 : 0)
        .append(" ost ").append(ctx.originalSeasonalityTest).append('\n');
    emitState(out, id + ":s1", m);
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
          .append(" mean ").append(m.getDescription().isMean() ? 1 : 0)
          .append(" seasonal ").append(ctx.seasonal ? 1 : 0)
          .append(" ost ").append(ctx.originalSeasonalityTest).append('\n');
      emitState(out, id + ":" + names[i], m);
    }
    ProcessingResult s2 = sctl.process(m, ctx);
    out.append("seas2 ").append(id).append(' ').append(s2)
        .append(" spec ").append(specOf(m.getDescription()))
        .append(" seasonal ").append(ctx.seasonal ? 1 : 0)
        .append(" ost ").append(ctx.originalSeasonalityTest).append('\n');
    out.append("final ").append(id).append(" spec ").append(specOf(m.getDescription()))
        .append(" mean ").append(m.getDescription().isMean() ? 1 : 0).append('\n');
    for (String e : obs.evts) {
      out.append(e).append('\n');
    }
  }

  // Harness entry for the capture script's -Xlog:class+load side runs.
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
        case "BAT":
          bat(out, h, y);
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
