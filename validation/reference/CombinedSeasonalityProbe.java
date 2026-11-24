import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
import jdplus.sa.base.api.ComponentType;
import jdplus.sa.base.api.DecompositionMode;
import jdplus.sa.base.core.diagnostics.SaDiagnosticsUtility;
import jdplus.toolkit.base.api.modelling.ComponentInformation;
import jdplus.toolkit.base.api.timeseries.TsData;
import jdplus.toolkit.base.api.timeseries.TsPeriod;
import jdplus.toolkit.base.api.timeseries.regression.ModellingContext;
import jdplus.toolkit.base.core.stats.tests.TestsUtility;
import jdplus.tramoseats.base.api.tramoseats.TramoSeatsSpec;
import jdplus.tramoseats.base.core.tramoseats.TramoSeatsKernel;
import jdplus.toolkit.base.api.arima.SarimaSpec;
import jdplus.toolkit.base.api.modelling.TransformationType;
import jdplus.toolkit.base.api.timeseries.calendars.LengthOfPeriodType;
import jdplus.x13.base.api.regarima.*;
import jdplus.x13.base.api.x11.SeasonalFilterOption;
import jdplus.x13.base.api.x11.X11Spec;
import jdplus.x13.base.api.x13.X13Spec;
import jdplus.x13.base.core.x13.X13Kernel;

/** Observe the all-years SI seasonality getter and the numerical inputs to its verdict. */
public final class CombinedSeasonalityProbe {
    static final String KEY = "diagnostics.seas-si-combined";

    static String bits(double x) { return String.format("%016x", Double.doubleToRawLongBits(x)); }

    static String observe(String id, TsData si, boolean multiplicative, String seasonalSource) {
        System.out.printf("case %s %s %b%n", id, seasonalSource, multiplicative);
        System.out.printf("si %d %d %d %d", si.getStart().start().getYear(), si.getStart().annualPosition() + 1,
            si.getAnnualFrequency(), si.length());
        for (double value : si.getValues().toArray()) System.out.print(" " + bits(value));
        System.out.println();
        var combined = SaDiagnosticsUtility.combinedSeasonalityTest(si, 0, multiplicative ? 1 : 0, false);
        if (combined == null) {
            System.out.println("summary unavailable");
            return null;
        }
        var stable = TestsUtility.ofAnova(combined.getStableSeasonalityTest());
        var moving = TestsUtility.ofAnova(combined.getEvolutiveSeasonalityTest());
        var rank = combined.getNonParametricTestForStableSeasonality().build();
        System.out.printf("statistics %s %s %s %s %s %s%n", bits(stable.getValue()), bits(stable.getPvalue()),
            bits(moving.getValue()), bits(moving.getPvalue()), bits(rank.getValue()), bits(rank.getPvalue()));
        var summary = combined.getSummary().name();
        System.out.println("summary " + summary);
        return summary;
    }

    static void compare(String expected, String actual, String id) {
        if (!java.util.Objects.equals(expected, actual)) throw new AssertionError(id + " " + expected + " " + actual);
    }

    static void tramo(String id, TsData data, String preset, TramoSeatsSpec spec) {
        var result = TramoSeatsKernel.of(spec, new ModellingContext()).process(data, null);
        var finalComponents = result.getDecomposition().getFinalComponents();
        var s = finalComponents.getSeries(ComponentType.Seasonal, ComponentInformation.Value);
        var i = finalComponents.getSeries(ComponentType.Irregular, ComponentInformation.Value);
        var mul = result.getPreprocessing().getDescription().isLogTransformation();
        var si = mul ? TsData.multiply(s, i) : TsData.add(s, i);
        compare(result.getData(KEY, String.class),
            observe(preset + "_" + id, si, mul, s == null ? "wk_irregular" : i == null ? "wk_seasonal" : "wk_both"), id);
    }

    static void x13(String id, TsData data, String preset, DecompositionMode requested) {
        var spec = X13Spec.fromString(preset);
        if (requested != null) spec = spec.toBuilder().x11(spec.getX11().toBuilder().mode(requested)
            .backcastHorizon(3).forecastHorizon(6).build()).build();
        var result = X13Kernel.of(spec, new ModellingContext()).process(data, null);
        var x11 = result.getDecomposition();
        var si = TsData.fitToDomain(x11.getD8(), x11.getActualDomain());
        String suffix = requested == null ? "" : "_" + requested.name();
        if (requested != null && x11.getMode() != requested) throw new AssertionError("explicit X11 mode");
        compare(result.getData(KEY, String.class),
            observe("x13_" + preset + "_" + id + suffix, si, x11.getMode().isMultiplicative(), "d8"), id);
    }

