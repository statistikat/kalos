/*
 * Standalone pinned-reference observer for the item 21-B transformation-
 * decision corpus (see validation/reference/capture-transformation-
 * decision.R).
 *
 * It implements no estimation algorithm and transcribes no decision rule.
 * It builds the same ModelDescription the pinned stage-T driver hands to
 * the module (monthly TsData, explicit regression variables, mean off,
 * caller-given SarimaOrders), wraps it in a RegSarimaModelling, constructs
 * the pinned sa-core LogLevelModule through its public builder with the
 * decoder-pinned values (estimationPrecision 1.0E-7, preadjust None,
 * outliersCorrection false; the AICC correction arrives as an argument) and
 * runs LogLevelModule.process. It then prints the module's own decision
 * bits and both branch estimations: isChoosingLog, the aiccLevel/aiccLog
 * module fields and, per branch, the final SarimaModel.parameters() vector
 * (the pinned raw [phi, bphi, theta, btheta] coefficients, "null" when the
 * module keeps no estimation there), the concentrated log-likelihood and
 * the LikelihoodStatistics observation/parameter counts and AICC. The
 * design is passed as raw columns wrapped in the product's own UserVariable
 * (the X13ModelBuilder user-variable path): a Constant.C Variable added
 * this way is silently dropped by the pinned check(), so explicit columns
 * must arrive as user variables to reach the estimation. The level-branch
 * design matrix actually used (regarima().variables(), after any pinned
 * column exclusion) is printed so the replay re-creates the identical
 * columns. Failures are reported as a single "error|..." line.
 */
import jdplus.sa.base.core.regarima.LogLevelModule;
import jdplus.toolkit.base.api.arima.SarimaOrders;
import jdplus.toolkit.base.api.timeseries.TsData;
import jdplus.toolkit.base.api.timeseries.TsPeriod;
import jdplus.toolkit.base.api.timeseries.calendars.LengthOfPeriodType;
import jdplus.toolkit.base.api.timeseries.regression.UserVariable;
import jdplus.toolkit.base.api.timeseries.regression.Variable;
import jdplus.toolkit.base.core.regarima.RegArimaEstimation;
import jdplus.toolkit.base.core.regarima.RegArimaModel;
import jdplus.toolkit.base.core.sarima.SarimaModel;
import jdplus.toolkit.base.core.stats.likelihood.LikelihoodStatistics;
import jdplus.toolkit.base.core.regsarima.regular.ModelDescription;
import jdplus.toolkit.base.core.regsarima.regular.RegSarimaModelling;

public final class TransformationDecisionProbe {

  private TransformationDecisionProbe() {
  }

  private static String num(double v) {
    return String.format(java.util.Locale.ROOT, "%.17g", v);
  }

  private static String branch(RegArimaEstimation<SarimaModel> est) {
    if (est == null) {
      return "null|-|-|-|-|-";
    }
    double[] par = est.getModel().arima().parameters().toArray();
    LikelihoodStatistics st = est.statistics();
    StringBuilder sb = new StringBuilder();
    for (int i = 0; i < par.length; ++i) {
      if (i != 0) {
        sb.append(';');
      }
      sb.append(num(par[i]));
    }
    sb.append('|').append(num(est.getConcentratedLikelihood().logLikelihood()));
    sb.append('|').append(st.getObservationsCount());
    sb.append('|').append(st.getEffectiveObservationsCount());
    sb.append('|').append(st.getEstimatedParametersCount());
    sb.append('|').append(num(st.getAICC()));
    return sb.toString();
  }

  public static String run(int period, int d, int seasonalD, double[] y, double[] x,
      int columns, int p, int q, int bp, int bq, double correction) {
    try {
      SarimaOrders orders = new SarimaOrders(period);
      orders.setP(p);
      orders.setD(d);
      orders.setQ(q);
      orders.setBp(bp);
      orders.setBd(seasonalD);
      orders.setBq(bq);
      TsData data = TsData.ofInternal(TsPeriod.monthly(2000, 1), y);
      ModelDescription desc = new ModelDescription(data, null);
      desc.setMean(false);
      desc.setSpecification(orders);
      for (int c = 0; c < columns; ++c) {
        double[] column = new double[y.length];
        System.arraycopy(x, c * y.length, column, 0, y.length);
        TsData xv = TsData.ofInternal(TsPeriod.monthly(2000, 1), column);
        desc.addVariable(Variable.variable("u" + (c + 1), new UserVariable("u" + (c + 1), xv)));
      }
      RegSarimaModelling modelling = RegSarimaModelling.of(desc);
      LogLevelModule module = LogLevelModule.builder()
          .aiccLogCorrection(correction)
          .estimationPrecision(1.0E-7)
          .preadjust(LengthOfPeriodType.None)
          .outliersCorrection(false)
          .build();
      module.process(modelling);
      // The design actually consumed by the level branch, after any pinned
      // exclusion inside regarima(); the log branch shares it unchanged.
      RegArimaModel<SarimaModel> model = desc.regarima();
      int cols = model.variables().getColumnsCount();
      int rows = model.variables().getRowsCount();
      StringBuilder mx = new StringBuilder();
      for (int c = 0; c < cols; ++c) {
        for (int r = 0; r < rows; ++r) {
          if (c != 0 || r != 0) {
            mx.append(';');
          }
          mx.append(num(model.variables().get(r, c)));
        }
      }
      StringBuilder sb = new StringBuilder("ok|");
      sb.append(module.isChoosingLog() ? 1 : 0).append('|');
      sb.append(num(module.getAICcLevel())).append('|').append(num(module.getAICcLog()));
      sb.append('|').append(branch(module.getLevel()));
      sb.append('|').append(branch(module.getLog()));
      sb.append('|').append(cols).append('|').append(mx);
      return sb.toString();
    } catch (Exception error) {
      return "error|" + error.getClass().getName() + ": " + error.getMessage();
    }
  }
}
