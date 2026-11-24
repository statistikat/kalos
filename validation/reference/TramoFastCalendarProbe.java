import java.util.LinkedHashSet;
import java.util.Map;
import jdplus.toolkit.base.api.data.DoubleSeq;
import jdplus.toolkit.base.api.timeseries.TsData;
import jdplus.toolkit.base.api.timeseries.TsPeriod;
import jdplus.toolkit.base.api.timeseries.calendars.*;
import jdplus.toolkit.base.api.timeseries.regression.*;
import jdplus.toolkit.base.core.regsarima.regular.*;
import jdplus.tramoseats.base.core.tramo.FastRegressionTest;

/** Observe the actual calendar removals and equality boundaries of FastRegressionTest. */
public final class TramoFastCalendarProbe {
    private static String vector(String name, DoubleSeq values) {
        var text = new StringBuilder(name);
        for (double x : values.toArray()) text.append(" ").append(CalendarFTestProbe.bits(x));
        return text.toString();
    }
    private static boolean has(ModelDescription d, String name) {
        return d.variables().anyMatch(v -> v.getName().equals(name));
    }
    public static void main(String[] args) {
        var start = TsPeriod.monthly(1990, 1);
        var attrs = Map.of("ami", "tramo");
        for (int shape : new int[]{0, 1, 2, 3}) {
            for (int effect : new int[]{0, 1, 2}) {
                var data = TsData.of(start, DoubleSeq.of(CalendarFTestProbe.series(effect)));
                var d = new ModelDescription(data, null);
                boolean mean = effect != 0;
                d.setAirline(true);
                d.setMean(mean);
                var td = new GenericTradingDaysVariable(shape == 1 ? DayClustering.TD7 : DayClustering.TD2);
                var lp = new LengthOfPeriod(LengthOfPeriodType.LeapYear);
                var easter = new JulianEasterVariable(2, false);
                if (shape != 2) d.addVariable(Variable.variable("td", td, attrs));
                if (shape != 3) d.addVariable(Variable.variable("lp", lp, attrs));
                d.addVariable(Variable.variable("easter", easter, attrs));
                var fit = RegSarimaModelling.of(d);
                fit.estimate(1e-7);
                var cl = fit.getEstimation().getConcentratedLikelihood();
                int tpos = shape == 2 ? -1 : d.findPosition(td), lpos = shape == 3 ? -1 : d.findPosition(lp);
                int epos = d.findPosition(easter), k = d.getArimaSpec().freeParametersCount();
                System.out.printf("fit %b %d %d %d %d %d %d %s%n", mean, tpos, td.dim(), lpos, epos,
                    k, cl.dim(), CalendarFTestProbe.bits(cl.ssq()));
                System.out.println(vector("coef", cl.coefficients()));
                System.out.println(vector("diag", cl.unscaledCovariance().diagonal()));
                var cuts = new LinkedHashSet<Double>();
                cuts.add(1.96);
                cuts.add(0.0);
                for (int i = 0; i < cl.coefficients().length(); ++i) {
                    double t = Math.abs(cl.tstat(i, k, true));
                    cuts.add(t);
                    cuts.add(Math.nextUp(t));
                }
                for (double threshold : cuts) {
                    var model = RegSarimaModelling.copyOf(fit);
                    var status = FastRegressionTest.builder().testMean(true).varThreshold(threshold).build().test(model);
                    var post = model.getDescription();
                    System.out.printf("result %s %s %b %b %b %b %b%n", CalendarFTestProbe.bits(threshold),
                        status, post.isMean(), has(post, "td"), has(post, "lp"), has(post, "easter"),
                        model.getEstimation() == null);
                }
            }
        }
    }
}