    static void fixed(String id, TsData data) {
        for (var mode : new DecompositionMode[]{DecompositionMode.Additive, DecompositionMode.Multiplicative,
                DecompositionMode.LogAdditive, DecompositionMode.PseudoAdditive}) {
            boolean log = mode == DecompositionMode.Multiplicative || mode == DecompositionMode.LogAdditive;
            var model = RegArimaSpec.builder().usingAutoModel(false)
                .basic(BasicSpec.builder().preprocessing(true).build())
                .transform(TransformSpec.builder().function(log ? TransformationType.Log : TransformationType.None)
                    .adjust(LengthOfPeriodType.None).outliersCorrection(false).build())
                .regression(RegressionSpec.builder().build()).outliers(OutlierSpec.DEFAULT_DISABLED)
                .arima(SarimaSpec.builder().d(1).bd(1).buildWithoutValidation()).estimate(EstimateSpec.DEFAULT).build();
            var x11 = X11Spec.builder().mode(mode).seasonal(true).filters(new SeasonalFilterOption[]{SeasonalFilterOption.S3X5})
                .hendersonFilterLength(13).lowerSigma(1.5).upperSigma(2.5).forecastHorizon(0).backcastHorizon(0).build();
            var fit = X13Kernel.of(X13Spec.builder().regArima(model).x11(x11).build(), new ModellingContext()).process(data, null);
            var output = fit.getDecomposition();
            if (output.getMode() != mode) throw new AssertionError("fixed X-13 decomposition mode");
            compare(fit.getData(KEY, String.class), observe("fixed_" + mode.name() + "_" + id, output.getD8(),
                mode.isMultiplicative(), "fixed_d8"), id);
        }
    }

    static void products(Path path, boolean retries, boolean fixedOnly) throws Exception {
        String id = "";
        for (String line : Files.readAllLines(path)) {
            var fields = line.split(" ");
            if (fields[0].equals("case")) id = fields[1];
            if (!fields[0].equals("input")) continue;
            var values = Arrays.stream(fields, 4, fields.length).mapToDouble(hex ->
                Double.longBitsToDouble(Long.parseUnsignedLong(hex, 16))).toArray();
            var data = TsData.ofInternal(TsPeriod.monthly(Integer.parseInt(fields[1]), Integer.parseInt(fields[2])), values);
            if (fixedOnly) {
                if (id.equals("fit_tr0_air")) fixed(id, data);
            } else if (retries) {
                if (id.equals("fit_ref_retry_0_5") || id.equals("fit_ref_retry_0_1")) tramo(id, data, "rsa3", TramoSeatsSpec.RSA3);
            } else {
                tramo(id, data, "rsa0", TramoSeatsSpec.RSA0);
                tramo(id, data, "rsa1", TramoSeatsSpec.RSA1);
                tramo(id, data, "rsa3", TramoSeatsSpec.RSA3);
                x13(id, data, "rsa0", null);
                x13(id, data, "rsa3", null);
                if (id.equals("fit_tr0_air")) {
                    x13(id, data, "rsa3", DecompositionMode.LogAdditive);
                    x13(id, data, "rsa3", DecompositionMode.PseudoAdditive);
                }
            }
        }
    }

    static void synthetic() {
        for (int k : new int[]{4, 12}) {
            for (int sample = 0; sample < 14; ++sample) {
                int n = sample == 0 ? 3*k-1 : sample == 1 ? 3*k : 6*k+2;
                double[] x = new double[n];
                long state = 71;
                for (int t = 0; t < n; ++t) {
                    state = (1664525*state + 1013904223) & 0xffffffffL;
                    double noise = ((state >>> 16) - 32768) / 32768.0;
                    double seasonal = (t % k) / (double)(k-1) - 0.5;
                    double level = sample % 2;
                    x[t] = level + noise + (sample - 3) * .34 * seasonal;
                    if (sample == 12) x[t] = level + ((t + t/k) % 4);
                    if (sample == 13) x[t] = level + (t % k);
                }
                var period = k == 12 ? TsPeriod.monthly(2000, 4) : TsPeriod.quarterly(2000, 2);
                observe("band_" + k + "_" + sample, TsData.ofInternal(period, x), sample % 2 == 1, "direct");
            }
        }
    }

    public static void main(String[] args) throws Exception {
        if (args.length == 3) { products(Path.of(args[0]), false, true); return; }
        synthetic();
        products(Path.of(args[0]), false, false);
        products(Path.of(args[1]), true, false);
    }
}
