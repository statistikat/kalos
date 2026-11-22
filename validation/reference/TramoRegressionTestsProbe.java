// Pinned-reference observer for item 23-E: the NO-CALENDAR branch of the
// tramo regression-test layer of the pinned jdplus 3.9.0 stack, observed only
// through live public/protected entry points: ModelStatistics.of (public
// static), FastRegressionTest.test / DefaultRegressionTest.test (public
// IRegressionModule), the pass2 refStats gate of TramoKernel (protected,
// called in place from this same-runtime-package observer), the public
// ConcentratedLikelihoodWithMissing$Builder for crafted likelihood hand-overs
// and the public ModelStatistics$Builder for crafted reference rows.
// TramoKernel's constructor is private, so TramoKernel.of(spec, context)
// builds the live options through the private static readAmiOptions and the
// kernel's private state (pass/round/pcr/curva/refAuto/refStats/context.
// seasonal) is PLUMBING-ONLY injected through reflective writes - reflection
// never computes a decision here, every decision runs from the pinned jar
// bytecode (TramoDifferencingDecisionProbe precedent). Raw getter values only
// (%.17g doubles, enum names; identity-hash toStrings are never echoed -
// pinning notes 6.1/6.2).
//
// Calendar-branch requests (auto-identified calendar variables for Fast,
// non-null calendar builder fields for Default) are SEALED WITNESSES for the
// 21-I.3 seam: the JVM runs its T kernels there, the kalos unit refuses them.
//
// Case grammar (one case per run() call; lines[0] is the header, a "values"
// line carries the series as %.17g tokens; every token read back is %.17g or
// "nan"):
//   MS <id> <freq> <log y|n> <mean y|n> <d> <bd> <p> <q> <bp> <bq> <nout>
//      <ip tok> craft|real <ndata> <nmissing> <ssqerr tok>
//   ModelStatistics.of(desc, cl) for the residual block of the values line:
//   "real" estimates the description live at ip and hands the estimation
//   likelihood over, "craft" builds a ConcentratedLikelihoodWithMissing from
//   the ndata/nmissing/ssqErr header fields plus the optional "b ..." and
//   "cov ..." lines (coefficients and unscaledCovariance diagonal, empty when
//   absent). nout AO regressors are added before estimation/counting. The
//   consumed row block is echoed as `msr`, the likelihood hand-over as `msc`
//   (coefficients, unscaledCovariance diagonal, ssq, dim, dof, sigma2, nx and
//   both tstats arms), the gate inputs as `ms`; the skewness/stable rows ride
//   along record-only (their kernels are decision-inert and NOT transcribed).
//   A df <= 0 build throws out of of() - echoed as threw <class>.
//   FR <id> <freq> <log y|n> <mean y|n> <d> <bd> <p> <q> <bp> <bq> <ip tok>
//      <tmean tok> <testMean y|n> <vars> <exact y|n>
//   Live m.estimate(ip) then the live FastRegressionTest.test. vars is a
//   comma list of ao@<i>, ls@<i>, tc@<i>, u1, tdn|tda, lpn|lpa, ean|eaa
//   (a = the variable carries the live TramoModelBuilder.calendarAMI
//   attributes, i.e. isAutomaticallyIdentified). The likelihood the machine
//   reads is echoed as request data (`clc`); with exact=y a fresh copy is
//   re-estimated and the machine re-run with
//   meanThreshold(|tstats(k, true)[0]|) - the addmu arm the accept() call
//   reads - to pin the >= boundary in both directions.
//   DR <id> <freq> <log y|n> <mean y|n> <d> <bd> <p> <q> <bp> <bq> <ip tok>
//      <testMean y|n> <tmean tok> <prec tok> <adjust y|n> <join y|n>
//      <td n|t> <lp n|l> <easter n|e> <exact y|n>
//   Live estimation, then the live DefaultRegressionTest.test. With no
//   calendar builder field and testMean=y the machine's own test-model
//   estimation, RegArimaUtility.processor(true, prec).process(
//   copyOf(desc).regarima(), desc.mapping()), is pre-run and echoed as `drb`
//   request data (createTestModel is a plain copy in the no-calendar branch);
//   exact=y repeats the pre-run on the same state and re-runs the machine
//   with meanThreshold(|t|) to pin the boundary.
//   PG <id> <spec> <pass> <round> <pcr tok> <changed y|n> <seasonal y|n>
//      <curva tok> <curOrders6> <curmean y|n> <curout> <refmode none|real|
//      craft> <refOrders6> <refout> <reflbp tok> <refse tok> <phi1 tok|->
//      <bphi1 tok|->
//   Live TramoKernel.of(spec, DEFAULT) + live kernel.pass2(changed, m). The
//   current model (orders/mean/outliers from the header) is live-estimated at
//   1e-8; refmode real adds a second live estimation whose live
//   ModelStatistics.of row becomes refStats, craft builds the row through the
//   public Builder (reflbp/refse/outliersCount). phi1/bphi1 replace the
//   estimated arimaSpec with Estimated parameters carrying those values (the
//   shapeA/shapeB pins). `pgin` echoes every ladder input (including the 1-lbp
//   forms the bytecode builds and the nb/nr outlier counts), `pg` the kernel
//   state after the call and the return value, `pgm` the mutated model, the
//   events carry the remark / LAST_CHANCE warning.
//   AM <id> <spec>
//   The live readAmiOptions derivation, echoed field by field from the
//   kernel's options (reflective read) plus isOutliersDetection().
//
// Monthly (freq 12) from 1990-01; the freq 4 rows are the D11 refusal mirrors.
package jdplus.tramoseats.base.core.tramo;

