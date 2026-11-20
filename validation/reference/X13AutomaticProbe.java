// Public-API observer for the automatic-spec X-13 pipeline (item 21-H.2).
// Builds an automatic X13Spec from decoder-equivalent facts (the 12.1/12.13
// pinning values, all arriving as input lines), runs the REAL decoder-driven
// pipeline (X13Kernel.of(spec, ctx).process(ts, log) - the same entry point
// rjd3x13 drives, with the product's ProcessingLog replaced by an observer),
// and emits the decision-event stream (every ProcessingLog call, in order,
// with the machine detail objects serialized), the final model description
// (log/mean flags, orders, raw ARMA parameters, variables with the pinned
// ami/regeffect attributes), the landing statistics, and the stored
// preadjustment / X-11 / finals tables. The observer implements the
// ProcessingLog interface only; no pipeline logic is re-implemented here, so
// the event stream is the JVM's own decision trace (the H.2 oracle surface
// for the native x13_automatic_fit decision chain).
import jdplus.sa.base.api.ComponentType;
import jdplus.sa.base.api.DecompositionMode;
import jdplus.toolkit.base.api.arima.SarimaSpec;
import jdplus.toolkit.base.api.data.DoubleSeq;
import jdplus.toolkit.base.api.modelling.TransformationType;
import jdplus.toolkit.base.api.processing.ProcessingLog;
import jdplus.toolkit.base.api.timeseries.TsData;
import jdplus.toolkit.base.api.timeseries.TsPeriod;
import jdplus.toolkit.base.api.timeseries.calendars.LengthOfPeriodType;
import jdplus.toolkit.base.api.timeseries.regression.ModellingContext;
import jdplus.toolkit.base.api.timeseries.regression.Variable;
import jdplus.toolkit.base.core.regsarima.regular.IOutliersDetectionModule;
import jdplus.toolkit.base.core.regsarima.regular.IDifferencingModule;
import jdplus.toolkit.base.core.regsarima.regular.RegSarimaModel;
import jdplus.toolkit.base.core.stats.likelihood.LikelihoodStatistics;
import jdplus.x13.base.api.regarima.AutoModelSpec;
import jdplus.x13.base.api.regarima.BasicSpec;
import jdplus.x13.base.api.regarima.EasterSpec;
import jdplus.x13.base.api.regarima.EstimateSpec;
import jdplus.x13.base.api.regarima.MeanSpec;
import jdplus.x13.base.api.regarima.OutlierSpec;
import jdplus.x13.base.api.regarima.RegressionSpec;
import jdplus.x13.base.api.regarima.RegArimaSpec;
import jdplus.x13.base.api.regarima.SingleOutlierSpec;
import jdplus.x13.base.api.regarima.TradingDaysSpec;
import jdplus.x13.base.api.regarima.TransformSpec;
import jdplus.x13.base.api.x11.X11Spec;
import jdplus.x13.base.api.x13.X13Spec;
import jdplus.x13.base.core.x11.X11Results;
import jdplus.x13.base.core.x13.X13Finals;
import jdplus.x13.base.core.x13.X13Kernel;
import jdplus.x13.base.core.x13.X13Preadjustment;
import jdplus.x13.base.core.x13.X13Results;
import jdplus.x13.base.core.x13.regarima.ArmaModule;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;

public final class X13AutomaticProbe {

  private X13AutomaticProbe() {
  }

  private static String fmt(double v) {
    return Double.isNaN(v) ? "NaN" : String.format(Locale.ROOT, "%.17g", v);
  }

  private static String csv(double[] v) {
    StringBuilder sb = new StringBuilder();
    for (int i = 0; i < v.length; ++i) {
      if (i != 0) {
        sb.append(';');
      }
      sb.append(fmt(v[i]));
    }
    return sb.toString();
  }

  private static String csv(int[] v) {
    StringBuilder sb = new StringBuilder();
    for (int i = 0; i < v.length; ++i) {
      if (i != 0) {
        sb.append(';');
      }
      sb.append(v[i]);
    }
    return sb.toString();
  }

