// Pinned-reference observer for item 23-B (part 2): the round-0 TRAMO outlier
// scan of the pinned jdplus 3.9.0 stack, observed through the LIVE module
// entry point tramo.internal.OutliersDetectionModule (builder + process over
// a RegSarimaModelling built from a live ModelDescription, exactly as
// TramoKernel/outlierSelection drives it - the builder exposes BOTH JVM
// topologies: the spec module with the user factory mix and the robust AO+LS
// module used by the transformation pre-stage). The case grammar pins the
// full builder surface: factory mix, tc rate, estimation precision, maximum
// likelihood on/off, round/outlier caps, selection span, critical value
// (calc = the internal TramoUtility.calcCv grid), interpolation of the
// description, and pre-registered outlier variables (which the module
// excludes at estimation-domain coordinates). Raw getter values only, %.17g
// doubles, identity-hash toStrings never echoed (pinning notes 6.1/6.2).
// Coordinate facts pinned by this corpus: TsPeriod.dateAt(n) is EPOCH-
// anchored (a raw domain index gives pre-series dates -> the selector hits
// nothing, OutliersDetectionModule.make collapses the bounds, the driver
// finds nothing: the fail-closed 'spannone' shape); in-domain pre-registered
// variables act twice (X column + excluded cell); out-of-domain pre variables
// are tagged {excluded=true} by ModelDescription and the module's exclude is
// a no-op. Inputs are %.17g tokens; the capture script pins the G2 recipe.
//
// Case grammar (one case per run() call; lines[0] is the header):
//   SCAN <id> <freq> <mean y|n> <log y|n> <d> <D> <p> <q> <bp> <bq>
//        <ao y|n> <ls y|n> <tc y|n> <so y|n> <tcrate tok> <prec tok>
//        <ml y|n> <maxround> <maxout> <span all|idx:l-h|idxe:l-h> <cv tok|calc>
//        <interp y|n> [pre -|CODE:idx+CODE:idx]
//   values <tok...>
// Values tokens are %.17g decimals or "nan". Monthly calendar; start 1990:1.
import jdplus.toolkit.base.api.arima.SarimaOrders;
import jdplus.toolkit.base.api.data.DoubleSeq;
import jdplus.toolkit.base.api.processing.ProcessingLog;
import jdplus.toolkit.base.api.timeseries.TsData;
import jdplus.toolkit.base.api.timeseries.TsDomain;
import jdplus.toolkit.base.api.timeseries.TsPeriod;
import jdplus.toolkit.base.api.timeseries.TimeSelector;
import jdplus.toolkit.base.api.timeseries.regression.Variable;
import jdplus.toolkit.base.core.data.interpolation.AverageInterpolator;
import jdplus.toolkit.base.core.modelling.regression.AdditiveOutlierFactory;
import jdplus.toolkit.base.core.modelling.regression.LevelShiftFactory;
import jdplus.toolkit.base.core.modelling.regression.PeriodicOutlierFactory;
import jdplus.toolkit.base.core.modelling.regression.TransitoryChangeFactory;
import jdplus.toolkit.base.core.regsarima.regular.IOutliersDetectionModule;
import jdplus.toolkit.base.core.regsarima.regular.ModelDescription;
import jdplus.toolkit.base.core.regsarima.regular.ProcessingResult;
import jdplus.toolkit.base.core.regsarima.regular.RegSarimaModelling;
import jdplus.tramoseats.base.core.tramo.internal.OutliersDetectionModule;
import jdplus.tramoseats.base.core.tramo.internal.TramoUtility;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;

public final class TramoOutlierScanProbe {

