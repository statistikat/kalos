// Public-API observer for the item 23-A tramo_model_builder unit: drives the
// LIVE TRAMO builder of the pinned jdplus 3.9.0 stack
// (jdplus-tramoseats-base-{api,core}-3.9.0.jar) through its public entry
// new TramoModelBuilder(spec, new ModellingContext()).build(ts, log) and
// reports, through getters only, what the built ModelDescription contains.
// No decision rule is re-implemented here: the fact tables are the builder's
// own outputs (raw getter values, %.17g for doubles) and the event stream is
// the JVM's own ProcessingLog trace. Display-form toString()s are never
// echoed (M5 pinning notes 6.1/6.2): enums print their constant names, every
// double is formatted here.
//
// Input protocol: one case = an array of lines (id, series geometry, spec
// lines); the probe echoes the "req" block read back from the constructed
// TramoSpec through raw getters, then the builder facts. Determinism: the
// series is built with StrictMath only; no wall clock, no identity hashes.
import jdplus.toolkit.base.api.arima.SarimaSpec;
import jdplus.toolkit.base.api.data.DoubleSeq;
import jdplus.toolkit.base.api.data.Parameter;
import jdplus.toolkit.base.api.data.ParameterType;
import jdplus.toolkit.base.api.modelling.TransformationType;
import jdplus.toolkit.base.api.processing.ProcessingLog;
import jdplus.toolkit.base.api.timeseries.TsData;
import jdplus.toolkit.base.api.timeseries.TsDomain;
import jdplus.toolkit.base.api.timeseries.TsPeriod;
import jdplus.toolkit.base.api.timeseries.calendars.LengthOfPeriodType;
import jdplus.toolkit.base.api.timeseries.calendars.TradingDaysType;
import jdplus.toolkit.base.api.timeseries.regression.AdditiveOutlier;
import jdplus.toolkit.base.api.timeseries.regression.EasterVariable;
import jdplus.toolkit.base.api.timeseries.regression.GenericTradingDaysVariable;
import jdplus.toolkit.base.api.timeseries.regression.IOutlier;
import jdplus.toolkit.base.api.timeseries.regression.ITsVariable;
import jdplus.toolkit.base.api.timeseries.regression.InterventionVariable;
import jdplus.toolkit.base.api.timeseries.regression.JulianEasterVariable;
import jdplus.toolkit.base.api.timeseries.regression.LengthOfPeriod;
import jdplus.toolkit.base.api.timeseries.regression.LevelShift;
import jdplus.toolkit.base.api.timeseries.regression.ModellingContext;
import jdplus.toolkit.base.api.timeseries.regression.PeriodicOutlier;
import jdplus.toolkit.base.api.timeseries.regression.Ramp;
import jdplus.toolkit.base.api.timeseries.regression.StockTradingDays;
import jdplus.toolkit.base.api.timeseries.regression.TrendConstant;
import jdplus.toolkit.base.api.timeseries.regression.TsContextVariable;
import jdplus.toolkit.base.api.timeseries.regression.Variable;
import jdplus.toolkit.base.core.regsarima.regular.ModelDescription;
import jdplus.tramoseats.base.api.tramo.CalendarSpec;
import jdplus.tramoseats.base.api.tramo.EasterSpec;
import jdplus.tramoseats.base.api.tramo.MeanSpec;
import jdplus.tramoseats.base.api.tramo.RegressionSpec;
import jdplus.tramoseats.base.api.tramo.RegressionTestType;
import jdplus.tramoseats.base.api.tramo.TramoSpec;
import jdplus.tramoseats.base.api.tramo.TradingDaysSpec;
import jdplus.tramoseats.base.api.tramo.TransformSpec;
import java.time.LocalDateTime;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;

public final class TramoModelBuilderProbe {

  private TramoModelBuilderProbe() {
  }

  static String g(double v) {
    return String.format(Locale.ROOT, "%.17g", v);
  }

