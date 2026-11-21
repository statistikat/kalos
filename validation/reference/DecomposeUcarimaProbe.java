// Item 24-B oracle observer: the SEATS decomposition seam of the LIVE pinned
// jdplus 3.9.0 stack - DefaultModelValidator output -> the raw selector-chain
// UCARIMA split (ModelDecomposer + RootDecomposer), UcarimaModel
// .setVarianceMax(-1, noisy) through DefaultModelDecomposer (what the kernel
// consumes), and SeatsModel.compactUcarimaModel(false, false) exactly as
// SeatsKernel.makeResults derives the compact model - plus the flags and
// ProcessingLog events the kernel emits (kevt frame).
//
// Public entry points only, no instrumentation: SeatsToolkit.of(spec),
// SeatsModel.of(SeatsModelSpec), IModelValidator.validate/getNewModel,
// DefaultModelDecomposer.decompose, the selector chain wired as SeatsToolkit
// .of does, SeatsModel.compactUcarimaModel, and SeatsKernel(SeatsToolkit)
// .process(SeatsModelSpec, ProcessingLog). Facts are read through getters as
// RAW values (%.17g doubles, enum names, never a display toString - pinning
// notes 6.1/6.2/7). The one-sided MA/variance of the RootDecomposer's
// symmetric components come from getMa()/getInnovationVariance() (the lazy
// SymmetricFilter.factorize()); getter failures are captured as maerr/varr
// fact lines PER COMPONENT, so the whole block survives one bad factorization
// (the 24-A probe's of() conversion stopped at the first one).
//
// Protocol (lines given by capture-decompose-ucarima.R) - identical to the
// 24-A TramoSeatsModelProbe request language:
//   id <name>              n <int>      start <year> <month>   src A|B|C|D
//   log 0|1                mean 0|1     var <double>
//   spec <p> <d> <q> <bp> <bd> <bq>    (orders ARE the parameter lengths)
//   phi <v...>             bphi <v...>  theta <v...>  btheta <v...>
//   specname airline                  (use SarimaSpec.airline() verbatim)
//   xl|epsphi|rmod|smod|smod1 <double> (omit a knob for its builder default)
//   mode None|Legacy|Noisy            forecastcount|backcastcount <int>
//   method Default|Burman             bias Legacy|New
//   knobs validate                    (DecompositionSpec.build() validation)
// Fact blocks: req/... toolkit/... v.* model <x> evt* (validator seam),
// r.* (raw chain), d.* (decomposer result = setVarianceMax output),
// cmp.* (SeatsModel.compactUcarimaModel(false,false) of the d model),
// status k.* (kernel surface) and kevt* (kernel frame).
import jdplus.toolkit.base.api.arima.SarimaSpec;
import jdplus.toolkit.base.api.data.DoubleSeq;
import jdplus.toolkit.base.api.data.Parameter;
import jdplus.toolkit.base.api.processing.ProcessingLog;
import jdplus.toolkit.base.api.timeseries.TsData;
import jdplus.toolkit.base.api.timeseries.TsPeriod;
import jdplus.tramoseats.base.api.seats.DecompositionSpec;
import jdplus.tramoseats.base.api.seats.SeatsModelSpec;
import jdplus.toolkit.base.core.arima.IArimaModel;
import jdplus.toolkit.base.core.sarima.SarimaModel;
import jdplus.tramoseats.base.core.seats.SeatsKernel;
import jdplus.tramoseats.base.core.seats.DefaultModelDecomposer;
import jdplus.toolkit.base.core.ucarima.ModelDecomposer;
import jdplus.toolkit.base.core.ucarima.SeasonalSelector;
import jdplus.toolkit.base.core.ucarima.TrendCycleSelector;
import jdplus.tramoseats.base.core.seats.SeatsModel;
import jdplus.tramoseats.base.core.seats.SeatsResults;
import jdplus.tramoseats.base.core.seats.SeatsToolkit;
import jdplus.toolkit.base.core.ucarima.UcarimaModel;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;

public final class DecomposeUcarimaProbe {

