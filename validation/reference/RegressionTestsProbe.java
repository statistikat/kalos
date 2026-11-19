/*
 * Public-API observer for the item 21-G pinned-reference capture: the stage-R
 * regression tests and mean rules of the pinned jdplus 3.9.0 pipeline
 * (rjd3 + rjd3x13 jars).
 *
 * Nothing here computes any pipeline decision. run() reproduces the CALL
 * SEQUENCE of the live stage-R call sites on the real pinned classes -
 *   1. a ModelDescription with the design variables the decoder's
 *      X13ModelBuilder factories produce (tradingDays / leapYear / easter)
 *      plus UserVariable columns, estimated through
 *      RegSarimaModelling.estimate(eps) exactly as the kernel's call sites
 *      do (processor(true, eps));
 *   2. RegressionVariablesTest.builder().meanTest(tmu).tdTest(ttd, derived)
 *      .movingHolidaysTest(tmh).build().process(m) on a fresh copy of the
 *      description per configuration - the machine under observation; the
 *      post-state (result, mean, remaining variables, preadjustment) is the
 *      decision ground truth;
 *   3. the per-variable test forms the pinned constructor builds are called
 *      separately for OBSERVATION ONLY (new TRegressionTest(t),
 *      new DerivedRegressionTest(t, true): accept, getTStat, getValue);
 *   4. MeanController(cval).test(m) on a fresh modelling per cval (the
 *      mean-added estimation its mean-off branch runs internally is
 *      duplicated on a copy through the same public
 *      RegArimaUtility.processor(true, 1.0E-5) call for the statistics
 *      lines only; the RESULT remains the pinned test's own);
 *   5. new ModelController(0.95).accept(m) + getRTval for the final-mean
 *      rule's rtval0 input (the rule itself is a kernel round-structure
 *      comparison; the measure is captured from the pinned ModelController).
 * All doubles print with %.17g.
 *
 * usage (probe args are packed by the capture script; see
 * capture-regression-tests.R).
 */
import jdplus.toolkit.base.api.timeseries.TsData;
import jdplus.toolkit.base.api.timeseries.TsDomain;
import jdplus.toolkit.base.api.timeseries.TsPeriod;
import jdplus.toolkit.base.api.timeseries.calendars.DayClustering;
import jdplus.toolkit.base.api.timeseries.calendars.GenericTradingDays;
import jdplus.toolkit.base.api.timeseries.calendars.LengthOfPeriodType;
import jdplus.toolkit.base.api.timeseries.regression.EasterVariable;
import jdplus.toolkit.base.api.timeseries.regression.GenericTradingDaysVariable;
import jdplus.toolkit.base.api.timeseries.regression.ITsVariable;
import jdplus.toolkit.base.api.timeseries.regression.LengthOfPeriod;
import jdplus.toolkit.base.api.timeseries.regression.UserVariable;
import jdplus.toolkit.base.api.timeseries.regression.Variable;
import jdplus.toolkit.base.core.math.matrices.FastMatrix;
import jdplus.toolkit.base.core.regarima.RegArimaEstimation;
import jdplus.toolkit.base.core.regarima.RegArimaUtility;
import jdplus.toolkit.base.core.regsarima.regular.IRegressionTest;
import jdplus.toolkit.base.core.regsarima.regular.ModelDescription;
import jdplus.toolkit.base.core.regsarima.regular.ProcessingResult;
import jdplus.toolkit.base.core.regsarima.regular.RegSarimaModelling;
import jdplus.toolkit.base.core.regsarima.regular.RegressionVariablesTest;
import jdplus.toolkit.base.core.regsarima.regular.TRegressionTest;
import jdplus.toolkit.base.core.regarima.DerivedRegressionTest;
import jdplus.toolkit.base.core.stats.likelihood.ConcentratedLikelihoodWithMissing;
import jdplus.x13.base.core.x13.regarima.MeanController;
import jdplus.x13.base.core.x13.regarima.ModelController;
import java.util.List;
import java.util.Locale;

public final class RegressionTestsProbe {

  private RegressionTestsProbe() {
  }

  private static String fmt(double v) {
    return String.format(Locale.ROOT, "%.17g", v);
  }

  private static String seq(double[] v) {
    StringBuilder sb = new StringBuilder();
    for (int i = 0; i < v.length; ++i) {
      if (i != 0) {
        sb.append(';');
      }
      sb.append(fmt(v[i]));
    }
    return sb.toString();
  }

