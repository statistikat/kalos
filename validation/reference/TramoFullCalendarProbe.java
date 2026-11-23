import java.lang.reflect.Field;
import java.lang.reflect.Method;
import jdplus.toolkit.base.api.data.DoubleSeq;
import jdplus.toolkit.base.api.timeseries.TsData;
import jdplus.toolkit.base.api.timeseries.TsDomain;
import jdplus.toolkit.base.api.timeseries.TsPeriod;
import jdplus.toolkit.base.api.timeseries.regression.*;
import jdplus.toolkit.base.core.modelling.regression.Regression;
import jdplus.toolkit.base.core.regarima.RegArimaUtility;
import jdplus.toolkit.base.core.regsarima.regular.*;
import jdplus.tramoseats.base.api.tramo.TramoSpec;
import jdplus.tramoseats.base.core.tramo.AutomaticFRegressionTest;
import jdplus.tramoseats.base.core.tramo.TramoKernel;

/** Observe the regression module actually configured by the stock TRfull template. */
public final class TramoFullCalendarProbe {
    static Object option(AutomaticFRegressionTest module, String name) throws Exception {
        Field field = AutomaticFRegressionTest.class.getDeclaredField(name);
        field.setAccessible(true);
        return field.get(module);
    }

    public static void main(String[] args) throws Exception {
        var kernel = TramoKernel.of(TramoSpec.fromString("TRfull"), new ModellingContext());
        Method factory = TramoKernel.class.getDeclaredMethod("regressionModule", boolean.class);
        factory.setAccessible(true);
        var module = (AutomaticFRegressionTest) factory.invoke(kernel, false);
        var easter = (EasterVariable) option(module, "easter");
        var lp = (ILengthOfPeriodVariable) option(module, "lp");
        var precision = (double) option(module, "precision");
        Method candidate = AutomaticFRegressionTest.class.getDeclaredMethod("createTestModel",
            RegSarimaModelling.class, ITradingDaysVariable.class, ILengthOfPeriodVariable.class);
        candidate.setAccessible(true);
        var start = TsPeriod.monthly(1990, 1);
        var matrix = Regression.matrix(TsDomain.of(start, 180), lp, easter);
        for (int effect = 0; effect < 10; ++effect) {
            for (boolean log : new boolean[]{false, true}) {
                int td = effect == 0 || effect == 7 || effect == 9 ? 0 : effect == 2 ? 2 : 1;
                double[] series = CalendarFTestProbe.series(td);
                StringBuilder values = new StringBuilder("input " + effect + " " + (log ? 1 : 0));
                for (int t = 0; t < series.length; ++t) {
                    if (effect == 3 || effect == 4 || effect == 6)
                        series[t] += (effect == 6 ? -30 : 30) * matrix.get(t, 0);
                    if (effect >= 4 && effect <= 7) series[t] += 24 * matrix.get(t, 1);
                    if (effect == 4 || effect >= 8) series[t] += 0.08*t*t;
                    if (log) series[t] = StrictMath.exp(series[t]/200.0);
                    values.append(" ").append(CalendarFTestProbe.bits(series[t]));
                }
                System.out.println(values);
                var observer = new CalendarFTestProbe.Observer();
                var description = new ModelDescription(TsData.of(start, DoubleSeq.of(series)), null);
                description.setAirline(true);
                description.setMean(true);
                description.setLogTransformation(log);
                var before = RegSarimaModelling.of(ModelDescription.copyOf(description));
                var frame = RegSarimaModelling.of(description, observer);
                var status = module.test(frame);
                var info = observer.observation;
                if (!status.isProcessed() || info == null) throw new AssertionError(status);
                var result = frame.getDescription();
                int selected = result.variables().filter(v -> v.getName().equals("td"))
                    .mapToInt(v -> v.getCore().dim()).sum();

                // Refit the selected candidate using the module's builder and actual
                // processor; these t statistics explain the observed updates above.
                var variable = selected == 0 ? null : (ITradingDaysVariable)
                    option(module, selected == 6 ? "td" : "wd");
                var test = (ModelDescription) candidate.invoke(module, before, variable, selected == 0 ? null : lp);
                var cl = RegArimaUtility.processor(true, precision).process(test.regarima(), test.mapping())
                    .getConcentratedLikelihood();
                int pos = selected == 0 ? -1 : test.findPosition(lp);
                System.out.printf("result %s %d %b %b %b %s %s %s %s %s %s %s %s%n", status, selected,
                    result.isMean(), result.variables().anyMatch(v -> v.getName().equals("lp")),
                    result.variables().anyMatch(v -> v.getName().equals("easter")),
                    CalendarFTestProbe.bits(info.getSs0()), CalendarFTestProbe.bits(info.getSs1()),
                    CalendarFTestProbe.bits(info.getSs6()), CalendarFTestProbe.bits(info.getPftd1()),
                    CalendarFTestProbe.bits(info.getPftd6()), CalendarFTestProbe.bits(cl.tstat(0, 2, true)),
                    pos < 0 ? "-" : CalendarFTestProbe.bits(cl.tstat(pos, 2, true)),
                    CalendarFTestProbe.bits(cl.tstat(test.findPosition(easter), 2, true)));
            }
        }
    }
}