  private DecomposeUcarimaProbe() {
  }

  static String g(double v) {
    return String.format(Locale.ROOT, "%.17g", v);
  }

  static String seq(double[] v) {
    StringBuilder sb = new StringBuilder();
    for (double x : v) {
      if (sb.length() != 0) {
        sb.append(' ');
      }
      sb.append(g(x));
    }
    return sb.toString();
  }

  // StrictMath-only fixed series (input bits intrinsic-independent); same
  // generators as the 24-A probe.
  static double[] values(String src, int n) {
    double[] y = new double[n];
    for (int t = 0; t < n; ++t) {
      double k = t + 1;
      switch (src) {
        case "B": // airline-friendly: strong seasonal, unit-root-ish trend
          y[t] = 10 * k / 12.0 + 8 * StrictMath.sin(2 * StrictMath.PI * k / 12)
              + 4 * StrictMath.cos(4 * StrictMath.PI * k / 12)
              + StrictMath.sin(6 * StrictMath.PI * k / 12);
          break;
        case "C": // no seasonality at all
          y[t] = 50 + 0.05 * k;
          break;
        case "D": // stationary-ish: flat mean, alternating seasonal
          y[t] = 100 + 4 * StrictMath.sin(2 * StrictMath.PI * k / 12)
              + 0.5 * StrictMath.cos(14 * StrictMath.PI * k / 12);
          break;
        default: // A: the M5-open probe series
          y[t] = 50 + 0.05 * k + 5 * StrictMath.sin(2 * StrictMath.PI * k / 12)
              + 3 * StrictMath.cos(4 * StrictMath.PI * k / 12)
              + 0.5 * StrictMath.sin(14 * StrictMath.PI * k / 12);
      }
    }
    return y;
  }

  private static final class Observer implements ProcessingLog {

    final StringBuilder sb = new StringBuilder();
    final String prefix;
    int depth = 0;
    long seq = 0;

    Observer(String prefix) {
      this.prefix = prefix;
    }

    static String detail(Object d) {
      if (d == null) {
        return "-";
      }
      if (d instanceof Double) {
        return "num " + g((Double) d);
      }
      if (d instanceof Object[]) {
        return "arr " + Arrays.deepToString((Object[]) d);
      }
      // SarimaOrders (the approximation remark detail) prints its own
      // identity hash: format through the getters (pinning notes 6.1).
      if (d instanceof jdplus.toolkit.base.api.arima.SarimaOrders) {
        jdplus.toolkit.base.api.arima.SarimaOrders o
            = (jdplus.toolkit.base.api.arima.SarimaOrders) d;
        return "orders p" + o.getP() + "d" + o.getD() + "q" + o.getQ()
            + "P" + o.getBp() + "D" + o.getBd() + "Q" + o.getBq()
            + " per" + o.getPeriod();
      }
      return "obj " + String.valueOf(d);
    }

    void emit(String kind, String msg, Object details) {
      ++seq;
      sb.append(prefix).append(' ').append(seq).append(' ').append(kind)
          .append(' ').append(depth).append(' ')
          .append(msg == null ? "-" : msg.replace('\n', ' '))
          .append(" det ").append(detail(details)).append('\n');
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

    public void error(String s) {
      emit("error", s, null);
    }

    public void warning(String s) {
      emit("warning", s, null);
    }

    public void remark(String s) {
      emit("remark", s, null);
    }

    public void remark(String s, Object d) {
      emit("remark", s, d);
    }

    public void clearRemarks() {
      emit("clear", null, null);
    }
  }

