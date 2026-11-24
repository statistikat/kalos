import java.nio.file.Files;
import java.nio.file.Path;
import java.time.YearMonth;
import java.util.Arrays;
import java.util.HexFormat;
import java.util.List;
import java.util.Random;
import jdplus.sa.base.core.regarima.AutomaticTradingRegressionModule;
import jdplus.sa.base.core.regarima.EasterDetectionModule;
import jdplus.sa.base.core.regarima.LogLevelModule;
import jdplus.toolkit.base.core.regsarima.regular.ILogLevelModule;
import jdplus.toolkit.base.api.processing.ProcessingLog;
import jdplus.toolkit.base.api.timeseries.TsData;
import jdplus.toolkit.base.api.timeseries.TsDomain;
import jdplus.toolkit.base.api.timeseries.TsPeriod;
import jdplus.toolkit.base.api.timeseries.regression.ModellingContext;
import jdplus.toolkit.base.api.timeseries.calendars.LengthOfPeriodType;
import jdplus.toolkit.base.core.timeseries.simplets.Transformations;
import jdplus.toolkit.base.core.modelling.regression.Regression;
import jdplus.toolkit.base.core.regsarima.regular.IRegressionModule;
import jdplus.toolkit.base.core.stats.likelihood.LikelihoodStatistics;
import jdplus.toolkit.base.api.timeseries.regression.EasterVariable;
import jdplus.x13.base.api.x13.X13Spec;
import jdplus.x13.base.core.x13.regarima.RegArimaKernel;

/** Observe the actual RegArimaKernel calendar and Easter tests in stock RSA2c. */
public final class X13Rsa2cCalendarProbe {
    static String bits(double x) { return HexFormat.of().toHexDigits(Double.doubleToRawLongBits(x)); }

    static void vector(String name, double[] data) {
        var line = new StringBuilder(name);
        for (double x : data) line.append(' ').append(bits(x));
        System.out.println(line);
    }

    static void candidate(String name, LikelihoodStatistics s) {
        System.out.printf("candidate %s %d %d %s %s %s %s%n", name, s.getEffectiveObservationsCount(),
            s.getEstimatedParametersCount(), bits(s.getTransformationAdjustment()), bits(s.getLogLikelihood()),
            bits(s.getSsqErr()), bits(s.getAICC()));
    }

    static final class Decisions implements ProcessingLog {
        int calendar = -1, easter = -2;
        public boolean isVerbose() { return true; }
        public void info(String message, String origin, Object info) {
            if (info instanceof AutomaticTradingRegressionModule.Info td) {
                if (calendar != -1 || td.getLl().length != 2) throw new AssertionError("TD comparison");
                calendar = td.getBest();
                candidate("without_td", td.getLl()[0]);
                candidate("with_td", td.getLl()[1]);
            } else if (info instanceof EasterDetectionModule.Info e) {
                if (easter != -2 || !Arrays.equals(e.getNames(), new String[]{"easter [1]", "easter [8]", "easter [15]"}))
                    throw new AssertionError("Easter candidates");
                easter = e.getBest();
                candidate("without_easter", e.getLl0());
                int[] durations = {1, 8, 15};
                for (int i = 0; i < durations.length; ++i) candidate("easter_" + durations[i], e.getLl()[i]);
            }
        }
    }

