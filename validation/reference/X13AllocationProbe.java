// Public-API observer for the pinned X-13 deterministic-effect allocation
// (item 22c decode). Builds a fixed-coefficient X13Spec, runs the pinned
// X13Kernel and re-derives the preadjustment/finals composition through the
// pinned public pieces (deterministicEffect, RegArimaDecomposer,
// backTransform, TsData ops, X11Utility.correctSeries), comparing every
// stored table bit-exactly against the recomposition. No allocation logic is
// implemented here beyond the composition forms transcribed from the pinned
// bytecode; the kernel's own tables are the oracle.
import jdplus.sa.base.api.ComponentType;
import jdplus.sa.base.api.DecompositionMode;
import jdplus.sa.base.core.modelling.RegArimaDecomposer;
import jdplus.toolkit.base.api.arima.SarimaSpec;
import jdplus.toolkit.base.api.data.DoubleSeq;
import jdplus.toolkit.base.api.data.Parameter;
import jdplus.toolkit.base.api.modelling.TransformationType;
import jdplus.toolkit.base.api.processing.DefaultProcessingLog;
import jdplus.toolkit.base.api.timeseries.TsData;
import jdplus.toolkit.base.api.timeseries.TsPeriod;
import jdplus.toolkit.base.api.timeseries.calendars.LengthOfPeriodType;
import jdplus.toolkit.base.api.timeseries.calendars.TradingDaysType;
import jdplus.toolkit.base.api.timeseries.regression.AdditiveOutlier;
import jdplus.toolkit.base.api.timeseries.regression.IOutlier;
import jdplus.toolkit.base.api.timeseries.regression.LevelShift;
import jdplus.toolkit.base.api.timeseries.regression.ModellingContext;
import jdplus.toolkit.base.api.timeseries.regression.ModellingUtility;
import jdplus.toolkit.base.api.timeseries.regression.TsContextVariable;
import jdplus.toolkit.base.api.timeseries.StaticTsDataSupplier;
import jdplus.toolkit.base.api.timeseries.regression.TsDataSuppliers;
import jdplus.toolkit.base.api.timeseries.regression.UserVariable;
import jdplus.toolkit.base.api.timeseries.regression.Variable;
import jdplus.toolkit.base.core.math.matrices.FastMatrix;
import jdplus.toolkit.base.core.modelling.regression.Regression;
import jdplus.x13.base.api.regarima.BasicSpec;
import jdplus.x13.base.api.regarima.EasterSpec;
import jdplus.x13.base.api.regarima.EstimateSpec;
import jdplus.x13.base.api.regarima.MeanSpec;
import jdplus.x13.base.api.regarima.OutlierSpec;
import jdplus.x13.base.api.regarima.RegressionSpec;
import jdplus.x13.base.api.regarima.RegressionTestSpec;
import jdplus.x13.base.api.regarima.RegArimaSpec;
import jdplus.x13.base.api.regarima.TradingDaysSpec;
import jdplus.x13.base.api.regarima.TransformSpec;
import jdplus.x13.base.api.x11.SeasonalFilterOption;
import jdplus.x13.base.api.x11.X11Spec;
import jdplus.x13.base.api.x13.X13Spec;
import jdplus.x13.base.core.x11.X11Utility;
import jdplus.x13.base.core.x13.X13Finals;
import jdplus.x13.base.core.x13.X13Kernel;
import jdplus.x13.base.core.x13.X13Preadjustment;
import jdplus.x13.base.core.x13.X13Results;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.function.Predicate;

public final class X13AllocationProbe {

  private X13AllocationProbe() {
  }

  private static String fmt(double v) {
    return Double.isNaN(v) ? "NaN" : String.format(Locale.ROOT, "%.17g", v);
  }

  private static String fmts(TsData x) {
    if (x == null) {
      return "null";
    }
    StringBuilder sb = new StringBuilder();
    double[] v = x.getValues().toArray();
    for (int i = 0; i < v.length; ++i) {
      if (i != 0) {
        sb.append(';');
      }
      sb.append(fmt(v[i]));
    }
    return sb.toString();
  }

