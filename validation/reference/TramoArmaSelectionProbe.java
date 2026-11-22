// Package-local observer for item 23-D: the LIVE tramo.internal.ArmaModule
// arma selection of the pinned jdplus 3.9.0 stack (docs/tramo-seats-roadmap.md).
// The probe lives in jdplus.tramoseats.base.core.tramo.internal so it can
// read the package-private static gates of ArmaModule (comespa / maxInic /
// calcmaxspec) next to the live module run; every echoed value is a raw
// getter (%.17g doubles, orders from the numeric getters, never a
// default-toString). Each case builds a fresh ModelDescription, echoes the
// olsResiduals that feed the selector (request data for the C++ unit),
// replays the selector directly through the public builder (panel rows of
// gePreferredModels() + BIC) and then runs the LIVE ArmaModule.process over
// a RegSarimaModelling driven by an Observer ProcessingLog; the CInfo detail
// of the "selected model" info event is formatted from its public getters.
// main() reads one case file (header line + values line) so the force-load
// witness runs the same case in a fresh JVM with -Xlog:class+load=info.
package jdplus.tramoseats.base.core.tramo.internal;

import jdplus.toolkit.base.api.arima.SarmaOrders;
import jdplus.toolkit.base.api.arima.SarimaOrders;
import jdplus.toolkit.base.api.data.DoubleSeq;
import jdplus.toolkit.base.api.processing.ProcessingLog;
import jdplus.toolkit.base.api.timeseries.TsData;
import jdplus.toolkit.base.api.timeseries.TsPeriod;
import jdplus.toolkit.base.core.regarima.RegArimaUtility;
import jdplus.toolkit.base.core.regsarima.regular.ModelDescription;
import jdplus.toolkit.base.core.regsarima.regular.ProcessingResult;
import jdplus.toolkit.base.core.regsarima.regular.RegSarimaModelling;

import java.util.ArrayList;
import java.util.List;

public final class TramoArmaSelectionProbe {

  private TramoArmaSelectionProbe() {
  }

  static String g(double v) {
    return String.format("%.17g", v);
  }

  static double tok(String s) {
    return Double.parseDouble(s);
  }

  static boolean yes(String s) {
    return s.equals("y") || s.equals("true") || s.equals("1");
  }

  static final class Observer implements ProcessingLog {
    final List<String> evts = new ArrayList<>();
    int depth = 0;
    long seq = 0;

    static String spec(SarmaOrders o) {
      return o == null ? "null"
          : "(" + o.getP() + "," + o.getQ() + ")(" + o.getBp() + "," + o.getBq() + ")";
    }