  // Getter-safe component dump: sar/ur/sfd are representation-free (the raw
  // RootDecomposer components are the SYMMETRIC ArimaModel variant); ma/var
  // come from the lazy spectral factorization and are captured per-getter,
  // failures included (ArimaException "invalid ARIMA" on getMa, NPE reading
  // the scaling of a null Factorization on getInnovationVariance).
  static void dumpModel(StringBuilder sb, String key, IArimaModel m) {
    try {
      sb.append(key).append(" sar ").append(
          seq(m.getStationaryAr().asPolynomial().toArray())).append('\n');
      sb.append(key).append(" ur ").append(
          seq(m.getNonStationaryAr().asPolynomial().toArray())).append('\n');
    } catch (RuntimeException e) {
      sb.append(key).append(" arrexcep ").append(e.getClass().getName())
          .append('\n');
      return;
    }
    try {
      sb.append(key).append(" ma ").append(
          seq(m.getMa().asPolynomial().toArray())).append('\n');
    } catch (RuntimeException e) {
      sb.append(key).append(" maerr ").append(e.getClass().getName())
          .append('\n');
    }
    try {
      sb.append(key).append(" var ").append(g(m.getInnovationVariance()))
          .append('\n');
    } catch (RuntimeException e) {
      sb.append(key).append(" varr ").append(e.getClass().getName())
          .append('\n');
    }
    if (m instanceof jdplus.toolkit.base.core.arima.ArimaModel) {
      try {
        sb.append(key).append(" sfd ").append(seq(
            ((jdplus.toolkit.base.core.arima.ArimaModel) m)
            .symmetricMa().coefficientsAsPolynomial().toArray())).append('\n');
      } catch (RuntimeException e) {
        sb.append(key).append(" sfdr ").append(e.getClass().getName())
            .append('\n');
      }
    }
  }

  static void dumpUcarima(StringBuilder sb, String key, UcarimaModel um) {
    if (um == null) {
      sb.append(key).append(" null\n");
      return;
    }
    IArimaModel model = um.getModel();
    try {
      sb.append(key).append(" count ")
          .append(um.getComponentsCount()).append(" valid ")
          .append(um.isValid() ? 1 : 0).append('\n');
    } catch (RuntimeException e) {
      sb.append(key).append(" excep ").append(e.getClass().getName())
          .append('\n');
      return;
    }
    dumpModel(sb, key + " model", model);
    jdplus.toolkit.base.core.arima.ArimaModel[] cs = um.getComponents();
    for (int i = 0; i < cs.length; ++i) {
      dumpModel(sb, key + " c" + i, cs[i]);
    }
  }

  static double[] pars(Parameter[] p) {
    double[] v = new double[p.length];
    for (int i = 0; i < p.length; ++i) {
      v[i] = p[i].getValue();
    }
    return v;
  }