  TramoOutlierScanProbe() {
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

  static String scan(String[] h, double[] y) {
    StringBuilder out = new StringBuilder();
    out.append("case ").append(h[1]).append('\n');
    StringBuilder rq = new StringBuilder("req");
    for (int i = 2; i < h.length; ++i) {
      rq.append(' ').append(h[i]);
    }
    out.append(rq).append('\n');
    int freq = Integer.parseInt(h[2]);
    boolean mean = h[3].equals("y");
    boolean logTr = h[4].equals("y");
    int d = Integer.parseInt(h[5]);
    int D = Integer.parseInt(h[6]);
    int p = Integer.parseInt(h[7]);
    int q = Integer.parseInt(h[8]);
    int bp = Integer.parseInt(h[9]);
    int bq = Integer.parseInt(h[10]);
    String fac = h[11];
    boolean ao = fac.charAt(0) == 'y';
    boolean ls = fac.charAt(1) == 'y';
    boolean tc = fac.charAt(2) == 'y';
    boolean so = fac.charAt(3) == 'y';
    double tcrate = tok(h[12]);
    double prec = tok(h[13]);
    boolean ml = h[14].equals("y");
    int maxround = Integer.parseInt(h[15]);
    int maxout = Integer.parseInt(h[16]);
    String span = h[17];
    double cv = h[18].equals("calc") ? TramoUtility.calcCv(y.length) : tok(h[18]);
    boolean interp = h[19].equals("y");
    List<String> pres = new ArrayList<>();
    if (h.length > 20 && !h[20].equals("-")) {
      for (String pt : h[20].split("\\+")) {
        pres.add(pt);
      }
    }
    out.append("in n ").append(y.length).append(" freq ").append(freq)
        .append(" mean ").append(mean ? 1 : 0).append(" log ").append(logTr ? 1 : 0)
        .append(" orders ").append(p).append(' ').append(d).append(' ').append(q)
        .append(' ').append(bp).append(' ').append(D).append(' ').append(bq)
        .append(" fac ").append(ao ? 1 : 0).append(ls ? 1 : 0).append(tc ? 1 : 0)
        .append(so ? 1 : 0)
        .append(" tcrate ").append(g(tcrate)).append(" prec ").append(g(prec))
        .append(" ml ").append(ml ? 1 : 0).append(" maxround ").append(maxround)
        .append(" maxout ").append(maxout).append(" span ").append(span)
        .append(" cv ").append(g(cv)).append(" interp ").append(interp ? 1 : 0).append('\n');
    Observer log = new Observer();
    TsPeriod start = TsPeriod.monthly(1990, 1);
    TsData ts = TsData.of(start, DoubleSeq.of(y));
    ModelDescription desc = new ModelDescription(ts, TsDomain.of(start, y.length));
    desc.setMean(mean);
    if (logTr) {
      desc.setLogTransformation(true);
    }
    SarimaOrders orders = new SarimaOrders(freq);
    orders.setRegular(p, d, q);
    orders.setSeasonal(bp, D, bq);
    desc.setSpecification(orders);
    if (interp) {
      desc.interpolate(AverageInterpolator.interpolator());
    }
    TimeSelector sel = TimeSelector.all();
    if (span.startsWith("idx:") || span.startsWith("idxe:")) {
      // idx:  between(d(l), d(h)) with domain-relative dates; idxe: the same
      // selector built from RAW indices - TsPeriod.dateAt(n) is epoch-
      // anchored, so the window lands before the series and the module's
      // bounds collapse (fail-closed golden).
      boolean epoch = span.startsWith("idxe:");
      String[] lr = span.substring(epoch ? 5 : 4).split("-");
      long l = Long.parseLong(lr[0]);
      long h2 = Long.parseLong(lr[1]);
      long base = epoch ? 0 : start.getId();
      sel = TimeSelector.between(start.dateAt(base + l), start.dateAt(base + h2));
    }
    OutliersDetectionModule mod = OutliersDetectionModule.builder()
        .ao(ao).ls(ls).tc(tc).so(so).maximumLikelihood(ml)
        .tcrate(tcrate).precision(prec).span(sel)
        .maxOutliers(maxout).maxRound(maxround)
        .build();
    for (String pre : pres) {
      String[] pc = pre.split(":");
      java.time.LocalDateTime at = start.dateAt(start.getId() + Long.parseLong(pc[1]));
      jdplus.toolkit.base.api.timeseries.regression.IOutlier core;
      switch (pc[0]) {
        case "AO" -> core = AdditiveOutlierFactory.FACTORY.make(at);
        case "LS" -> core = LevelShiftFactory.FACTORY_ZEROENDED.make(at);
        case "TC" -> core = new TransitoryChangeFactory(tcrate).make(at);
        default -> core = new PeriodicOutlierFactory(freq, true).make(at);
      }
      desc.addVariable(Variable.<jdplus.toolkit.base.api.timeseries.regression.IOutlier>builder()
          .name(pc[0] + "." + pc[1]).core(core).build());
    }
    if (!pres.isEmpty()) {
      out.append("pre_vars ").append(varsOf(desc)).append('\n');
    }
    TsDomain ed = desc.getEstimationDomain();
    out.append("estdom start ").append(ed.getStartPeriod().toString()).append(" len ")
        .append(ed.getLength()).append('\n');
    RegSarimaModelling modelling = RegSarimaModelling.of(desc, log);
    ProcessingResult r = mod.process(modelling, cv);
    out.append("scan_status ").append(r.name()).append('\n');
    out.append("after_vars ").append(varsOf(desc)).append('\n');
    out.append("need_estimation ").append(modelling.needEstimation() ? 1 : 0).append('\n');
    out.append("spec_after ").append(desc.specification()).append('\n');
    for (String e : log.evts) {
      out.append(e).append('\n');
    }
    return out.toString();
  }

  public static String run(String[] lines) {
    try {
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
      double[] y = new double[vals.size()];
      for (int i = 0; i < vals.size(); ++i) {
        y[i] = vals.get(i).equals("nan") ? Double.NaN : Double.parseDouble(vals.get(i));
      }
      return scan(hdr.split("\\s+"), y);
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