  private static String fmts(TsData x) {
    if (x == null) {
      return "null";
    }
    return csv(x.getValues().toArray());
  }

  private static double[] parseValues(String s) {
    String[] parts = s.split(";", -1);
    double[] v = new double[parts.length];
    for (int i = 0; i < parts.length; ++i) {
      v[i] = Double.parseDouble(parts[i].trim());
    }
    return v;
  }

  // The observer log. Records every callback (kind, nesting depth, message,
  // serialized detail) in call order. The detail payload is serialized per
  // pinned machine-info class; anything else falls back to a deterministic
  // string form (never an identity hash).
  private static final class Observer implements ProcessingLog {
    final StringBuilder sb = new StringBuilder();
    int depth = 0;
    long seq = 0;

    static String detail(Object d) {
      if (d == null) {
        return "-";
      }
      if (d instanceof IOutliersDetectionModule.Info) {
        IOutliersDetectionModule.Info i = (IOutliersDetectionModule.Info) d;
        StringBuilder s = new StringBuilder("odm ");
        s.append(String.join(",", i.getTypes())).append(' ');
        for (int[] o : i.getOutliers()) {
          s.append('[').append(csv(o)).append(']');
        }
        s.append(' ').append(fmt(i.getVa()));
        return s.toString();
      }
      if (d instanceof IDifferencingModule.Info) {
        IDifferencingModule.Info i = (IDifferencingModule.Info) d;
        return "diff d=" + i.getD() + " bd=" + i.getBd() + " mean=" + i.isMean()
            + " tmean=" + fmt(i.getTmean());
      }
      if (d instanceof ArmaModule.CInfo) {
        ArmaModule.CInfo c = (ArmaModule.CInfo) d;
        StringBuilder s = new StringBuilder("arma sel=" + c.getSelection());
        ArmaModule.RegArmaBic[] ms = c.getModels();
        if (ms != null) {
          for (ArmaModule.RegArmaBic m : ms) {
            s.append(" [").append(m.getSpecification()).append(':').append(fmt(m.getBIC())).append(']');
          }
        }
        return s.toString();
      }
      if (d instanceof Double) {
        return "num " + fmt((Double) d);
      }
      if (d instanceof String) {
        return "str " + d;
      }
      if (d instanceof Object[]) {
        return "arr " + Arrays.deepToString((Object[]) d);
      }
      if (d instanceof int[][]) {
        StringBuilder s = new StringBuilder("imtx");
        for (int[] r : (int[][]) d) {
          s.append('[').append(csv(r)).append(']');
        }
        return s.toString();
      }
      return "obj " + String.valueOf(d);
    }