  private static String err(Throwable e) {
    String msg = e.getMessage();
    return "error|" + e.getClass().getName() + "|" + (msg == null ? "" : msg);
  }

  private static String line(String... parts) {
    return String.join("|", parts) + "\n";
  }

  private static String flag(boolean b) {
    return b ? "1" : "0";
  }

  private static String kindOf(Variable v) {
    ITsVariable core = v.getCore();
    if (core instanceof jdplus.toolkit.base.api.timeseries.regression
        .ITradingDaysVariable) {
      return "td";
    }
    if (core instanceof jdplus.toolkit.base.api.timeseries.regression
        .ILengthOfPeriodVariable) {
      return "lop";
    }
    if (core instanceof jdplus.toolkit.base.api.timeseries.regression
        .IMovingHolidayVariable) {
      return "mh";
    }
    if (core instanceof jdplus.toolkit.base.api.timeseries.regression
        .IUserVariable) {
      return "user";
    }
    return "other";
  }

  private static RegressionVariablesTest buildTest(double tmu, double ttd,
      double tmh, boolean derived) {
    // Exactly the decoder's conditional builder calls (readRegression);
    // values <= 0 are never set and the pinned constructor maps them to
    // null tests. ftd stays 0: the decoder never calls tdJointTest.
    RegressionVariablesTest.Builder b = RegressionVariablesTest.builder();
    if (ttd > 0) {
      b = b.tdTest(ttd, derived);
    }
    if (tmh > 0) {
      b = b.movingHolidaysTest(tmh);
    }
    if (tmu > 0) {
      b = b.meanTest(tmu);
    }
    return b.build();
  }

