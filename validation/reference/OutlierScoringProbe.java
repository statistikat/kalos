/*
 * Standalone pinned-reference observer for the item 21-C outlier-scoring
 * corpus (see validation/reference/capture-outlier-scoring.R).
 *
 * It implements no scoring, scan or selection algorithm and transcribes no
 * decision rule. Every mode is a thin observer over the pinned jdplus 3.9.0
 * classes: cdf/quantile/calccv call the public statics the pinned critical
 * value chain uses (internal.toolkit.base.core.dstats.Utility.intProbability,
 * jdplus.toolkit.base.core.dstats.Normal.getProbabilityInverse and
 * jdplus.x13.base.core.x13.regarima.X13Utility.calcCv next to the raw
 * CriticalValueComputer.advancedComputer(0.05)); scan runs the pinned
 * ExactSingleOutlierDetector process(T) through its public API and prints the
 * full T and coefficient tables, the searchMax winner and the exclusion
 * state; driver runs the pinned ExactOutliersDetector process(regarima, map)
 * with the pinned X13Utility.processor(true, 1.0E-7) and prints the changed
 * flag, the kept (position, factory) pairs, the final tstats vector, the
 * final design column count and the final SarimaModel.parameters() vector
 * (the pinned raw [phi, bphi, theta, btheta] coefficients). The tstats
 * vector is the driver's private decision array; it has no public getter, so
 * it is read reflectively for observation only - nothing here computes it.
 * All doubles print with %.17g. Failures are reported as one "error|..."
 * line instead of throwing.
 *
 * Construction mirrors OutliersDetectionModule.make: the scan detector is
 * ExactSingleOutlierDetector(RobustStandardDeviationComputer.mad(false),
 * null, X13Utility.mlComputer()) with the pinned factory order AO, LS
 * (zero-ended), TC (TransitoryChangeFactory.rate(freq, tcrate)) and SO
 * (zero-ended, freq > 1), prepare(n) then setBounds(l, r) then the missing
 * exclusions. The driver is ExactOutliersDetector.builder() with that
 * detector, the caller's critical value, the pinned processor and the caller
 * maxRound/maxOutliers (the x13 module decoder uses 50/30, the driver
 * statics are 100/50); mapping null is the fully free map, matching the
 * qualified 21-A null-mapping convention.
 */
import internal.toolkit.base.core.dstats.Utility;
import jdplus.toolkit.base.api.arima.SarimaOrders;
import jdplus.toolkit.base.api.data.Doubles;
import jdplus.toolkit.base.api.stats.ProbabilityType;
import jdplus.toolkit.base.core.dstats.Normal;
import jdplus.toolkit.base.core.math.matrices.FastMatrix;
import jdplus.toolkit.base.core.modelling.regression.AdditiveOutlierFactory;
import jdplus.toolkit.base.core.modelling.regression.IOutlierFactory;
import jdplus.toolkit.base.core.modelling.regression.LevelShiftFactory;
import jdplus.toolkit.base.core.modelling.regression.PeriodicOutlierFactory;
import jdplus.toolkit.base.core.modelling.regression.TransitoryChangeFactory;
import jdplus.toolkit.base.core.regarima.RegArimaModel;
import jdplus.toolkit.base.core.regarima.outlier.CriticalValueComputer;
import jdplus.toolkit.base.core.regarima.outlier.ExactSingleOutlierDetector;
import jdplus.toolkit.base.core.regarima.outlier.SingleOutlierDetector;
import jdplus.toolkit.base.core.regsarima.ami.ExactOutliersDetector;
import jdplus.toolkit.base.core.sarima.SarimaModel;
import jdplus.toolkit.base.core.stats.RobustStandardDeviationComputer;
import jdplus.x13.base.core.x13.regarima.X13Utility;
import java.lang.reflect.Field;

public final class OutlierScoringProbe {

  private OutlierScoringProbe() {
  }

  private static String fmt(double v) {
    return String.format(java.util.Locale.ROOT, "%.17g", v);
  }

