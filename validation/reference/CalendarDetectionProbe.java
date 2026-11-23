package jdplus.tramoseats.base.core.tramo;

import jdplus.sa.base.core.regarima.AutomaticTradingDaysRegressionTest;
import jdplus.sa.base.core.regarima.AutomaticTradingDaysWaldTest;
import jdplus.sa.base.core.regarima.AutomaticTradingRegressionModule;
import jdplus.toolkit.base.api.arima.SarimaOrders;
import jdplus.toolkit.base.api.data.DoubleSeq;
import jdplus.toolkit.base.api.processing.ProcessingLog;
import jdplus.toolkit.base.api.timeseries.TsData;
import jdplus.toolkit.base.api.timeseries.TsPeriod;
import jdplus.toolkit.base.api.timeseries.calendars.DayClustering;
import jdplus.toolkit.base.api.timeseries.calendars.LengthOfPeriodType;
import jdplus.toolkit.base.api.timeseries.regression.IEasterVariable;
import jdplus.toolkit.base.api.timeseries.regression.ILengthOfPeriodVariable;
import jdplus.toolkit.base.api.timeseries.regression.ITradingDaysVariable;
import jdplus.toolkit.base.api.timeseries.regression.ITsVariable;
import jdplus.toolkit.base.api.timeseries.regression.JulianEasterVariable;
import jdplus.toolkit.base.api.timeseries.regression.LengthOfPeriod;
import jdplus.toolkit.base.api.timeseries.regression.ModellingContext;
import jdplus.toolkit.base.api.timeseries.regression.ModellingUtility;
import jdplus.toolkit.base.api.timeseries.regression.Variable;
import jdplus.toolkit.base.core.math.matrices.FastMatrix;
import jdplus.toolkit.base.core.regarima.RegArimaEstimation;
import jdplus.toolkit.base.core.regarima.RegArimaUtility;
import jdplus.toolkit.base.core.regsarima.regular.ModelDescription;
import jdplus.toolkit.base.core.regsarima.regular.ProcessingResult;
import jdplus.toolkit.base.core.regsarima.regular.RegSarimaModelling;
import jdplus.toolkit.base.core.regsarima.regular.TradingDaysRegressionComparator;
import jdplus.toolkit.base.core.sarima.SarimaModel;
import jdplus.toolkit.base.core.stats.linearmodel.JointTest;
import jdplus.toolkit.base.core.stats.likelihood.ConcentratedLikelihood;
import jdplus.toolkit.base.core.stats.likelihood.ConcentratedLikelihoodWithMissing;
import jdplus.toolkit.base.core.stats.likelihood.LikelihoodStatistics;
import jdplus.tramoseats.base.api.tramo.CalendarSpec;
import jdplus.tramoseats.base.api.tramo.TramoSpec;
import jdplus.tramoseats.base.api.tramo.TradingDaysSpec;
import jdplus.x13.base.api.regarima.RegArimaSpec;
import jdplus.x13.base.api.x13.X13Spec;
import jdplus.x13.base.core.x13.X13Kernel;
import jdplus.x13.base.r.X13;
import java.lang.reflect.Method;
import java.nio.file.Files;
import java.nio.file.Path;
import java.time.LocalDate;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/**
 * 21-I.3 pinned-reference probe (jdplus 3.9.0): the automatic
 * calendar-detection machines, driven machine-per-case over live
 * descriptions, plus the shared TradingDaysRegressionComparator algebra, the
 * JointTest quadratic form and the full-pipeline witnesses (TRAMO TRfull
 * auto-method / X-13 rsa4 auto-method overrides). One command = one case;
 * every estimation is re-run from the same series, so two independent
 * processes (capA/capB) must be byte-identical.
 *
 * Case families (first header token; "kind" = clean|tdeff|qclean; the header
 * is "<fam> id kind log orders ip ..." with orders = p,d,q,bp,bd,bq and the
 * family-specific trailing tokens documented below):
 *   CM  comparator ladder over a live description: every row (statistics +
 *       concentrated-likelihood reads, raw bits), bestModel(aicc|bic) and
 *       waldTest echoes. tokens: ... plp(y/n) tdlist pmodel pconstraint
 *       wald(y/n)
 *   TA  TRAMO AutomaticRegressionTest (sel=aicc|bic) /
 *       AutomaticWaldRegressionTest (sel=wald) run on a live model, with the
 *       comparator-row echoes and every per-row createTestModel candidate
 *       hand-over. tokens: ... sel pmodel pconstraint easter(y/n) testmean
 *       adjust plp descvars
 *   FT  TRAMO AutomaticFRegressionTest (td=TD7, wd=TD2): the Info models
 *       (none, td, wd - NO lp) and the branch models (td+lp, wd+lp) all
 *       dumped; the Info arrives through the log. tokens: ... easter(y/n)
 *       testmean adjust fp descvars
 *   SA  sa AutomaticTradingDaysRegressionTest (sel=aicc|bic) /
 *       AutomaticTradingDaysWaldTest (sel=wald) over descriptions with
 *       optional existing "td"/"lp" variables (descvars); the ladder comes
 *       from list=all ([TD2c,TD2,TD3,TD3c,TD4,TD7]) or list=nested
 *       ([TD2,TD3,TD4,TD7]) built through X13ModelBuilder.td on the rsa4
 *       RegArimaSpec (holidays-aware). tokens: ... sel pmodel pconstraint
 *       list tlp adjust descvars
 *   FS  FastRegressionTest over auto-identified calendar variables (the 23-E
 *       seam calendar branch activated). tokens: ... tmean tvar testmean
 *       descvars
 *   DS  DefaultRegressionTest with non-null calendar builder fields, with
 *       the replicated test-model estimation (coef, FULL unscaled covariance)
 *       and a JointTest witness when join=y. tokens: ... tmean testmean
 *       adjust join td lp easter descvars (td = "n" or a cluster name;
 *       lp/easter = "n" or "y")
 *   JT  JointTest quadratic form over a live estimation. tokens: ... pos
 *       dim k
 *   PT  full TramoKernel on TRfull with tradingDays AutoMethod overridden
 *       (method = FTESTraw|FTEST|AIC|BIC|WALD). tokens: id method kind
 *   PX  full X13Kernel on rsa4 with tradingDays.auto overridden
 *       (method = base|AIC|BIC|WALD|WALD5). tokens: id method kind
 *
 * descvars tokens are comma separated: "td:TD2:p" (plain), "td:TD2c:a"
 * (calendarAMI attribute), "td:sel0:a" (the ladder's first variable, sa
 * only), "td:other:a" (TD4, different from every sel), "lp:p", "lp:a",
 * "ea:p", "ea:a" (family-appropriate attribute map).
 */
