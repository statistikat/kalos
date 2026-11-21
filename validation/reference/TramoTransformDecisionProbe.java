// Pinned-reference observer for item 23-B (part 1): the TRAMO transformation
// pre-stage of the pinned jdplus 3.9.0 stack, observed through the LIVE
// public entry points only - TramoSeasonalityDetector.hasSeasonality and the
// SeasonalityTests raw getters (the seasonality machine of
// TramoKernel.testSeasonality: Qs + non-parametric Friedman tests and the
// verdict ladder), the internal TramoUtility.calcCv critical-value grid, the
// optional robust AO+LS outlier correction through the LIVE
// tramo.internal.OutliersDetectionModule builder (the robust topology
// TramoKernel.testTransformation wires for transform='automatic'), and the
// LIVE LogLevelModule (builder + process over a live-built ModelDescription,
// exactly as TramoKernel.testTransformation drives it). Every case returns
// through the static run() entry point as one block of fact lines followed by
// the observer's ProcessingLog trace; raw getter values only, %.17g doubles,
// enum constant names, identity-hash toStrings never echoed (pinning notes
// 6.1/6.2). Inputs are %.17g tokens, so the input bits are intrinsic-
// independent, and the capture script pins the JVM with the G2 recipe anyway.
//
// Case grammar (one case per run() call; lines[0] is the header):
//   CALCCV <id> <n>
//   SEASON <id> <freq>
//   values <tok...>
//   TRANSFORM <id> <freq> <mean y|n> <detect y|n> <specseas y|n> \
//             <logpref tok|def> <prec tok> <robust y|n> <cv tok|calc>
//   values <tok...>
// Values tokens are %.17g decimals or "nan". Monthly calendar; start 1990:1.
// SEASON feeds the raw series to hasSeasonality; TRANSFORM runs the full
// TramoKernel pre-stage (detector on the description's transformed series,
// the toInt()<2 downgrade to m011 with its remark, the optional robust
// correction, then LogLevelModule).
import jdplus.toolkit.base.api.arima.SarimaOrders;
import jdplus.toolkit.base.api.data.DoubleSeq;
import jdplus.toolkit.base.api.processing.ProcessingLog;
import jdplus.toolkit.base.api.stats.StatisticalTest;
import jdplus.toolkit.base.api.timeseries.TsData;
import jdplus.toolkit.base.api.timeseries.TsDomain;
import jdplus.toolkit.base.api.timeseries.TsPeriod;
import jdplus.toolkit.base.api.timeseries.TimeSelector;
import jdplus.toolkit.base.api.timeseries.regression.Variable;
import jdplus.toolkit.base.core.regsarima.regular.IOutliersDetectionModule;
import jdplus.toolkit.base.core.regsarima.regular.ModelDescription;
import jdplus.toolkit.base.core.regsarima.regular.ProcessingResult;
import jdplus.toolkit.base.core.regsarima.regular.RegSarimaModelling;
import jdplus.toolkit.base.core.regsarima.regular.SeasonalityDetector;
import jdplus.tramoseats.base.core.tramo.LogLevelModule;
import jdplus.tramoseats.base.core.tramo.SeasonalityTests;
import jdplus.tramoseats.base.core.tramo.TramoSeasonalityDetector;
import jdplus.tramoseats.base.core.tramo.internal.TramoUtility;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;

public final class TramoTransformDecisionProbe {

  TramoTransformDecisionProbe() {
  }

  static String g(double v) {
    return String.format(Locale.ROOT, "%.17g", v);
  }

