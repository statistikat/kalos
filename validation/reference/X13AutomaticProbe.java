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
import jdplus.toolkit.base.core.regsarima.regular.IAutoModellingModule;
import jdplus.toolkit.base.core.regsarima.regular.IDifferencingModule;
import jdplus.toolkit.base.core.regsarima.regular.ModelDescription;
import jdplus.toolkit.base.core.regsarima.regular.ProcessingResult;
import jdplus.toolkit.base.core.regsarima.regular.RegSarimaModelling;
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

  // ---- 21-H.3 measurement wrappers ------------------------------------------
  // The two RegArimaKernel module fields the decision trace cannot reach are
  // instrumented WITHOUT changing pipeline behavior: each wrapper measures on
  // a private deep copy, then delegates to the decoder-built module untouched.
  // No decision rule is re-implemented; every measured quantity is produced
  // by the pinned machinery itself (the module's own private make(...) + the
  // detector it returns, RegArimaUtility.linearizedData at the module's own
  // entry state). The measurement lines are emitted as extra fact lines
  // ("odmscan", "refinestate"); the event stream stays byte-identical.

  // OutliersDetectionModule: after running a fresh ExactOutliersDetector (the
  // module's own private make on a snapshot of the entry description), report
  // the exit scan's max t (SingleOutlierDetector.getMaxTStat - the JVM's best
  // candidate |t| at stop, the quantity that decides the |t| > cv gate), the
  // detector's own changed/round/picks (a self-check against the Info event),
  // and the entry-state echo (log/mean/spec, estimation presence, parameters,
  // regression coefficients) that defines the whitened input state.
  private static final class OdmInstrumented implements IOutliersDetectionModule {
    private final IOutliersDetectionModule real;
    private final Observer obs;
    private int idx = 0;

    OdmInstrumented(IOutliersDetectionModule real, Observer obs) {
      this.real = real;
      this.obs = obs;
    }

    public ProcessingResult process(RegSarimaModelling m, double cv) {
      StringBuilder sb = new StringBuilder();
      sb.append("odmscan ").append(idx++);
      try {
        ModelDescription desc = m.getDescription();
        var so = desc.specification();
        sb.append(" spec ").append(so.getP()).append(' ').append(so.getD())
            .append(' ').append(so.getQ()).append(' ').append(so.getBp())
            .append(' ').append(so.getBd()).append(' ').append(so.getBq())
            .append(" log ").append(desc.isLogTransformation() ? 1 : 0)
            .append(" mean ").append(desc.isMean() ? 1 : 0)
            .append(" needest ").append(m.needEstimation() ? 1 : 0);
        var est = m.getEstimation();
        sb.append(" pars ");
        if (est == null) {
          sb.append('-');
        } else {
          sb.append(csv(est.getModel().arima().parameters().toArray()));
        }
        sb.append(" coef ");
        if (est == null) {
          sb.append('-');
        } else {
          sb.append(csv(est.getConcentratedLikelihood().coefficients().toArray()));
        }
        sb.append(" cv ").append(fmt(cv));
        ModelDescription snap = ModelDescription.copyOf(desc);
        java.lang.reflect.Method make = real.getClass().getDeclaredMethod(
            "make", ModelDescription.class, double.class);
        make.setAccessible(true);
        jdplus.toolkit.base.core.regsarima.ami.ExactOutliersDetector det
            = (jdplus.toolkit.base.core.regsarima.ami.ExactOutliersDetector)
                make.invoke(real, snap, cv);
        boolean changed = det.process(snap.regarima(), snap.mapping());
        java.lang.reflect.Field sodF = det.getClass().getDeclaredField("sod");
        sodF.setAccessible(true);
        Object sod = sodF.get(det);
        java.lang.reflect.Method mts = sod.getClass().getMethod("getMaxTStat");
        mts.setAccessible(true);
        double tmax = (Double) mts.invoke(sod);
        java.lang.reflect.Field roundF = det.getClass().getDeclaredField("round");
        roundF.setAccessible(true);
        int argp = (Integer) sod.getClass().getMethod("getMaxOutlierPosition").invoke(sod);
        int argt = (Integer) sod.getClass().getMethod("getMaxOutlierType").invoke(sod);
        sb.append(" t ").append(fmt(tmax))
            .append(" argmax ").append(argp).append(';').append(argt)
            .append(" changed ").append(changed ? 1 : 0)
            .append(" rounds ").append(roundF.getInt(det))
            .append(" picks");
        for (int[] o : det.getOutliers()) {
          sb.append('[').append(o[0]).append(';').append(o[1]).append(']');
        }
        // Whitened scan state of the replayed detector (ExactSingleOutlier-
        // Detector internals): the differenced/Ansley-filtered response, the
        // whitened design and QR state, the candidate numerators - every
        // input of the per-cell t arithmetic. Facts only; the pipeline never
        // sees these objects (the snapshot is discarded after measuring).
        StringBuilder wb = new StringBuilder();
        wb.append("odmw ").append(idx - 1);
        wb.append(" yl ").append(dblField(sod, "yl"));
        wb.append(" b ").append(dblField(sod, "b"));
        wb.append(" z ").append(dblField(sod, "z"));
        wb.append(" piv ").append(intField(sod, "pivot"));
        wb.append(" xl ").append(matField(sod, "Xl"));
        wb.append(" u ").append(matField(sod, "U"));
        obs.emitFact(wb.toString());
      } catch (Throwable t) {
        Throwable c = t.getCause() != null ? t.getCause() : t;
        sb.append(" error ").append(c.getClass().getName());
      }
      obs.emitFact(sb.toString());
      ProcessingResult res = real.process(m, cv);
      // Exit-state re-measure: the module may have estimated internally
      // (needest=1 on first entry), so the decision scan the JVM actually
      // stopped on is only observable after the delegate. Rebuilding the
      // detector from the exit description re-runs exactly the scan whose
      // argmax |t| < cv terminates the module - the quantity the native
      // round-0 accept/reject decision is qualified against.
      StringBuilder pb = new StringBuilder();
      pb.append("odmscanpost ").append(idx - 1);
      try {
        ModelDescription desc = m.getDescription();
        var so = desc.specification();
        pb.append(" spec ").append(so.getP()).append(' ').append(so.getD())
            .append(' ').append(so.getQ()).append(' ').append(so.getBp())
            .append(' ').append(so.getBd()).append(' ').append(so.getBq())
            .append(" log ").append(desc.isLogTransformation() ? 1 : 0)
            .append(" mean ").append(desc.isMean() ? 1 : 0)
            .append(" needest ").append(m.needEstimation() ? 1 : 0);
        var est = m.getEstimation();
        pb.append(" pars ");
        if (est == null) {
          pb.append('-');
        } else {
          pb.append(csv(est.getModel().arima().parameters().toArray()));
        }
        pb.append(" cv ").append(fmt(cv));
        ModelDescription snap = ModelDescription.copyOf(desc);
        java.lang.reflect.Method make = real.getClass().getDeclaredMethod(
            "make", ModelDescription.class, double.class);
        make.setAccessible(true);
        jdplus.toolkit.base.core.regsarima.ami.ExactOutliersDetector det
            = (jdplus.toolkit.base.core.regsarima.ami.ExactOutliersDetector)
                make.invoke(real, snap, cv);
        boolean changed = det.process(snap.regarima(), snap.mapping());
        java.lang.reflect.Field sodF = det.getClass().getDeclaredField("sod");
        sodF.setAccessible(true);
        Object sod = sodF.get(det);
        double tmax = (Double) sod.getClass().getMethod("getMaxTStat").invoke(sod);
        int argp = (Integer) sod.getClass().getMethod("getMaxOutlierPosition").invoke(sod);
        int argt = (Integer) sod.getClass().getMethod("getMaxOutlierType").invoke(sod);
        pb.append(" t ").append(fmt(tmax))
            .append(" argmax ").append(argp).append(';').append(argt)
            .append(" changed ").append(changed ? 1 : 0)
            .append(" picks");
        for (int[] o : det.getOutliers()) {
          pb.append('[').append(o[0]).append(';').append(o[1]).append(']');
        }
      } catch (Throwable t) {
        Throwable c = t.getCause() != null ? t.getCause() : t;
        pb.append(" error ").append(c.getClass().getName());
      }
      obs.emitFact(pb.toString());
      return res;
    }
  }

  // AutoModellingModule: report the entry state (log/mean/spec, estimation
  // presence, parameters, regression coefficients) and the sequence the
  // module itself feeds to its differencing machine - the pinned
  // RegArimaUtility.linearizedData(model, cl) at the entry estimation -
  // so the linearization input of every refine round is oracle data.
  private static final class RefineInstrumented implements IAutoModellingModule {
    private final IAutoModellingModule real;
    private final Observer obs;
    private int idx = 0;

    RefineInstrumented(IAutoModellingModule real, Observer obs) {
      this.real = real;
      this.obs = obs;
    }

    public ProcessingResult process(RegSarimaModelling m) {
      StringBuilder sb = new StringBuilder();
      sb.append("refinestate ").append(idx++);
      try {
        ModelDescription desc = m.getDescription();
        var so = desc.specification();
        sb.append(" spec ").append(so.getP()).append(' ').append(so.getD())
            .append(' ').append(so.getQ()).append(' ').append(so.getBp())
            .append(' ').append(so.getBd()).append(' ').append(so.getBq())
            .append(" log ").append(desc.isLogTransformation() ? 1 : 0)
            .append(" mean ").append(desc.isMean() ? 1 : 0)
            .append(" needest ").append(m.needEstimation() ? 1 : 0);
        var est = m.getEstimation();
        sb.append(" pars ");
        if (est == null) {
          sb.append("- coef - x -");
        } else {
          sb.append(csv(est.getModel().arima().parameters().toArray()));
          sb.append(" coef ").append(csv(est.getConcentratedLikelihood().coefficients().toArray()));
          DoubleSeq x = est.linearizedSeries();
          sb.append(" x ").append(csv(x.toArray()));
        }
      } catch (Throwable t) {
        Throwable c = t.getCause() != null ? t.getCause() : t;
        sb.append(" error ").append(c.getClass().getName());
      }
      obs.emitFact(sb.toString());
      return real.process(m);
    }
  }

  // Swap the kernel's decoder-built outliers/autoModel modules for the
  // measuring wrappers through the private final fields (classpath classes,
  // unnamed module: setAccessible is permitted). A missing module (disabled
  // spec section) is skipped exactly as the kernel sees it.
  private static void instrument(X13Kernel kernel, Observer obs) throws Exception {
    java.lang.reflect.Field rkF = X13Kernel.class.getDeclaredField("regarima");
    rkF.setAccessible(true);
    Object rk = rkF.get(kernel);
    if (rk == null) {
      return;
    }
    java.lang.reflect.Field oF = rk.getClass().getDeclaredField("outliers");
    oF.setAccessible(true);
    Object odm = oF.get(rk);
    if (odm instanceof IOutliersDetectionModule mod) {
      oF.set(rk, new OdmInstrumented(mod, obs));
    }
    java.lang.reflect.Field aF = rk.getClass().getDeclaredField("autoModel");
    aF.setAccessible(true);
    Object am = aF.get(rk);
    if (am instanceof IAutoModellingModule mod) {
      aF.set(rk, new RefineInstrumented(mod, obs));
    }
  }

  // Reflection dump helpers for the measurement wrappers: ";"-joined %.17g
  // lists ("-" for a null/absent field), matrices row-major.
  private static java.lang.reflect.Field anyField(Class<?> c, String name)
      throws NoSuchFieldException {
    for (; c != null; c = c.getSuperclass()) {
      try {
        return c.getDeclaredField(name);
      } catch (NoSuchFieldException e) {
        // keep walking
      }
    }
    throw new NoSuchFieldException(name);
  }

  private static String dblField(Object o, String name) throws Exception {
    java.lang.reflect.Field f = anyField(o.getClass(), name);
    f.setAccessible(true);
    double[] v = (double[]) f.get(o);
    if (v == null) {
      return "-";
    }
    return csv(v);
  }

  private static String intField(Object o, String name) throws Exception {
    java.lang.reflect.Field f = anyField(o.getClass(), name);
    f.setAccessible(true);
    int[] v = (int[]) f.get(o);
    if (v == null) {
      return "-";
    }
    StringBuilder s = new StringBuilder();
    for (int i = 0; i < v.length; ++i) {
      if (i != 0) {
        s.append(',');
      }
      s.append(v[i]);
    }
    return s.toString();
  }

  private static String matField(Object o, String name) throws Exception {
    java.lang.reflect.Field f = anyField(o.getClass(), name);
    f.setAccessible(true);
    Object m = f.get(o);
    if (m == null) {
      return "-";
    }
    jdplus.toolkit.base.core.math.matrices.FastMatrix mat
        = (jdplus.toolkit.base.core.math.matrices.FastMatrix) m;
    StringBuilder s = new StringBuilder();
    for (int i = 0; i < mat.getRowsCount(); ++i) {
      for (int j = 0; j < mat.getColumnsCount(); ++j) {
        if (i != 0 || j != 0) {
          s.append(';');
        }
        s.append(fmt(mat.get(i, j)));
      }
    }
    return s.length() == 0 ? "-" : s.toString();
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
  // string form (never an identity hash). The `facts` sink carries the
  // measurement-wrapper lines (21-H.3); they are facts, never events.
  private static final class Observer implements ProcessingLog {
    final StringBuilder sb = new StringBuilder();
    final StringBuilder facts = new StringBuilder();
    int depth = 0;
    long seq = 0;

    void emitFact(String line) {
      facts.append(line).append('\n');
    }

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

  static X13Spec specification(String[] lines) {
    return buildSpec(parse(lines));
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
      X13Kernel kernel = X13Kernel.of(spec, ctx);
      instrument(kernel, obs);
      X13Results results = kernel.process(ts.cleanExtremities(), obs);
      out.append("status ").append(results.isValid() ? "ok" : "invalid").append('\n');
      out.append(obs.sb);
      out.append(obs.facts);
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