public final class CalendarDetectionProbe {

  CalendarDetectionProbe() {
  }

  // rsa4 template (the same pinned bytes as validation/reference/I21Probe).
  static final String HEX_RSA4
      = "0ab7010a060a0010011801120b080a1900000000000000c01a1f0a040a02414f0a040a024c530a040a025443120029666666666666e63f301e220c18012202100130013a0210012a4c080111666666666666ee3f19000000000000f03f21280af4893c49c23f29cdccccccccccf03f31d151812fae7ef03f392fbae8a28b2ef23f419a9999999999b93f4967202a1dd933f03f580132161a0a08011001300250016200220808011008180152003a0b0a001148afbc9af2d77a3e1224100122010029000000000000f83f31000000000000044038ffffffffffffffffff0160031a16100219000000000000f03f21000000000000f03f2801";

  // GOLDEN PAYLOAD FORMATTER: raw doubleToRawLongBits hex. (Java's
  // String.format %.17g is NOT a faithful 17-digit renderer - it printed
  // 0x3fb52536ff790458 as 0.082599103328008800 where glibc prints
  // ...8794 for the SAME double; every corpus double therefore rides its
  // bit pattern and the golden generator parses hex. Live witness
  // 2026-11-23 in the 21-I.3 session notes.)
  static String g(double v) {
    return Long.toHexString(Double.doubleToRawLongBits(v));
  }

  static String bits(double v) {
    return Long.toHexString(Double.doubleToRawLongBits(v));
  }

  static double tok(String s) {
    return s.equals("nan") ? Double.NaN : Double.parseDouble(s);
  }

  static boolean yes(String s) {
    return s.equals("y");
  }

  static byte[] unhex(String h) {
    byte[] b = new byte[h.length() / 2];
    for (int i = 0; i < b.length; ++i) {
      b[i] = (byte) Integer.parseInt(h.substring(2 * i, 2 * i + 2), 16);
    }
    return b;
  }

  // -- fixed inputs (identical to validation/reference/I21Probe) --------------
  static double[] clean(int n) {
    double[] y = new double[n];
    for (int t = 0; t < n; ++t) {
      double k = t + 1;
      y[t] = 50 + 0.05 * k + 5 * StrictMath.sin(2 * StrictMath.PI * k / 12)
          + 3 * StrictMath.cos(4 * StrictMath.PI * k / 12)
          + 0.5 * StrictMath.sin(14 * StrictMath.PI * k / 12);
    }
    return y;
  }

  static double[] tdeff(int n) {
    double[] y = new double[n];
    int year = 1990, month = 1;
    int k = 0;
    for (int t = 0; t < n; ++t) {
      int len = LocalDate.of(year, month, 1).lengthOfMonth();
      double s = 0;
      for (int d = 1; d <= len; ++d) {
        LocalDate dt = LocalDate.of(year, month, d);
        int wd = dt.getDayOfWeek().getValue() - 1;
        double w;
        switch (wd) {
          case 0: w = 0.9; break;
          case 1: w = 1.15; break;
          case 2: w = 0.8; break;
          case 3: w = 1.25; break;
          case 4: w = 0.6; break;
          default: w = 0.0; break;
        }
        double noise = 0.15 * StrictMath.sin(k * 0.7 + 1)
            + 0.1 * StrictMath.sin(k * 2.3);
        s += (20.0 + 0.02 * k + noise) * w;
        ++k;
      }
      y[t] = s;
      ++month;
      if (month == 13) {
        month = 1;
        ++year;
      }
    }
    return y;
  }

  static int freqOf(String kind) {
    return kind.startsWith("q") ? 4 : 12;
  }

  static TsData ts(String kind) {
    if (kind.equals("clean")) {
      return TsData.of(TsPeriod.monthly(1990, 1), DoubleSeq.of(clean(96)));
    }
    if (kind.equals("qclean")) {
      return TsData.of(TsPeriod.quarterly(1990, 1), DoubleSeq.of(clean(40)));
    }
    return TsData.of(TsPeriod.monthly(1990, 1), DoubleSeq.of(tdeff(156)));
  }

  static SarimaOrders orders(int freq, String spec) {
    String[] f = spec.split(",");
    SarimaOrders o = new SarimaOrders(freq);
    o.setP(Integer.parseInt(f[0]));
    o.setD(Integer.parseInt(f[1]));
    o.setQ(Integer.parseInt(f[2]));
    o.setBp(Integer.parseInt(f[3]));
    o.setBd(Integer.parseInt(f[4]));
    o.setBq(Integer.parseInt(f[5]));
    return o;
  }

  static ModelDescription describe(String kind, boolean log, boolean mean,
      SarimaOrders o) {
    TsData ts = ts(kind);
    ModelDescription desc = new ModelDescription(ts, ts.getDomain());
    desc.setSpecification(o);
    desc.setMean(mean);
    desc.setLogTransformation(log);
    return desc;
  }

  static String specOf(ModelDescription desc) {
    SarimaOrders o = desc.specification();
    return o.getP() + "," + o.getD() + "," + o.getQ() + "," + o.getBp() + ","
        + o.getBd() + "," + o.getBq() + "@" + o.getPeriod();
  }

  // -- observer ---------------------------------------------------------------
  static final class Observer implements ProcessingLog {
    final List<String> evts = new ArrayList<>();
    int depth = 0;
    long seq = 0;

    void emit(String kind, String msg, Object d) {
      ++seq;
      StringBuilder s = new StringBuilder();
      s.append("evt ").append(seq).append(' ').append(kind).append(' ')
          .append(depth).append(' ')
          .append(msg == null ? "-" : msg.replace('\n', ' '));
      dumpInfo(s, d);
      evts.add(s.toString());
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

    public boolean isVerbose() {
      return true;
    }
  }