  static double tok(String s) {
    return s.equals("nan") ? Double.NaN : Double.parseDouble(s);
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
      if (d instanceof IOutliersDetectionModule.Info) {
        return odmInfo((IOutliersDetectionModule.Info) d);
      }
      if (d instanceof LogLevelModule.Info) {
        LogLevelModule.Info i = (LogLevelModule.Info) d;
        return "loglevel info logs=" + g(i.getLogs()) + " levels=" + g(i.getLevels())
            + " logpref=" + g(i.getLogpreference());
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
      return "obj " + String.valueOf(d);
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

  static String odmInfo(IOutliersDetectionModule.Info i) {
    if (i == null) {
      return "odmInfo null";
    }
    StringBuilder s = new StringBuilder("odmInfo cv=").append(g(i.getVa())).append(" types[");
    String[] t = i.getTypes();
    for (int k = 0; k < t.length; ++k) {
      if (k > 0) {
        s.append(',');
      }
      s.append(t[k]);
    }
    s.append("] outliers[");
    int[][] o = i.getOutliers();
    for (int k = 0; k < o.length; ++k) {
      if (k > 0) {
        s.append(',');
      }
      s.append(Arrays.toString(o[k]));
    }
    return s.append(']').toString();
  }

  @SuppressWarnings("unchecked")
  static String varsOf(ModelDescription desc) {
    StringBuilder s = new StringBuilder();
    for (Variable<?> vr : (java.util.List<? extends Variable<?>>) (java.util.List<?>) desc
        .variables().toList()) {
      if (s.length() > 0) {
        s.append(';');
      }
      s.append(vr.getName()).append('{');
      boolean first = true;
      for (java.util.Map.Entry<String, String> e : vr.getAttributes().entrySet()) {
        if (!first) {
          s.append(',');
        }
        first = false;
        s.append(e.getKey()).append('=').append(e.getValue());
      }
      s.append('}');
    }
    return s.length() == 0 ? "-" : s.toString();
  }

  static TsData monthly(double[] v) {
    return TsData.of(TsPeriod.monthly(1990, 1), DoubleSeq.of(v));
  }

  static TsDomain fullDomain(int n) {
    return TsDomain.of(TsPeriod.monthly(1990, 1), n);
  }

  // -- CALCCV: the pinned critical-value grid ---------------------------------
  static void calcCv(StringBuilder out, String[] h) {
    out.append("case ").append(h[1]).append('\n');
    out.append("req calccv ").append(h[2]).append('\n');
    out.append("calccv n ").append(h[2]).append(" v ")
        .append(g(TramoUtility.calcCv(Integer.parseInt(h[2])))).append('\n');
  }

  // -- SEASON: the seasonality machine on the raw series ----------------------
  static void season(StringBuilder out, String[] h, double[] y) {
    String id = h[1];
    int freq = Integer.parseInt(h[2]);
    out.append("case ").append(id).append('\n');
    out.append("req season freq ").append(freq).append('\n');
    out.append("in n ").append(y.length).append(" freq ").append(freq).append('\n');
    if (freq <= 1) {
      out.append("season_verdict NotApplicable ")
          .append(SeasonalityDetector.Seasonality.NotApplicable.toInt()).append('\n');
      return;
    }
    TramoSeasonalityDetector det = new TramoSeasonalityDetector();
    SeasonalityDetector.Seasonality v = det.hasSeasonality(DoubleSeq.of(y), freq);
    out.append("season_verdict ").append(v.name()).append(' ').append(v.toInt()).append('\n');
    SeasonalityTests st = det.getTests();
    StatisticalTest qs = st.getQs();
    StatisticalTest np = st.getNonParametricTest();
    out.append("qs_val ").append(g(qs.getValue())).append('\n');
    out.append("qs_p ").append(g(qs.getPvalue())).append('\n');
    out.append("qs_desc ").append(qs.getDescription()).append('\n');
    out.append("qs_valid ").append(qs.isValid()).append('\n');
    out.append("np_val ").append(g(np.getValue())).append('\n');
    out.append("np_p ").append(g(np.getPvalue())).append('\n');
    out.append("np_desc ").append(np.getDescription()).append('\n');
    out.append("np_valid ").append(np.isValid()).append('\n');
    double[] dseq = st.getDifferencing().getDifferenced().toArray();
    out.append("diff n ").append(dseq.length);
    for (double d : dseq) {
      out.append(' ').append(g(d));
    }
    out.append('\n');
  }

  // -- TRANSFORM: full TramoKernel pre-stage ---------------------------------
  // testSeasonality + testTransformation as TramoKernel wires it: verdict ->
  // seasonal flag (auto path: toInt()<2 downgrades to m011 with the
  // "No identifiable seasonality" remark; fixed path: spec flag verbatim),
  // optional robust-module AO+LS correction (precision 1e-3, caller cv), then
  // the LogLevelModule log-vs-level decision on the (corrected) description.
  static void transform(StringBuilder out, String[] h, double[] y) {
    out.append("case ").append(h[1]).append('\n');
    StringBuilder rq = new StringBuilder("req");
    for (int i = 2; i < h.length; ++i) {
      rq.append(' ').append(h[i]);
    }
    out.append(rq).append('\n');
    int freq = Integer.parseInt(h[2]);
    boolean mean = h[3].equals("y");
    boolean detect = h[4].equals("y");
    boolean specseas = h[5].equals("y");
    double logpref = h[6].equals("def") ? Math.log(0.95) : tok(h[6]);
    double prec = tok(h[7]);
    boolean robust = h[8].equals("y");
    double cv = h[9].equals("calc") ? TramoUtility.calcCv(y.length) : tok(h[9]);
    out.append("in n ").append(y.length).append(" freq ").append(freq)
        .append(" mean ").append(mean ? 1 : 0)
        .append(" detect ").append(detect ? 1 : 0)
        .append(" specseas ").append(specseas ? 1 : 0)
        .append(" logpref ").append(g(logpref)).append(" prec ").append(g(prec))
        .append(" robust ").append(robust ? 1 : 0).append(" cv ").append(g(cv)).append('\n');
    Observer log = new Observer();
    TsData ts = monthly(y);
    ModelDescription desc = new ModelDescription(ts, fullDomain(y.length));
    desc.setMean(mean);
    desc.setAirline(true);
    boolean seasonal = specseas;
    if (detect && freq > 1) {
      TramoSeasonalityDetector det = new TramoSeasonalityDetector();
      SeasonalityDetector.Seasonality v = det.hasSeasonality(
          desc.getTransformedSeries().getValues(), freq);
      out.append("season_verdict ").append(v.name()).append(' ').append(v.toInt()).append('\n');
      StatisticalTest qst = det.getTests().getQs();
      StatisticalTest npt = det.getTests().getNonParametricTest();
      out.append("qs ").append(g(qst.getValue())).append(' ').append(g(qst.getPvalue()))
          .append('\n');
      out.append("np ").append(g(npt.getValue())).append(' ').append(g(npt.getPvalue()))
          .append('\n');
      if (v.toInt() < 2) {
        log.remark("No identifiable seasonality", null, null);
        desc.setSpecification(SarimaOrders.m011(freq));
        seasonal = false;
      }
    }
    out.append("seasonal ").append(seasonal ? 1 : 0).append('\n');
    out.append("spec_pre " + desc.specification()).append('\n');
    RegSarimaModelling modelling = RegSarimaModelling.of(desc, log);
    if (robust) {
      jdplus.tramoseats.base.core.tramo.internal.OutliersDetectionModule robustMod
          = jdplus.tramoseats.base.core.tramo.internal.OutliersDetectionModule.builder()
              .ao(true).ls(true).tc(false).so(false).maximumLikelihood(false)
              .tcrate(0).precision(1e-3).span(TimeSelector.all())
              .maxOutliers(
                  jdplus.tramoseats.base.core.tramo.internal.OutliersDetectionModule.DEF_MAXOUTLIERS)
              .maxRound(
                  jdplus.tramoseats.base.core.tramo.internal.OutliersDetectionModule.DEF_MAXROUND)
              .build();
      ProcessingResult r1 = robustMod.process(modelling, cv);
      out.append("robust_status ").append(r1.name()).append('\n');
      jdplus.toolkit.base.core.math.matrices.FastMatrix X2 = desc.regarima().variables();
      out.append("xcols2 r ").append(X2.getRowsCount()).append(" c ").append(X2.getColumnsCount())
          .append(" empty ").append(X2.isEmpty() ? 1 : 0).append('\n');
      out.append("after_robust_vars ").append(varsOf(desc)).append('\n');
      out.append("spec_robust ").append(desc.specification()).append('\n');
    }
    LogLevelModule llm = LogLevelModule.builder().logPreference(logpref)
        .estimationPrecision(prec).seasonal(seasonal).build();
    ProcessingResult r2 = llm.process(modelling);
    out.append("loglevel_status ").append(r2.name()).append('\n');
    out.append("choosing_log ").append(llm.isChoosingLog() ? 1 : 0).append('\n');
    out.append("log_ll ").append(g(llm.getLogLL())).append('\n');
    out.append("level_ll ").append(g(llm.getLevelLL())).append('\n');
    out.append("log_correction ").append(g(llm.getLogCorrection())).append('\n');
    out.append("log_pref ").append(g(llm.getLogPreference())).append('\n');
    out.append("log_transform ").append(desc.isLogTransformation() ? 1 : 0).append('\n');
    out.append("need_estimation ").append(modelling.needEstimation() ? 1 : 0).append('\n');
    for (String e : log.evts) {
      out.append(e).append('\n');
    }
  }

  public static String run(String[] lines) {
    try {
      StringBuilder out = new StringBuilder();
      String hdr = null;
      List<String> vals = new ArrayList<>();
      for (String line0 : lines) {
        String line = line0.trim();
        if (line.isEmpty()) {
          continue;
        }
        if (line.startsWith("values ")) {
          for (String t : line.substring(7).trim().split("\\s+")) {
            vals.add(t);
          }
        } else if (hdr == null) {
          hdr = line;
        } else {
          throw new IllegalStateException("unexpected line: " + line);
        }
      }
      if (hdr == null) {
        throw new IllegalStateException("missing header");
      }
      String[] h = hdr.split("\\s+");
      double[] y = new double[vals.size()];
      for (int i = 0; i < vals.size(); ++i) {
        y[i] = tok(vals.get(i));
      }
      switch (h[0]) {
        case "CALCCV" -> calcCv(out, h);
        case "SEASON" -> season(out, h, y);
        case "TRANSFORM" -> transform(out, h, y);
        default -> throw new IllegalStateException("unknown case kind " + h[0]);
      }
      return out.toString();
    } catch (Throwable t) {
      StringBuilder out = new StringBuilder();
      out.append("case ?\nfatal ").append(t.getClass().getName());
      String m = t.getMessage();
      if (m != null) {
        out.append(' ').append(m.replace('\n', ' '));
      }
      return out.append('\n').toString();
    }
  }
}