  // -- input case -------------------------------------------------------------
  // Series recipe: StrictMath only, same shape as the M5-open probe series.
  static double[] baseSeries(int n) {
    double[] y = new double[n];
    for (int t = 0; t < n; ++t) {
      double k = t + 1;
      y[t] = 50 + 0.05 * k + 5 * StrictMath.sin(2 * StrictMath.PI * k / 12)
          + 3 * StrictMath.cos(4 * StrictMath.PI * k / 12)
          + 0.5 * StrictMath.sin(14 * StrictMath.PI * k / 12);
    }
    return y;
  }

  static LocalDateTime at(String spec) { // "yyyy:p"
    String[] sp = spec.split(":");
    return LocalDateTime.of(Integer.parseInt(sp[0]), Integer.parseInt(sp[1]), 1, 0, 0);
  }

  static Parameter param(String tok) { // "F:v" fixed, "U" undefined, "R" free
    if (tok.equals("U")) {
      return Parameter.of(0.0, ParameterType.Undefined);
    }
    if (tok.equals("R")) {
      return Parameter.of(0.0, ParameterType.Estimated);
    }
    return Parameter.of(Double.parseDouble(tok.substring(2)), ParameterType.Fixed);
  }

  // ProcessingLog observer (interface implementation only). Detail objects are
  // formatted here, %.17g for doubles; nothing relies on a toString of a
  // decision Info (identity-hash trap, pinning notes 6.1).
  private static final class Observer implements ProcessingLog {
    final StringBuilder sb = new StringBuilder();
    int depth = 0;
    long seq = 0;

    static String detail(Object d) {
      if (d == null) {
        return "-";
      }
      if (d instanceof Double) {
        return "num " + g((Double) d);
      }
      if (d instanceof double[]) {
        StringBuilder s = new StringBuilder("d[");
        double[] a = (double[]) d;
        for (int i = 0; i < a.length; ++i) {
          if (i > 0) {
            s.append(',');
          }
          s.append(g(a[i]));
        }
        return s.append(']').toString();
      }
      if (d instanceof Object[]) {
        StringBuilder s = new StringBuilder("arr[");
        Object[] a = (Object[]) d;
        for (int i = 0; i < a.length; ++i) {
          if (i > 0) {
            s.append(',');
          }
          s.append(detail(a[i]));
        }
        return s.append(']').toString();
      }
      return "obj " + String.valueOf(d);
    }

    void emit(String kind, String src, String msg, Object details) {
      ++seq;
      String text = (src == null || src.isEmpty()) ? msg : src + " :: " + msg;
      sb.append("evt ").append(seq).append(' ').append(kind).append(' ')
          .append(depth).append(' ')
          .append(text == null ? "-" : text.replace('\n', ' ')).append(" det ")
          .append(detail(details)).append('\n');
    }

    public void push(String s) {
      emit("push", null, s, null);
      ++depth;
    }

    public void pop() {
      if (depth > 0) {
        --depth;
      }
      emit("pop", null, null, null);
    }

    public void error(String m, String o, Object d) {
      emit("error", m, o, d);
    }

    public void warning(String m, String o, Object d) {
      emit("warn", m, o, d);
    }

    public void remark(String m, String o, Object d) {
      emit("remark", m, o, d);
    }

    public void info(String m, String o, Object d) {
      emit("info", m, o, d);
    }

    public void step(String m, Object d) {
      emit("step", null, m, d);
    }

    public boolean isVerbose() {
      return true;
    }
  }

  // -- raw getter formatting (never a toString) --------------------------------
  static String fmtParam(Parameter p) {
    if (p == null) {
      return "null";
    }
    if (p.isFixed()) {
      return "F:" + g(p.getValue());
    }
    if (p.isFree()) {
      return "R:" + g(p.getValue());
    }
    return "U";
  }

  static String fmtParams(Parameter[] ps) {
    if (ps == null || ps.length == 0) {
      return "-";
    }
    StringBuilder s = new StringBuilder();
    for (int i = 0; i < ps.length; ++i) {
      if (i > 0) {
        s.append(';');
      }
      s.append(fmtParam(ps[i]));
    }
    return s.toString();
  }