  public static String run(String[] lines) {
    StringBuilder sb = new StringBuilder();
    String id = "?";
    String src = "A";
    int n = 96, year = 1990, month = 1;
    boolean log = false, mean = true, knobsValidate = false;
    double var = 0.0;
    Integer p = null, d = null, q = null, bp = null, bd = null, bq = null;
    double[] phi = null, bphi = null, theta = null, btheta = null;
    boolean airline = false;
    int period = 12;
    int freq = 12;
    Double xl = null, epsphi = null, rmod = null, smod = null, smod1 = null;
    String mode = null;
    Integer forecasts = null, backcasts = null;
    String method = null, bias = null;

    for (String raw : lines) {
      String ln = raw.trim();
      if (ln.isEmpty()) {
        continue;
      }
      String[] tk = ln.split("\\s+");
      switch (tk[0]) {
        case "id": id = tk[1]; break;
        case "src": src = tk[1]; break;
        case "n": n = Integer.parseInt(tk[1]); break;
        case "start": year = Integer.parseInt(tk[1]); month = Integer.parseInt(tk[2]); break;
        case "log": log = tk[1].equals("1"); break;
        case "mean": mean = tk[1].equals("1"); break;
        case "var": var = Double.parseDouble(tk[1]); break;
        case "specname": airline = tk[1].equals("airline"); break;
        case "period": period = Integer.parseInt(tk[1]); break;
        case "freq": freq = Integer.parseInt(tk[1]); break;
        case "spec":
          p = Integer.parseInt(tk[1]); d = Integer.parseInt(tk[2]);
          q = Integer.parseInt(tk[3]); bp = Integer.parseInt(tk[4]);
          bd = Integer.parseInt(tk[5]); bq = Integer.parseInt(tk[6]);
          break;
        case "phi": phi = dbls(tk); break;
        case "bphi": bphi = dbls(tk); break;
        case "theta": theta = dbls(tk); break;
        case "btheta": btheta = dbls(tk); break;
        case "xl": xl = Double.parseDouble(tk[1]); break;
        case "epsphi": epsphi = Double.parseDouble(tk[1]); break;
        case "rmod": rmod = Double.parseDouble(tk[1]); break;
        case "smod": smod = Double.parseDouble(tk[1]); break;
        case "smod1": smod1 = Double.parseDouble(tk[1]); break;
        case "mode": mode = tk[1]; break;
        case "forecastcount": forecasts = Integer.parseInt(tk[1]); break;
        case "backcastcount": backcasts = Integer.parseInt(tk[1]); break;
        case "method": method = tk[1]; break;
        case "bias": bias = tk[1]; break;
        case "knobs": knobsValidate = tk[1].equals("validate"); break;
        default: throw new IllegalArgumentException("unknown line: " + ln);
      }
    }
    sb.append("case ").append(id).append('\n');
    try {
      double[] y = values(src, n);
      sb.append("src ").append(src).append(" n ").append(y.length).append('\n');
      sb.append("values ").append(seq(y)).append('\n');
      TsData ts = TsData.of(freq == 12 ? TsPeriod.monthly(year, month)
          : TsPeriod.quarterly(year, month), DoubleSeq.of(y));

      SarimaSpec ss;
      if (airline) {
        SarimaSpec t = SarimaSpec.airline();
        sb.append("tpl defined ").append(t.isDefined() ? 1 : 0)
            .append(" theta ").append(seq(pars(t.getTheta())))
            .append(" btheta ").append(seq(pars(t.getBtheta())))
            .append(" types ").append(t.getTheta()[0].getType().name())
            .append('\n');
        ss = SarimaSpec.builder().period(12).p(0).d(1).q(1).bp(0).bd(1).bq(1)
            .theta(defs(new double[] {0.5}))
            .btheta(defs(new double[] {0.4}))
            .buildWithoutValidation();
      } else {
        SarimaSpec.Builder b = SarimaSpec.builder()
            .p(p).d(d).q(q).bp(bp).bd(bd).bq(bq).period(period);
        b.phi(defs(phi)).bphi(defs(bphi)).theta(defs(theta))
            .btheta(defs(btheta));
        ss = b.buildWithoutValidation();
      }
      SeatsModelSpec sms = SeatsModelSpec.builder().series(ts).log(log)
          .meanCorrection(mean).innovationVariance(var).sarimaSpec(ss).build();

      DecompositionSpec.Builder db = DecompositionSpec.builder();
      if (xl != null) {
        db.xlBoundary(xl);
      }
      if (epsphi != null) {
        db.seasTolerance(epsphi);
      }
      if (rmod != null) {
        db.trendBoundary(rmod);
      }
      if (smod != null) {
        db.seasBoundary(smod);
      }
      if (smod1 != null) {
        db.seasBoundaryAtPi(smod1);
      }
      if (mode != null) {
        db.approximationMode(DecompositionSpec.ModelApproximationMode.valueOf(mode));
      }
      if (forecasts != null) {
        db.forecastCount(forecasts);
      }
      if (backcasts != null) {
        db.backcastCount(backcasts);
      }
      if (method != null) {
        db.method(DecompositionSpec.ComponentsEstimationMethod.valueOf(method));
      }
      if (bias != null) {
        db.biasCorrection(DecompositionSpec.BiasCorrection.valueOf(bias));
      }
      DecompositionSpec ds = knobsValidate ? db.build()
          : db.buildWithoutValidation();

      // Request echo (raw getters).
      SarimaSpec es = sms.getSarimaSpec();
      sb.append("req spec ").append(es.getP()).append(' ').append(es.getD())
          .append(' ').append(es.getQ()).append(' ').append(es.getBp())
          .append(' ').append(es.getBd()).append(' ').append(es.getBq())
          .append(' ').append(es.getPeriod()).append('\n');
      sb.append("req phi ").append(seq(pars(es.getPhi()))).append('\n');
      sb.append("req bphi ").append(seq(pars(es.getBphi()))).append('\n');
      sb.append("req theta ").append(seq(pars(es.getTheta()))).append('\n');
      sb.append("req btheta ").append(seq(pars(es.getBtheta()))).append('\n');
      sb.append("req log ").append(sms.isLog() ? 1 : 0)
          .append(" mean ").append(sms.isMeanCorrection() ? 1 : 0)
          .append(" var ").append(g(sms.getInnovationVariance())).append('\n');
      sb.append("req knobs ").append(g(ds.getXlBoundary())).append(' ')
          .append(g(ds.getSeasTolerance())).append(' ')
          .append(g(ds.getTrendBoundary())).append(' ')
          .append(g(ds.getSeasBoundary())).append(' ')
          .append(g(ds.getSeasBoundaryAtPi())).append(' ')
          .append(ds.getApproximationMode().name()).append(' ')
          .append(ds.getForecastCount()).append(' ')
          .append(ds.getBackcastCount()).append(' ')
          .append(ds.getMethod().name()).append(' ')
          .append(ds.getBiasCorrection().name()).append('\n');

      if (!ss.isDefined()) {
        sb.append("status invalid:estimated-model-out-of-envelope\n");
        return sb.toString();
      }

      SeatsToolkit tk1 = SeatsToolkit.of(ds);
      sb.append("toolkit validator ")
          .append(tk1.getModelValidator().getClass().getName()).append('\n');
      sb.append("toolkit decomposer ")
          .append(tk1.getModelDecomposer().getClass().getName()).append('\n');
      sb.append("toolkit approximator ")
          .append(tk1.getModelApproximator() == null ? "null"
              : tk1.getModelApproximator().getClass().getName()).append('\n');

      // The exact SeatsKernel.validate seam, toolkit-owned validator: the
      // 24-B unit's input shape.
      SeatsModel sm = SeatsModel.of(sms);
      sm.setCurrentModel(sm.getOriginalModel());
      Observer vobs = new Observer("evt");
      vobs.push("validation");
      boolean ok = tk1.getModelValidator().validate(sm.getCurrentModel());
      if (!ok) {
        sm.setCurrentModel(tk1.getModelValidator().getNewModel());
        sm.setParametersCutOff(true);
        vobs.remark("arima parameters cut off");
      }
      vobs.pop();
      SarimaModel m1 = sm.getCurrentModel();
      sb.append("v.changed ").append(ok ? 0 : 1).append('\n');
      sb.append("v.xl ").append(
          g(((jdplus.tramoseats.base.core.seats.DefaultModelValidator)
              tk1.getModelValidator()).getXl())).append('\n');
      sb.append("v.orders ").append(m1.orders().getP()).append(' ')
          .append(m1.orders().getD()).append(' ').append(m1.orders().getQ())
          .append(' ').append(m1.orders().getBp()).append(' ')
          .append(m1.orders().getBd()).append(' ').append(m1.orders().getBq())
          .append('\n');
      sb.append("v.phi ").append(seq(m1.getPhi().toArray())).append('\n');
      sb.append("v.bphi ").append(seq(m1.getBphi().toArray())).append('\n');
      sb.append("v.theta ").append(seq(m1.getTheta().toArray())).append('\n');
      sb.append("v.btheta ").append(seq(m1.getBtheta().toArray())).append('\n');
      sb.append("model var ").append(g(sm.getInnovationVariance())).append('\n');
      sb.append("model seasonality ").append(sm.isSignificantSeasonality() ? 1 : 0)
          .append('\n');
      sb.append(vobs.sb);

      // The decomposition seam itself.
      {
        final SarimaModel vm = m1;
        // d: the toolkit DefaultModelDecomposer (selector chain +
        // setVarianceMax(-1, mode == Noisy)) - what the kernel consumes.
        DefaultModelDecomposer dmd = new DefaultModelDecomposer(ds);
        UcarimaModel dum = null;
        try {
          dum = dmd.decompose(vm);
          sb.append("d.decompose ").append(dum == null ? "null" : "ok")
              .append('\n');
        } catch (RuntimeException e) {
          sb.append("d.excep ").append(e.getClass().getName()).append('\n');
        }
        if (dum != null) {
          dumpUcarima(sb, "d", dum);
          // cmp: SeatsModel.compactUcarimaModel(false, false), the exact
          // call SeatsKernel.makeResults makes on the decomposition result.
          try {
            SeatsModel sm2 = SeatsModel.of(sms);
            sm2.setCurrentModel(vm);
            sm2.setUcarimaModel(dum);
            UcarimaModel cm = sm2.compactUcarimaModel(false, false);
            dumpUcarima(sb, "cmp", cm);
          } catch (RuntimeException e) {
            sb.append("cmp.excep ").append(e.getClass().getName()).append('\n');
          }
        }
        // r: the raw selector-chain decomposition BEFORE setVarianceMax.
        try {
          TrendCycleSelector tcs = new TrendCycleSelector(
              ds.getTrendBoundary());
          tcs.setDefaultLowFreqThreshold(vm.getPeriod());
          SeasonalSelector ss1 = new SeasonalSelector(vm.getPeriod(),
              ds.getSeasTolerance());
          ss1.setK(vm.orders().getBd() + vm.orders().getBp() > 0
              ? ds.getSeasBoundary() : ds.getSeasBoundaryAtPi());
          ModelDecomposer md = new ModelDecomposer();
          md.add(tcs);
          md.add(ss1);
          UcarimaModel raw = md.decompose(vm);
          sb.append("r.decompose ").append(raw == null ? "null" : "ok")
              .append('\n');
          if (raw != null) {
            jdplus.toolkit.base.core.arima.ArimaModel[] cs
                = raw.getComponents();
            sb.append("r.count ").append(cs.length).append('\n');
            for (int i = 0; i < cs.length; ++i) {
              dumpModel(sb, "r c" + i, cs[i]);
            }
          }
        } catch (RuntimeException e) {
          sb.append("r.excep ").append(e.getClass().getName()).append('\n');
        }
      }

      // The live kernel: the durable behavior record (validation,
      // decomposition, flags, results). Estimation stages run as usual;
      // their component values are out of scope for this unit.
      SeatsKernel kernel = new SeatsKernel(SeatsToolkit.of(ds));
      Observer kobs = new Observer("kevt");
      try {
        SeatsResults r = kernel.process(sms, kobs);
        sb.append("status ok\n");
        sb.append("k.cutOff ").append(r.isParametersCutOff() ? 1 : 0).append('\n');
        sb.append("k.modelChanged ").append(r.isModelChanged() ? 1 : 0).append('\n');
        UcarimaModel um = r.getUcarimaModel();
        dumpUcarima(sb, "k.ucar", um);
      } catch (RuntimeException e) {
        sb.append("status error:").append(e.getClass().getName());
        if (e.getMessage() != null) {
          sb.append(':').append(e.getMessage().replace('\n', ' '));
        }
        sb.append('\n');
      }
      sb.append(kobs.sb);
    } catch (RuntimeException e) {
      sb.append("status exception:").append(e.getClass().getName());
      if (e.getMessage() != null) {
        sb.append(':').append(e.getMessage().replace('\n', ' '));
      }
      sb.append('\n');
    }
    return sb.toString();
  }

  static double[] dbls(String[] tk) {
    double[] v = new double[tk.length - 1];
    for (int i = 1; i < tk.length; ++i) {
      v[i - 1] = Double.parseDouble(tk[i]);
    }
    return v;
  }

  static Parameter[] defs(double[] v) {
    if (v == null) {
      return new Parameter[0];
    }
    Parameter[] r = new Parameter[v.length];
    for (int i = 0; i < v.length; ++i) {
      r[i] = Parameter.of(v[i], jdplus.toolkit.base.api.data.ParameterType.Fixed);
    }
    return r;
  }
}
