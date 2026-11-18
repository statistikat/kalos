/*
 * Standalone pinned-reference observer for the item 21-D differencing-decision
 * corpus (see validation/reference/capture-differencing-decision.R).
 *
 * It implements no decision rule and transcribes no threshold. decide()
 * constructs the pinned jdplus 3.9.0 jdplus.x13.base.core.x13.regarima.
 * DifferencingModule exactly as X13SpecDecoder.readAutoModel does -
 * DifferencingModule.builder().cancel(spec.getCancel()).ub1(1.0 /
 * spec.getUb1()).ub2(1.0 / spec.getUb2()).precision(1.0E-7).build() over the
 * default AutoModelSpec (DEF_CANCEL 0.1, DEF_UB1 1.0309278350515465,
 * DEF_UB2 1.1363636363636365; maxd/maxbd keep the builder defaults 2/1, the
 * module ml flag is never set) - then runs the public process(DoubleSeq, int)
 * entry point on the case series and prints the IDifferencingModule$Info
 * quadruple (d, bd, mean, tmean) plus the machine state the pinned
 * IDifferencingModule chain itself reads after the run: the effective request
 * scalars and the private fields iter, mlused, useml, rmax, rsmax, c_, the
 * final SarimaOrders p/q/bp/bq and the final SarimaModel parameter blocks
 * [phi, bphi, theta, btheta] in the pinned raw convention. The private fields
 * have no public getters; they are read reflectively for observation only,
 * following the OutlierScoringProbe precedent - nothing here computes them.
 * The 1/ub1 and 1/ub2 divisions are performed by the JVM, so the corpus
 * stores the exact doubles the pinned machine used. All doubles print with
 * %.17g. Failures are reported as one "error|..." line instead of throwing.
 */
import jdplus.toolkit.base.api.data.Doubles;
import jdplus.x13.base.api.regarima.AutoModelSpec;
import jdplus.x13.base.core.x13.regarima.DifferencingModule;
import java.lang.reflect.Field;

public final class DifferencingDecisionProbe {

  private DifferencingDecisionProbe() {
  }

  private static String fmt(double v) {
    return String.format(java.util.Locale.ROOT, "%.17g", v);
  }

  private static String error(Throwable e) {
    String msg = e.getMessage();
    return "error|" + e.getClass().getName() + ": " + (msg == null ? "" : msg);
  }

  private static double field(Object o, String name) throws Exception {
    Field f = DifferencingModule.class.getDeclaredField(name);
    f.setAccessible(true);
    return f.getDouble(o);
  }

  private static int ifield(Object o, String name) throws Exception {
    Field f = DifferencingModule.class.getDeclaredField(name);
    f.setAccessible(true);
    return f.getInt(o);
  }

  private static boolean bfield(Object o, String name) throws Exception {
    Field f = DifferencingModule.class.getDeclaredField(name);
    f.setAccessible(true);
    return f.getBoolean(o);
  }

  public static String decide(int period, double[] y) {
    try {
      AutoModelSpec spec = AutoModelSpec.builder().build();
      DifferencingModule module = DifferencingModule.builder()
          .cancel(spec.getCancel())
          .ub1(1.0 / spec.getUb1())
          .ub2(1.0 / spec.getUb2())
          .precision(1.0E-7)
          .build();
      boolean ok = module.process(Doubles.of(y), period);
      StringBuilder sb = new StringBuilder("ok|");
      sb.append(ok ? 1 : 0).append('|')
          .append(module.getD()).append('|').append(module.getBd()).append('|')
          .append(module.isMeanCorrection() ? 1 : 0).append('|')
          .append(fmt(module.getTMean())).append('|');
      // Effective request scalars as the pinned module stored them.
      sb.append(ifield(module, "maxd")).append('|').append(ifield(module, "maxbd"))
          .append('|').append(fmt(field(module, "ub1"))).append('|')
          .append(fmt(field(module, "ub2"))).append('|')
          .append(fmt(field(module, "cancel"))).append('|')
          .append(fmt(field(module, "eps"))).append('|')
          .append(bfield(module, "ml") ? 1 : 0).append('|');
      // Machine end state (reflective observation only).
      sb.append(ifield(module, "iter")).append('|')
          .append(bfield(module, "mlused") ? 1 : 0).append('|')
          .append(bfield(module, "useml") ? 1 : 0).append('|')
          .append(fmt(field(module, "rmax"))).append('|')
          .append(fmt(field(module, "rsmax"))).append('|')
          .append(fmt(field(module, "c_"))).append('|');
      // Final spec orders and final raw model parameters.
      Field specField = DifferencingModule.class.getDeclaredField("spec");
      specField.setAccessible(true);
      jdplus.toolkit.base.api.arima.SarimaOrders orders
          = (jdplus.toolkit.base.api.arima.SarimaOrders) specField.get(module);
      sb.append(orders.getP()).append(',').append(orders.getQ()).append(',')
          .append(orders.getBp()).append(',').append(orders.getBq()).append('|');
      Field modelField = DifferencingModule.class.getDeclaredField("lastModel");
      modelField.setAccessible(true);
      jdplus.toolkit.base.core.sarima.SarimaModel model
          = (jdplus.toolkit.base.core.sarima.SarimaModel) modelField.get(module);
      boolean first = true;
      for (int i = 1; i <= orders.getP(); ++i) {
        if (!first) {
          sb.append(';');
        }
        first = false;
        sb.append(fmt(model.phi(i)));
      }
      for (int i = 1; i <= orders.getBp(); ++i) {
        sb.append(';').append(fmt(model.bphi(i)));
      }
      for (int i = 1; i <= orders.getQ(); ++i) {
        sb.append(';').append(fmt(model.theta(i)));
      }
      for (int i = 1; i <= orders.getBq(); ++i) {
        sb.append(';').append(fmt(model.btheta(i)));
      }
      return sb.toString();
    } catch (Throwable e) {
      return error(e);
    }
  }
}