  static String fmtYm(LocalDateTime t) {
    return t == null ? "-" : t.getYear() + ":" + t.getMonthValue();
  }

  static String fmtSarima(SarimaSpec a) {
    return "p" + a.getP() + "d" + a.getD() + "q" + a.getQ()
        + "P" + a.getBp() + "D" + a.getBd() + "Q" + a.getBq()
        + " per" + a.getPeriod()
        + " und" + yn(a.isUndefined()) + " air" + yn(a.isAirline())
        + " phi[" + fmtParams(a.getPhi()) + "] theta[" + fmtParams(a.getTheta())
        + "] bphi[" + fmtParams(a.getBphi()) + "] btheta[" + fmtParams(a.getBtheta()) + "]";
  }

  static String yn(boolean b) {
    return b ? "y" : "n";
  }

  static String fmtDomain(TsDomain d) {
    return d.getStartPeriod().year() + ":" + d.getStartPeriod().annualPosition()
        + ":" + d.getAnnualFrequency() + ":" + d.getLength();
  }

  static String coreInfo(ITsVariable c) {
    if (c instanceof TrendConstant) {
      TrendConstant t = (TrendConstant) c;
      return "d" + t.getD() + " bd" + t.getBd() + " ref" + fmtYm(t.getReference());
    }
    if (c instanceof IOutlier) {
      IOutlier o = (IOutlier) c;
      String extra = c instanceof PeriodicOutlier
          ? " per" + ((PeriodicOutlier) c).getPeriod() : "";
      return "code" + o.getCode() + " pos" + fmtYm(o.getPosition()) + extra;
    }
    if (c instanceof Ramp) {
      Ramp r = (Ramp) c;
      return "start" + fmtYm(r.getStart()) + " end" + fmtYm(r.getEnd());
    }
    if (c instanceof InterventionVariable) {
      InterventionVariable iv = (InterventionVariable) c;
      StringBuilder s = new StringBuilder("delta").append(g(iv.getDelta()))
          .append(" dseason").append(g(iv.getDeltaSeasonal())).append(" seq[");
      for (int i = 0; i < iv.getSequences().size(); ++i) {
        if (i > 0) {
          s.append(',');
        }
        s.append(fmtYm(iv.getSequences().get(i).start())).append('-')
            .append(fmtYm(iv.getSequences().get(i).end()));
      }
      return s.append(']').toString();
    }
    if (c instanceof GenericTradingDaysVariable) {
      GenericTradingDaysVariable td = (GenericTradingDaysVariable) c;
      return "type" + td.getClustering().getType()
          + " groups" + td.getClustering().getGroupsCount();
    }
    if (c instanceof StockTradingDays) {
      return "w" + ((StockTradingDays) c).getW();
    }
    if (c instanceof LengthOfPeriod) {
      return "lop" + ((LengthOfPeriod) c).getType();
    }
    if (c instanceof JulianEasterVariable) {
      JulianEasterVariable e = (JulianEasterVariable) c;
      return "dur" + e.getDuration() + " gregorian" + yn(e.isGregorianDates());
    }
    if (c instanceof EasterVariable) {
      EasterVariable e = (EasterVariable) c;
      return "dur" + e.getDuration() + " end" + e.getEndPosition()
          + " mean" + e.getMeanCorrection();
    }
    if (c instanceof TsContextVariable) {
      return "ctxname" + ((TsContextVariable) c).getId();
    }
    return "class " + c.getClass().getName();
  }

