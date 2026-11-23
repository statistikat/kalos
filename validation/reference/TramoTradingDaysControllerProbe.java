import java.lang.reflect.Constructor;
import java.lang.reflect.Method;
import java.util.HashMap;
import java.util.Map;
import java.util.List;
import jdplus.toolkit.base.api.data.DoubleSeq;
import jdplus.toolkit.base.api.timeseries.TsData;
import jdplus.toolkit.base.api.timeseries.TsPeriod;
import jdplus.toolkit.base.api.timeseries.calendars.DayClustering;
import jdplus.toolkit.base.api.timeseries.regression.GenericTradingDaysVariable;
import jdplus.toolkit.base.api.timeseries.regression.ITradingDaysVariable;
import jdplus.toolkit.base.core.regsarima.regular.IModelEstimator;
import jdplus.toolkit.base.core.regsarima.regular.ModelDescription;
import jdplus.toolkit.base.core.regsarima.regular.RegSarimaModelling;
import jdplus.tramoseats.base.core.tramo.ModelStatistics;

/** Run the late controller with its real estimator, observing each accepted or rejected candidate. */
public final class TramoTradingDaysControllerProbe {
    private static final Map<String, Integer> residualIds = new HashMap<>();

    private static void fit(String label, RegSarimaModelling model) {
        var d = model.getDescription();
        var cl = model.getEstimation().getConcentratedLikelihood();
        var stats = ModelStatistics.of(d, cl);
        System.out.printf("fit %s %b %d %s %s %s %s %s %s %s %s %s %s %s%n",
            label, d.isMean(), d.variables().count(), CalendarFTestProbe.bits(cl.ssq()),
            CalendarFTestProbe.bits(stats.getBic()), CalendarFTestProbe.bits(stats.getLjungBox()),
            CalendarFTestProbe.bits(stats.getLjungBoxPvalue()), CalendarFTestProbe.bits(stats.getSeasonalLjungBox()),
            CalendarFTestProbe.bits(stats.getSeasonalLjungBoxPvalue()), CalendarFTestProbe.bits(stats.getSkewnessAbsvalue()),
            CalendarFTestProbe.bits(stats.getSkewnessPvalue()), CalendarFTestProbe.bits(stats.getStableMean()),
            CalendarFTestProbe.bits(stats.getStableMeanPvalue()), CalendarFTestProbe.bits(stats.getStableVariance()));
        System.out.print("params");
        for (double x : d.arima().parameters().toArray()) System.out.print(" " + CalendarFTestProbe.bits(x));
        System.out.print("\nbeta");
        for (double x : cl.coefficients().toArray()) System.out.print(" " + CalendarFTestProbe.bits(x));
        System.out.println();
        System.out.println("variance_p " + CalendarFTestProbe.bits(stats.getStableVariancePvalue()));
        StringBuilder residuals = new StringBuilder();
        for (double x : cl.e().toArray()) residuals.append(" ").append(CalendarFTestProbe.bits(x));
        String values = residuals.toString();
        int next = residualIds.size();
        Integer existing = residualIds.putIfAbsent(values, next);
        if (existing == null) System.out.println("residual_values " + next + values);
        System.out.println("residual_id " + (existing == null ? next : existing));
    }

    public static void main(String[] args) throws Exception {
        String pkg = "jdplus.tramoseats.base.core.tramo.";
        Class<?> ctrl = Class.forName(pkg + "TradingDaysController"), ctx = Class.forName(pkg + "TramoContext");
        Constructor<?> makeController = ctrl.getDeclaredConstructor(ITradingDaysVariable.class, double.class);
        Constructor<?> makeContext = ctx.getDeclaredConstructor();
        Constructor<?> makeEstimator = Class.forName(pkg + "ModelEstimator").getDeclaredConstructor(
            double.class, double.class, Class.forName(pkg + "internal.OutliersDetectionModule"));
        Method setEstimator = ctrl.getSuperclass().getDeclaredMethod("setEstimator", IModelEstimator.class);
        Method process = ctrl.getDeclaredMethod("process", RegSarimaModelling.class, ctx);
        for (var member : List.of(makeController, makeEstimator, makeContext, setEstimator, process))
            member.setAccessible(true);
        var td = new GenericTradingDaysVariable(DayClustering.TD2);
        for (int n : new int[]{180, 272}) {
            for (int effect = 0; effect < 4; ++effect) {
                double[] y = CalendarFTestProbe.series(effect, n);
                StringBuilder input = new StringBuilder("series " + n + " " + effect);
                for (double x : y) input.append(" ").append(CalendarFTestProbe.bits(x));
                System.out.println(input);
                for (boolean mean : new boolean[]{false, true}) {
                    for (double alpha : new double[]{0.01, 1.0}) {
                        System.out.printf("case %b %s%n", mean, CalendarFTestProbe.bits(alpha));
                        var data = TsData.of(TsPeriod.monthly(1990, 1), DoubleSeq.of(y));
                        var description = new ModelDescription(data, null);
                        description.setAirline(true);
                        description.setMean(mean);
                        var frame = RegSarimaModelling.of(description);
                        frame.estimate(1e-7);
                        fit("original", frame);
                        var controller = makeController.newInstance(td, alpha);
                        var delegate = (IModelEstimator) makeEstimator.newInstance(1e-7, 3.5, null);
                        int[] attempts = {0};
                        IModelEstimator observer = candidate -> {
                            boolean ok = delegate.estimate(candidate);
                            if (ok) fit("candidate", candidate);
                            ++attempts[0];
                            return ok;
                        };
                        setEstimator.invoke(controller, observer);
                        var status = process.invoke(controller, frame, makeContext.newInstance());
                        System.out.printf("result %s %d %b %d%n", status, attempts[0],
                            frame.getDescription().isMean(), frame.getDescription().variables().count());
                    }
                }
            }
        }
    }
}
