import java.nio.file.Files;
import java.nio.file.Path;
import java.util.HexFormat;
import jdplus.sa.base.core.diagnostics.GenericSaTests;
import jdplus.toolkit.base.api.information.Explorable;
import jdplus.toolkit.base.api.stats.StatisticalTest;
import jdplus.toolkit.base.api.timeseries.TsData;
import jdplus.toolkit.base.api.timeseries.TsPeriod;
import jdplus.toolkit.base.api.timeseries.regression.ModellingContext;
import jdplus.tramoseats.base.api.tramoseats.TramoSeatsSpec;
import jdplus.tramoseats.base.core.tramoseats.TramoSeatsKernel;
import jdplus.x13.base.api.x13.X13Spec;
import jdplus.x13.base.core.x13.X13Kernel;

/** Public residual seasonality of the observed adjusted series on both methods. */
public final class AdjustedSeasonalityProbe {
    static String bits(double value) {
        return HexFormat.of().toHexDigits(Double.doubleToRawLongBits(value));
    }

    static void vector(String tag, double[] values) {
        var row = new StringBuilder(tag);
        for (double value : values) row.append(' ').append(bits(value));
        System.out.println(row);
    }

    static void report(String id, Explorable result, GenericSaTests diagnostics) {
        var tests = diagnostics.residualSeasonalityTestsOnSa();
        var diff = tests.getDifferencing();
        var series = tests.getSeries();
        System.out.printf("case %s %d %d %d %b%n", id, series.getStart().start().getYear(),
            series.getStart().annualPosition()+1, diff.getDifferencingOrder(), diff.isMeanCorrection());
        vector("model_scale_sa", series.getValues().toArray());
        vector("differenced", diff.getDifferenced().toArray());
        for (String key : new String[]{"diagnostics.seas-sa-qs", "diagnostics.seas-sa-f"}) {
            var t = result.getData(key, StatisticalTest.class);
            System.out.println(key + (t == null ? " unavailable" : " " + bits(t.getValue()) + " " + bits(t.getPvalue())));
        }
    }

    public static void main(String[] args) throws Exception {
        for (String id : new String[]{"air", "ao", "seas"}) {
            var lines = Files.readAllLines(Path.of(args[0], id + ".csv"));
            var start = lines.get(0).split(",");
            var data = TsData.ofInternal(TsPeriod.monthly(Integer.parseInt(start[0]), Integer.parseInt(start[1])),
                lines.stream().skip(1).mapToDouble(Double::parseDouble).toArray());
            for (String name : new String[]{"rsa0", "rsa1", "rsa2c", "rsa3"}) {
                var r = X13Kernel.of(X13Spec.fromString(name), new ModellingContext()).process(data, null);
                report("x13_" + name + "_" + id, r, r.getDiagnostics().getGenericDiagnostics());
            }
            for (var spec : new TramoSeatsSpec[]{TramoSeatsSpec.RSA0, TramoSeatsSpec.RSA1, TramoSeatsSpec.RSA3, TramoSeatsSpec.RSAfull}) {
                var r = TramoSeatsKernel.of(spec, new ModellingContext()).process(data, null);
                report("tramo_" + spec.display().toLowerCase(java.util.Locale.ROOT) + "_" + id, r,
                    r.getDiagnostics().getGenericDiagnostics());
            }
        }
    }
}