  // -- the case driver ----------------------------------------------------------
  public static String run(String[] lines) {
    StringBuilder out = new StringBuilder();
    String id = "?";
    int freq = 12, year = 1990, per = 1, n = 96;
    List<int[]> nans = new ArrayList<>();
    String template = null;
    TramoSpec.Builder b = null;
    // Deferred regression parts (built after the base template is read).
    boolean haveMean = false;
    MeanSpec mean = null;
    boolean haveCalendar = false;
    CalendarSpec calendar = null;
    TradingDaysSpec tdSpec = null;
    EasterSpec easterSpec = null;
    List<Variable<IOutlier>> outliers = new ArrayList<>();
    List<Variable<TsContextVariable>> users = new ArrayList<>();
    List<Variable<Ramp>> ramps = new ArrayList<>();
    List<Variable<InterventionVariable>> ivars = new ArrayList<>();
    TransformSpec transform = null;
    for (String line0 : lines) {
      String line = line0.trim();
      if (line.isEmpty()) {
        continue;
      }
      String[] t = line.split("\\s+");
      switch (t[0]) {
        case "id":
          id = t[1];
          break;
        case "freq":
          freq = Integer.parseInt(t[1]);
          break;
        case "start":
          year = Integer.parseInt(t[1]);
          per = Integer.parseInt(t[2]);
          break;
        case "n":
          n = Integer.parseInt(t[1]);
          break;
        case "nan":
          nans.add(new int[]{Integer.parseInt(t[1]), Integer.parseInt(t[2])});
          break;
        case "template":
          template = t[1];
          b = TramoSpec.fromString(template).toBuilder();
          break;
        case "automodel":
          b.usingAutoModel(t[1].equals("on"));
          break;
        case "arima": { // arima p d q bp bd bq [phi..., theta..., bphi..., btheta...]
          int p = Integer.parseInt(t[1]), d = Integer.parseInt(t[2]),
              q = Integer.parseInt(t[3]), bp = Integer.parseInt(t[4]),
              bd = Integer.parseInt(t[5]), bq = Integer.parseInt(t[6]);
          int k = 7;
          Parameter[] phi = new Parameter[p];
          for (int i = 0; i < p; ++i) {
            phi[i] = param(t[k++]);
          }
          Parameter[] th = new Parameter[q];
          for (int i = 0; i < q; ++i) {
            th[i] = param(t[k++]);
          }
          Parameter[] bphi = new Parameter[bp];
          for (int i = 0; i < bp; ++i) {
            bphi[i] = param(t[k++]);
          }
          Parameter[] bth = new Parameter[bq];
          for (int i = 0; i < bq; ++i) {
            bth[i] = param(t[k++]);
          }
          b.arima(SarimaSpec.builder().p(p).d(d).q(q).bp(bp).bd(bd).bq(bq)
              .phi(phi).theta(th).bphi(bphi).btheta(bth)
              .buildWithoutValidation());
          break;
        }
        case "function":
          transform = (transform == null ? TransformSpec.builder() : transform.toBuilder())
              .function(TransformationType.valueOf(t[1])).buildWithoutValidation();
          b.transform(transform);
          break;
        case "adjust":
          transform = (transform == null ? TransformSpec.builder() : transform.toBuilder())
              .adjust(LengthOfPeriodType.valueOf(t[1])).buildWithoutValidation();
          b.transform(transform);
          break;
        case "mean":
          if (t[1].equals("none")) {
            mean = MeanSpec.none();
          } else if (t[1].equals("used")) {
            mean = MeanSpec.mean(Parameter.of(0.0, ParameterType.Undefined));
          } else { // fixed:<v>
            mean = MeanSpec.mean(Parameter.of(
                Double.parseDouble(t[1].substring("fixed:".length())),
                ParameterType.Fixed));
          }
          haveMean = true;
          break;
        case "td": // td <TD2|TD7> <lop> <testtype Separate_T|None>
          tdSpec = TradingDaysSpec.td(TradingDaysType.valueOf(t[1]),
              LengthOfPeriodType.valueOf(t[2]), RegressionTestType.valueOf(t[3]), false);
          haveCalendar = true;
          break;
        case "tdauto": // tdauto <lop> <FTEST> <pftd double>
          tdSpec = TradingDaysSpec.automatic(LengthOfPeriodType.valueOf(t[1]),
              TradingDaysSpec.AutoMethod.valueOf(t[2]),
              Double.parseDouble(t[3]), true);
          haveCalendar = true;
          break;
        case "tdstock": // tdstock <k> <testtype>
          tdSpec = TradingDaysSpec.stockTradingDays(Integer.parseInt(t[1]),
              RegressionTestType.valueOf(t[2]));
          haveCalendar = true;
          break;
        case "easter": // easter <Type> <test 0|1> <dur> <julian 0|1>
          easterSpec = EasterSpec.builder()
              .type(EasterSpec.Type.valueOf(t[1]))
              .test(t[2].equals("1"))
              .duration(Integer.parseInt(t[3]))
              .julian(t[4].equals("1"))
              .buildWithoutValidation();
          haveCalendar = true;
          break;
        case "outlier": { // outlier <code> <yyyy:p> [regeffect=<Val>]
          String[] rest = Arrays.copyOfRange(t, 3, t.length);
          Variable.Builder<ITsVariable> vb = Variable.builder()
              .name(t[2] + "." + t[1] + "." + t[2].replace(':', 'M'))
              .core(outlierCore(t[1], at(t[2])));
          for (String r : rest) {
            if (r.startsWith("regeffect=")) {
              vb = vb.attribute("regeffect", r.substring("regeffect=".length()));
            }
          }
          @SuppressWarnings("unchecked")
          Variable<IOutlier> v = (Variable<IOutlier>) (Variable<?>) vb.build();
          outliers.add(v);
          break;
        }
        case "user":
          users.add(Variable.<TsContextVariable>builder().name(t[1])
              .core(new TsContextVariable(t[1])).build());
          break;
        case "ramp":
          ramps.add(Variable.<Ramp>builder().name("RAMP")
              .core(new Ramp(at(t[1]), at(t[2]))).build());
          break;
        case "intervention":
          ivars.add(Variable.<InterventionVariable>builder().name("IV")
              .core(InterventionVariable.builder()
                  .sequence(jdplus.toolkit.base.api.data.Range.of(at(t[1]), at(t[2])))
                  .build())
              .build());
          break;
        default:
          throw new IllegalArgumentException("unknown line: " + line);
      }
    }

    RegressionSpec.Builder rb = TramoSpec.fromString(
        template == null ? "TR0" : template).getRegression().toBuilder();
    if (haveCalendar) {
      CalendarSpec.Builder cb = CalendarSpec.builder();
      if (tdSpec != null) {
        cb.tradingDays(tdSpec);
      }
      if (easterSpec != null) {
        cb.easter(easterSpec);
      }
      rb.calendar(cb.buildWithoutValidation());
    }
    if (haveMean) {
      rb.mean(mean);
    }
    if (!outliers.isEmpty()) {
      rb.outliers(outliers);
    }
    if (!users.isEmpty()) {
      rb.userDefinedVariables(users);
    }
    if (!ramps.isEmpty()) {
      rb.ramps(ramps);
    }
    if (!ivars.isEmpty()) {
      rb.interventionVariables(ivars);
    }
    b.regression(rb.buildWithoutValidation());
    TramoSpec spec = b.buildWithoutValidation();

    double[] y = baseSeries(n);
    for (int[] rg : nans) {
      for (int i = rg[0]; i < rg[0] + rg[1] && i < n; ++i) {
        y[i] = Double.NaN;
      }
    }

    out.append("case ").append(id).append('\n');
    for (String line0 : lines) {
      String line = line0.trim();
      if (line.isEmpty() || line.startsWith("id ")) {
        continue;
      }
      out.append("src ").append(line).append('\n');
    }
    out.append("req template ").append(template).append('\n');
    out.append("req auto ").append(yn(spec.isUsingAutoModel())).append('\n');
    out.append("req arima ").append(fmtSarima(spec.getArima())).append('\n');
    out.append("req function ").append(spec.getTransform().getFunction())
        .append(" adjust ").append(spec.getTransform().getAdjust())
        .append(" tspan all=").append(yn(spec.getTransform().getSpan().isAll())).append('\n');
    out.append("req espan all=").append(yn(spec.getEstimate().getSpan().isAll())).append('\n');
    MeanSpec ms = spec.getRegression().getMean();
    out.append("req mean used=").append(yn(ms.isUsed()))
        .append(" tconst=").append(yn(ms.isTrendConstant()))
        .append(" test=").append(yn(ms.isTest()))
        .append(" coef ").append(fmtParam(ms.getCoefficient())).append('\n');
    out.append("req ospec ao=").append(yn(spec.getOutliers().isAo()))
        .append(" ls=").append(yn(spec.getOutliers().isLs()))
        .append(" tc=").append(yn(spec.getOutliers().isTc()))
        .append(" so=").append(yn(spec.getOutliers().isSo()))
        .append(" deltatc ").append(g(spec.getOutliers().getDeltaTC())).append('\n');
    TradingDaysSpec tds = spec.getRegression().getCalendar().getTradingDays();
    out.append("req td used=").append(yn(tds.isUsed()))
        .append(" test=").append(yn(tds.isTest()))
        .append(" auto=").append(yn(tds.isAutomatic()))
        .append(" stock=").append(yn(tds.isStockTradingDays()))
        .append(" type ").append(tds.getTradingDaysType())
        .append(" autoM ").append(tds.getAutomaticMethod())
        .append(" stockk ").append(tds.getStockTradingDays())
        .append(" lop ").append(tds.getLengthOfPeriodType()).append('\n');
    EasterSpec easter = spec.getRegression().getCalendar().getEaster();
    out.append("req easter used=").append(yn(easter.isUsed()))
        .append(" test=").append(yn(easter.isTest()))
        .append(" type ").append(easter.getType())
        .append(" dur ").append(easter.getDuration())
        .append(" julian ").append(yn(easter.isJulian())).append('\n');
    out.append("req counts outliers ").append(spec.getRegression().getOutliers().size())
        .append(" users ").append(spec.getRegression().getUserDefinedVariables().size())
        .append(" ramps ").append(spec.getRegression().getRamps().size())
        .append(" ivars ").append(spec.getRegression().getInterventionVariables().size())
        .append(" regused ").append(yn(spec.getRegression().isUsed())).append('\n');
    StringBuilder vals = new StringBuilder("values ");
    for (int i = 0; i < n; ++i) {
      if (i > 0) {
        vals.append(';');
      }
      vals.append(Double.isNaN(y[i]) ? "nan" : g(y[i]));
    }
    out.append(vals).append('\n');

    TsPeriod start;
    if (freq == 12) {
      start = TsPeriod.monthly(year, per);
    } else if (freq == 4) {
      start = TsPeriod.quarterly(year, per);
    } else {
      start = TsPeriod.yearly(year);
    }
    TsData ts = TsData.of(start, DoubleSeq.of(y));

    Observer log = new Observer();
    try {
      // TramoModelBuilder is a package-private CLASS with a public
      // constructor and public build(TsData, ProcessingLog) (roadmap 23-A);
      // reaching it from outside the package needs reflection with
      // setAccessible — no instrumentation, same public entry point.
      Class<?> builderClass =
          Class.forName("jdplus.tramoseats.base.core.tramo.TramoModelBuilder");
      java.lang.reflect.Constructor<?> ctor =
          builderClass.getConstructor(TramoSpec.class, ModellingContext.class);
      ctor.setAccessible(true);
      Object builder = ctor.newInstance(spec, new ModellingContext());
      java.lang.reflect.Method buildMethod =
          builderClass.getMethod("build", TsData.class, ProcessingLog.class);
      buildMethod.setAccessible(true);
      ModelDescription desc;
      try {
        desc = (ModelDescription) buildMethod.invoke(builder, ts, log);
      } catch (java.lang.reflect.InvocationTargetException ite) {
        throw (Exception) ite.getCause();
      }
      out.append("status ok\n");
      out.append("afreq ").append(desc.getAnnualFrequency()).append('\n');
      out.append("domain ").append(fmtDomain(desc.getDomain())).append('\n');
      out.append("estdomain ").append(fmtDomain(desc.getEstimationDomain())).append('\n');
      int[] missing = desc.getMissing();
      out.append("missing ");
      if (missing.length == 0) {
        out.append("none");
      } else {
        DoubleSeq interp = desc.getInterpolatedSeries().getValues();
        StringBuilder s = new StringBuilder();
        StringBuilder f = new StringBuilder();
        for (int i = 0; i < missing.length; ++i) {
          if (i > 0) {
            s.append(';');
            f.append(';');
          }
          s.append(missing[i]);
          f.append(missing[i]).append('=').append(g(interp.get(missing[i])));
        }
        out.append(s);
        out.append("\ninterp ").append(f);
      }
      out.append('\n');
      out.append("log ").append(yn(desc.isLogTransformation()))
          .append(" preadj ").append(desc.getPreadjustment())
          .append(" mean ").append(yn(desc.isMean())).append('\n');
      out.append("arima ").append(fmtSarima(desc.getArimaSpec())).append('\n');
      List<Variable> vars = new ArrayList<>();
      desc.variables().forEach(vars::add);
      out.append("nvar ").append(vars.size()).append('\n');
      for (int i = 0; i < vars.size(); ++i) {
        Variable<?> v = vars.get(i);
        ITsVariable c = v.getCore();
        out.append("var ").append(i)
            .append(" name=").append(v.getName())
            .append(" kind=").append(coreClass(c))
            .append(" dim=").append(v.dim())
            .append(" regeffect=").append(v.hasAttribute("regeffect") ? v.attribute("regeffect") : "-")
            .append(" ami=").append(yn(v.hasAttribute("ami")))
            .append(" coef=").append(fmtParams(v.getCoefficients()))
            .append(" info=").append(coreInfo(c)).append('\n');
      }
    } catch (Exception e) {
      out.append("status error msg ").append(e.getClass().getName())
          .append(' ').append(String.valueOf(e.getMessage())).append('\n');
    }
    out.append(log.sb);
    return out.toString();
  }