import jdplus.toolkit.base.api.arima.SarimaOrders;
import jdplus.toolkit.base.api.arima.SarimaSpec;
import jdplus.toolkit.base.api.data.DoubleSeq;
import jdplus.toolkit.base.api.data.Parameter;
import jdplus.toolkit.base.api.data.ParameterType;
import jdplus.toolkit.base.api.processing.ProcessingLog;
import jdplus.toolkit.base.api.timeseries.TsData;
import jdplus.toolkit.base.api.timeseries.TsPeriod;
import jdplus.toolkit.base.api.timeseries.calendars.DayClustering;
import jdplus.toolkit.base.api.timeseries.calendars.LengthOfPeriodType;
import jdplus.toolkit.base.api.timeseries.regression.IEasterVariable;
import jdplus.toolkit.base.api.timeseries.regression.ILengthOfPeriodVariable;
import jdplus.toolkit.base.api.timeseries.regression.IOutlier;
import jdplus.toolkit.base.api.timeseries.regression.ITradingDaysVariable;
import jdplus.toolkit.base.api.timeseries.regression.IUserVariable;
import jdplus.toolkit.base.api.timeseries.regression.JulianEasterVariable;
import jdplus.toolkit.base.api.timeseries.regression.LengthOfPeriod;
import jdplus.toolkit.base.api.timeseries.regression.ModellingUtility;
import jdplus.toolkit.base.api.timeseries.regression.UserVariable;
import jdplus.toolkit.base.api.timeseries.regression.Variable;
import jdplus.toolkit.base.core.math.matrices.FastMatrix;
import jdplus.toolkit.base.core.modelling.regression.AdditiveOutlierFactory;
import jdplus.toolkit.base.core.modelling.regression.LevelShiftFactory;
import jdplus.toolkit.base.core.modelling.regression.TransitoryChangeFactory;
import jdplus.toolkit.base.core.regsarima.regular.ModelDescription;
import jdplus.toolkit.base.core.regsarima.regular.ProcessingResult;
import jdplus.toolkit.base.core.regsarima.regular.RegSarimaModelling;
import jdplus.toolkit.base.core.regarima.RegArimaUtility;
import jdplus.toolkit.base.core.sarima.SarimaModel;
import jdplus.toolkit.base.core.stats.likelihood.ConcentratedLikelihood;
import jdplus.toolkit.base.core.stats.likelihood.ConcentratedLikelihoodWithMissing;
import jdplus.tramoseats.base.api.tramo.TramoSpec;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

public final class TramoRegressionTestsProbe {

