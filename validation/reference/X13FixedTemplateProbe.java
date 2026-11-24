import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
import java.util.HexFormat;
import java.util.List;
import jdplus.toolkit.base.api.timeseries.TsData;
import jdplus.toolkit.base.api.timeseries.TsPeriod;
import jdplus.toolkit.base.api.timeseries.regression.ModellingContext;
import jdplus.x13.base.api.x13.X13Spec;
import jdplus.x13.base.core.x13.X13Kernel;

/** Observe the public fixed-structure, estimated-coefficient monthly X-13 templates. */
public final class X13FixedTemplateProbe {
    static String bits(double value) {
        return HexFormat.of().toHexDigits(Double.doubleToRawLongBits(value));
    }

    static void vector(String name, double[] values) {
        var line = new StringBuilder(name);
        for (double value : values) line.append(' ').append(bits(value));
        System.out.println(line);
    }

    static void series(String name, TsData data) {
        if (data == null) {
            System.out.println(name + " absent");
            return;
        }
        var start = data.getStart();
        vector(name + " " + start.year() + " " + (1 + start.annualPosition()) + " " + data.length(),
               data.getValues().toArray());
    }

    public static void main(String[] args) throws Exception {
        Path root = Path.of(args[0]);
        for (String template : List.of("rsa0", "rsa1", "rsa2c")) {
            var spec = X13Spec.fromString(template);
            var x11 = spec.getX11();
            for (String id : List.of("air", "ao", "seas")) {
                var lines = Files.readAllLines(root.resolve(id + ".csv"));
                String[] date = lines.getFirst().split(",");
                var start = TsPeriod.monthly(Integer.parseInt(date[0]), Integer.parseInt(date[1]));
                double[] values = lines.subList(1, lines.size()).stream().mapToDouble(Double::parseDouble).toArray();
                var data = TsData.ofInternal(start, values);
                var result = X13Kernel.of(spec, new ModellingContext()).process(data, null);
                if (result == null || !result.isValid()) throw new AssertionError(template + "/" + id);
                var model = result.getPreprocessing();
                var orders = model.specification();
                var est = model.getEstimation();
                var stats = est.getStatistics();
                var tables = result.getDecomposition();
                var finals = result.getFinals();
                System.out.printf("case %s %s%n", template, id);
                System.out.printf("requested %s %d %d %d %s%n", x11.getMode(), x11.getBackcastHorizon(),
                    x11.getForecastHorizon(), x11.getHendersonFilterLength(), Arrays.toString(x11.getFilters()));
                System.out.printf("model %b %b %d %d %d %d %d %d%n", model.getDescription().isLogTransformation(),
                    model.isMeanCorrection(), orders.getP(), orders.getD(), orders.getQ(),
                    orders.getBp(), orders.getBd(), orders.getBq());
                vector("arma", model.arima().parameters().toArray());
                for (var v : model.getDescription().getVariables()) {
                    System.out.printf("variable %s %s %d %s%n", v.getName(), v.getCore().getClass().getSimpleName(), v.dim(), v.attribute("regeffect"));
                }
                vector("coefficients", est.getCoefficients().toArray());
                System.out.printf("likelihood %d %d %s %s%n", stats.getEffectiveObservationsCount(),
                    stats.getEstimatedParametersCount(), bits(stats.getLogLikelihood()), bits(stats.getSsqErr()));
                series("residuals", model.fullResiduals());
                System.out.printf("selected %s %d %d %s %s%n", tables.getMode(), tables.getD7HendersonFilterLength(),
                    tables.getFinalHendersonFilterLength(), tables.getD9filter(), Arrays.toString(tables.getFinalSeasonalFilter()));
                series("y_f", result.getData("y_f", TsData.class));
                series("b1", tables.getB1());
                series("b7", tables.getB7());
                series("c7", tables.getC7());
                series("d7", tables.getD7());
                series("d10", tables.getD10());
                series("d11", tables.getD11());
                series("d12", tables.getD12());
                series("d13", tables.getD13());
                series("final_sa", finals.getD11final());
                series("final_trend", finals.getD12final());
                series("final_irregular", finals.getD13final());
                series("forecast_sa", finals.getD11a());
                series("forecast_trend", finals.getD12a());
            }
        }
    }
}