  static void dumpInfo(StringBuilder s, Object d) {
    if (d == null) {
      s.append(" det -");
      return;
    }
    if (d instanceof AutomaticTradingRegressionModule.Info) {
      AutomaticTradingRegressionModule.Info i
          = (AutomaticTradingRegressionModule.Info) d;
      s.append(" det ATD best=").append(i.getBest())
          .append(" names=").append(Arrays.toString(i.getNames()));
      LikelihoodStatistics[] ll = i.getLl();
      for (int k = 0; k < ll.length; ++k) {
        if (ll[k] == null) {
          s.append(" ll[").append(k).append("] absent");
          continue;
        }
        s.append(" ll[").append(k).append("] n=").append(ll[k].getObservationsCount())
            .append(" eff=").append(ll[k].getEffectiveObservationsCount())
            .append(" np=").append(ll[k].getEstimatedParametersCount())
            .append(" ssq=").append(g(ll[k].getSsqErr()))
            .append(" aicc=").append(g(ll[k].getAICC()))
            .append(" bic=").append(g(ll[k].getBIC()))
            .append(" bic2=").append(g(ll[k].getBIC2()))
            .append(" bicc=").append(g(ll[k].getBICC()));
      }
      return;
    }
    if (d instanceof AutomaticFRegressionTest.Info) {
      AutomaticFRegressionTest.Info i = (AutomaticFRegressionTest.Info) d;
      s.append(" det FT nhp=").append(i.getNhp())
          .append(" ss0=").append(g(i.getSs0())).append(" ss1=").append(g(i.getSs1()))
          .append(" ss6=").append(g(i.getSs6()))
          .append(" ssmc1=").append(g(i.getSsmc1())).append(" ssmc6=").append(g(i.getSsmc6()))
          .append(" pftd1=").append(g(i.getPftd1())).append(" pftd6=").append(g(i.getPftd6()));
      if (i.getLl0() != null) {
        s.append(" dof0=").append(i.getLl0().degreesOfFreedom())
            .append(" nx0=").append(i.getLl0().nx());
      }
      if (i.getLl1() != null) {
        s.append(" dof1=").append(i.getLl1().degreesOfFreedom())
            .append(" nx1=").append(i.getLl1().nx());
      }
      if (i.getLl6() != null) {
        s.append(" dof6=").append(i.getLl6().degreesOfFreedom())
            .append(" nx6=").append(i.getLl6().nx());
      }
      return;
    }
    s.append(" det obj ").append(String.valueOf(d).replace('\n', ' '));
  }

  // -- variables ---------------------------------------------------------------
  static final ModellingContext CTX = new ModellingContext();

  static DayClustering cluster(String s) {
    switch (s) {
      case "TD2": return DayClustering.TD2;
      case "TD2c": return DayClustering.TD2c;
      case "TD3": return DayClustering.TD3;
      case "TD3c": return DayClustering.TD3c;
      case "TD4": return DayClustering.TD4;
      case "TD7": return DayClustering.TD7;
      default: throw new RuntimeException("unknown cluster " + s);
    }
  }

  static ITradingDaysVariable trtd(String c) {
    return TramoModelBuilder.td(TramoSpec.TRfull, cluster(c), CTX);
  }

  // TRAMO wiring (TramoKernel.alltd).
  static ITradingDaysVariable[] trAlltd() {
    String[] cs = {"TD2", "TD2c", "TD3", "TD3c", "TD4", "TD7"};
    ITradingDaysVariable[] td = new ITradingDaysVariable[cs.length];
    for (int i = 0; i < cs.length; ++i) {
      td[i] = trtd(cs[i]);
    }
    return td;
  }

  static ILengthOfPeriodVariable trLp() {
    return TramoModelBuilder.leapYear(
        TramoSpec.TRfull.getRegression().getCalendar().getTradingDays());
  }

  static IEasterVariable trEaster() {
    return TramoModelBuilder.easter(TramoSpec.TRfull);
  }

  // sa wiring (X13SpecDecoder): the ladder comes from the rsa4 RegArimaSpec.
  // X13ModelBuilder is package-private in the jar; the decoder calls these
  // same public statics, so the probe reaches them reflectively (the
  // variables stay jar-produced).
  static final Class<?> X13MB;

  static {
    try {
      X13MB = Class.forName("jdplus.x13.base.core.x13.regarima.X13ModelBuilder");
    } catch (ClassNotFoundException e) {
      throw new RuntimeException(e);
    }
  }

  static ITradingDaysVariable x13td(RegArimaSpec ra, DayClustering c) {
    try {
      Method m = X13MB.getMethod("td", RegArimaSpec.class, DayClustering.class,
          ModellingContext.class);
      m.setAccessible(true);
      return (ITradingDaysVariable) m.invoke(null, ra, c, CTX);
    } catch (ReflectiveOperationException e) {
      throw new RuntimeException("x13 td", e);
    }
  }

  static ILengthOfPeriodVariable x13lp(jdplus.x13.base.api.regarima.TradingDaysSpec t) {
    try {
      Method m = X13MB.getMethod("leapYear",
          jdplus.x13.base.api.regarima.TradingDaysSpec.class);
      m.setAccessible(true);
      return (ILengthOfPeriodVariable) m.invoke(null, t);
    } catch (ReflectiveOperationException e) {
      throw new RuntimeException("x13 lp", e);
    }
  }

  static X13Spec rsa4() {
    return X13.specOf(unhex(HEX_RSA4));
  }

  static ITradingDaysVariable[] saLadder(String list) {
    RegArimaSpec ra = rsa4().getRegArima();
    String[] cs;
    if (list.equals("all")) {
      cs = new String[]{"TD2c", "TD2", "TD3", "TD3c", "TD4", "TD7"};
    } else if (list.equals("nested")) {
      cs = new String[]{"TD2", "TD3", "TD4", "TD7"};
    } else {
      cs = list.split(",");
    }
    ITradingDaysVariable[] td = new ITradingDaysVariable[cs.length];
    for (int i = 0; i < cs.length; ++i) {
      td[i] = x13td(ra, cluster(cs[i]));
    }
    return td;
  }

  static ILengthOfPeriodVariable saLp() {
    return x13lp(rsa4().getRegArima().getRegression().getTradingDays());
  }

  static Map<String, String> amiMap(boolean tramo) {
    return tramo ? TramoModelBuilder.calendarAMI
        : jdplus.sa.base.core.regarima.ModelBuilder.calendarAMI;
  }

  static void addVar(ModelDescription desc, String tok, boolean tramo,
      ITradingDaysVariable[] ladder) {
    String[] f = tok.split(":");
    boolean ami = (f.length >= 3 ? f[2] : f[1]).equals("a");
    Map<String, String> attrs = amiMap(tramo);
    if (f[0].equals("td")) {
      ITradingDaysVariable core;
      if (f[1].equals("sel0") && ladder != null && ladder.length > 0) {
        core = ladder[0];
      } else if (f[1].equals("sel1") && ladder != null && ladder.length > 1) {
        core = ladder[1];
      } else {
        core = tramo ? trtd(f[1])
            : x13td(rsa4().getRegArima(), cluster(f[1]));
      }
      addVarCore(desc, "td", core, ami ? attrs : null);
    } else if (f[0].equals("lp")) {
      addVarCore(desc, "lp", new LengthOfPeriod(LengthOfPeriodType.LeapYear),
          ami ? attrs : null);
    } else if (f[0].equals("ea")) {
      addVarCore(desc, "easter", new JulianEasterVariable(2, false),
          ami ? attrs : null);
    } else {
      throw new RuntimeException("unknown descvar " + tok);
    }
  }