    static String detail(Object d) {
      if (d == null) {
        return "-";
      }
      if (d instanceof ArmaModule.CInfo) {
        ArmaModule.CInfo c = (ArmaModule.CInfo) d;
        StringBuilder s = new StringBuilder("armaInfo best=").append(spec(c.bestModel()));
        SarmaOrders[] ms = c.models();
        double[] bs = c.bic();
        for (int i = 0; i < ms.length; ++i) {
          s.append(" m").append(i).append(ms[i] == null ? "=null" : "=" + spec(ms[i]));
          s.append(" b").append(i).append('=').append(g(bs[i]));
        }
        return s.toString();
      }
      if (d instanceof String || d instanceof Integer || d instanceof Boolean) {
        return "lit " + String.valueOf(d);
      }
      if (d instanceof Double) {
        return "num " + g((Double) d);
      }
      return "obj " + d.getClass().getName();
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

    // The ProcessingLog (String, Object) convenience defaults route as
    // kind(null, message, detail): the human text arrives in the operation
    // argument, so the observer prefers the first non-null string.
    public void error(String m, String o, Object d) {
      emit("error", m != null ? m : o, d);
    }

    public void warning(String m, String o, Object d) {
      emit("warn", m != null ? m : o, d);
    }

    public void remark(String m, String o, Object d) {
      emit("remark", m != null ? m : o, d);
    }

    public void info(String m, String o, Object d) {
      emit("info", m != null ? m : o, d);
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
    return "(" + o.getP() + "," + o.getD() + "," + o.getQ() + ")("
        + o.getBp() + "," + o.getBd() + "," + o.getBq() + ")";
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

  // -- ARM: live ArmaModule over a fresh description -------------------------
  static void arm(StringBuilder out, String[] h, double[] y) {
    String id = h[1];
    int freq = Integer.parseInt(h[2]);
    boolean seasonal = yes(h[3]);
    boolean wn = yes(h[4]);
    boolean log = yes(h[5]);
    int d = Integer.parseInt(h[6]);
    int bd = Integer.parseInt(h[7]);
    int p = Integer.parseInt(h[8]);
    int q = Integer.parseInt(h[9]);
    int bp = Integer.parseInt(h[10]);
    int bq = Integer.parseInt(h[11]);
    boolean mean = yes(h[12]);
    out.append("case ").append(id).append('\n');
    out.append("req arma freq ").append(freq).append(" seasonal ").append(seasonal ? 1 : 0)
        .append(" wn ").append(wn ? 1 : 0).append('\n');

    ModelDescription desc = describe(y, freq, log, mean, orders(freq, d, bd, p, q, bp, bq));
    int nobs = desc.regarima().getObservationsCount();
    int freqA = desc.getAnnualFrequency();
    SarimaOrders spec0 = desc.specification();
    out.append("in ").append(id).append(" n ").append(y.length)
        .append(" nobs ").append(nobs)
        .append(" afreq ").append(freqA)
        .append(" spec ").append(specOf(desc))
        .append(" mean ").append(desc.isMean() ? 1 : 0)
        .append(" log ").append(desc.isLogTransformation() ? 1 : 0)
        .append('\n');

    // Package-private static gates as the module reads them.
    int pcnt = ArmaModule.comespa(spec0.getPeriod(), nobs,
        ArmaModule.maxInic(spec0.getPeriod()), spec0.getD(), spec0.getBd(), seasonal);
    out.append("pcnt ").append(id).append(' ').append(pcnt).append('\n');
    if (pcnt != 0) {
      SarimaOrders mx = ArmaModule.calcmaxspec(freqA, pcnt, spec0.getD(), spec0.getBd(),
          seasonal);
      out.append("maxspec ").append(id).append(" (").append(mx.getP()).append(',').append(mx.getD())
          .append(',').append(mx.getQ()).append(")(").append(mx.getBp()).append(',')
          .append(mx.getBd()).append(',').append(mx.getBq()).append(")\n");
    }

    // olsResiduals: the exact selector input (request data for the unit).
    StringBuilder rv = new StringBuilder();
    DoubleSeq res = null;
    try {
      res = RegArimaUtility.olsResiduals(desc.regarima());
      rv.append("resvals ").append(id).append(' ').append(res.length());
      for (int i = 0; i < res.length(); ++i) {
        rv.append(' ').append(g(res.get(i)));
      }
      rv.append('\n');
    } catch (Throwable t) {
      rv.append("resvals ").append(id).append(" error ").append(t.getClass().getName())
          .append('\n');
    }
    out.append(rv);

    // Direct selector replay (module createModule pin: acceptWhiteNoise(wn),
    // modelsCount default, max* from maxspec) with the panel of
    // gePreferredModels(); null rows are echoed inert.
    if (pcnt != 0 && res != null) {
      SarimaOrders mx = ArmaModule.calcmaxspec(freqA, pcnt, spec0.getD(), spec0.getBd(),
          seasonal);
      try {
        ArmaModelSelector sel = ArmaModelSelector.builder().acceptWhiteNoise(wn)
            .maxP(mx.getP()).maxQ(mx.getQ()).maxBp(mx.getBp()).maxBq(mx.getBq()).build();
        SarmaOrders s = sel.process(res, freqA, mx.getD(), mx.getBd(), seasonal);
        out.append("direct_sel ").append(id).append(' ')
            .append(s == null ? "null" : Observer.spec(s)).append('\n');
        ArmaModelSelector.FastBIC[] hm = sel.gePreferredModels();
        for (int i = 0; i < hm.length; ++i) {
          if (hm[i] == null) {
            out.append("dpanel ").append(id).append(' ').append(i).append(" null\n");
          } else {
            SarmaOrders o = hm[i].getSpecification();
            out.append("dpanel ").append(id).append(' ').append(i)
                .append(" (").append(o.getP()).append(',').append(o.getQ()).append(")(")
                .append(o.getBp()).append(',').append(o.getBq()).append(") ")
                .append(g(hm[i].getBIC())).append('\n');
          }
        }
      } catch (Throwable t) {
        out.append("direct_sel ").append(id).append(" error ").append(t.getClass().getName())
            .append('\n');
      }
    }

    // Live module run over a fresh description (same inputs).
    Observer obs = new Observer();
    ModelDescription desc2 = describe(y, freq, log, mean, orders(freq, d, bd, p, q, bp, bq));
    RegSarimaModelling m = RegSarimaModelling.of(desc2, obs);
    ArmaModule mod = ArmaModule.builder().acceptWhiteNoise(wn).seasonal(seasonal).build();
    ProcessingResult r;
    try {
      r = mod.process(m);
    } catch (Throwable t) {
      out.append("fatal ").append(t.getClass().getName()).append(' ')
          .append(String.valueOf(t.getMessage()).replace('\n', ' ')).append('\n');
      return;
    }
    out.append("arm_result ").append(id).append(' ').append(r).append('\n');
    out.append("after ").append(id).append(" spec ").append(specOf(desc2))
        .append(" mean ").append(desc2.isMean() ? 1 : 0).append('\n');
    for (String e : obs.evts) {
      out.append(e).append('\n');
    }
  }

  // -- HRB: per-candidate diagnostics over the full grid (debug material,
  // values are the residual block itself). Echoes the fitted parameters and
  // the fast BIC per candidate so the C++ seam can be compared stage-wise.
  static void hrb(StringBuilder out, String[] h, double[] r) {
    out.append("case ").append(h[1]).append('\n');
    for (int p = 0; p <= 3; ++p) {
      for (int q = 0; q <= 3; ++q) {
        for (int bp = 0; bp <= 1; ++bp) {
          for (int bq = 0; bq <= 1; ++bq) {
            SarmaOrders o = new SarmaOrders(12);
            o.setP(p);
            o.setQ(q);
            o.setBp(bp);
            o.setBq(bq);
            StringBuilder s = new StringBuilder("cand ")
                .append("(").append(p).append(',').append(q).append(")(")
                .append(bp).append(',').append(bq).append(')');
            try {
              jdplus.toolkit.base.core.sarima.estimation.HannanRissanen hr =
                  jdplus.toolkit.base.core.sarima.estimation.HannanRissanen.builder().build();
              boolean ok = hr.process(DoubleSeq.of(r), o);
              jdplus.toolkit.base.core.sarima.SarimaModel m = hr.getModel();
              boolean stable = m.isStable(true);
              s.append(" ok ").append(ok ? 1 : 0).append(" st ").append(stable ? 1 : 0);
              s.append(" p ");
              double[] pa = m.parameters().toArray();
              for (double v : pa) {
                s.append(' ').append(g(v));
              }
              if (stable) {
                jdplus.toolkit.base.core.data.DataBlock e =
                    new jdplus.toolkit.base.core.arima.estimation.FastKalmanFilter(m)
                        .fastFilter(DoubleSeq.of(r));
                double ssq = e.ssq();
                s.append(" ssq ").append(g(ssq)).append(" n ").append(e.length());
                double bic = new jdplus.toolkit.base.core.arima.estimation.FastKalmanFilter(m)
                    .fastProcessing(DoubleSeq.of(r), m.getParametersCount());
                s.append(" bic ").append(g(bic));
              }
            } catch (Throwable t) {
              s.append(" error ").append(t.getClass().getName());
            }
            out.append(s).append('\n');
          }
        }
      }
    }
  }

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
        case "ARM":
          arm(out, h, y);
          break;
        case "HRB":
          hrb(out, h, y);
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