  TramoRegressionTestsProbe() {
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

    void emit(String kind, String msg) {
      ++seq;
      evts.add("evt " + seq + " " + kind + " " + depth + " "
          + (msg == null ? "-" : msg.replace('\n', ' ')));
    }

    public void push(String s) {
      emit("push", s);
      ++depth;
    }

    public void pop() {
      if (depth > 0) {
        --depth;
      }
      emit("pop", null);
    }

    public void error(String m, String o, Object d) {
      emit("error", m);
    }

    public void warning(String m, String o, Object d) {
      emit("warn", m);
    }

    public void remark(String m, String o, Object d) {
      emit("remark", m);
    }

    public void info(String m, String o, Object d) {
      emit("info", m);
    }

    public void step(String m, Object d) {
      emit("step", m);
    }

    // The single-argument forms are default methods on ProcessingLog that
    // route to the three-argument form with the message dropped; override them
    // so that the pinned messages (the LAST_CHANCE warning, the critical-value
    // remark) reach the corpus.
    public void remark(String m) {
      emit("remark", m);
    }

    public void info(String m) {
      emit("info", m);
    }

    public void warning(String m) {
      emit("warn", m);
    }

    public void error(String m) {
      emit("error", m);
    }

    public void step(String m) {
      emit("step", m);
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

  static ModelDescription describe(double[] y, int freq, boolean log, boolean mean,
      SarimaOrders o) {
    TsData ts = calendar(y, freq);
    ModelDescription desc = new ModelDescription(ts, ts.getDomain());
    desc.setSpecification(o);
    desc.setMean(mean);
    desc.setLogTransformation(log);
    return desc;
  }

  static Object rf(Object target, String name) {
    try {
      Field f = target.getClass().getDeclaredField(name);
      f.setAccessible(true);
      return f.get(target);
    } catch (ReflectiveOperationException e) {
      throw new RuntimeException("field " + name, e);
    }
  }

  static void wf(Object target, String name, Object value) {
    try {
      Field f = target.getClass().getDeclaredField(name);
      f.setAccessible(true);
      f.set(target, value);
    } catch (ReflectiveOperationException e) {
      throw new RuntimeException("field " + name, e);
    }
  }

  static double rfD(Object target, String name) {
    return (Double) rf(target, name);
  }

  static int rfI(Object target, String name) {
    return (Integer) rf(target, name);
  }

  static boolean rfB(Object target, String name) {
    return (Boolean) rf(target, name);
  }

  // isOutliersDetection() is private: echoed for the record (the 23-E unit
  // receives it as request data), never used to steer a decision here.
  static boolean outliersDetection(TramoKernel kernel) {
    try {
      Method m = TramoKernel.class.getDeclaredMethod("isOutliersDetection");
      m.setAccessible(true);
      return (Boolean) m.invoke(kernel);
    } catch (ReflectiveOperationException e) {
      throw new RuntimeException("isOutliersDetection", e);
    }
  }

  // isOutlier(v, true) is "core instanceof IOutlier AND the variable carries
  // the ami attribute" (ModellingUtility bytecode), which is also what
  // ModelStatistics.outliersCount and the pass2 nb/nr counts filter on; the
  // attribute is echoed as the /a suffix instead of hiding it.
  static String kindOf(Variable v) {
    String kind = "other";
    if (v.getCore() instanceof IOutlier) {
      kind = "outlier";
    } else if (ModellingUtility.isTradingDays(v)) {
      kind = "td";
    } else if (ModellingUtility.isLengthOfPeriod(v)) {
      kind = "lp";
    } else if (ModellingUtility.isEaster(v)) {
      kind = "mh";
    } else if (v.getCore() instanceof IUserVariable) {
      kind = "user";
    }
    return kind + "/" + (ModellingUtility.isAutomaticallyIdentified(v) ? "a" : "p");
  }

  static void echoVars(StringBuilder out, String tag, String id, ModelDescription desc) {
    StringBuilder s = new StringBuilder();
    desc.variables().forEach(v -> {
      if (s.length() > 0) {
        s.append(',');
      }
      s.append(kindOf(v)).append(':').append(v.getName());
    });
    out.append(tag).append(' ').append(id).append(" vars ").append(s).append('\n');
  }

  static java.time.LocalDateTime at(ModelDescription desc, int offset) {
    TsPeriod start = desc.getEstimationDomain().getStartPeriod();
    return start.dateAt(start.getId() + offset);
  }

  // Outliers are placed inside the estimation domain so that they survive the
  // estimation and reach the nb/nr counts of the pass2 ladder. `ami` mirrors
  // the attributes the live OutliersDetectionModule attaches ({"ami": "tramo",
  // "regeffect": <type>}); only such variables are counted by
  // isOutlier(v, true), so the flag decides the nb/nr and outliersCount rows.
  static void addOutliers(ModelDescription desc, int nout, boolean ami) {
    for (int i = 0; i < nout; ++i) {
      Variable.Builder<IOutlier> vb = Variable.<IOutlier>builder()
          .name("AO" + (i + 1))
          .core(AdditiveOutlierFactory.FACTORY.make(at(desc, 2 + 7 * i)));
      if (ami) {
        vb.attribute("ami", "tramo").attribute("regeffect", "AO");
      }
      desc.addVariable(vb.build());
    }
  }

  static void addKind(ModelDescription desc, String spec) {
    if (spec.startsWith("ao@")) {
      desc.addVariable(Variable.<IOutlier>builder().name("AO")
          .core(AdditiveOutlierFactory.FACTORY.make(at(desc,
              Integer.parseInt(spec.substring(3))))).build());
    } else if (spec.startsWith("ls@")) {
      desc.addVariable(Variable.<IOutlier>builder().name("LS")
          .core(LevelShiftFactory.FACTORY_ZEROENDED.make(at(desc,
              Integer.parseInt(spec.substring(3))))).build());
    } else if (spec.startsWith("tc@")) {
      desc.addVariable(Variable.<IOutlier>builder().name("TC")
          .core(new TransitoryChangeFactory(0.7).make(at(desc,
              Integer.parseInt(spec.substring(3))))).build());
    } else if (spec.equals("u1")) {
      int n = desc.getDomain().getLength();
      double[] u = new double[n];
      for (int i = 0; i < n; ++i) {
        u[i] = 1.0 + 0.01 * i;
      }
      desc.addVariable(Variable.<IUserVariable>builder().name("u1")
          .core(new UserVariable("u1", calendar(u, desc.getAnnualFrequency()))).build());
    } else if (spec.equals("tdn") || spec.equals("tda")) {
      ITradingDaysVariable core = TramoModelBuilder.td(TramoSpec.TRfull,
          DayClustering.TD2,
          new jdplus.toolkit.base.api.timeseries.regression.ModellingContext());
      Variable.Builder<ITradingDaysVariable> vb
          = Variable.<ITradingDaysVariable>builder().name("td").core(core);
      if (spec.equals("tda")) {
        vb.attributes(TramoModelBuilder.calendarAMI);
      }
      desc.addVariable(vb.build());
    } else if (spec.equals("lpn") || spec.equals("lpa")) {
      ILengthOfPeriodVariable core = new LengthOfPeriod(LengthOfPeriodType.LeapYear);
      Variable.Builder<ILengthOfPeriodVariable> vb
          = Variable.<ILengthOfPeriodVariable>builder().name("lp")
              .core(core);
      if (spec.equals("lpa")) {
        vb.attributes(TramoModelBuilder.calendarAMI);
      }
      desc.addVariable(vb.build());
    } else if (spec.equals("ean") || spec.equals("eaa")) {
      IEasterVariable core = new JulianEasterVariable(2, false);
      Variable.Builder<IEasterVariable> vb
          = Variable.<IEasterVariable>builder().name("easter").core(core);
      if (spec.equals("eaa")) {
        vb.attributes(TramoModelBuilder.calendarAMI);
      }
      desc.addVariable(vb.build());
    } else {
      throw new RuntimeException("unknown varspec " + spec);
    }
  }

  // The ConcentratedLikelihood hand-over the C++ unit consumes as request
  // data, plus BOTH arms of cl.tstats: TRegressionTest.accept(cl, nobs, ...)
  // calls cl.tstats(nobs, nobs >= 0), so the TRAMO sites (nobs = k >= 0) read
  // the addmu arm (denominator dim - nx - nobs); the plain arm (denominator
  // dim, the form the 21-G stage-R sites read) rides along as a witness.
  static void echoCl(StringBuilder out, String tag, String id,
      ConcentratedLikelihood cl, int k) {
    double[] coef = cl.coefficients().toArray();
    double[] cov = cl.unscaledCovariance().diagonal().toArray();
    out.append(tag).append(' ').append(id).append(" coef");
    for (double v : coef) {
      out.append(' ').append(g(v));
    }
    out.append(" covdiag");
    for (double v : cov) {
      out.append(' ').append(g(v));
    }
    out.append(" ssq ").append(g(cl.ssq())).append(" dim ").append(cl.dim())
        .append(" dof ").append(cl.degreesOfFreedom()).append(" sigma2 ")
        .append(g(cl.sigma2())).append(" nx ").append(cl.nx()).append(" k ")
        .append(k);
    double[] t = coef.length == 0 ? new double[0] : cl.tstats(k, true);
    out.append(" tstats");
    for (double v : t) {
      out.append(' ').append(g(v));
    }
    double[] td = coef.length == 0 ? new double[0] : cl.tstats(k, false);
    out.append(" tstatsd");
    for (double v : td) {
      out.append(' ').append(g(v));
    }
    out.append('\n');
  }

  // The rows the refStats gate and the refStats-facing decisions consume, and
  // only those, are the unit's boundary; every other row of the record rides
  // along here as a record-only witness (pinning 25.2).
  static void echoMs(StringBuilder out, String id, ModelStatistics s) {
    out.append("msr ").append(id)
        .append(" lb ").append(g(s.getLjungBox()))
        .append(" lbp ").append(g(s.getLjungBoxPvalue()))
        .append(" se ").append(g(s.getSe()))
        .append(" bic ").append(g(s.getBic()))
        .append(" slb ").append(g(s.getSeasonalLjungBox()))
        .append(" slbp ").append(g(s.getSeasonalLjungBoxPvalue()))
        .append(" outliers ").append(s.getOutliersCount())
        .append(" obs ").append(s.getObservationsCount())
        .append(" effobs ").append(s.getEffectiveObservationsCount())
        .append('\n');
    out.append("msrx ").append(id)
        .append(" skew ").append(g(s.getSkewnessAbsvalue()))
        .append(" skewp ").append(g(s.getSkewnessPvalue()))
        .append(" stabM ").append(g(s.getStableMean()))
        .append(" stabMp ").append(g(s.getStableMeanPvalue()))
        .append(" stabV ").append(g(s.getStableVariance()))
        .append(" stabVp ").append(g(s.getStableVariancePvalue()))
        .append('\n');
  }

  static double[] numField(String[] lines, String tag) {
    for (String ln : lines) {
      if (ln.startsWith(tag + " ")) {
        String[] parts = ln.split(" ");
        double[] v = new double[parts.length - 1];
        for (int i = 1; i < parts.length; ++i) {
          v[i - 1] = tok(parts[i]);
        }
        return v;
      }
    }
    return new double[0];
  }

  // -- MS: ModelStatistics.of ---------------------------------------------------
  static void ms(StringBuilder out, String[] h, String[] lines, double[] y) {
    String id = h[1];
    int freq = Integer.parseInt(h[2]);
    boolean log = yes(h[3]);
    boolean mean = yes(h[4]);
    SarimaOrders o = orders(freq, Integer.parseInt(h[5]), Integer.parseInt(h[6]),
        Integer.parseInt(h[7]), Integer.parseInt(h[8]), Integer.parseInt(h[9]),
        Integer.parseInt(h[10]));
    int nout = Integer.parseInt(h[11]);
    double ip = tok(h[12]);
    String mode = h[13];
    boolean ami = h.length > 17 && yes(h[17]);
    out.append("case ").append(id).append('\n');
    out.append("req ms freq ").append(freq).append('\n');
    Observer obs = new Observer();
    ModelDescription desc = describe(y, freq, log, mean, o);
    addOutliers(desc, nout, ami);
    ConcentratedLikelihood cl;
    if (mode.equals("real")) {
      RegSarimaModelling m = RegSarimaModelling.of(desc, obs);
      m.estimate(ip);
      cl = m.getEstimation().getConcentratedLikelihood();
      desc = m.getDescription();
    } else {
      double[] b = numField(lines, "b");
      double[] cov = numField(lines, "cov");
      FastMatrix vm = b.length == 0 ? FastMatrix.EMPTY
          : FastMatrix.identity(b.length);
      for (int i = 0; i < b.length; ++i) {
        vm.set(i, i, cov[i]);
      }
      cl = ConcentratedLikelihoodWithMissing.builder()
          .ndata(Integer.parseInt(h[14])).nmissing(Integer.parseInt(h[15]))
          .ssqErr(tok(h[16])).residuals(DoubleSeq.of(y))
          .coefficients(DoubleSeq.of(b)).unscaledCovariance(vm)
          .scalingFactor(true).build();
    }
    int k = desc.getArimaSpec().freeParametersCount();
    int n = cl.e().length();
    int m = jdplus.toolkit.base.core.stats.tests.LjungBox
        .defaultAutoCorrelationsCount(freq, n);
    out.append("ms ").append(id).append(" freq ").append(freq).append(" n ")
        .append(n).append(" k ").append(k).append(" m ").append(m)
        .append(" spec ").append(specOf(desc)).append(" mean ")
        .append(desc.isMean() ? 1 : 0).append(" nx ").append(cl.nx())
        .append('\n');
    echoCl(out, "msc", id, cl, k);
    // The residual block ModelStatistics.of consumes (golden replay input;
    // craft mode carries the request values back, real mode the ML
    // residuals).
    out.append("mse ").append(id);
    for (double v : cl.e().toArray()) {
      out.append(' ').append(g(v));
    }
    out.append('\n');
    try {
      ModelStatistics s = ModelStatistics.of(desc, cl);
      out.append("ms ").append(id).append(" built 1 threw -\n");
      echoMs(out, id, s);
    } catch (Throwable ex) {
      out.append("ms ").append(id).append(" built 0 threw ")
          .append(ex.getClass().getName()).append('\n');
    }
    for (String e : obs.evts) {
      out.append(e).append('\n');
    }
  }

  // -- FR: FastRegressionTest ---------------------------------------------------
  static void fr(StringBuilder out, String[] h, double[] y) {
    String id = h[1];
    int freq = Integer.parseInt(h[2]);
    boolean log = yes(h[3]);
    boolean mean = yes(h[4]);
    SarimaOrders o = orders(freq, Integer.parseInt(h[5]), Integer.parseInt(h[6]),
        Integer.parseInt(h[7]), Integer.parseInt(h[8]), Integer.parseInt(h[9]),
        Integer.parseInt(h[10]));
    double ip = tok(h[11]);
    double tmean = tok(h[12]);
    boolean testMean = yes(h[13]);
    String[] varspecs = h[14].equals("-") ? new String[0] : h[14].split(",");
    boolean exact = yes(h[15]);
    out.append("case ").append(id).append('\n');
    out.append("req fr freq ").append(freq).append('\n');
    Observer obs = new Observer();
    ModelDescription desc = describe(y, freq, log, mean, o);
    for (String vs : varspecs) {
      if (!vs.isEmpty()) {
        addKind(desc, vs);
      }
    }
    ModelDescription pristine = ModelDescription.copyOf(desc);
    RegSarimaModelling m = RegSarimaModelling.of(desc, obs);
    m.estimate(ip);
    echoVars(out, "frin", id, m.getDescription());
    ConcentratedLikelihood cl = m.getEstimation().getConcentratedLikelihood();
    int k = m.getDescription().getArimaSpec().freeParametersCount();
    echoCl(out, "clc", id, cl, k);
    out.append("frin ").append(id).append(" spec ").append(specOf(m.getDescription()))
        .append(" mean ").append(m.getDescription().isMean() ? 1 : 0)
        .append(" k ").append(k).append('\n');
    FastRegressionTest test = FastRegressionTest.builder()
        .testMean(testMean).meanThreshold(tmean).build();
    ProcessingResult r = test.test(m);
    out.append("fr ").append(id).append(" run a tmean ").append(g(tmean))
        .append(" result ").append(r)
        .append(" spec ").append(specOf(m.getDescription()))
        .append(" mean ").append(m.getDescription().isMean() ? 1 : 0).append('\n');
    echoVars(out, "fr", id, m.getDescription());
    if (exact) {
      Observer obs2 = new Observer();
      RegSarimaModelling m2 = RegSarimaModelling.of(
          ModelDescription.copyOf(pristine), obs2);
      m2.estimate(ip);
      ConcentratedLikelihood cl2 = m2.getEstimation().getConcentratedLikelihood();
      int k2 = m2.getDescription().getArimaSpec().freeParametersCount();
      echoCl(out, "clcb", id, cl2, k2);
      double[] t = cl2.tstats(k2, true);
      double cut = t.length == 0 ? 0.0 : Math.abs(t[0]);
      FastRegressionTest test2 = FastRegressionTest.builder()
          .testMean(testMean).meanThreshold(cut).build();
      ProcessingResult r2 = test2.test(m2);
      out.append("fr ").append(id).append(" run b tmean ").append(g(cut))
          .append(" result ").append(r2)
          .append(" spec ").append(specOf(m2.getDescription()))
          .append(" mean ").append(m2.getDescription().isMean() ? 1 : 0).append('\n');
      echoVars(out, "frb", id, m2.getDescription());
      // The other side of the boundary: one ulp above the machine's own |t|.
      double up = Math.nextUp(cut);
      Observer obs3 = new Observer();
      RegSarimaModelling m3 = RegSarimaModelling.of(
          ModelDescription.copyOf(pristine), obs3);
      m3.estimate(ip);
      ProcessingResult r3 = FastRegressionTest.builder()
          .testMean(testMean).meanThreshold(up).build().test(m3);
      out.append("fr ").append(id).append(" run c tmean ").append(g(up))
          .append(" result ").append(r3)
          .append(" spec ").append(specOf(m3.getDescription()))
          .append(" mean ").append(m3.getDescription().isMean() ? 1 : 0).append('\n');
      echoVars(out, "frc", id, m3.getDescription());
    }
    for (String e : obs.evts) {
      out.append(e).append('\n');
    }
  }

  // -- DR: DefaultRegressionTest ------------------------------------------------
  static DefaultRegressionTest.Builder drbuilder(String[] h) {
    DefaultRegressionTest.Builder b = DefaultRegressionTest.builder()
        .testMean(yes(h[12])).meanThreshold(tok(h[13]))
        .estimationPrecision(tok(h[14])).adjust(yes(h[15])).useJoinTest(yes(h[16]));
    if (!h[17].equals("n")) {
      b.tradingDays(TramoModelBuilder.td(TramoSpec.TRfull, DayClustering.TD2,
          new jdplus.toolkit.base.api.timeseries.regression.ModellingContext()));
    }
    if (!h[18].equals("n")) {
      b.leapYear(new LengthOfPeriod(LengthOfPeriodType.LeapYear));
    }
    if (!h[19].equals("n")) {
      b.easter(new JulianEasterVariable(2, false));
    }
    return b;
  }

  static void dr(StringBuilder out, String[] h, double[] y) {
    String id = h[1];
    int freq = Integer.parseInt(h[2]);
    boolean log = yes(h[3]);
    boolean mean = yes(h[4]);
    SarimaOrders o = orders(freq, Integer.parseInt(h[5]), Integer.parseInt(h[6]),
        Integer.parseInt(h[7]), Integer.parseInt(h[8]), Integer.parseInt(h[9]),
        Integer.parseInt(h[10]));
    double ip = tok(h[11]);
    boolean exact = yes(h[20]);
    out.append("case ").append(id).append('\n');
    out.append("req dr freq ").append(freq).append('\n');
    Observer obs = new Observer();
    ModelDescription desc = describe(y, freq, log, mean, o);
    ModelDescription pristine = ModelDescription.copyOf(desc);
    RegSarimaModelling m = RegSarimaModelling.of(desc, obs);
    m.estimate(ip);
    boolean nocal = h[17].equals("n") && h[18].equals("n") && h[19].equals("n");
    if (nocal && yes(h[12])) {
      // Request data for the C++ replay: the machine's own test-model
      // estimation over the no-calendar shape (createTestModel is a plain
      // copyOf there, the calendar fields only add variables in the refused
      // branch).
      echoDrb(out, "drb", id, m);
    }
    ProcessingResult r = drbuilder(h).build().test(m);
    out.append("dr ").append(id).append(" run a tmean ").append(g(tok(h[13])))
        .append(" result ").append(r)
        .append(" spec ").append(specOf(m.getDescription()))
        .append(" mean ").append(m.getDescription().isMean() ? 1 : 0)
        .append(" est ").append(m.getEstimation() == null ? "null" : "set")
        .append('\n');
    echoVars(out, "dr", id, m.getDescription());
    if (exact) {
      Observer obs2 = new Observer();
      RegSarimaModelling m2 = RegSarimaModelling.of(
          ModelDescription.copyOf(pristine), obs2);
      m2.estimate(ip);
      echoDrb(out, "drbb", id, m2);
      double[] t2 = tcur(m2);
      double cut = t2.length == 0 ? 0.0 : Math.abs(t2[0]);
      ProcessingResult r2 = DefaultRegressionTest.builder()
          .testMean(true).meanThreshold(cut).estimationPrecision(tok(h[14]))
          .build().test(m2);
      out.append("dr ").append(id).append(" run b tmean ").append(g(cut))
          .append(" result ").append(r2)
          .append(" spec ").append(specOf(m2.getDescription()))
          .append(" mean ").append(m2.getDescription().isMean() ? 1 : 0)
          .append(" est ").append(m2.getEstimation() == null ? "null" : "set")
          .append('\n');
      echoDrb(out, "drbc", id, m2);
      ProcessingResult r3 = DefaultRegressionTest.builder()
          .testMean(true).meanThreshold(Math.nextUp(cut))
          .estimationPrecision(tok(h[14])).build().test(m2);
      out.append("dr ").append(id).append(" run c tmean ")
          .append(g(Math.nextUp(cut))).append(" result ").append(r3)
          .append(" spec ").append(specOf(m2.getDescription()))
          .append(" mean ").append(m2.getDescription().isMean() ? 1 : 0)
          .append(" est ").append(m2.getEstimation() == null ? "null" : "set")
          .append('\n');
      for (String e : obs2.evts) {
        out.append(e).append('\n');
      }
    }
    for (String e : obs.evts) {
      out.append(e).append('\n');
    }
  }

  // The machine's test-model estimation (its own call sequence, same state):
  // copyOf(description).regarima() through RegArimaUtility.processor(true,
  // precision) with the description mapping.
  static void echoDrb(StringBuilder out, String tag, String id,
      RegSarimaModelling m) {
    ModelDescription testModel = ModelDescription.copyOf(m.getDescription());
    var est = RegArimaUtility.processor(true, 1e-5)
        .process(testModel.regarima(), m.getDescription().mapping());
    int tk = testModel.getArimaSpec().freeParametersCount();
    echoCl(out, tag, id, est.getConcentratedLikelihood(), tk);
    out.append(tag).append(' ').append(id).append(" spec ")
        .append(specOf(testModel)).append(" mean ")
        .append(testModel.regarima().isMean() ? 1 : 0).append(" k ").append(tk)
        .append('\n');
  }

  static double[] tcur(RegSarimaModelling m) {
    ModelDescription testModel = ModelDescription.copyOf(m.getDescription());
    var est = RegArimaUtility.processor(true, 1e-5)
        .process(testModel.regarima(), m.getDescription().mapping());
    int tk = testModel.getArimaSpec().freeParametersCount();
    ConcentratedLikelihood cl = est.getConcentratedLikelihood();
    return cl.coefficients().length() == 0 ? new double[0] : cl.tstats(tk, true);
  }

  // -- PG: TramoKernel.pass2 ------------------------------------------------------
  // phi/bphi override: replace the description's arimaSpec by Estimated
  // parameters carrying the requested first-ar / first-seasonal-ar value so
  // that the shapeA (phi(1) <= -0.82) and shapeB (bphi(1) <= -0.65) arms are
  // reachable without an unstable live estimation.
  static void override_ar(ModelDescription desc, String phiTok, String bphiTok) {
    SarimaOrders cur = desc.specification();
    SarimaModel live = desc.arima();
    SarimaSpec.Builder sb = SarimaSpec.builder().period(cur.getPeriod())
        .d(cur.getD()).bd(cur.getBd()).p(cur.getP()).q(cur.getQ())
        .bp(cur.getBp()).bq(cur.getBq());
    sb.phi(par(cur.getP(), phiTok, live, 0));
    sb.theta(par(cur.getQ(), "-", live, 2));
    sb.bphi(par(cur.getBp(), bphiTok, live, 1));
    sb.btheta(par(cur.getBq(), "-", live, 3));
    desc.setArimaSpec(sb.build());
  }

  static Parameter[] par(int n, String tok, SarimaModel live, int sel) {
    Parameter[] p = new Parameter[n];
    for (int i = 0; i < n; ++i) {
      double v;
      if (tok.equals("-")) {
        v = switch (sel) {
          case 0 -> live.phi(i + 1);
          case 1 -> live.bphi(i + 1);
          case 2 -> live.theta(i + 1);
          default -> live.btheta(i + 1);
        };
      } else if (i == 0) {
        v = tok(tok);
      } else {
        v = switch (sel) {
          case 0 -> live.phi(i + 1);
          case 1 -> live.bphi(i + 1);
          case 2 -> live.theta(i + 1);
          default -> live.btheta(i + 1);
        };
      }
      p[i] = Parameter.of(v, ParameterType.Estimated);
    }
    return p;
  }

  static void pg(StringBuilder out, String[] h, double[] y) {
    String id = h[1];
    TramoSpec spec = TramoSpec.fromString(h[2]);
    int pass = Integer.parseInt(h[3]);
    int round = Integer.parseInt(h[4]);
    double pcr = tok(h[5]);
    boolean changed = yes(h[6]);
    boolean seasonal = yes(h[7]);
    double curva = tok(h[8]);
    SarimaOrders co = orders(12, Integer.parseInt(h[9]), Integer.parseInt(h[10]),
        Integer.parseInt(h[11]), Integer.parseInt(h[12]), Integer.parseInt(h[13]),
        Integer.parseInt(h[14]));
    boolean curmean = yes(h[15]);
    int curout = Integer.parseInt(h[16]);
    String refmode = h[17];
    SarimaOrders ro = orders(12, Integer.parseInt(h[18]), Integer.parseInt(h[19]),
        Integer.parseInt(h[20]), Integer.parseInt(h[21]), Integer.parseInt(h[22]),
        Integer.parseInt(h[23]));
    int refout = Integer.parseInt(h[24]);
    double reflbp = tok(h[25]);
    double refse = tok(h[26]);
    String phiTok = h.length > 27 ? h[27] : "-";
    String bphiTok = h.length > 28 ? h[28] : "-";
    boolean ami = h.length > 29 ? h[29].equals("ami") : false;
    out.append("case ").append(id).append('\n');
    out.append("req pg spec ").append(h[2]).append('\n');
    Observer obs = new Observer();
    double ip = 1e-8;
    ModelDescription cdesc = describe(y, 12, false, curmean, co);
    addOutliers(cdesc, curout, ami);
    RegSarimaModelling m = RegSarimaModelling.of(cdesc, obs);
    m.estimate(ip);
    if (!phiTok.equals("-") || !bphiTok.equals("-")) {
      override_ar(m.getDescription(), phiTok, bphiTok);
    }
    RegSarimaModelling ref = null;
    ModelStatistics refStats = null;
    if (!refmode.equals("none")) {
      ModelDescription rdesc = describe(y, 12, false, curmean, ro);
      addOutliers(rdesc, refout, ami);
      ref = RegSarimaModelling.of(rdesc, new Observer());
      ref.estimate(ip);
      if (refmode.equals("real")) {
        refStats = ModelStatistics.of(ref.getDescription(),
            ref.getEstimation().getConcentratedLikelihood());
      } else {
        try {
          Method factory = ModelStatistics.class.getDeclaredMethod("builder");
          factory.setAccessible(true);
          ModelStatistics.Builder mb = (ModelStatistics.Builder) factory.invoke(null);
          refStats = mb.ljungBoxPvalue(reflbp).se(refse).outliersCount(refout).build();
        } catch (ReflectiveOperationException e) {
          throw new RuntimeException("ModelStatistics.builder", e);
        }
      }
    }
    ConcentratedLikelihood ccl = m.getEstimation().getConcentratedLikelihood();
    int k = m.getDescription().getArimaSpec().freeParametersCount();
    echoCl(out, "pgcl", id, ccl, k);
    ModelStatistics stats = ModelStatistics.of(m.getDescription(), ccl);
    echoMs(out, id + "-cur", stats);
    if (refStats != null) {
      echoMs(out, id + "-ref", refStats);
    }
    int nb = m.getDescription().countRegressors(
        v -> ModellingUtility.isOutlier(v, true));
    int nr = ref == null ? -1 : ref.getDescription().countRegressors(
        v -> ModellingUtility.isOutlier(v, true));
    TramoKernel kernel = TramoKernel.of(spec,
        new jdplus.toolkit.base.api.timeseries.regression.ModellingContext());
    out.append("pgin ").append(id).append(" pass ").append(pass)
        .append(" round ").append(round).append(" pcr ").append(g(pcr))
        .append(" changed ").append(changed ? 1 : 0)
        .append(" seasonal ").append(seasonal ? 1 : 0)
        .append(" curva ").append(g(curva))
        .append(" spec ").append(specOf(m.getDescription()))
        .append(" mean ").append(m.getDescription().isMean() ? 1 : 0)
        .append(" nb ").append(nb).append(" nr ").append(nr)
        .append(" lbp ").append(g(1.0 - stats.getLjungBoxPvalue()))
        .append(" se ").append(g(stats.getSe()))
        .append(" reflbp ").append(g(refStats == null ? Double.NaN
            : 1.0 - refStats.getLjungBoxPvalue()))
        .append(" refse ").append(g(refStats == null ? Double.NaN : refStats.getSe()))
        .append(" outliers ").append(outliersDetection(kernel)).append('\n');
    SarimaModel ar = m.getDescription().arima();
    out.append("pgin ").append(id).append(" phi1 ")
        .append(g(ar.orders().getP() > 0 ? ar.phi(1) : Double.NaN))
        .append(" bphi1 ").append(g(ar.orders().getBp() > 0 ? ar.bphi(1) : Double.NaN))
        .append('\n');
    wf(kernel, "pass", pass);
    wf(kernel, "round", round);
    wf(kernel, "pcr", pcr);
    wf(kernel, "curva", curva);
    wf(kernel, "refAuto", ref);
    wf(kernel, "refStats", refStats);
    TramoContext ctx = (TramoContext) rf(kernel, "context");
    ctx.seasonal = seasonal;
    boolean ret = kernel.pass2(changed, m);
    ModelStatistics after = (ModelStatistics) rf(kernel, "refStats");
    RegSarimaModelling refAfter = (RegSarimaModelling) rf(kernel, "refAuto");
    out.append("pg ").append(id).append(" ret ").append(ret ? 1 : 0)
        .append(" pass ").append(rfI(kernel, "pass"))
        .append(" round ").append(rfI(kernel, "round"))
        .append(" pcr ").append(g(rfD(kernel, "pcr")))
        .append(" curva ").append(g(rfD(kernel, "curva")))
        .append(" needAuto ").append(rfB(kernel, "needAutoModelling") ? 1 : 0)
        .append(" needOutliers ").append(rfB(kernel, "needOutliers") ? 1 : 0)
        .append(" refNull ").append(refAfter == null ? 1 : 0)
        .append(" refLbp ").append(g(after == null ? Double.NaN
            : after.getLjungBoxPvalue()))
        .append(" refSe ").append(g(after == null ? Double.NaN : after.getSe()))
        .append('\n');
    if (after != null) {
      echoMs(out, id + "-refafter", after);
    }
    out.append("pgm ").append(id).append(" spec ").append(specOf(m.getDescription()))
        .append(" mean ").append(m.getDescription().isMean() ? 1 : 0)
        .append(" nb ").append(m.getDescription().countRegressors(
            v -> ModellingUtility.isOutlier(v, true)))
        .append('\n');
    SarimaModel ar2 = m.getDescription().arima();
    out.append("pgm ").append(id).append(" phi1 ")
        .append(g(ar2.orders().getP() > 0 ? ar2.phi(1) : Double.NaN))
        .append(" bphi1 ").append(g(ar2.orders().getBp() > 0 ? ar2.bphi(1) : Double.NaN))
        .append('\n');
    for (String e : obs.evts) {
      out.append(e).append('\n');
    }
  }

  // -- AM: readAmiOptions ---------------------------------------------------------
  static void am(StringBuilder out, String[] h) {
    String id = h[1];
    TramoSpec spec = TramoSpec.fromString(h[2]);
    out.append("case ").append(id).append('\n');
    out.append("req am spec ").append(h[2]).append('\n');
    TramoKernel kernel = TramoKernel.of(spec,
        new jdplus.toolkit.base.api.timeseries.regression.ModellingContext());
    Object options = rf(kernel, "options");
    out.append("am ").append(id).append(" precision ").append(g((Double) rf(options, "precision")))
        .append(" ur ").append(g((Double) rf(options, "ur")))
        .append(" va ").append(g((Double) rf(options, "va")))
        .append(" reduceVa ").append(g((Double) rf(options, "reduceVa")))
        .append(" checkMu ").append(rfB(options, "checkMu") ? 1 : 0)
        .append(" ljungBoxLimit ").append(g((Double) rf(options, "ljungBoxLimit")))
        .append(" acceptAirline ").append(rfB(options, "acceptAirline") ? 1 : 0)
        .append(" outliers ").append(outliersDetection(kernel) ? 1 : 0)
        .append(" seasonal ").append(rfB(rf(kernel, "context"), "seasonal") ? 1 : 0)
        .append('\n');
  }

  public static String run(String[] lines) {
    StringBuilder out = new StringBuilder();
    String[] h = lines[0].split(" ");
    double[] y = new double[0];
    for (String ln : lines) {
      if (ln.startsWith("values")) {
        String rest = ln.substring("values".length()).trim();
        if (!rest.isEmpty()) {
          String[] parts = rest.split(" ");
          y = new double[parts.length];
          for (int i = 0; i < parts.length; ++i) {
            y[i] = tok(parts[i]);
          }
        }
      }
    }
    try {
      switch (h[0]) {
        case "MS" -> ms(out, h, lines, y);
        case "FR" -> fr(out, h, y);
        case "DR" -> dr(out, h, y);
        case "PG" -> pg(out, h, y);
        case "AM" -> am(out, h);
        default -> out.append("fatal unknown command ").append(h[0]).append('\n');
      }
    } catch (Throwable ex) {
      out.append("fatal ").append(ex.getClass().getName()).append(' ')
          .append(String.valueOf(ex.getMessage()).replace('\n', ' ')).append('\n');
    }
    return out.toString();
  }

  public static void main(String[] args) throws Exception {
    List<String> lines = Files.readAllLines(Path.of(args[0]));
    System.out.print(run(lines.toArray(new String[0])));
    System.out.flush();
  }
}
