import java.time.YearMonth;
import java.util.HexFormat;
import java.util.Random;
import jdplus.toolkit.base.api.data.DoubleSeq;
import jdplus.toolkit.base.api.processing.ProcessingLog;
import jdplus.toolkit.base.api.timeseries.TsData;
import jdplus.toolkit.base.api.timeseries.TsPeriod;
import jdplus.toolkit.base.api.timeseries.calendars.DayClustering;
import jdplus.toolkit.base.api.timeseries.regression.GenericTradingDaysVariable;
import jdplus.toolkit.base.core.regsarima.regular.ModelDescription;
import jdplus.toolkit.base.core.regsarima.regular.RegSarimaModelling;
import jdplus.tramoseats.base.core.tramo.AutomaticFRegressionTest;

/** Observe likelihoods and the actual selected variables from the public TRAMO F module. */
public final class CalendarFTestProbe {
    static final class Observer implements ProcessingLog {
        AutomaticFRegressionTest.Info observation;
        public void info(String message, Object data) {
            if (data instanceof AutomaticFRegressionTest.Info result) observation = result;
        }
    }

    static double[] series(int effect) {
        return series(effect, 180);
    }

    static double[] series(int effect, int length) {
        double[] result = new double[length];
        Random noise = new Random(8712);
        for (int t = 0; t < result.length; ++t) {
            YearMonth month = YearMonth.of(1990, 1).plusMonths(t);
            int weekdays = 0, mondays = 0;
            for (int day = 1; day <= month.lengthOfMonth(); ++day) {
                int dow = month.atDay(day).getDayOfWeek().getValue();
                if (dow <= 5) ++weekdays;
                if (dow == 1) ++mondays;
            }
            double workingDays = weekdays - 2.5 * (month.lengthOfMonth() - weekdays);
            double monday = mondays - (month.lengthOfMonth() - mondays) / 6.0;
            double calendar = switch (effect) {
                case 1 -> 15 * workingDays;
                case 2 -> 15 * monday;
                case 3 -> 0.25 * workingDays;
                default -> 0;
            };
            result[t] = 150 + 0.14*t + 12*StrictMath.cos(2*StrictMath.PI*t / 12)
                + 3*noise.nextGaussian() + calendar;
        }
        return result;
    }

    static String bits(double value) {
        return HexFormat.of().toHexDigits(Double.doubleToRawLongBits(value));
    }

    static AutomaticFRegressionTest.Info run(String label, int effect, double[] y,
                                             boolean mean, double alpha) {
        var observer = new Observer();
        var domain = TsData.of(TsPeriod.monthly(1990, 1), DoubleSeq.of(y));
        var description = new ModelDescription(domain, null);
        description.setAirline(true);
        description.setMean(mean);
        var frame = RegSarimaModelling.of(description, observer);
        var module = AutomaticFRegressionTest.builder()
            .workingDays(new GenericTradingDaysVariable(DayClustering.TD2))
            .tradingDays(new GenericTradingDaysVariable(DayClustering.TD7))
            .testMean(false).fPValue(alpha).estimationPrecision(1e-3).build();
        var status = module.test(frame);
        var info = observer.observation;
        if (info == null || !status.isProcessed()) throw new AssertionError(label + " " + status);
        int selected = frame.getDescription().variables().filter(v -> v.getName().equals("td"))
            .mapToInt(v -> v.getCore().dim()).sum();
        System.out.printf("fit %s %d %d %s %s %d %d %s %d %s %d %s %s %s %s%n",
            label, effect, mean ? 1 : 0, bits(alpha), bits(info.getSs0()), info.getNhp(),
            info.getLl1().degreesOfFreedom(), bits(info.getSs1()),
            info.getLl6().degreesOfFreedom(), bits(info.getSs6()), selected,
            bits(info.getSsmc1()), bits(info.getSsmc6()), bits(info.getPftd1()), bits(info.getPftd6()));
        return info;
    }

    public static void main(String[] args) {
        for (int effect = 0; effect < 4; ++effect) {
            double[] y = series(effect);
            StringBuilder input = new StringBuilder("input " + effect);
            for (double value : y) input.append(" ").append(bits(value));
            System.out.println(input);
            var info = run("default", effect, y, false, 0.01);
            run("mean", effect, y, true, 0.01);
            if (effect == 0 || effect == 3) {
                double p = effect == 0 ? info.getPftd6() : info.getPftd1();
                run("equal", effect, y, false, 1 - p);
                run("above", effect, y, false, 1 - Math.nextDown(p));
                run("below", effect, y, false, 1 - Math.nextUp(p));
            }
        }
    }
}