  static void addVarCore(ModelDescription desc, String name, ITsVariable core,
      Map<String, String> attrs) {
    desc.addVariable(attrs == null
        ? Variable.variable(name, core) : Variable.variable(name, core, attrs));
  }

  static String kindOf(Variable<?> v) {
    String kind = "other";
    if (ModellingUtility.isTradingDays(v)) {
      kind = "td";
    } else if (ModellingUtility.isLengthOfPeriod(v)) {
      kind = "lp";
    } else if (ModellingUtility.isEaster(v)) {
      kind = "mh";
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

  // -- likelihood / matrix dumps -------------------------------------------------
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
    out.append(" ssq ").append(g(cl.ssq())).append(" ssqb ").append(bits(cl.ssq()))
        .append(" dim ").append(cl.dim()).append(" dof ").append(cl.degreesOfFreedom())
        .append(" nx ").append(cl.nx()).append(" k ").append(k).append('\n');
  }

  // The full unscaled covariance (lower triangle), the JointTest hand-over.
  static void echoUcov(StringBuilder out, String tag, String id,
      ConcentratedLikelihood cl) {
    FastMatrix m = cl.unscaledCovariance();
    int n = m.getRowsCount();
    for (int i = 0; i < n; ++i) {
      out.append(tag).append(' ').append(id).append(' ').append(i);
      for (int j = 0; j <= i; ++j) {
        out.append(' ').append(g(m.get(i, j)));
      }
      out.append('\n');
    }
  }

  static void echoRow(StringBuilder out, String id, int idx,
      RegArimaEstimation<SarimaModel> est) {
    if (est == null) {
      out.append("cmr ").append(id).append(' ').append(idx).append(" absent\n");
      return;
    }
    LikelihoodStatistics ls = est.statistics();
    ConcentratedLikelihood cl = est.getConcentratedLikelihood();
    out.append("cmr ").append(id).append(' ').append(idx).append(" present")
        .append(" n ").append(ls.getObservationsCount())
        .append(" eff ").append(ls.getEffectiveObservationsCount())
        .append(" np ").append(ls.getEstimatedParametersCount())
        .append(" aicc ").append(g(ls.getAICC())).append(" aiccb ").append(bits(ls.getAICC()))
        .append(" bic ").append(g(ls.getBIC())).append(" bicb ").append(bits(ls.getBIC()))
        .append(" aic ").append(g(ls.getAIC()))
        .append(" bicc ").append(g(ls.getBICC()))
        .append(" bic2 ").append(g(ls.getBIC2()))
        .append(" ssq ").append(g(cl.ssq())).append(" ssqb ").append(bits(cl.ssq()))
        .append(" nx ").append(cl.nx()).append(" dof ").append(cl.degreesOfFreedom())
        .append(" dim ").append(cl.dim()).append('\n');
  }

  // createTestModel replicas. AR/Wald/sa families force setAirline(true) +
  // setMean(true) first; the FTEST machine's createTestModel does NOT (its
  // candidates keep the current specification - errata vs the pre-existing
  // pinning, verified against the fresh bytecode).
  static ModelDescription candDesc(ModelDescription cur, ITradingDaysVariable td,
      ILengthOfPeriodVariable lp, IEasterVariable ea, Map<String, String> ami,
      boolean forceAirlineMean) {
    ModelDescription d = ModelDescription.copyOf(cur);
    if (forceAirlineMean) {
      d.setAirline(true);
      d.setMean(true);
    }
    if (td != null) {
      d.addVariable(Variable.variable("td", td, ami));
    }
    if (lp != null) {
      d.addVariable(Variable.variable("lp", lp, ami));
    }
    if (ea != null) {
      d.addVariable(Variable.variable("easter", ea, ami));
    }
    return d;
  }

  static void echoCand(StringBuilder out, String tag, String id, String ctag,
      ModelDescription cur, double precision, ITradingDaysVariable td,
      ILengthOfPeriodVariable lp, IEasterVariable ea, boolean tramo,
      boolean forceAirlineMean) {
    ModelDescription cand = candDesc(cur, td, lp, ea, amiMap(tramo),
        forceAirlineMean);
    RegArimaEstimation<SarimaModel> est
        = RegArimaUtility.processor(true, precision)
            .process(cand.regarima(), cand.mapping());
    int k = cand.getArimaSpec().freeParametersCount();
    echoCl(out, tag, id + ":" + ctag, est.getConcentratedLikelihood(), k);
    out.append(tag).append(' ').append(id).append(':').append(ctag)
        .append(" pos td ").append(td == null ? -9 : cand.findPosition(td))
        .append(" lp ").append(lp == null ? -9 : cand.findPosition(lp))
        .append(" ea ").append(ea == null ? -9 : cand.findPosition(ea))
        .append(" nhp ").append(k)
        .append(" curk ").append(cur.getArimaSpec().freeParametersCount())
        .append(" prespec ").append(specOf(cur))
        .append(" curmean ").append(cur.isMean() ? 1 : 0)
        .append(" mean ").append(cand.isMean() ? 1 : 0)
        .append(" adj ").append(cand.isAdjusted() ? 1 : 0)
        .append(" log ").append(cand.isLogTransformation() ? 1 : 0)
        .append(" spec ").append(specOf(cand)).append('\n');
  }

  // Live preamble: describe + descvars + estimate (one modelling per use).
  static RegSarimaModelling live(Observer obs, String kind, boolean log,
      boolean mean, String ordersSpec, double ip, String descvars,
      boolean tramo, ITradingDaysVariable[] ladder) {
    ModelDescription desc = describe(kind, log, mean, orders(freqOf(kind), ordersSpec));
    if (!descvars.equals("-")) {
      for (String t : descvars.split(",")) {
        if (!t.isEmpty()) {
          addVar(desc, t, tramo, ladder);
        }
      }
    }
    RegSarimaModelling m = RegSarimaModelling.of(desc, obs);
    m.estimate(ip);
    return m;
  }

  // -- dispatch -----------------------------------------------------------------
  public static String run(String[] lines) {
    StringBuilder out = new StringBuilder();
    String[] h = lines[0].split(" ");
    try {
      switch (h[0]) {
        case "CM" -> cm(out, h);
        case "TA" -> ta(out, h);
        case "FT" -> ft(out, h);
        case "SA" -> sa(out, h);
        case "FS" -> fs(out, h);
        case "DS" -> ds(out, h);
        case "JT" -> jt(out, h);
        case "PT" -> pt(out, h);
        case "PX" -> px(out, h);
        default -> out.append("fatal unknown command ").append(h[0]).append('\n');
      }
    } catch (Throwable ex) {
      out.append("fatal ").append(ex.getClass().getName()).append(' ')
          .append(String.valueOf(ex.getMessage()).replace('\n', ' ')).append('\n');
      StackTraceElement[] st = ex.getStackTrace();
      for (int i = 0; i < st.length && i < 8; ++i) {
        out.append("fr ").append(i).append(' ').append(st[i]).append('\n');
      }
    }
    return out.toString();
  }

  static void echoKind(StringBuilder out, String kind) {
    double[] y = ts(kind).getValues().toArray();
    out.append("kind ").append(kind).append(" n ").append(y.length).append('\n');
    out.append("values");
    for (double v : y) {
      out.append(' ').append(g(v));
    }
    out.append('\n');
  }

  static void dumpEvents(StringBuilder out, Observer obs) {
    for (String e : obs.evts) {
      out.append(e).append('\n');
    }
  }

  // -- CM: the comparator algebra -------------------------------------------------
  static void cm(StringBuilder out, String[] h) {
    String id = h[1];
    String kind = h[2];
    String orders = h[4];
    double ip = tok(h[5]);
    boolean plp = yes(h[6]);
    String[] tds = h[7].equals("n") ? new String[0] : h[7].split(",");
    double pmodel = tok(h[8]), pconstraint = tok(h[9]);
    boolean wald = yes(h[10]);
    out.append("case CM ").append(id).append('\n');
    echoKind(out, kind);
    Observer obs = new Observer();
    RegSarimaModelling m = live(obs, kind, yes(h[3]), true, orders, ip, "-", true, null);
    out.append("cmn ").append(id).append(" names ")
        .append(Arrays.toString(AutomaticTradingRegressionModule.modelNames(
            trLadder(tds)))).append('\n');
    RegArimaEstimation<SarimaModel>[] ests = TradingDaysRegressionComparator.test(
        m.getDescription(), trLadder(tds), plp ? trLp() : null, ip);
    for (int i = 0; i < ests.length; ++i) {
      echoRow(out, id, i, ests[i]);
    }
    int ba = TradingDaysRegressionComparator.bestModel(ests,
        TradingDaysRegressionComparator.aiccComparator());
    int bb = TradingDaysRegressionComparator.bestModel(ests,
        TradingDaysRegressionComparator.bicComparator());
    int w = -2;
    if (wald) {
      w = TradingDaysRegressionComparator.waldTest(ests, pmodel, pconstraint);
    }
    out.append("cmb ").append(id).append(" bestaicc ").append(ba)
        .append(" bestbic ").append(bb).append(" wald ").append(w)
        .append(" pmodel ").append(g(pmodel))
        .append(" pconstraint ").append(g(pconstraint)).append('\n');
    dumpEvents(out, obs);
  }

  static ITradingDaysVariable[] trLadder(String[] tds) {
    ITradingDaysVariable[] td = new ITradingDaysVariable[tds.length];
    for (int i = 0; i < tds.length; ++i) {
      td[i] = trtd(tds[i]);
    }
    return td;
  }

  // -- TA: TRAMO AutomaticRegressionTest / AutomaticWaldRegressionTest -----------
  static void ta(StringBuilder out, String[] h) {
    String id = h[1];
    String kind = h[2];
    String orders = h[4];
    double ip = tok(h[5]);
    String sel = h[6];
    double pmodel = tok(h[7]), pconstraint = tok(h[8]);
    boolean pea = yes(h[9]);
    boolean testmean = yes(h[10]);
    boolean adjust = yes(h[11]);
    boolean plp = yes(h[12]);
    String descvars = h[13];
    out.append("case TA ").append(id).append(" sel ").append(sel).append('\n');
    echoKind(out, kind);
    ITradingDaysVariable[] ladder = trAlltd();
    ILengthOfPeriodVariable lp = plp ? trLp() : null;
    IEasterVariable ea = pea ? trEaster() : null;
    // The comparator echo (deterministically the machine's own ladder).
    Observer obsA = new Observer();
    RegSarimaModelling mA = live(obsA, kind, true, true, orders, ip, descvars, true, ladder);
    RegArimaEstimation<SarimaModel>[] ests = TradingDaysRegressionComparator.test(
        mA.getDescription(), ladder, lp, ip);
    for (int i = 0; i < ests.length; ++i) {
      echoRow(out, id, i, ests[i]);
    }
    int ba = TradingDaysRegressionComparator.bestModel(ests,
        TradingDaysRegressionComparator.aiccComparator());
    int bb = TradingDaysRegressionComparator.bestModel(ests,
        TradingDaysRegressionComparator.bicComparator());
    int w = sel.equals("wald")
        ? TradingDaysRegressionComparator.waldTest(ests, pmodel, pconstraint) : -2;
    out.append("cmb ").append(id).append(" bestaicc ").append(ba)
        .append(" bestbic ").append(bb).append(" wald ").append(w).append('\n');
    // Every per-row createTestModel candidate hand-over (the machine
    // re-estimates only its selection; capturing all rows removes the need
    // to know `best` at capture time).
    for (int i = 0; i < ests.length; ++i) {
      ITradingDaysVariable tdi = (i >= 2) ? ladder[i - 2] : null;
      ILengthOfPeriodVariable lpi = (i >= 1) ? lp : null;
      echoCand(out, "cand", id, "r" + i, mA.getDescription(), ip, tdi, lpi, ea, true, true);
    }
    // The live machine run (its own fresh modelling).
    Observer obs = new Observer();
    RegSarimaModelling m = live(obs, kind, true, true, orders, ip, descvars, true, ladder);
    echoVars(out, "tain", id, m.getDescription());
    ProcessingResult r;
    if (sel.equals("wald")) {
      r = AutomaticWaldRegressionTest.builder()
          .tradingDays(ladder).leapYear(lp).easter(ea)
          .meanThreshold(1.96).lpThreshold(2.0).easterThreshold(2.2)
          .pmodel(pmodel).pconstraint(pconstraint)
          .testMean(testmean).estimationPrecision(ip).adjust(adjust)
          .build().test(m);
    } else {
      AutomaticRegressionTest.Builder b = AutomaticRegressionTest.builder()
          .tradingDays(ladder).leapYear(lp).easter(ea)
          .meanThreshold(1.96).lpThreshold(2.0).easterThreshold(2.2)
          .testMean(testmean).estimationPrecision(ip).adjust(adjust);
      if (sel.equals("bic")) {
        b.bic();
      } else {
        b.aic();
      }
      r = b.build().test(m);
    }
    out.append("res ").append(id).append(" ").append(sel).append(' ').append(r)
        .append(" spec ").append(specOf(m.getDescription()))
        .append(" mean ").append(m.getDescription().isMean() ? 1 : 0).append('\n');
    echoVars(out, "ta", id, m.getDescription());
    dumpEvents(out, obs);
  }

  // -- FT: TRAMO AutomaticFRegressionTest -----------------------------------------
  static void ft(StringBuilder out, String[] h) {
    String id = h[1];
    String kind = h[2];
    String orders = h[4];
    double ip = tok(h[5]);
    boolean pea = yes(h[6]);
    boolean testmean = yes(h[7]);
    boolean adjust = yes(h[8]);
    double fp = tok(h[9]);
    String descvars = h[10];
    out.append("case FT ").append(id).append('\n');
    echoKind(out, kind);
    ITradingDaysVariable td = trtd("TD7");
    ITradingDaysVariable wd = trtd("TD2");
    ILengthOfPeriodVariable lp = trLp();
    IEasterVariable ea = pea ? trEaster() : null;
    Observer obsA = new Observer();
    RegSarimaModelling mA = live(obsA, kind, true, true, orders, ip, descvars, true, null);
    // The Info models carry NO lp (the bytecode passes aconst_null).
    echoCand(out, "cand", id, "info-none", mA.getDescription(), ip, null, null, ea, true, false);
    echoCand(out, "cand", id, "info-td", mA.getDescription(), ip, td, null, ea, true, false);
    echoCand(out, "cand", id, "info-wd", mA.getDescription(), ip, wd, null, ea, true, false);
    // The branch-A / branch-C update models carry lp.
    echoCand(out, "cand", id, "upd-td", mA.getDescription(), ip, td, lp, ea, true, false);
    echoCand(out, "cand", id, "upd-wd", mA.getDescription(), ip, wd, lp, ea, true, false);
    Observer obs = new Observer();
    RegSarimaModelling m = live(obs, kind, true, true, orders, ip, descvars, true, null);
    echoVars(out, "ftin", id, m.getDescription());
    ProcessingResult r = AutomaticFRegressionTest.builder()
        .tradingDays(td).workingDays(wd).leapYear(lp).easter(ea)
        .meanThreshold(1.96).lpThreshold(2.0).easterThreshold(2.2)
        .fPValue(fp).testMean(testmean).estimationPrecision(ip).adjust(adjust)
        .build().test(m);
    out.append("res ").append(id).append(" ftest ").append(r)
        .append(" spec ").append(specOf(m.getDescription()))
        .append(" mean ").append(m.getDescription().isMean() ? 1 : 0).append('\n');
    echoVars(out, "ft", id, m.getDescription());
    dumpEvents(out, obs);
  }

  // -- SA: the sa machines ----------------------------------------------------------
  static void sa(StringBuilder out, String[] h) {
    String id = h[1];
    String kind = h[2];
    String orders = h[4];
    double ip = tok(h[5]);
    String sel = h[6];
    double pmodel = tok(h[7]), pconstraint = tok(h[8]);
    String list = h[9];
    double tlp = tok(h[10]);
    boolean adjust = yes(h[11]);
    String descvars = h[12];
    out.append("case SA ").append(id).append(" sel ").append(sel)
        .append(" list ").append(list).append('\n');
    echoKind(out, kind);
    ITradingDaysVariable[] ladder = saLadder(list);
    ILengthOfPeriodVariable lp = saLp();
    Observer obsA = new Observer();
    RegSarimaModelling mA = live(obsA, kind, false, true, orders, ip, descvars, false, ladder);
    RegArimaEstimation<SarimaModel>[] ests = TradingDaysRegressionComparator.test(
        mA.getDescription(), ladder, lp, ip);
    for (int i = 0; i < ests.length; ++i) {
      echoRow(out, id, i, ests[i]);
    }
    int ba = TradingDaysRegressionComparator.bestModel(ests,
        TradingDaysRegressionComparator.aiccComparator());
    int bb = TradingDaysRegressionComparator.bestModel(ests,
        TradingDaysRegressionComparator.bicComparator());
    int w = sel.equals("wald")
        ? TradingDaysRegressionComparator.waldTest(ests, pmodel, pconstraint) : -2;
    out.append("cmb ").append(id).append(" bestaicc ").append(ba)
        .append(" bestbic ").append(bb).append(" wald ").append(w)
        .append(" names ")
        .append(Arrays.toString(AutomaticTradingRegressionModule.modelNames(ladder)))
        .append('\n');
    for (int i = 0; i < ests.length; ++i) {
      ITradingDaysVariable tdi = (i >= 2) ? ladder[i - 2] : null;
      ILengthOfPeriodVariable lpi = (i >= 1) ? lp : null;
      echoCand(out, "cand", id, "r" + i, mA.getDescription(), ip, tdi, lpi, null, false, true);
    }
    Observer obs = new Observer();
    RegSarimaModelling m = live(obs, kind, false, true, orders, ip, descvars, false, ladder);
    // The update() witnesses on the CURRENT description.
    ModelDescription desc = m.getDescription();
    Variable<?> vtd = desc.variable("td");
    Variable<?> vlp = desc.variable("lp");
    out.append("saw ").append(id)
        .append(" td ").append(vtd != null ? 1 : 0)
        .append(" lp ").append(vlp != null ? 1 : 0)
        .append(" adj ").append(desc.isAdjusted() ? 1 : 0)
        .append(" log ").append(desc.isLogTransformation() ? 1 : 0)
        .append(" equals");
    for (ITradingDaysVariable t : ladder) {
      out.append(' ').append(vtd != null && vtd.getCore().equals(t) ? 1 : 0);
    }
    out.append('\n');
    echoVars(out, "sain", id, desc);
    ProcessingResult r;
    if (sel.equals("wald")) {
      r = AutomaticTradingDaysWaldTest.builder()
          .tradingDays(ladder).leapYear(lp).lpThreshold(tlp)
          .pmodel(pmodel).pconstraint(pconstraint)
          .estimationPrecision(ip).adjust(adjust)
          .build().test(m);
    } else {
      AutomaticTradingDaysRegressionTest.Builder b
          = AutomaticTradingDaysRegressionTest.builder()
              .tradingDays(ladder).leapYear(lp).lpThreshold(tlp)
              .estimationPrecision(ip).adjust(adjust);
      if (sel.equals("bic")) {
        b.bic();
      } else {
        b.aic();
      }
      r = b.build().test(m);
    }
    out.append("res ").append(id).append(" sa-").append(sel).append(' ').append(r)
        .append(" spec ").append(specOf(m.getDescription()))
        .append(" mean ").append(m.getDescription().isMean() ? 1 : 0).append('\n');
    echoVars(out, "sa", id, m.getDescription());
    dumpEvents(out, obs);
  }

  // -- FS: FastRegressionTest over auto-identified calendar variables -------------
  static void fs(StringBuilder out, String[] h) {
    String id = h[1];
    String kind = h[2];
    String orders = h[4];
    double ip = tok(h[5]);
    double tmean = tok(h[6]);
    double tvar = tok(h[7]);
    boolean testmean = yes(h[8]);
    String descvars = h[9];
    out.append("case FS ").append(id).append('\n');
    echoKind(out, kind);
    Observer obs = new Observer();
    RegSarimaModelling m = live(obs, kind, true, true, orders, ip, descvars, true, null);
    ModelDescription desc = m.getDescription();
    ConcentratedLikelihood cl = m.getEstimation().getConcentratedLikelihood();
    int k = desc.getArimaSpec().freeParametersCount();
    echoCl(out, "clc", id, cl, k);
    echoUcov(out, "ucov", id, cl);
    out.append("fsin ").append(id).append(" spec ").append(specOf(desc))
        .append(" mean ").append(desc.isMean() ? 1 : 0)
        .append(" k ").append(k).append('\n');
    echoVars(out, "fsin", id, desc);
    // Slot witnesses: the first auto-identified variable of each family.
    out.append("fssl ").append(id)
        .append(" td ").append(slotOf(desc, "td"))
        .append(" lp ").append(slotOf(desc, "lp"))
        .append(" mh ").append(slotOf(desc, "mh")).append('\n');
    ProcessingResult r = FastRegressionTest.builder()
        .testMean(testmean).meanThreshold(tmean).varThreshold(tvar)
        .build().test(m);
    out.append("res ").append(id).append(" fast ").append(r)
        .append(" spec ").append(specOf(m.getDescription()))
        .append(" mean ").append(m.getDescription().isMean() ? 1 : 0)
        .append(" est ").append(m.getEstimation() == null ? "null" : "set").append('\n');
    echoVars(out, "fs", id, m.getDescription());
    dumpEvents(out, obs);
  }

  static String slotOf(ModelDescription desc, String fam) {
    final String[] res = {"-:-"};
    desc.variables().forEach(o -> {
      Variable<?> v = (Variable<?>) o;
      if (res[0].equals("-:-") && ModellingUtility.isAutomaticallyIdentified(v)
          && kindOf(v).startsWith(fam)) {
        ITsVariable core = v.getCore();
        res[0] = desc.findPosition(core) + ":" + core.dim();
      }
    });
    return res[0];
  }

  // -- DS: DefaultRegressionTest with calendar fields -------------------------------
  static void ds(StringBuilder out, String[] h) {
    String id = h[1];
    String kind = h[2];
    String orders = h[4];
    double ip = tok(h[5]);
    double tmean = tok(h[6]);
    boolean testmean = yes(h[7]);
    boolean adjust = yes(h[8]);
    boolean join = yes(h[9]);
    ITradingDaysVariable td = h[10].equals("n") ? null : trtd(h[10]);
    ILengthOfPeriodVariable lp = h[11].equals("n")
        ? null : new LengthOfPeriod(LengthOfPeriodType.LeapYear);
    IEasterVariable ea = h[12].equals("n")
        ? null : new JulianEasterVariable(2, false);
    String descvars = h[13];
    out.append("case DS ").append(id).append(" join ").append(join ? 1 : 0).append('\n');
    echoKind(out, kind);
    Observer obs = new Observer();
    RegSarimaModelling m = live(obs, kind, true, true, orders, ip, descvars, true, null);
    ConcentratedLikelihood cl = m.getEstimation().getConcentratedLikelihood();
    int k = m.getDescription().getArimaSpec().freeParametersCount();
    echoCl(out, "clc", id, cl, k);
    out.append("dsin ").append(id).append(" spec ").append(specOf(m.getDescription()))
        .append(" mean ").append(m.getDescription().isMean() ? 1 : 0)
        .append(" k ").append(k).append('\n');
    echoVars(out, "dsin", id, m.getDescription());
    // The replicated test model: copyOf + the machine's calendar variables
    // (calendarAMI attributes), the estimation the tests read.
    ModelDescription dt = ModelDescription.copyOf(m.getDescription());
    if (td != null) {
      dt.addVariable(Variable.variable("td", td, TramoModelBuilder.calendarAMI));
    }
    if (lp != null) {
      dt.addVariable(Variable.variable("lp", lp, TramoModelBuilder.calendarAMI));
    }
    if (ea != null) {
      dt.addVariable(Variable.variable("easter", ea, TramoModelBuilder.calendarAMI));
    }
    RegArimaEstimation<SarimaModel> testEst = RegArimaUtility.processor(true, ip)
        .process(dt.regarima(), m.getDescription().mapping());
    ConcentratedLikelihood tcl = testEst.getConcentratedLikelihood();
    int tk = dt.getArimaSpec().freeParametersCount();
    echoCl(out, "dtc", id, tcl, tk);
    echoUcov(out, "dtu", id, tcl);
    out.append("dt ").append(id).append(" td ")
        .append(td == null ? "-:-" : posDim(dt, td))
        .append(" lp ").append(lp == null ? "-:-" : posDim(dt, lp))
        .append(" ea ").append(ea == null ? "-:-" : posDim(dt, ea))
        .append(" log ").append(dt.isLogTransformation() ? 1 : 0)
        .append(" mean ").append(dt.regarima().isMean() ? 1 : 0).append('\n');
    if (join && td != null) {
      int pos = dt.findPosition(td);
      int dim = td.dim();
      JointTest jt = new JointTest((ConcentratedLikelihoodWithMissing) tcl)
          .variableSelection(pos, dim).hyperParametersCount(tk);
      out.append("jt ").append(id).append(" q ").append(dim)
          .append(" df ").append(tcl.dim() - tcl.nx() - tk)
          .append(" value ").append(g(jt.build().getValue()))
          .append(" pvalue ").append(g(jt.build().getPvalue())).append('\n');
    }
    DefaultRegressionTest.Builder b = DefaultRegressionTest.builder()
        .testMean(testmean).meanThreshold(tmean).estimationPrecision(ip)
        .adjust(adjust).useJoinTest(join);
    if (td != null) {
      b.tradingDays(td);
    }
    if (lp != null) {
      b.leapYear(lp);
    }
    if (ea != null) {
      b.easter(ea);
    }
    ProcessingResult r = b.build().test(m);
    out.append("res ").append(id).append(" default ").append(r)
        .append(" spec ").append(specOf(m.getDescription()))
        .append(" mean ").append(m.getDescription().isMean() ? 1 : 0)
        .append(" est ").append(m.getEstimation() == null ? "null" : "set").append('\n');
    echoVars(out, "ds", id, m.getDescription());
    dumpEvents(out, obs);
  }

  static String posDim(ModelDescription desc, ITsVariable core) {
    return desc.findPosition(core) + ":" + core.dim();
  }

  // -- JT: the JointTest algebra ------------------------------------------------------
  static void jt(StringBuilder out, String[] h) {
    String id = h[1];
    String kind = h[2];
    String orders = h[4];
    double ip = tok(h[5]);
    int pos = Integer.parseInt(h[6]);
    int dim = Integer.parseInt(h[7]);
    int k = Integer.parseInt(h[8]);
    out.append("case JT ").append(id).append('\n');
    echoKind(out, kind);
    Observer obs = new Observer();
    RegSarimaModelling m = live(obs, kind, true, true, orders, ip, "td:TD7:a", true, null);
    ConcentratedLikelihoodWithMissing cl
        = (ConcentratedLikelihoodWithMissing) m.getEstimation().getConcentratedLikelihood();
    echoCl(out, "clc", id, cl, m.getDescription().getArimaSpec().freeParametersCount());
    echoUcov(out, "ucov", id, cl);
    JointTest t = new JointTest(cl).variableSelection(pos, dim)
        .hyperParametersCount(k);
    out.append("jt ").append(id).append(" pos ").append(pos).append(" q ").append(dim)
        .append(" k ").append(k)
        .append(" value ").append(g(t.build().getValue()))
        .append(" pvalue ").append(g(t.build().getPvalue())).append('\n');
    dumpEvents(out, obs);
  }

  // -- PT/PX: the full pipelines -------------------------------------------------------
  static TramoSpec tramoOverride(String method) {
    TradingDaysSpec td = TradingDaysSpec.automatic(LengthOfPeriodType.LeapYear,
        TradingDaysSpec.AutoMethod.valueOf(method), 0.01, false);
    CalendarSpec cal = CalendarSpec.builder()
        .tradingDays(td)
        .easter(TramoSpec.TRfull.getRegression().getCalendar().getEaster())
        .buildWithoutValidation();
    return TramoSpec.TRfull.toBuilder()
        .regression(TramoSpec.TRfull.getRegression().toBuilder()
            .calendar(cal).buildWithoutValidation())
        .buildWithoutValidation();
  }

  static void pt(StringBuilder out, String[] h) {
    String id = h[1], method = h[2], kind = h[3];
    out.append("case PT ").append(id).append(" method ").append(method).append('\n');
    echoKind(out, kind);
    TramoSpec spec = method.equals("FTESTraw") ? TramoSpec.TRfull : tramoOverride(method);
    TradingDaysSpec t = spec.getRegression().getCalendar().getTradingDays();
    out.append("tdspec ").append(id)
        .append(" method=").append(t.getAutomaticMethod())
        .append(" automatic=").append(t.isAutomatic())
        .append(" ptd=").append(g(t.getProbabilityForFTest()))
        .append(" adjust=").append(t.isAutoAdjust())
        .append(" rtest=").append(t.getRegressionTestType()).append('\n');
    Observer obs = new Observer();
    jdplus.toolkit.base.core.regsarima.regular.RegSarimaModel r;
    try {
      r = TramoKernel.of(spec, new ModellingContext()).process(ts(kind), obs);
    } catch (Exception ex) {
      out.append("run THROW ").append(ex.getClass().getName()).append(' ')
          .append(String.valueOf(ex.getMessage()).replace('\n', ' ')).append('\n');
      StackTraceElement[] st = ex.getStackTrace();
      for (int i = 0; i < st.length && i < 12; ++i) {
        out.append("fr ").append(i).append(' ').append(st[i]).append('\n');
      }
      dumpEvents(out, obs);
      return;
    }
    out.append("run OK ").append(r == null ? "null" : "set").append('\n');
    dumpEvents(out, obs);
  }

  static X13Spec x13Override(String method) {
    X13Spec base = rsa4();
    if (method.equals("base")) {
      return base;
    }
    double p1 = method.equals("WALD5") ? 0.5 : 0.01;
    double p2 = method.equals("WALD5") ? 0.001 : 0.1;
    jdplus.x13.base.api.regarima.TradingDaysSpec.AutoMethod am
        = jdplus.x13.base.api.regarima.TradingDaysSpec.AutoMethod.valueOf(
            method.equals("WALD5") ? "WALD" : method);
    jdplus.x13.base.api.regarima.TradingDaysSpec td
        = jdplus.x13.base.api.regarima.TradingDaysSpec.automatic(
            LengthOfPeriodType.LeapYear, am, p1, p2, true);
    RegArimaSpec ra = base.getRegArima();
    RegArimaSpec ra2 = ra.toBuilder()
        .regression(ra.getRegression().toBuilder().tradingDays(td)
            .buildWithoutValidation())
        .buildWithoutValidation();
    return base.toBuilder().regArima(ra2).buildWithoutValidation();
  }

  static void px(StringBuilder out, String[] h) {
    String id = h[1], method = h[2], kind = h[3];
    out.append("case PX ").append(id).append(" method ").append(method).append('\n');
    echoKind(out, kind);
    X13Spec spec = x13Override(method);
    jdplus.x13.base.api.regarima.TradingDaysSpec t
        = spec.getRegArima().getRegression().getTradingDays();
    out.append("x13tdspec ").append(id)
        .append(" method=").append(t.getAutomaticMethod())
        .append(" automatic=").append(t.isAutomatic())
        .append(" p1=").append(g(t.getAutoPvalue1()))
        .append(" p2=").append(g(t.getAutoPvalue2()))
        .append(" adjust=").append(t.isAutoAdjust())
        .append(" rtest=").append(t.getRegressionTestType()).append('\n');
    Observer obs = new Observer();
    jdplus.x13.base.core.x13.X13Results r;
    try {
      r = X13Kernel.of(spec, new ModellingContext()).process(
          ts(kind).cleanExtremities(), obs);
    } catch (Exception ex) {
      out.append("run THROW ").append(ex.getClass().getName()).append(' ')
          .append(String.valueOf(ex.getMessage()).replace('\n', ' ')).append('\n');
      StackTraceElement[] st = ex.getStackTrace();
      for (int i = 0; i < st.length && i < 12; ++i) {
        out.append("fr ").append(i).append(' ').append(st[i]).append('\n');
      }
      dumpEvents(out, obs);
      return;
    }
    out.append("run OK ").append(r == null ? "null" : "set").append('\n');
    dumpEvents(out, obs);
  }

  public static void main(String[] args) throws Exception {
    String[] lines = Files.readAllLines(Path.of(args[0])).toArray(new String[0]);
    System.out.print(run(lines));
  }
}