  private static double[] parseValues(String s) {
    String[] parts = s.split(";", -1);
    double[] v = new double[parts.length];
    for (int i = 0; i < parts.length; ++i) {
      v[i] = Double.parseDouble(parts[i].trim());
    }
    return v;
  }

  private static final class Case {
    String id = "case";
    int freq = 4;
    int year = 2000, period = 1; // 1-based position within the year
    double[] values;
    boolean log;
    String mode = "Additive";
    int[] filters;
    int henderson = 5;
    double lower = 1.5, upper = 2.5;
    String mean = "unused";
    String td = "off";
    String easter = "off";
    int easterDur = 2;
    int d = 1, D = 1;
    final List<String> outliers = new ArrayList<>();
    final List<String> users = new ArrayList<>();
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
        case "freq": c.freq = Integer.parseInt(val); break;
        case "start": {
          String[] ap = val.split(":");
          c.year = Integer.parseInt(ap[0]);
          c.period = Integer.parseInt(ap[1]);
          break;
        }
        case "values": c.values = parseValues(val); break;
        case "log": c.log = Boolean.parseBoolean(val); break;
        case "mode": c.mode = val; break;
        case "filters": {
          String[] ap = val.split(",");
          c.filters = new int[ap.length];
          for (int i = 0; i < ap.length; ++i) {
            c.filters[i] = Integer.parseInt(ap[i].trim());
          }
          break;
        }
        case "henderson": c.henderson = Integer.parseInt(val); break;
        case "sigma": {
          String[] ap = val.split(";");
          c.lower = Double.parseDouble(ap[0]);
          c.upper = Double.parseDouble(ap[1]);
          break;
        }
        case "mean": c.mean = val; break;
        case "td": c.td = val; break;
        case "easter": c.easter = val; break;
        case "easterdur": c.easterDur = Integer.parseInt(val); break;
        case "arima": {
          String[] ap = val.split(";");
          c.d = Integer.parseInt(ap[0]);
          c.D = Integer.parseInt(ap[1]);
          break;
        }
        case "outlier": c.outliers.add(val); break;
        case "user": c.users.add(val); break;
        default: throw new IllegalArgumentException("unknown key: " + key);
      }
    }
    return c;
  }

  private static TsPeriod startPeriod(Case c) {
    switch (c.freq) {
      case 1: return TsPeriod.yearly(c.year);
      case 4: return TsPeriod.quarterly(c.year, c.period);
      case 12: return TsPeriod.monthly(c.year, c.period);
      default: throw new IllegalArgumentException("unsupported frequency " + c.freq);
    }
  }

  private static Parameter parameter(String spec) {
    return spec.equals("free") ? Parameter.estimated(0.) : Parameter.fixed(Double.parseDouble(spec));
  }

  private static SeasonalFilterOption filter(int code) {
    switch (code) {
      case 0: return SeasonalFilterOption.Stable;
      case 1: return SeasonalFilterOption.S3X1;
      case 3: return SeasonalFilterOption.S3X3;
      case 5: return SeasonalFilterOption.S3X5;
      case 9: return SeasonalFilterOption.S3X9;
      case 15: return SeasonalFilterOption.S3X15;
      case -1: return SeasonalFilterOption.X11Default;
      default: return SeasonalFilterOption.Msr;
    }
  }

  private static ComponentType effect(String name) {
    return ComponentType.valueOf(name);
  }

  private static String varClass(Variable<?> v) {
    if (ModellingUtility.isOutlier(v)) {
      return "outlier";
    }
    if (ModellingUtility.isDaysRelated(v)) {
      return "days";
    }
    if (ModellingUtility.isMovingHoliday(v)) {
      return "holiday";
    }
    if (ModellingUtility.isUser(v)) {
      return "user";
    }
    return "other";
  }

  // Composition of TsData.add(x, y...): left-associative per row.
  private static TsData addAll(TsData x, TsData... rest) {
    double[] v = x.getValues().toArray();
    for (TsData y : rest) {
      double[] w = y.getValues().toArray();
      for (int i = 0; i < v.length; ++i) {
        v[i] = v[i] + w[i];
      }
    }
    return TsData.of(x.getStart(), DoubleSeq.of(v));
  }

  private static TsData invOp(DecompositionMode m, TsData a, TsData b) {
    double[] x = a.getValues().toArray(), y = b.getValues().toArray();
    double[] v = new double[x.length];
    // invOp branches on isMultiplicative() || mode == PseudoAdditive, and
    // isMultiplicative() is (Multiplicative || LogAdditive).
    boolean mult = m.isMultiplicative() || m == DecompositionMode.PseudoAdditive;
    for (int i = 0; i < v.length; ++i) {
      v[i] = mult ? x[i] * y[i] : x[i] + y[i];
    }
    return TsData.of(a.getStart(), DoubleSeq.of(v));
  }

  private static TsData op(DecompositionMode m, TsData a, TsData b) {
    double[] x = a.getValues().toArray(), y = b.getValues().toArray();
    double[] v = new double[x.length];
    // op branches on the explicit (mode == Multiplicative || mode ==
    // PseudoAdditive) test, so LogAdditive subtracts.
    boolean mult = m == DecompositionMode.Multiplicative || m == DecompositionMode.PseudoAdditive;
    for (int i = 0; i < v.length; ++i) {
      v[i] = mult ? x[i] / y[i] : x[i] - y[i];
    }
    return TsData.of(a.getStart(), DoubleSeq.of(v));
  }

  private static double meanOf(DecompositionMode m) {
    return m.isMultiplicative() || m == DecompositionMode.PseudoAdditive ? 1. : 0.;
  }

  private static TsData correct(TsData x, TsData w, TsData y) {
    return TsData.of(x.getStart(), X11Utility.correctSeries(x.getValues(), w.getValues(), y.getValues()));
  }

  private static TsData correct(TsData x, TsData w, double y) {
    return TsData.of(x.getStart(), X11Utility.correctSeries(x.getValues(), w.getValues(), y));
  }

  public static String run(String[] lines) {
    Case c = parse(lines);
    StringBuilder out = new StringBuilder();
    out.append("case ").append(c.id).append('\n');
    try {
      TsPeriod start = startPeriod(c);
      TsData ts = TsData.of(start, DoubleSeq.of(c.values));

      // Context: the pinned active context (default calendars, factories).
      ModellingContext ctx = ModellingContext.getActiveContext();

      // Regression spec: fixed-coefficient user variables and outliers.
      RegressionSpec.Builder reg = RegressionSpec.builder();
      switch (c.mean) {
        case "unused": reg.mean(MeanSpec.DEFAULT_UNUSED); break;
        case "free": reg.mean(MeanSpec.builder().coefficient(Parameter.estimated(0.)).build()); break;
        default: reg.mean(MeanSpec.builder().coefficient(parameter(c.mean)).build()); break;
      }
      if (c.td.equals("off")) {
        reg.tradingDays(TradingDaysSpec.none());
      } else {
        // td=free | td=fixed:v0,v1,...,v5 (working-days clustering)
        String[] ap = c.td.split(":");
        Parameter[] coefs;
        Parameter lp = Parameter.undefined();
        if (ap[1].equals("free")) {
          coefs = Parameter.make(7);
        } else {
          String[] vs = ap[1].split(",");
          coefs = new Parameter[vs.length];
          for (int i = 0; i < vs.length; ++i) {
            coefs[i] = Parameter.fixed(Double.parseDouble(vs[i]));
          }
        }
        reg.tradingDays(TradingDaysSpec.td(TradingDaysType.TD7, LengthOfPeriodType.None, coefs, lp));
      }
      if (c.easter.equals("off")) {
        reg.easter(EasterSpec.DEFAULT_UNUSED);
      } else {
        reg.easter(EasterSpec.builder().type(EasterSpec.Type.Easter).duration(c.easterDur)
            .test(RegressionTestSpec.None).coefficient(parameter(c.easter)).build());
      }
      for (String o : c.outliers) {
        // code|pos|effect|coef
        String[] ap = o.split("\\|");
        int pos = Integer.parseInt(ap[1]);
        TsPeriod at = start.plus(pos);
        IOutlier core = ap[0].equals("AO") ? new AdditiveOutlier(at.start())
            : ap[0].equals("LS") ? new LevelShift(at.start(), false)
            : throwO("outlier code " + ap[0]);
        Variable<IOutlier> v = Variable.<IOutlier>builder().name(ap[0] + pos).core(core)
            .coefficients(new Parameter[]{parameter(ap[3])})
            .attribute("regeffect", ap[2]).build();
        reg.outlier(v);
      }
      for (String u : c.users) {
        // name|effect|coef
        String[] ap = u.split("\\|");
        String name = c.id + "_" + ap[0];
        jdplus.toolkit.base.api.util.NameManager<TsDataSuppliers> mgrs = ctx.getTsVariableManagers();
        mgrs.unlock();
        TsDataSuppliers sup = mgrs.get("kalos");
        if (sup == null) {
          sup = new TsDataSuppliers();
          mgrs.set("kalos", sup);
        }
        sup.unlock();
        sup.set(name, new StaticTsDataSupplier(TsData.of(start, DoubleSeq.of(parseValues(ap[3])))));
        Variable<TsContextVariable> v = Variable.<TsContextVariable>builder().name(name)
            .core(new TsContextVariable("kalos." + name))
            .coefficients(new Parameter[]{parameter(ap[2])})
            .attribute("regeffect", ap[1]).build();
        reg.userDefinedVariable(v);
      }

      RegArimaSpec ra = RegArimaSpec.builder()
          .usingAutoModel(false)
          .basic(BasicSpec.builder().preprocessing(true).build())
          .transform(TransformSpec.builder().function(c.log ? TransformationType.Log : TransformationType.None)
              .adjust(LengthOfPeriodType.None).outliersCorrection(false).build())
          .regression(reg.build())
          .outliers(OutlierSpec.DEFAULT_DISABLED)
          .arima(SarimaSpec.builder().d(c.d).bd(c.D).buildWithoutValidation())
          .estimate(EstimateSpec.DEFAULT)
          .build();
      SeasonalFilterOption[] fs = new SeasonalFilterOption[c.filters.length];
      for (int i = 0; i < fs.length; ++i) {
        fs[i] = filter(c.filters[i]);
      }
      X11Spec x11 = X11Spec.builder().mode(DecompositionMode.valueOf(c.mode)).seasonal(true)
          .filters(fs).hendersonFilterLength(c.henderson)
          .lowerSigma(c.lower).upperSigma(c.upper)
          .forecastHorizon(0).backcastHorizon(0)
          .build();
      X13Spec spec = X13Spec.builder().regArima(ra).x11(x11).build();

      DefaultProcessingLog log = new DefaultProcessingLog();
      X13Results results = X13Kernel.of(spec, ctx).process(ts, log);
      for (var info : log.all()) {
        out.append("log ").append(String.valueOf(info).replace('\n', ' ')).append('\n');
      }
      if (!results.isValid()) {
        out.append("status invalid\n");
        return out.toString();
      }
      out.append("status ok\n");
      out.append("mode_requested ").append(c.mode).append('\n');

      X13Preadjustment pre = results.getPreadjustment();
      jdplus.x13.base.core.x11.X11Results xres = results.getDecomposition();
      X13Finals fin = results.getFinals();
      out.append("mode_effective ").append(xres.getMode()).append('\n');

      // Captured tables.
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
      out.append("a1a ").append(fmts(pre.getA1a())).append('\n');
      out.append("a1b ").append(fmts(pre.getA1b())).append('\n');
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
      out.append("d11a ").append(fmts(fin.getD11a())).append('\n');

      // Model variables (the given fixed specification): class, effect,
      // coefficient, free flag and the pinned regression columns.
      Object model = results.getPreprocessing();
      jdplus.toolkit.base.core.regsarima.regular.RegSarimaModel m
          = (jdplus.toolkit.base.core.regsarima.regular.RegSarimaModel) model;
      Variable<?>[] vars = m.getDescription().getVariables();
      double[] cursor = m.getEstimation().getCoefficients().toArray();
      int cur = 0;
      for (int k = 0; k < vars.length; ++k) {
        Variable<?> v = vars[k];
        FastMatrix X = Regression.matrix(pre.getA1().getDomain(), v.getCore());
        StringBuilder co = new StringBuilder(), cl = new StringBuilder();
        for (int j = 0; j < X.getColumnsCount(); ++j) {
          Parameter p = v.getCoefficient(j);
          double value;
          if (p.isFree()) {
            value = cursor[cur++];
          } else {
            value = p.getValue();
          }
          if (j != 0) {
            co.append(',');
            cl.append(';');
          }
          co.append(p.isFree() ? "free:" : "fixed:").append(fmt(value));
          double[] col = X.column(j).toArray();
          for (int i = 0; i < col.length; ++i) {
            if (i != 0) {
              cl.append(';');
            }
            cl.append(fmt(col[i]));
          }
        }
        out.append("var ").append(k).append(' ').append(v.getName()).append(' ')
            .append(varClass(v)).append(' ')
            .append(v.attribute("regeffect") == null ? "-" : v.attribute("regeffect")).append(' ')
            .append("outlier=").append(ModellingUtility.isOutlier(v) ? 1 : 0)
            .append(",days=").append(ModellingUtility.isDaysRelated(v) ? 1 : 0)
            .append(",holiday=").append(ModellingUtility.isMovingHoliday(v) ? 1 : 0)
            .append(",user=").append(ModellingUtility.isUser(v) ? 1 : 0).append(' ')
            .append(co).append(' ').append(cl).append('\n');
      }

      // Compositional recomputation from the pinned pieces (the transcribed
      // initialStep/finals forms) compared bit-exactly with the stored tables.
      boolean isLog = m.getDescription().isLogTransformation();
      TsData inv = m.interpolatedSeries(false);
      var dom = inv.getDomain();
      TsData v11 = m.deterministicEffect(dom, ModellingUtility::isMovingHoliday);
      TsData v12 = m.deterministicEffect(dom, ModellingUtility::isDaysRelated);
      TsData v13 = RegArimaDecomposer.deterministicEffect(m, dom, ComponentType.Trend, true, ModellingUtility::isOutlier);
      TsData v14 = RegArimaDecomposer.deterministicEffect(m, dom, ComponentType.Seasonal, true, ModellingUtility::isOutlier);
      TsData v15 = RegArimaDecomposer.deterministicEffect(m, dom, ComponentType.Irregular, true, ModellingUtility::isOutlier);
      TsData v16 = RegArimaDecomposer.deterministicEffect(m, dom, ComponentType.Trend, true, ModellingUtility::isUser);
      TsData v17 = RegArimaDecomposer.deterministicEffect(m, dom, ComponentType.Seasonal, true, ModellingUtility::isUser);
      TsData v18 = RegArimaDecomposer.deterministicEffect(m, dom, ComponentType.Irregular, true, ModellingUtility::isUser);
      TsData v19 = RegArimaDecomposer.deterministicEffect(m, dom, ComponentType.SeasonallyAdjusted, true, ModellingUtility::isUser);
      TsData v20 = RegArimaDecomposer.deterministicEffect(m, dom, ComponentType.Series, true, ModellingUtility::isUser);
      TsData v21 = RegArimaDecomposer.deterministicEffect(m, dom, ComponentType.Undefined, true, ModellingUtility::isUser);
      TsData v22 = RegArimaDecomposer.deterministicEffect(m, dom, ComponentType.CalendarEffect, true, ModellingUtility::isUser);
      v13 = TsData.add(v13, v16);
      v14 = TsData.add(v14, v17);
      v15 = TsData.add(v15, v18);
      TsData v23 = addAll(v13, v14, v15);
      TsData v25 = addAll(v22, v19, v20);
      TsData v26 = addAll(v12, v11, v23, v25);
      check(out, "a1", inv, pre.getA1());
      check(out, "a6", m.backTransform(v12, true), pre.getA6());
      check(out, "a7", m.backTransform(v11, false), pre.getA7());
      check(out, "a8", m.backTransform(v23, false), pre.getA8());
      check(out, "a8t", m.backTransform(v13, false), pre.getA8t());
      check(out, "a8s", m.backTransform(v14, false), pre.getA8s());
      check(out, "a8i", m.backTransform(v15, false), pre.getA8i());
      check(out, "a9", m.backTransform(v25, false), pre.getA9());
      check(out, "a9u", m.backTransform(v21, false), pre.getA9u());
      check(out, "a9cal", m.backTransform(v22, false), pre.getA9cal());
      check(out, "a9sa", m.backTransform(v19, false), pre.getA9sa());
      check(out, "a9ser", m.backTransform(v20, false), pre.getA9ser());
      TsData outv = isLog ? TsData.divide(inv, m.backTransform(v26, true))
          : TsData.subtract(inv, m.backTransform(v26, true));
      check(out, "out", outv, xres.getB1());

      DecompositionMode mode = xres.getMode();
      TsData a1 = pre.getA1(), a6 = pre.getA6(), a7 = pre.getA7();
      TsData a8t = pre.getA8t(), a8i = pre.getA8i(), a8s = pre.getA8s();
      TsData d10 = xres.getD10(), d11 = xres.getD11(), d12 = xres.getD12(), d13 = xres.getD13();
      TsData t21 = invOp(mode, invOp(mode, a6, a7), pre.getA9cal());
      TsData t16 = invOp(mode, invOp(mode, d10, a8s), t21);
      TsData t11 = invOp(mode, invOp(mode, invOp(mode, d11, a8t), a8i), pre.getA9sa());
      TsData t12 = invOp(mode, d12, a8t);
      TsData t13 = invOp(mode, d13, a8i);
      TsData f11 = TsData.fitToDomain(t11, a1.getDomain());
      TsData f12 = TsData.fitToDomain(t12, a1.getDomain());
      TsData f13 = TsData.fitToDomain(t13, a1.getDomain());
      TsData f16 = TsData.fitToDomain(t16, a1.getDomain());
      TsData f18 = TsData.fitToDomain(t21, a1.getDomain());
      TsData v28 = op(mode, a1, a8i);
      TsData v24b = op(mode, f11, a8i);
      TsData w = TsData.fitToDomain(xres.getC17(), a1.getDomain());
      TsData e1 = correct(v28, w, op(mode, a1, f13));
      TsData e2 = correct(v24b, w, d12);
      TsData e3 = correct(TsData.fitToDomain(d13, a1.getDomain()), w, meanOf(mode));
      TsData e11 = correct(v24b, w, invOp(mode, d12, op(mode, v28, e1)));
      check(out, "d11final", f11, fin.getD11final(), diag(m, pre, xres, mode));
      check(out, "d12final", f12, fin.getD12final(), diag(m, pre, xres, mode));
      check(out, "d13final", f13, fin.getD13final(), diag(m, pre, xres, mode));
      check(out, "d16", f16, fin.getD16(), diag(m, pre, xres, mode));
      check(out, "d18", f18, fin.getD18(), diag(m, pre, xres, mode));
      check(out, "e1", e1, fin.getE1(), diag(m, pre, xres, mode));
      check(out, "e2", e2, fin.getE2(), diag(m, pre, xres, mode));
      check(out, "e3", e3, fin.getE3(), diag(m, pre, xres, mode));
      check(out, "e11", e11, fin.getE11(), diag(m, pre, xres, mode));
    } catch (Throwable t) {
      out.append("error ").append(t.getClass().getName()).append(' ')
          .append(String.valueOf(t.getMessage()).replace('\n', ' ')).append('\n');
      for (StackTraceElement e : t.getStackTrace()) {
        out.append("at ").append(e.toString()).append('\n');
      }
    }
    return out.toString();
  }

  private static IOutlier throwO(String msg) {
    throw new IllegalArgumentException(msg);
  }

  private static String v0(TsData x) {
    return x == null || x.getValues().size() == 0 ? "null" : fmt(x.getValues().get(0));
  }

  private static String diag(jdplus.toolkit.base.core.regsarima.regular.RegSarimaModel m,
      X13Preadjustment pre, jdplus.x13.base.core.x11.X11Results xres, DecompositionMode mode) {
    return " diag[" + "mode=" + mode + " a1=" + v0(pre.getA1()) + " a6=" + v0(pre.getA6())
        + " a7=" + v0(pre.getA7()) + " a8t=" + v0(pre.getA8t()) + " a8s=" + v0(pre.getA8s())
        + " a8i=" + v0(pre.getA8i()) + " a9cal=" + v0(pre.getA9cal()) + " a9sa=" + v0(pre.getA9sa())
        + " a9ser=" + v0(pre.getA9ser()) + " a9u=" + v0(pre.getA9u()) + " d10=" + v0(xres.getD10())
        + " d11=" + v0(xres.getD11()) + " d12=" + v0(xres.getD12()) + " d13=" + v0(xres.getD13())
        + " c17=" + v0(xres.getC17()) + " t12v12=" + v0(m.deterministicEffect(
            m.interpolatedSeries(false).getDomain(), ModellingUtility::isDaysRelated))
        + " t13v11=" + v0(m.deterministicEffect(
            m.interpolatedSeries(false).getDomain(), ModellingUtility::isMovingHoliday)) + "]";
  }

  private static void check(StringBuilder out, String name, TsData computed, TsData stored,
      String extra) {
    if (stored == null || computed == null) {
      out.append("check ").append(name).append(computed == stored ? " ok" : " NULLMISMATCH").append(extra).append('\n');
      return;
    }
    double[] a = computed.getValues().toArray(), b = stored.getValues().toArray();
    if (a.length != b.length) {
      out.append("check ").append(name).append(" LENGTH ").append(a.length).append('/').append(b.length).append(extra).append('\n');
      return;
    }
    for (int i = 0; i < a.length; ++i) {
      if (Double.doubleToLongBits(a[i]) != Double.doubleToLongBits(b[i])) {
        out.append("check ").append(name).append(" DIFF ").append(i).append(' ')
            .append(fmt(a[i])).append(' ').append(fmt(b[i])).append(extra).append('\n');
        return;
      }
    }
    out.append("check ").append(name).append(" ok\n");
  }

  private static void check(StringBuilder out, String name, TsData computed, TsData stored) {
    if (stored == null || computed == null) {
      out.append("check ").append(name).append(computed == stored ? " ok" : " NULLMISMATCH").append('\n');
      return;
    }
    double[] a = computed.getValues().toArray(), b = stored.getValues().toArray();
    if (a.length != b.length) {
      out.append("check ").append(name).append(" LENGTH ").append(a.length).append('/').append(b.length).append('\n');
      return;
    }
    for (int i = 0; i < a.length; ++i) {
      if (Double.doubleToLongBits(a[i]) != Double.doubleToLongBits(b[i])) {
        out.append("check ").append(name).append(" DIFF ").append(i).append(' ')
            .append(fmt(a[i])).append(' ').append(fmt(b[i])).append('\n');
        return;
      }
    }
    out.append("check ").append(name).append(" ok\n");
  }
}