    void emit(String kind, String msg, Object details) {
      ++seq;
      sb.append("evt ").append(seq).append(' ').append(kind).append(' ')
          .append(depth).append(' ')
          .append(msg == null ? "-" : msg.replace('\n', ' ')).append(" det ")
          .append(detail(details)).append('\n');
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

  private static final class Case {
    String id = "case";
    int year = 1990, period = 1;
    double[] values;
    String mode = "Undefined";
    double lower = 1.5, upper = 2.5;
    boolean outliers = true;
    List<String> types = new ArrayList<>(Arrays.asList("AO", "LS", "TC", "SO"));
    int maxIter = 30;
    // All defaults are the pinned spec defaults the decoder path promotes:
    // OutlierSpec DEF_TCRATE .7 / DEF_NMAX 30, AutoModelSpec DEF_CANCEL .1,
    // DEF_UB1/DEF_UB2 (the module stores the JVM reciprocals), DEF_MIXED
    // true / DEF_BALANCED false; TransformSpec DEF_AICDIFF -2; predcv and
    // the ljung limit are DEF_PREDCV/DEF_LJUNGBOX (inert on this path: the
    // decoder wires no such scalar into x13.regarima.ArmaModule).
    double tcrate = 0.7, ub1 = 1.0309278350515465, ub2 = 1.1363636363636365;
    double cancel = 0.1, aicDiff = -2.0, predcv = 0.14286, lbLimit = 0.95;
    boolean mixed = true, balanced = false;
  }

  private static Case parse(String[] lines) {
    Case c = new Case();
    for (String line : lines) {
      int k = line.indexOf(' ');
      if (k < 0) {
        throw new IllegalArgumentException("bad line: " + line);
      }
      String key = line.substring(0, k), val = line.substring(k + 1).trim();
      switch (key) {
        case "id": c.id = val; break;
        case "start": {
          String[] ap = val.split(":");
          c.year = Integer.parseInt(ap[0]);
          c.period = Integer.parseInt(ap[1]);
          break;
        }
        case "values": c.values = parseValues(val); break;
        case "mode": c.mode = val; break;
        case "sigma": {
          String[] ap = val.split(";");
          c.lower = Double.parseDouble(ap[0]);
          c.upper = Double.parseDouble(ap[1]);
          break;
        }
        case "outliers": c.outliers = val.equals("on"); break;
        case "otypes": {
          c.types = new ArrayList<>();
          for (String t : val.split(",")) {
            if (!t.trim().isEmpty()) {
              c.types.add(t.trim());
            }
          }
          break;
        }
        case "maxiter": c.maxIter = Integer.parseInt(val); break;
        case "tcrate": c.tcrate = Double.parseDouble(val); break;
        case "ub1": c.ub1 = Double.parseDouble(val); break;
        case "ub2": c.ub2 = Double.parseDouble(val); break;
        case "cancel": c.cancel = Double.parseDouble(val); break;
        case "aicdiff": c.aicDiff = Double.parseDouble(val); break;
        case "predcv": c.predcv = Double.parseDouble(val); break;
        case "lblimit": c.lbLimit = Double.parseDouble(val); break;
        case "mixed": c.mixed = val.equals("true"); break;
        case "balanced": c.balanced = val.equals("true"); break;
        default: throw new IllegalArgumentException("unknown key: " + key);
      }
    }
    return c;
  }

  private static X13Spec buildSpec(Case c) {
    TransformSpec tr = TransformSpec.builder()
        .function(TransformationType.Auto)
        .adjust(LengthOfPeriodType.None)
        .aicDiff(c.aicDiff)
        .outliersCorrection(false)
        .build();
    RegressionSpec reg = RegressionSpec.builder()
        .mean(MeanSpec.DEFAULT_UNUSED)
        .tradingDays(TradingDaysSpec.none())
        .easter(EasterSpec.DEFAULT_UNUSED)
        .build();
    OutlierSpec.Builder ob = OutlierSpec.builder().clearTypes()
        .lsRun(6).method(OutlierSpec.Method.AddOne)
        .monthlyTCRate(c.tcrate).defaultCriticalValue(0.0)
        .maxIter(c.maxIter);
    if (c.outliers) {
      for (String t : c.types) {
        ob.type(new SingleOutlierSpec(t, 0.));
      }
    }
    AutoModelSpec am = AutoModelSpec.builder()
        .enabled(true)
        .cancel(c.cancel)
        .ub1(c.ub1).ub2(c.ub2)
        .mixed(c.mixed).balanced(c.balanced)
        .hannanRissannen(false)
        .percentRSE(1.0126582278481011)
        .ljungBoxLimit(c.lbLimit)
        .predcv(c.predcv)
        .armaSignificance(1.0)
        .build();
    RegArimaSpec ra = RegArimaSpec.builder()
        .usingAutoModel(true)
        .basic(BasicSpec.builder().preprocessing(true).build())
        .transform(tr)
        .regression(reg)
        .outliers(c.outliers ? ob.build() : OutlierSpec.DEFAULT_DISABLED)
        .autoModel(am)
        .arima(SarimaSpec.builder().build())
        .estimate(EstimateSpec.DEFAULT)
        .build();
    X11Spec x11 = X11Spec.builder()
        .mode(DecompositionMode.valueOf(c.mode))
        .seasonal(true)
        .filters(new jdplus.x13.base.api.x11.SeasonalFilterOption[]{
          jdplus.x13.base.api.x11.SeasonalFilterOption.Msr})
        .crossValidationTables(new jdplus.x13.base.api.x11.CrossValidationTable[]{
          jdplus.x13.base.api.x11.CrossValidationTable.B3,
          jdplus.x13.base.api.x11.CrossValidationTable.B4,
          jdplus.x13.base.api.x11.CrossValidationTable.B8,
          jdplus.x13.base.api.x11.CrossValidationTable.C4,
          jdplus.x13.base.api.x11.CrossValidationTable.C9,
          jdplus.x13.base.api.x11.CrossValidationTable.D4,
          jdplus.x13.base.api.x11.CrossValidationTable.D8,
          jdplus.x13.base.api.x11.CrossValidationTable.D9})
        .crossValidationSelectionTable(jdplus.x13.base.api.x11.CrossValidationTable.B3)
        .crossValidationQualityCriteria(
            jdplus.x13.base.api.x11.CrossValidationQualityCriteria.RMSE)
        .lowerSigma(c.lower).upperSigma(c.upper)
        .calendarSigma(jdplus.x13.base.api.x11.CalendarSigmaOption.None)
        .hendersonFilterLength(0)
        .forecastHorizon(0).backcastHorizon(0)
        .bias(jdplus.x13.base.api.x11.BiasCorrection.Ratio)
        .build();
    return X13Spec.builder().regArima(ra).x11(x11).build();
  }

  public static String run(String[] lines) {
    Case c = parse(lines);
    StringBuilder out = new StringBuilder();
    out.append("case ").append(c.id).append('\n');
    // Configuration echo: every case-level scalar the spec actually held,
    // %.17g, so the corpus file is self-describing for the C++ mirror.
    out.append("req start ").append(c.year).append(':').append(c.period)
        .append(" n ").append(c.values.length)
        .append(" mode ").append(c.mode)
        .append(" sigma ").append(fmt(c.lower)).append(';').append(fmt(c.upper))
        .append(" outliers ").append(c.outliers ? "on" : "off")
        .append(" otypes ").append(String.join(",", c.types))
        .append(" maxiter ").append(c.maxIter)
        .append(" tcrate ").append(fmt(c.tcrate))
        .append(" ub1 ").append(fmt(c.ub1))
        .append(" ub2 ").append(fmt(c.ub2))
        .append(" cancel ").append(fmt(c.cancel))
        .append(" aicdiff ").append(fmt(c.aicDiff))
        .append(" predcv ").append(fmt(c.predcv))
        .append(" lblimit ").append(fmt(c.lbLimit))
        .append(" mixed ").append(c.mixed)
        .append(" balanced ").append(c.balanced)
        .append('\n');
    try {
      TsPeriod start = TsPeriod.monthly(c.year, c.period);
      TsData ts = TsData.of(start, DoubleSeq.of(c.values));
      ModellingContext ctx = ModellingContext.getActiveContext();
      X13Spec spec = buildSpec(c);
      Observer obs = new Observer();
      X13Results results = X13Kernel.of(spec, ctx).process(ts.cleanExtremities(), obs);
      out.append("status ").append(results.isValid() ? "ok" : "invalid").append('\n');
      out.append(obs.sb);
      if (!results.isValid()) {
        return out.toString();
      }
      RegSarimaModel m = (RegSarimaModel) results.getPreprocessing();
      var so = m.specification();
      out.append("log ").append(m.getDescription().isLogTransformation()).append('\n');
      // The mean fact via the public variable list: ModelDescription holds
      // the mean as the "const" variable (first slot when present).
      var vars = m.getDescription().getVariables();
      boolean hasMean = false;
      for (Variable<?> v : vars) {
        if (v.getName().equals("const")) {
          hasMean = true;
        }
      }
      out.append("mean ").append(hasMean).append('\n');
      out.append("orders ").append(so.getP()).append(' ').append(so.getD()).append(' ')
          .append(so.getQ()).append(' ').append(so.getBp()).append(' ')
          .append(so.getBd()).append(' ').append(so.getBq()).append('\n');
      out.append("arma ").append(csv(m.arima().parameters().toArray())).append('\n');
      var est = m.getEstimation();
      out.append("points ").append(csv(est.getParameters().getValues().toArray())).append('\n');
      out.append("regcoef ").append(csv(est.getCoefficients().toArray())).append('\n');
      var st = est.getStatistics();
      out.append("stats nobs=").append(st.getObservationsCount())
          .append(" neff=").append(st.getEffectiveObservationsCount())
          .append(" npar=").append(st.getEstimatedParametersCount())
          .append(" loglik=").append(fmt(st.getLogLikelihood()))
          .append(" aicc=").append(fmt(st.getAICC())).append('\n');
      for (int i = 0; i < vars.length; ++i) {
        Variable<?> v = vars[i];
        out.append("var ").append(i).append(' ').append(v.getName()).append(' ')
            .append(v.attribute("ami") == null ? "-" : v.attribute("ami")).append(' ')
            .append(v.attribute("regeffect") == null ? "-" : v.attribute("regeffect"))
            .append('\n');
      }
      X13Preadjustment pre = results.getPreadjustment();
      X11Results xres = results.getDecomposition();
      X13Finals fin = results.getFinals();
      out.append("mode_effective ").append(xres.getMode()).append('\n');
      out.append("a1 ").append(fmts(pre.getA1())).append('\n');
      out.append("a6 ").append(fmts(pre.getA6())).append('\n');
      out.append("a7 ").append(fmts(pre.getA7())).append('\n');
      out.append("a8 ").append(fmts(pre.getA8())).append('\n');
      out.append("a8t ").append(fmts(pre.getA8t())).append('\n');
      out.append("a8s ").append(fmts(pre.getA8s())).append('\n');
      out.append("a8i ").append(fmts(pre.getA8i())).append('\n');
      out.append("a9 ").append(fmts(pre.getA9())).append('\n');
      out.append("a9u ").append(fmts(pre.getA9u())).append('\n');
      out.append("a9cal ").append(fmts(pre.getA9cal())).append('\n');
      out.append("a9sa ").append(fmts(pre.getA9sa())).append('\n');
      out.append("a9ser ").append(fmts(pre.getA9ser())).append('\n');
      out.append("out ").append(fmts(xres.getB1())).append('\n');
      out.append("d10 ").append(fmts(xres.getD10())).append('\n');
      out.append("d11 ").append(fmts(xres.getD11())).append('\n');
      out.append("d12 ").append(fmts(xres.getD12())).append('\n');
      out.append("d13 ").append(fmts(xres.getD13())).append('\n');
      out.append("c17 ").append(fmts(xres.getC17())).append('\n');
      out.append("d11final ").append(fmts(fin.getD11final())).append('\n');
      out.append("d12final ").append(fmts(fin.getD12final())).append('\n');
      out.append("d13final ").append(fmts(fin.getD13final())).append('\n');
      out.append("d16 ").append(fmts(fin.getD16())).append('\n');
      out.append("d18 ").append(fmts(fin.getD18())).append('\n');
      out.append("e1 ").append(fmts(fin.getE1())).append('\n');
      out.append("e2 ").append(fmts(fin.getE2())).append('\n');
      out.append("e3 ").append(fmts(fin.getE3())).append('\n');
      out.append("e11 ").append(fmts(fin.getE11())).append('\n');
    } catch (Throwable t) {
      out.append("error ").append(t.getClass().getName()).append(' ')
          .append(String.valueOf(t.getMessage()).replace('\n', ' ')).append('\n');
      for (StackTraceElement e : t.getStackTrace()) {
        out.append("at ").append(e.toString()).append('\n');
      }
    }
    return out.toString();
  }
}
