import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;
import jdplus.toolkit.base.api.timeseries.TsData;
import jdplus.toolkit.base.api.timeseries.TsPeriod;
import jdplus.toolkit.base.api.timeseries.regression.ModellingContext;
import jdplus.x13.base.api.x13.X13Spec;
import jdplus.x13.base.core.x13.X13Kernel;
import jdplus.x13.base.core.x13.X13Results;

/**
 * Emits the published i_f (irregular forecast) product for the M14 slice evidence pack.
 * Each stock profile is fitted through the X-13 kernel (RSA3 is the automatic profile; the
 * fixed templates go through X13Spec.fromString) and the i_f row is dumped bit-exactly
 * through the public X13Results.getData("i_f", TsData.class) path -- the same extractor the
 * R user_defined tree consumes. Per case the decomposition mode (the extractor's own
 * multiplicative predicate), the log/mean flags, the finals D12a row (the domain the
 * extractor reads) and the i_f words follow. A zero-forecast-horizon RSA3 arm pins the
 * extractor's D12a-empty gate (both rows null). Line breaks are (char) 10 by construction;
 * the JVM runs with the bridge flags so the transcendental bit-patterns match the kalos lanes.
 *
 * Usage: java X13IrregularForecastProbe <profiles-dir>
 */
public final class X13IrregularForecastProbe {

    static String bits(double value) {
        return String.format("%016x", Double.doubleToRawLongBits(value));
    }

    static void dump(String tag, TsData series) {
        if (series == null) {
            OUT.append(tag).append(" null").append(LF);
            return;
        }
        TsPeriod period = series.getStart();
        double[] values = series.getValues().toArray();
        OUT.append(tag).append(' ').append(period.year())
                .append(' ').append(period.annualPosition() + 1)
                .append(' ').append(values.length);
        for (double value : values) {
            OUT.append(' ').append(bits(value));
        }
        OUT.append(LF);
    }

    static void report(String id, TsData observed, X13Results fit) {
        OUT.append("case ").append(id).append(LF);
        var model = fit.getPreprocessing();
        OUT.append("info ").append(fit.getDecomposition().getMode())
                .append(" log=").append(model.getDescription().isLogTransformation())
                .append(" mean=").append(model.isMeanCorrection())
                .append(" valid=").append(fit.isValid()).append(LF);
        dump("d12a", fit.getFinals() == null ? null : fit.getFinals().getD12a());
        dump("i_f", fit.getData("i_f", TsData.class));
    }

    static final char LF = (char) 10;
    static final StringBuilder OUT = new StringBuilder();

    static TsData read(Path path) throws Exception {
        List<String> lines = Files.readAllLines(path);
        String[] date = lines.get(0).split(",");
        TsPeriod start = TsPeriod.monthly(Integer.parseInt(date[0]), Integer.parseInt(date[1]));
        double[] values = lines.subList(1, lines.size()).stream()
                .mapToDouble(Double::parseDouble).toArray();
        return TsData.ofInternal(start, values);
    }

    public static void main(String[] args) throws Exception {
        Path dir = Path.of(args[0]);
        for (String stem : List.of("air", "ao", "seas")) {
            report("stock_rsa3_" + stem, read(dir.resolve(stem + ".csv")),
                    X13Kernel.of(X13Spec.RSA3, new ModellingContext())
                            .process(read(dir.resolve(stem + ".csv")), null));
        }
        // Zero-horizon arm: the extractor's D12a-empty gate must answer null.
        var zero = X13Spec.RSA3.toBuilder()
                .x11(X13Spec.RSA3.getX11().toBuilder().forecastHorizon(0).build()).build();
        report("stock_rsa3_air_h0", read(dir.resolve("air.csv")),
                X13Kernel.of(zero, new ModellingContext()).process(read(dir.resolve("air.csv")), null));
        for (String spec : List.of("rsa0", "rsa1", "rsa2c")) {
            for (String stem : List.of("air", "ao", "seas")) {
                var data = read(dir.resolve(stem + ".csv"));
                var fit = X13Kernel.of(X13Spec.fromString(spec), new ModellingContext())
                        .process(data, null);
                if (!fit.isValid()) {
                    throw new AssertionError("invalid fit for " + spec + " on " + stem);
                }
                report("stock_" + spec + "_" + stem, data, fit);
            }
        }
        System.out.print(OUT);
    }
}