  private static String error(Throwable e) {
    return "error|" + e.getClass().getName() + ": " + e.getMessage();
  }

  // ---- critical values -------------------------------------------------------

  public static String cdf(double x) {
    try {
      return "ok|" + fmt(Utility.intProbability(x, ProbabilityType.Lower));
    } catch (Throwable e) {
      return error(e);
    }
  }

  public static String quantile(double p) {
    try {
      return "ok|" + fmt(new Normal().getProbabilityInverse(p,
          ProbabilityType.Lower));
    } catch (Throwable e) {
      return error(e);
    }
  }

  public static String calccv(int n) {
    try {
      double adv = CriticalValueComputer.advancedComputer(0.05).applyAsDouble(n);
      return "ok|" + fmt(X13Utility.calcCv(n)) + "|" + fmt(adv);
    } catch (Throwable e) {
      return error(e);
    }
  }

  // ---- shared model construction ----------------------------------------------

  private static SarimaModel sarima(int period, int d, int seasonalD,
      double[] phi, double[] bphi, double[] theta, double[] btheta) {
    SarimaOrders orders = new SarimaOrders(period);
    orders.setP(phi.length);
    orders.setD(d);
    orders.setQ(theta.length);
    orders.setBp(bphi.length);
    orders.setBd(seasonalD);
    orders.setBq(btheta.length);
    double[] par = new double[phi.length + bphi.length + theta.length
        + btheta.length];
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
    return SarimaModel.builder(orders).parameters(Doubles.of(par)).build();
  }

  private static RegArimaModel<SarimaModel> regarima(double[] y, double[] x,
      int columns, SarimaModel model) {
    RegArimaModel.Builder<SarimaModel> builder
        = RegArimaModel.<SarimaModel>builder()
            .y(Doubles.of(y)).arima(model).meanCorrection(false);
    if (columns > 0) {
      FastMatrix xmat = FastMatrix.make(y.length, columns);
      for (int c = 0; c < columns; ++c) {
        for (int r = 0; r < y.length; ++r) {
          xmat.set(r, c, x[c * y.length + r]);
        }
      }
      builder = builder.addX(xmat);
    }
    return builder.build();
  }

  // Pinned factory order over the type codes 0 = AO, 1 = LS, 2 = TC, 3 = SO.
  private static IOutlierFactory[] factories(int[] codes, int freq,
      double tcrate) {
    IOutlierFactory[] fs = new IOutlierFactory[codes.length];
    for (int i = 0; i < codes.length; ++i) {
      switch (codes[i]) {
        case 0:
          fs[i] = AdditiveOutlierFactory.FACTORY;
          break;
        case 1:
          fs[i] = LevelShiftFactory.FACTORY_ZEROENDED;
          break;
        case 2:
          fs[i] = new TransitoryChangeFactory(
              TransitoryChangeFactory.rate(freq, tcrate));
          break;
        case 3:
          if (freq <= 1) {
            throw new IllegalArgumentException(
                "a periodic outlier factory needs freq > 1");
          }
          fs[i] = new PeriodicOutlierFactory(freq, true);
          break;
        default:
          throw new IllegalArgumentException("unknown outlier code " + codes[i]);
      }
    }
    return fs;
  }

  @SuppressWarnings("unchecked")
  private static SingleOutlierDetector<SarimaModel> scanDetector(int[] codes,
      int freq, double tcrate) {
    ExactSingleOutlierDetector<SarimaModel> det = new ExactSingleOutlierDetector(
        RobustStandardDeviationComputer.mad(false), null,
        X13Utility.mlComputer());
    det.setOutlierFactories(factories(codes, freq, tcrate));
    return det;
  }

  // ---- scan unit ---------------------------------------------------------------