  static ITsVariable outlierCore(String code, LocalDateTime pos) {
    switch (code) {
      case "AO":
        return new AdditiveOutlier(pos);
      case "LS":
        return new LevelShift(pos, true);
      case "TC":
        return new jdplus.toolkit.base.api.timeseries.regression.TransitoryChange(pos, 0.7);
      case "SO":
        return new PeriodicOutlier(pos, 12, true);
      default: // unknown codes travel on an IOutlier core carrying the odd
        // code; the builder reads only getCode()/getPosition() of the core,
        // finds no factory for the code and skips the registration.
        return new IOutlier() {
          @Override
          public String getCode() {
            return code;
          }

          @Override
          public LocalDateTime getPosition() {
            return pos;
          }
        };
    }
  }

  static String coreClass(ITsVariable c) {
    if (c instanceof TrendConstant) {
      return "TrendConstant";
    }
    if (c instanceof AdditiveOutlier) {
      return "AdditiveOutlier";
    }
    if (c instanceof LevelShift) {
      return "LevelShift";
    }
    if (c instanceof jdplus.toolkit.base.api.timeseries.regression.TransitoryChange) {
      return "TransitoryChange";
    }
    if (c instanceof PeriodicOutlier) {
      return "PeriodicOutlier";
    }
    if (c instanceof GenericTradingDaysVariable) {
      return "GenericTradingDaysVariable";
    }
    if (c instanceof StockTradingDays) {
      return "StockTradingDays";
    }
    if (c instanceof LengthOfPeriod) {
      return "LengthOfPeriod";
    }
    if (c instanceof JulianEasterVariable) {
      return "JulianEasterVariable";
    }
    if (c instanceof EasterVariable) {
      return "EasterVariable";
    }
    if (c instanceof TsContextVariable) {
      return "TsContextVariable";
    }
    if (c instanceof Ramp) {
      return "Ramp";
    }
    if (c instanceof InterventionVariable) {
      return "InterventionVariable";
    }
    return c.getClass().getSimpleName();
  }
}
