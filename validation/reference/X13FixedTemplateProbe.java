import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
import java.util.HexFormat;
import java.util.List;
import jdplus.toolkit.base.api.timeseries.TsData;
import jdplus.toolkit.base.api.timeseries.TsPeriod;
import jdplus.toolkit.base.api.timeseries.regression.ModellingContext;
import jdplus.x13.base.api.x13.X13Spec;
import jdplus.x13.base.api.x11.X11Spec;
import jdplus.x13.base.core.x11.X11Kernel;
import jdplus.x13.base.core.x11.X11Results;
import jdplus.x13.base.core.x11.filter.AutomaticHenderson;
import jdplus.x13.base.core.x11.filter.MsrFilterSelection;
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

    static void selection(String template, String id, X11Spec requested, X11Results product, int horizon) {
        var kernel = new X11Kernel();
        var spec = requested.toBuilder().mode(product.getMode()).forecastHorizon(horizon).build();
        var replay = kernel.process(product.getB1(), spec);
        for (var pair : List.of(List.of(product.getB7(), replay.getB7()),
                                List.of(product.getC7(), replay.getC7()),
                                List.of(product.getD7(), replay.getD7()),
                                List.of(product.getD10(), replay.getD10()),
                                List.of(product.getD12(), replay.getD12()))) {
            if (!pair.get(0).equals(pair.get(1))) throw new AssertionError("stock X11 replay " + template + "/" + id);
        }
        var ctx = kernel.getContext();
        var d = kernel.getDstep();
        System.out.println("case " + template + " " + id);
        var inputs = List.of(kernel.getBstep().getB6(), kernel.getCstep().getC6(), d.getD6(), d.getD11bis());
        var stages = List.of("b7", "c7", "d7", "final");
        for (int i = 0; i < inputs.size(); ++i) {
            double ratio = AutomaticHenderson.calcICR(ctx, inputs.get(i));
            int length = i == 0 && ratio >= 1 ? 13 : AutomaticHenderson.selectFilter(ratio, 12);
            if ((i == 2 && length != product.getD7HendersonFilterLength()) ||
                (i == 3 && (length != product.getFinalHendersonFilterLength() || ratio != product.getICRatio()))) {
                throw new AssertionError("stock Henderson selection");
            }
            System.out.printf("trend %s %d %s%n", stages.get(i), length, bits(ratio));
        }
        var selector = new MsrFilterSelection();
        var filter = selector.doMSR(d.getD9_g_bis(), ctx);
        if (filter != product.getD9filter() || selector.getIterCount() != d.getD9msriter()
            || !Arrays.equals(d.getSeasFilter(), product.getFinalSeasonalFilter())) {
            throw new AssertionError("stock MSR selection");
        }
        System.out.printf("msr %s %d %b %s%n", filter, selector.getIterCount(), selector.isUsingDefault(),
                          bits(selector.getGlobalMsr()));
    }

    static void diagnostics(String preset, String id, jdplus.x13.base.core.x13.X13Results fit) {
        System.out.println("case " + preset + " " + id);
        diagnostics(fit);
    }

    static void diagnostics(jdplus.x13.base.core.x13.X13Results fit) {
        var ser = fit.getData("residuals.ser", Double.class);
        if (ser == null) throw new AssertionError("residuals.ser");
        System.out.println("residuals.ser " + bits(ser));
        for (var key : List.of("residuals.lb", "residuals.doornikhansen")) {
            var test = fit.getData(key, jdplus.toolkit.base.api.stats.StatisticalTest.class);
            if (test == null) throw new AssertionError(key);
            System.out.println(key + " " + bits(test.getValue()) + " " + bits(test.getPvalue())
                + " " + test.getDescription().trim());
        }
        for (int h : new int[]{1, 7, 12, 24}) {
            var se = fit.getData("y_ef(" + h + ")", TsData.class);
            if (se == null || !se.equals(fit.getPreprocessing().forecasts(h).getForecastsStdev()))
                throw new AssertionError("y_ef(" + h + ")");
            series("y_ef_" + h, se);
        }
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
                if (args.length == 2 && args[1].equals("diagnostics")) {
                    diagnostics(template, id, result);
                    continue;
                }
                if (args.length == 2 && args[1].equals("selection")) {
                    selection(template, id, x11, tables, result.getData("y_f", TsData.class).length());
                    continue;
                }
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
