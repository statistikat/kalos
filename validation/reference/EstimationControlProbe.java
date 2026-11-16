/*
 * Standalone pinned-reference observer for the item 21-A estimation-control
 * corpus (see validation/reference/capture-estimation-control.R).
 *
 * It implements no estimation algorithm. It builds one complete fixed-order
 * regression model (a SarimaModel with caller-provided stationary-part
 * coefficients, optional regression columns and explicit missing positions),
 * constructs the pinned jdplus RegSarimaComputer either through its public
 * constructor with the exact pinned builder-default field values or with a
 * caller-selected starting point, runs RegSarimaComputer.process and prints
 * the final orders, the final SarimaModel.parameters() vector (the pinned
 * raw [phi, bphi, theta, btheta] polynomial coefficients) and the final
 * concentrated log-likelihood with 17 significant digits. Failures are
 * reported as a single "error|..." line instead of throwing, mirroring the
 * pinned exception-swallowing branches of the unit-root cancellation passes.
 */
import jdplus.toolkit.base.api.arima.SarimaOrders;
import jdplus.toolkit.base.api.data.Doubles;
import jdplus.toolkit.base.core.math.matrices.FastMatrix;
import jdplus.toolkit.base.core.regarima.RegArimaEstimation;
import jdplus.toolkit.base.core.regarima.RegArimaModel;
import jdplus.toolkit.base.core.regsarima.RegSarimaComputer;
import jdplus.toolkit.base.core.sarima.SarimaModel;

public final class EstimationControlProbe {

  private EstimationControlProbe() {
  }

  public static String run(int period, int d, int seasonalD, double[] y, double[] x,
      int columns, int[] missing, double[] phi, double[] bphi, double[] theta,
      double[] btheta, String startingPoint, double eps, double feps) {
    try {
      SarimaOrders orders = new SarimaOrders(period);
      orders.setP(phi.length);
      orders.setD(d);
      orders.setQ(theta.length);
      orders.setBp(bphi.length);
      orders.setBd(seasonalD);
      orders.setBq(btheta.length);
      double[] par = new double[phi.length + bphi.length + theta.length + btheta.length];
      int k = 0;
      for (double v : phi) {
        par[k++] = v;
      }
      for (double v : bphi) {
        par[k++] = v;
      }
      for (double v : theta) {
        par[k++] = v;
      }
      for (double v : btheta) {
        par[k++] = v;
      }
      SarimaModel sarima = SarimaModel.builder(orders)
          .parameters(Doubles.of(par)).build();
      RegArimaModel.Builder<SarimaModel> model
          = RegArimaModel.<SarimaModel>builder()
              .y(Doubles.of(y)).arima(sarima).meanCorrection(false).missing(missing);
      if (columns > 0) {
        FastMatrix xmat = FastMatrix.make(y.length, columns);
        for (int c = 0; c < columns; ++c) {
          for (int r = 0; r < y.length; ++r) {
            xmat.set(r, c, x[c * y.length + r]);
          }
        }
        model = model.addX(xmat);
      }
      // The pinned builder defaults are eps = 1.0E-7, feps = 1.0E-4,
      // ml = true, mt = false, cdf = true, fast = true and a default
      // LevenbergMarquardtMinimizer builder; only the starting point moves.
      RegSarimaComputer computer = new RegSarimaComputer(
          jdplus.toolkit.base.core.math.functions.levmar
              .LevenbergMarquardtMinimizer.builder(),
          eps, feps, true,
          RegSarimaComputer.StartingPoint.valueOf(startingPoint), true, false, true);
      RegArimaEstimation<SarimaModel> est
          = computer.process(model.build(), null);
      SarimaModel m = est.getModel().arima();
      SarimaOrders o = m.orders();
      double[] p = m.parameters().toArray();
      StringBuilder sb = new StringBuilder("ok|");
      sb.append(o.getP()).append('|').append(o.getQ()).append('|')
          .append(o.getBp()).append('|').append(o.getBq()).append('|')
          .append(o.getDifferenceOrder()).append('|').append(o.getBd()).append('|');
      for (int i = 0; i < p.length; ++i) {
        if (i != 0) {
          sb.append(';');
        }
        sb.append(String.format(java.util.Locale.ROOT, "%.17g", p[i]));
      }
      sb.append('|').append(String.format(java.util.Locale.ROOT, "%.17g",
          est.getConcentratedLikelihood().logLikelihood()));
      return sb.toString();
    } catch (Exception error) {
      return "error|" + error.getClass().getName() + ": " + error.getMessage();
    }
  }
}