  private static String runCase(int period, int dOrd, int pOrd, int qOrd,
      boolean log0, boolean mean0, int tdType, boolean lop, String easterWin,
      boolean preadjLp, double eps, String rvtCfgs, String cvals, double[] y,
      double[] users, int k) throws Exception {
    StringBuilder sb = new StringBuilder();
    TsPeriod start = period == 4 ? TsPeriod.quarterly(2000, 1)
        : TsPeriod.monthly(2000, 1);
    TsData ts = TsData.of(start, jdplus.toolkit.base.api.data.Doubles.of(y));
    TsDomain domain = TsDomain.of(start, y.length);

    ModelDescription desc = new ModelDescription(ts, domain);
    desc.setLogTransformation(log0);
    // The decoder wraps calendar variables with the calendarAMI map
    // (ami=x13, regeffect=CalendarEffect) on the regressiontest != None
    // path that also builds tdTest.
    java.util.Map<String, String> ami = new java.util.HashMap<>();
    ami.put("ami", "x13");
    ami.put("regeffect", "CalendarEffect");
    java.util.Map<String, String> calendarAMI = java.util.Collections
        .unmodifiableMap(ami);
    if (tdType != 0) {
      // The pinned X13ModelBuilder.defaultTradingDays call chain (its
      // bytecode): GenericTradingDays.contrasts(DayClustering.of(type)).
      DayClustering clustering = tdType == 1 ? DayClustering.TD4
          : DayClustering.TD2;
      ITsVariable td = new GenericTradingDaysVariable(
          GenericTradingDays.contrasts(clustering));
      desc.addVariable(Variable.variable("td", td, calendarAMI));
      if (lop) {
        // X13ModelBuilder.leapYear: new LengthOfPeriod(LeapYear).
        ITsVariable lp = new LengthOfPeriod(LengthOfPeriodType.LeapYear);
        desc.addVariable(Variable.variable("lp", lp, calendarAMI));
      }
    }
    if (easterWin != null && !easterWin.isEmpty()) {
      for (String w : easterWin.split(";")) {
        if (w.isEmpty()) {
          continue;
        }
        int win = Integer.parseInt(w);
        // X13ModelBuilder.easter(Type.Default, w): the EasterVariable
        // builder with the PreComputed mean correction, endPosition -1.
        ITsVariable e = EasterVariable.builder().duration(win)
            .meanCorrection(EasterVariable.Correction.PreComputed)
            .endPosition(-1).build();
        desc.addVariable(Variable.variable("easter" + win, e, calendarAMI));
      }
    }
    for (int c = 0; c < k; ++c) {
      double[] column = new double[y.length];
      System.arraycopy(users, c * y.length, column, 0, y.length);
      TsData ut = TsData.of(start,
          jdplus.toolkit.base.api.data.Doubles.of(column));
      desc.addVariable(Variable.variable("user" + (c + 1),
          new UserVariable("user" + (c + 1), ut)));
    }
    desc.setMean(mean0);
    desc.setPreadjustment(preadjLp ? LengthOfPeriodType.LeapYear
        : LengthOfPeriodType.None);
    jdplus.toolkit.base.api.arima.SarimaOrders orders
        = new jdplus.toolkit.base.api.arima.SarimaOrders(period);
    orders.setD(dOrd);
    orders.setP(pOrd);
    orders.setQ(qOrd);
    desc.setSpecification(orders);

    RegSarimaModelling m = RegSarimaModelling.of(desc);
    try {
      m.estimate(eps);
    } catch (Throwable e) {
      return line("EST", err(e));
    }
    RegArimaEstimation<?> est = m.getEstimation();
    ConcentratedLikelihoodWithMissing cl = est.getConcentratedLikelihood();
    int nx = cl.coefficients().size();
    sb.append(line("CL", Integer.toString(nx), Integer.toString(cl.dim()),
        fmt(cl.ssq())));
    double[] coef = new double[nx];
    for (int i = 0; i < nx; ++i) {
      coef[i] = cl.coefficients().get(i);
    }
    sb.append(line("COEF", seq(coef)));
    FastMatrix ucov = cl.unscaledCovariance();
    for (int i = 0; i < nx; ++i) {
      double[] row = new double[nx];
      for (int jj = 0; jj < nx; ++jj) {
        row[jj] = ucov.get(i, jj);
      }
      sb.append(line("UCOV", Integer.toString(i), seq(row)));
    }
    double[] tstats = cl.tstats(0, false);
    sb.append(line("TSTATS", seq(tstats)));
    sb.append(line("STATE", flag(desc.isMean()), flag(desc.isLogTransformation()),
        desc.getPreadjustment() == LengthOfPeriodType.None ? "None"
        : desc.getPreadjustment().name(),
        flag(desc.isAdjusted())));

    // Force the pinned sortVariables() (first findPosition side effect) so
    // the VAR/OBS dumps and the JVM passes observe the same order; a null
    // core never matches and only triggers the sort.
    desc.findPosition(null);
    int vi = 0;
    for (Variable v : desc.variables().toList()) {
      String kind = kindOf(v);
      int pos = desc.findPosition(v.getCore());
      sb.append(line("VAR", Integer.toString(vi), v.getName(), kind,
          Integer.toString(v.getCore().dim()), Integer.toString(pos),
          Integer.toString(v.freeCoefficientsCount()),
          flag(v.isPreadjustment())));
      ++vi;
    }

    // OBS lines: the pinned test forms the constructor would build, called
    // directly for observation (never fed to process).
    String[] cfgList = rvtCfgs.split(";");
    for (int c = 0; c < cfgList.length; ++c) {
      if (cfgList[c].isEmpty()) {
        continue;
      }
      String[] sc = cfgList[c].split(",");
      double tmu = Double.parseDouble(sc[0]);
      double ttd = Double.parseDouble(sc[1]);
      double tmh = Double.parseDouble(sc[2]);
      boolean derived = sc[3].equals("1");
      IRegressionTest tform = ttd > 0 ? new TRegressionTest(ttd) : null;
      IRegressionTest mform = tmh > 0 ? new TRegressionTest(tmh) : null;
      IRegressionTest muform = tmu > 0 ? new TRegressionTest(tmu) : null;
      int w = 0;
      for (Variable v : desc.variables().toList()) {
        String kind = kindOf(v);
        int pos = desc.findPosition(v.getCore());
        int dim = v.getCore().dim();
        if (tform != null && (kind.equals("td") || kind.equals("lop"))) {
          int width = kind.equals("td") ? dim : 1;
          boolean acc = tform.accept(cl, -1, pos, width);
          String dacc = "na", dv = "na", dt = "na";
          if (!acc && width > 1 && derived) {
            DerivedRegressionTest d
                = new DerivedRegressionTest(ttd, true);
            dacc = flag(d.accept(cl, -1, pos, width));
            dv = fmt(d.getValue());
            dt = fmt(d.getTStat());
          }
          sb.append(line("OBS", Integer.toString(c), Integer.toString(w),
              "tdlop", flag(acc), dacc, dv, dt));
        } else if (mform != null && kind.equals("mh")) {
          sb.append(line("OBS", Integer.toString(c), Integer.toString(w),
              "mh", flag(mform.accept(cl, -1, pos, 1)), "na", "na", "na"));
        }
        ++w;
      }
      if (muform != null && desc.isMean()) {
        sb.append(line("OBS", Integer.toString(c), "-1", "mean",
            flag(muform.accept(cl, -1, 0, 1)), "na", "na", "na"));
      }
      // The pinned machine, on a fresh copy + fresh estimation.
      try {
        ModelDescription d2 = ModelDescription.copyOf(desc);
        RegSarimaModelling m2 = RegSarimaModelling.of(d2);
        m2.estimate(eps);
        RegressionVariablesTest test = buildTest(tmu, ttd, tmh, derived);
        ProcessingResult res = test.process(m2);
        StringBuilder rest = new StringBuilder();
        for (Variable v : d2.variables().toList()) {
          if (rest.length() != 0) {
            rest.append(';');
          }
          rest.append(v.getName());
        }
        sb.append(line("RVT", Integer.toString(c), cfgList[c],
            res == ProcessingResult.Changed ? "1" : "0",
            flag(d2.isMean()),
            d2.getPreadjustment() == LengthOfPeriodType.None ? "None"
            : d2.getPreadjustment().name(),
            rest.length() == 0 ? "-" : rest.toString()));
      } catch (Throwable e) {
        sb.append(line("RVT", Integer.toString(c), cfgList[c], err(e)));
      }
    }

    // MeanController on a fresh modelling per cval.
    String[] cvList = cvals.split(";");
    for (int c = 0; c < cvList.length; ++c) {
      if (cvList[c].isEmpty()) {
        continue;
      }
      double cval = Double.parseDouble(cvList[c]);
      try {
        ModelDescription d3 = ModelDescription.copyOf(desc);
        RegSarimaModelling m3 = RegSarimaModelling.of(d3);
        ConcentratedLikelihoodWithMissing ev = null;
        if (mean0) {
          m3.estimate(eps);
          // Read the statistics BEFORE the test: the Changed branch clears
          // the estimation.
          ev = m3.getEstimation().getConcentratedLikelihood();
        }
        MeanController mc = new MeanController(cval);
        ProcessingResult res = mc.test(m3);
        sb.append(line("MEAN", cvList[c], "res",
            res == ProcessingResult.Changed ? "1" : "0",
            flag(d3.isMean())));
        ConcentratedLikelihoodWithMissing src;
        String tag;
        if (mean0) {
          src = ev;
          tag = "cur";
        } else {
          ModelDescription d4 = ModelDescription.copyOf(desc);
          d4.setMean(true);
          RegArimaEstimation<?> ea = d4.estimate(
              RegArimaUtility.processor(true, 1.0E-5));
          src = ea.getConcentratedLikelihood();
          tag = "add";
        }
        sb.append(line("MEANSTAT", cvList[c], tag,
            fmt(src.coefficients().get(0)),
            fmt(src.unscaledCovariance().get(0, 0)), fmt(src.ssq()),
            Integer.toString(src.dim()), fmt(src.tstat(0, 0, false))));
      } catch (Throwable e) {
        sb.append(line("MEAN", cvList[c], err(e)));
      }
    }

    // ModelController measures feeding the final mean rule.
    try {
      ModelController ctl = new ModelController(0.95);
      boolean ok = ctl.accept(m);
      sb.append(line("RTVAL", fmt(ctl.getRTval()), fmt(ctl.getRvr()),
          flag(ok), fmt(ctl.getLjungBoxTest().getPvalue())));
    } catch (Throwable e) {
      sb.append(line("RTVAL", err(e)));
    }
    return sb.toString();
  }

  public static String run(int period, int dOrd, int pOrd, int qOrd,
      boolean log0, boolean mean0, int tdType, boolean lop, String easterWin,
      boolean preadjLp, double eps, String rvtCfgs, String cvals,
      double[] y, double[] users, int k) {
    try {
      return runCase(period, dOrd, pOrd, qOrd, log0, mean0, tdType, lop,
          easterWin, preadjLp, eps, rvtCfgs, cvals, y, users, k);
    } catch (Throwable e) {
      return line("EST", err(e));
    }
  }
}
