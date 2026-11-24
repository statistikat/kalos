import java.lang.reflect.Method;
import jdplus.toolkit.base.api.data.DoubleSeq;
import jdplus.toolkit.base.api.timeseries.TsData;
import jdplus.toolkit.base.api.timeseries.TsPeriod;
import jdplus.toolkit.base.api.timeseries.calendars.DayClustering;
import jdplus.toolkit.base.api.timeseries.regression.GenericTradingDaysVariable;
import jdplus.toolkit.base.api.timeseries.regression.ITradingDaysVariable;
import jdplus.toolkit.base.core.modelling.regression.Regression;
import jdplus.toolkit.base.core.regsarima.regular.ModelDescription;
import jdplus.toolkit.base.core.regsarima.regular.RegSarimaModelling;
import jdplus.toolkit.base.core.stats.linearmodel.LinearModel;
import jdplus.toolkit.base.core.stats.linearmodel.Ols;

/** Observe the residual OLS test and the actual TradingDaysController.needProcessing decision. */
public final class TramoResidualTradingDaysProbe {
    public static void main(String[] args) throws Exception {
        var type = Class.forName("jdplus.tramoseats.base.core.tramo.TradingDaysController");
        var constructor = type.getDeclaredConstructor(ITradingDaysVariable.class, double.class);
        Method needProcessing = type.getDeclaredMethod("needProcessing", RegSarimaModelling.class);
        constructor.setAccessible(true);
        needProcessing.setAccessible(true);
        var workingDays = new GenericTradingDaysVariable(DayClustering.TD2);
        for (int effect = 0; effect < 4; ++effect) {
            for (boolean mean : new boolean[]{false, true}) {
                int year = 1990 + effect, month = 1 + 2*effect;
                var start = TsPeriod.monthly(year, month);
                var data = TsData.of(start, DoubleSeq.of(CalendarFTestProbe.series(effect)));
                var description = new ModelDescription(data, null);
                description.setAirline(true);
                description.setMean(mean);
                var frame = RegSarimaModelling.of(description);
                frame.estimate(1e-7);
                var e = frame.getEstimation().getConcentratedLikelihood().e();
                var domain = description.getEstimationDomain().drop(data.length() - e.length(), 0);
                var model = LinearModel.builder().y(e).addX(Regression.matrix(domain, workingDays)).build();
                var f = Ols.compute(model).Ftest();
                StringBuilder input = new StringBuilder("residuals " + year + " " + month + " " + data.length());
                for (double value : e.toArray()) input.append(" ").append(CalendarFTestProbe.bits(value));
                System.out.println(input);
                double p = f.getPvalue();
                for (double threshold : new double[]{0.01, p, Math.nextDown(p), Math.nextUp(p)}) {
                    if (threshold < 0 || threshold > 1) continue;
                    boolean decision = (boolean) needProcessing.invoke(constructor.newInstance(workingDays, threshold), frame);
                    System.out.printf("gate %s %s %s %b%n", CalendarFTestProbe.bits(threshold),
                        CalendarFTestProbe.bits(f.getValue()), CalendarFTestProbe.bits(p), decision);
                }
            }
        }
    }
}