  public static String scan(int period, int d, int seasonalD, double[] y,
      double[] x, int columns, double[] phi, double[] bphi, double[] theta,
      double[] btheta, int[] codes, double tcrate, int lbound, int ubound,
      int[] exclPos, int[] exclType) {
    try {
      SingleOutlierDetector<SarimaModel> sod
          = scanDetector(codes, period, tcrate);
      sod.prepare(y.length);
      sod.setBounds(lbound, ubound);
      for (int i = 0; i < exclPos.length; ++i) {
        sod.exclude(exclPos[i], exclType[i]);
      }
      RegArimaModel<SarimaModel> model = regarima(y, x, columns,
          sarima(period, d, seasonalD, phi, bphi, theta, btheta));
      boolean ok = sod.process(model);
      int rows = y.length, nf = codes.length;
      StringBuilder sb = new StringBuilder("ok|");
      sb.append(ok ? 1 : 0).append('|').append(fmt(sod.getMaxTStat()))
          .append('|').append(sod.getMaxOutlierType()).append('|')
          .append(sod.getMaxOutlierPosition()).append('|');
      for (int j = 0; j < nf; ++j) {
        for (int p = 0; p < rows; ++p) {
          if (j != 0 || p != 0) {
            sb.append(';');
          }
          sb.append(fmt(sod.T(p, j)));
        }
      }
      sb.append('|');
      for (int j = 0; j < nf; ++j) {
        for (int p = 0; p < rows; ++p) {
          if (j != 0 || p != 0) {
            sb.append(';');
          }
          sb.append(fmt(sod.coefficient(p, j)));
        }
      }
      sb.append('|');
      for (int j = 0; j < nf; ++j) {
        for (int p = 0; p < rows; ++p) {
          if (j != 0 || p != 0) {
            sb.append(';');
          }
          sb.append(sod.isAllowed(p, j) ? 1 : 0);
        }
      }
      return sb.toString();
    } catch (Throwable e) {
      return error(e);
    }
  }

  // ---- driver ------------------------------------------------------------------


  public static String driver(int period, int d, int seasonalD, double[] y,
      double[] x, int columns, double[] phi, double[] bphi, double[] theta,
      double[] btheta, int[] codes, double tcrate, double cv, int maxRound,
      int maxOutliers, int lbound, int ubound, int[] exclPos, int[] exclType) {
    try {
      ExactOutliersDetector det = ExactOutliersDetector.builder()
          .singleOutlierDetector(scanDetector(codes, period, tcrate))
          .criticalValue(cv)
          .processor(X13Utility.processor(true, 1.0E-7))
          .maxRound(maxRound)
          .maxOutliers(maxOutliers)
          .build();
      det.prepare(y.length);
      det.setBounds(lbound, ubound);
      for (int i = 0; i < exclPos.length; ++i) {
        det.exclude(exclPos[i], exclType[i]);
      }
      RegArimaModel<SarimaModel> model = regarima(y, x, columns,
          sarima(period, d, seasonalD, phi, bphi, theta, btheta));
      boolean changed = det.process(model, null);
      int[][] outs = det.getOutliers();
      Field f = ExactOutliersDetector.class.getDeclaredField("tstats");
      f.setAccessible(true);
      double[] tstats = (double[]) f.get(det);
      double[] par = det.getRegArima().arima().parameters().toArray();
      StringBuilder sb = new StringBuilder("ok|");
      sb.append(changed ? 1 : 0).append('|').append(outs.length).append('|');
      for (int i = 0; i < outs.length; ++i) {
        if (i != 0) {
          sb.append(';');
        }
        sb.append(outs[i][0]).append(',').append(outs[i][1]);
      }
      sb.append('|');
      if (tstats == null) {
        sb.append("null");
      } else {
        for (int i = 0; i < tstats.length; ++i) {
          if (i != 0) {
            sb.append(';');
          }
          sb.append(fmt(tstats[i]));
        }
      }
      sb.append('|').append(det.getRegArima().getXCount()).append('|');
      for (int i = 0; i < par.length; ++i) {
        if (i != 0) {
          sb.append(';');
        }
        sb.append(fmt(par[i]));
      }
      return sb.toString();
    } catch (Throwable e) {
      return error(e);
    }
  }
}