    static void run(String id, TsData data, boolean scale) throws Exception {
        var spec = X13Spec.fromString("rsa2c").getRegArima();
        var kernel = RegArimaKernel.of(spec, new ModellingContext());
        System.out.println("case " + id);
        var row = new StringBuilder("input " + data.getStart().year() + " " + (1 + data.getStart().annualPosition()));
        for (double v : data.getValues().toArray()) row.append(' ').append(bits(v));
        System.out.println(row);
        if (scale) {
            var field = RegArimaKernel.class.getDeclaredField("transformation");
            field.setAccessible(true);
            var module = (LogLevelModule) field.get(kernel);
            field.set(kernel, (ILogLevelModule) ctx -> {
                var result = module.process(ctx);
                if (module.getLevel() != null) candidate("level", module.getLevel().statistics());
                if (module.getLog() != null) candidate("log", module.getLog().statistics());
                return result;
            });
            var fit = kernel.process(data, new ProcessingLog() {});
            if (fit == null) throw new AssertionError("scale " + id);
            System.out.println("log " + fit.getDescription().isLogTransformation());
            return;
        }
        // Observe the scale at entry, before outlier detection or the final estimator.
        var hook = RegArimaKernel.class.getDeclaredField("calendarTest");
        hook.setAccessible(true);
        var test = (IRegressionModule) hook.get(kernel);
        hook.set(kernel, (IRegressionModule) ctx -> {
            var desc = ctx.getDescription();
            if (desc.isMean()) throw new AssertionError("stock mean");
            System.out.println("log " + desc.isLogTransformation());
            return test.test(ctx);
        });
        var easter = RegArimaKernel.class.getDeclaredField("easterTest");
        easter.setAccessible(true);
        var easterTest = (IRegressionModule) easter.get(kernel);
        easter.set(kernel, (IRegressionModule) ctx -> {
            var result = easterTest.test(ctx);
            var desc = ctx.getDescription();
            vector("transformed", desc.getTransformedSeries().getValues().toArray());
            var extended = TsDomain.of(data.getStart(), data.length() + 12);
            desc.variables().forEach(v -> vector("design " + v.getName(),
                Regression.matrix(extended, v.getCore()).column(0).toArray()));
            double[] ones = new double[extended.length()];
            Arrays.fill(ones, 1);
            var neutral = TsData.ofInternal(data.getStart(), ones);
            if (desc.isLogTransformation() && desc.getPreadjustment() == LengthOfPeriodType.LeapYear) {
                var lp = Transformations.lengthOfPeriod(LengthOfPeriodType.LeapYear);
                vector("preadjust", lp.transform(neutral, null).getValues().toArray());
                vector("restore", lp.converse().transform(neutral, null).getValues().toArray());
            }
            return result;
        });
        var log = new Decisions();
        if (kernel.process(data, log) == null || log.calendar < 0 || log.easter < -1)
            throw new AssertionError("missing stock decision " + id);
        System.out.printf("selected %b %d%n", log.calendar == 1, new int[]{0, 1, 8, 15}[log.easter + 1]);
    }

    static TsData generated(int duration, boolean td, boolean growing) {
        var start = TsPeriod.monthly(1993, 10);
        var domain = TsDomain.of(start, 180);
        var regressor = EasterVariable.builder().duration(duration).endPosition(-1)
            .meanCorrection(EasterVariable.Correction.PreComputed).build();
        var easter = Regression.matrix(domain, regressor).column(0);
        var random = new Random(77321);
        double[] values = new double[domain.length()];
        for (int i = 0; i < values.length; ++i) {
            var month = YearMonth.of(1993, 10).plusMonths(i);
            int working = 0;
            for (int day = 1; day <= month.lengthOfMonth(); ++day)
                if (month.atDay(day).getDayOfWeek().getValue() <= 5) ++working;
            double contrast = working - 2.5 * (month.lengthOfMonth() - working);
            double signal = 1.3 * random.nextGaussian() + 3 * StrictMath.cos(i * 2 * Math.PI / 12)
                          + 17 * easter.get(i) + (td ? 2 * contrast : 0);
            values[i] = growing ? StrictMath.exp(4.4 + i * .009 + .012 * signal) : 80 + .02 * i + signal;
        }
        return TsData.ofInternal(start, values);
    }

    public static void main(String[] args) throws Exception {
        boolean scale = args.length == 2 && args[1].equals("scale");
        for (String id : List.of("air", "ao", "seas")) {
            var lines = Files.readAllLines(Path.of(args[0], id + ".csv"));
            var date = lines.getFirst().split(",");
            double[] values = lines.stream().skip(1).mapToDouble(Double::parseDouble).toArray();
            run(id, TsData.ofInternal(TsPeriod.monthly(Integer.parseInt(date[0]), Integer.parseInt(date[1])), values), scale);
        }
        for (int duration : new int[]{1, 8, 15}) {
            for (boolean td : List.of(false, true)) {
                for (boolean growing : List.of(false, true)) {
                    run("easter_" + duration + (td ? "_td" : "_no_td") + (growing ? "_growing" : "_steady"),
                        generated(duration, td, growing), scale);
                }
            }
        }
        if (scale) {
            var base = generated(8, true, false);
            double[] shifted = base.getValues().toArray();
            for (int i = 0; i < shifted.length; ++i) shifted[i] -= 100;
            run("nonpositive", TsData.ofInternal(base.getStart(), shifted), true);
        }
    }
}
