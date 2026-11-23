import java.lang.reflect.Method;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
import java.util.HashSet;
import java.util.HexFormat;
import java.util.List;
import java.util.Map;
import java.util.Set;
import jdplus.sa.base.api.ComponentType;
import jdplus.sa.base.api.SaVariable;
import jdplus.sa.base.api.SeriesDecomposition;
import jdplus.sa.base.core.modelling.TwoStepsDecomposition;
import jdplus.toolkit.base.api.modelling.ComponentInformation;
import jdplus.tramoseats.base.api.seats.DecompositionSpec;
import jdplus.tramoseats.base.core.seats.SeatsKernel;
import jdplus.tramoseats.base.core.seats.SeatsToolkit;
import jdplus.toolkit.base.api.data.DoubleSeq;
import jdplus.toolkit.base.api.data.Parameter;
import jdplus.toolkit.base.api.data.ParameterType;
import jdplus.toolkit.base.api.processing.ProcessingLog;
import jdplus.toolkit.base.api.timeseries.TsData;
import jdplus.toolkit.base.api.timeseries.TsPeriod;
import jdplus.toolkit.base.api.timeseries.regression.ModellingContext;
import jdplus.toolkit.base.core.regsarima.regular.RegSarimaModel;
import jdplus.tramoseats.base.api.seats.SeatsModelSpec;
import jdplus.tramoseats.base.api.tramo.TramoSpec;
import jdplus.tramoseats.base.api.tramoseats.TramoSeatsSpec;
import jdplus.tramoseats.base.core.tramo.TramoKernel;
import jdplus.tramoseats.base.core.tramoseats.TramoSeatsKernel;

/** Capture both sides of the private jdplus 3.9.0 fitted-model bridge. */
public class TramoSeatsBridgeProbe {
    private static final Map<String, String> TEMPLATES = Map.of(
        "fit_tr0_air", "TR0", "fit_tr3_air", "TR3", "fit_tr1_ao", "TR1",
        "fit_ls", "TR3", "fit_multi", "TR3");

    private static String bits(double value) {
        return HexFormat.of().toHexDigits(Double.doubleToRawLongBits(value));
    }

    private static void require(boolean ok, String field) {
        if (!ok) throw new AssertionError(field);
    }

    private static void series(String label, TsData data) {
        TsPeriod start = data.getStart();
        StringBuilder line = new StringBuilder(label).append(' ').append(start.year())
            .append(' ').append(start.annualPosition() + 1).append(' ').append(data.length());
        for (double value : data.getValues().toArray()) line.append(' ').append(bits(value));
        System.out.println(line);
    }

    private static void coefficients(String block, DoubleSeq fitted, Parameter[] fixed) {
        require(fitted.length() == fixed.length, block + " length");
        StringBuilder line = new StringBuilder(block).append(' ').append(fixed.length);
        for (int i = 0; i < fixed.length; ++i) {
            require(fixed[i].getType() == ParameterType.Fixed, block + " type");
            require(bits(fitted.get(i)).equals(bits(fixed[i].getValue())), block + " value");
            line.append(' ').append(bits(fitted.get(i))).append("/Fixed");
        }
        System.out.println(line);
    }

    private static void components(String label, SeriesDecomposition decomposition) {
        for (var type : ComponentType.values()) {
            if (type == ComponentType.Undefined) continue;
            for (var info : List.of(ComponentInformation.Value, ComponentInformation.Stdev,
                    ComponentInformation.Forecast, ComponentInformation.StdevForecast,
                    ComponentInformation.Backcast, ComponentInformation.StdevBackcast)) {
                var data = decomposition.getSeries(type, info);
                if (data != null) series(label + " " + type + " " + info, data);
            }
        }
    }

    private static void merge(String id, String template, RegSarimaModel model, SeatsModelSpec seats,
                              int backcasts, int forecasts) {
        var options = DecompositionSpec.DEFAULT.toBuilder()
            .forecastCount(forecasts).backcastCount(backcasts).build();
        var extracted = new SeatsKernel(SeatsToolkit.of(options)).process(seats, new ProcessingLog() {});
        require(extracted != null, id + " SEATS extraction");
        var wk = extracted.getFinalComponents();
        var finals = TwoStepsDecomposition.merge(model, wk);
        System.out.printf("case %s %s backcasts %d forecasts %d cutoff %b changed %b%n", id,
            template, backcasts, forecasts, extracted.isParametersCutOff(), extracted.isModelChanged());
        components("wk", wk);
        components("merged", finals);
    }

    private static void observe(Method bridge, String id, String template, TsData ts, boolean mergeOnly)
            throws ReflectiveOperationException {
        var model = TramoKernel.of(TramoSpec.fromString(template), new ModellingContext())
            .process(ts, new ProcessingLog() {});
        require(model != null, id + " fitted model");
        SeatsModelSpec seats = (SeatsModelSpec) bridge.invoke(null, model);
        if (mergeOnly) {
            merge(id, template, model, seats, 0, -2);
            if (id.equals("fit_multi")) merge(id, template, model, seats, 3, 6);
            return;
        }
        var description = model.getDescription();
        var arima = model.arima();
        var spec = seats.getSarimaSpec();
        var stats = model.getEstimation().getStatistics();
        var input = model.interpolatedSeries(false);
        var effect = model.deterministicEffect(null,
            v -> !SaVariable.isRegressionEffect(v, ComponentType.Undefined));
        var restored = model.backTransform(effect, true);
        var expected = seats.isLog() ? TsData.divide(input, restored) : TsData.subtract(input, restored);
        require(seats.isLog() == description.isLogTransformation(), "transform");
        require(seats.isMeanCorrection() == model.isMeanCorrection(), "mean");
        require(arima.getD() == spec.getD() && arima.getBd() == spec.getBd(), "differences");
        double variance = stats.getSsqErr() /
            (stats.getEffectiveObservationsCount() - stats.getEstimatedParametersCount());
        require(bits(variance).equals(bits(seats.getInnovationVariance())), "variance");
        require(expected.getDomain().equals(seats.getSeries().getDomain()), "domain");

        System.out.println("case " + id + " " + template);
        System.out.printf("model log %b mean %b frequency %d d %d bd %d%n", seats.isLog(),
            seats.isMeanCorrection(), arima.getPeriod(), spec.getD(), spec.getBd());
        System.out.printf("likelihood ssq %s effective %d estimated %d transmitted %s%n",
            bits(stats.getSsqErr()), stats.getEffectiveObservationsCount(),
            stats.getEstimatedParametersCount(), bits(seats.getInnovationVariance()));
        coefficients("phi", arima.getPhi(), spec.getPhi());
        coefficients("bphi", arima.getBphi(), spec.getBphi());
        coefficients("theta", arima.getTheta(), spec.getTheta());
        coefficients("btheta", arima.getBtheta(), spec.getBtheta());
        var variables = description.getVariables();
        var estimates = model.getEstimation().getCoefficients();
        require(variables.length == estimates.length(), "one fitted column per variable");
        for (int i = 0; i < variables.length; ++i) {
            var variable = variables[i];
            System.out.printf("variable %s effect %s coefficient %s%n", variable.getName(),
                SaVariable.regressionEffect(variable), bits(estimates.get(i)));
            if (!SaVariable.isRegressionEffect(variable, ComponentType.Undefined))
                series("variable_effect", model.deterministicEffect(null, v -> v == variable));
        }
        series("interpolated", input);
        series("effect", effect);
        series("backtransformed_effect", restored);
        series("seats_series", seats.getSeries());
        int changed = 0;
        for (int i = 0; i < ts.length(); ++i) {
            String actual = bits(seats.getSeries().getValue(i));
            require(actual.equals(bits(expected.getValue(i))), "series[" + i + "]");
            if (!actual.equals(bits(input.getValue(i)))) ++changed;
        }
        System.out.println("changed " + changed);
    }

    // The public kernel receives only the original dated series and RSA0.
    // These rows never use the private fitted bridge or the isolated WK path.
    private static final Set<String> RSA0 = Set.of(
        "fit_tr0_air", "fit_noout", "fit_ls", "fit_ao", "fit_multi", "fit_nearur");

    private static void rawRsa0(String id, TsData series, String stage) {
        var context = new ModellingContext();
        var logged = new ProcessingLog() {};
        var fit = TramoKernel.of(TramoSpec.TR0, context).process(series, logged);
        require(fit != null, id + " TR0");
        if (stage.equals("rsa0-ljung-box")) {
            var sa = TramoSeatsKernel.of(TramoSeatsSpec.RSA0, new ModellingContext()).process(series, logged);
            var test = sa.getData("residuals.lb", jdplus.toolkit.base.api.stats.StatisticalTest.class);
            require(test != null, "public residuals.lb");
            System.out.println("case " + id);
            System.out.println("residuals.lb " + bits(test.getValue()) + " " + bits(test.getPvalue())
                    + " " + test.getDescription().trim());
            if (id.equals("fit_tr0_air")) {
                for (int n : new int[]{36, 37, 38}) {
                    var input = TsData.ofInternal(series.getStart(), java.util.Arrays.copyOf(series.getValues().toArray(), n));
                    var shortFit = TramoSeatsKernel.of(TramoSeatsSpec.RSA0, new ModellingContext()).process(input, logged);
                    var edge = shortFit.getData("residuals.lb", jdplus.toolkit.base.api.stats.StatisticalTest.class);
                    var values = shortFit.getData("residuals.tsres", TsData.class).getValues().toArray();
                    StringBuilder line = new StringBuilder("short ").append(n).append(" ").append(values.length)
                        .append(" ").append(bits(edge.getValue())).append(" ").append(bits(edge.getPvalue()));
                    for (double v : values) line.append(" ").append(bits(v));
                    System.out.println(line);
                }
            }
            return;
        }
        if (stage.equals("rsa0-residuals")) {
            var sa = TramoSeatsKernel.of(TramoSeatsSpec.RSA0, new ModellingContext()).process(series, logged);
            var residuals = sa.getData("residuals.tsres", TsData.class);
            var ser = sa.getData("residuals.ser", Double.class);
            require(residuals != null && residuals.equals(fit.fullResiduals()), "public residuals.tsres");
            require(ser != null && Double.isFinite(ser), "public residuals.ser");
            System.out.println("case " + id);
            series("residuals.tsres", residuals);
            System.out.println("residuals.ser " + bits(ser));
            return;
        }
        if (stage.equals("rsa0-forecast-errors")) {
            var sa = TramoSeatsKernel.of(TramoSeatsSpec.RSA0, new ModellingContext()).process(series, logged);
            var forecasts = fit.forecasts(24);
            var publicErrors = sa.getData("y_ef(24)", TsData.class);
            require(publicErrors != null && publicErrors.equals(forecasts.getForecastsStdev()), "public y_ef");
            System.out.println("case " + id);
            series("y_ef", publicErrors);
            series("wk_series_stdev_forecast", sa.getDecomposition().getFinalComponents()
                .getSeries(ComponentType.Series, ComponentInformation.StdevForecast));
            return;
        }
        var arima = fit.arima();
        var stats = fit.getEstimation().getStatistics();
        System.out.println("case " + id);
        series("input", series);
        require(!fit.getDescription().isLogTransformation() && arima.getD() == 1 && arima.getBd() == 1
                && arima.getPhi().isEmpty() && arima.getBphi().isEmpty()
                && arima.getTheta().length() == 1 && arima.getBtheta().length() == 1, "TR0 airline");
        var coefficients = fit.getEstimation().getCoefficients();
        require(coefficients.length() == 1 && fit.getDescription().getVariables()[0].getName().equals("const"),
                "TR0 mean");
        System.out.printf("fit %s %s %s %s %s %d %d%n", bits(arima.getTheta().get(0)),
            bits(arima.getBtheta().get(0)), bits(coefficients.get(0)), bits(stats.getLogLikelihood()),
            bits(stats.getSsqErr()), stats.getEffectiveObservationsCount(), stats.getEstimatedParametersCount());
        var result = TramoSeatsKernel.of(TramoSeatsSpec.RSA0, new ModellingContext()).process(series, logged);
        if (result == null) {
            System.out.println("product absent");
        } else {
            System.out.printf("product present changed %b%n", result.getDecomposition().isModelChanged());
            components("merged", result.getFinals());
        }
    }

    public static void main(String[] args) throws Exception {
        String stage = args.length > 1 ? args[1] : "bridge";
        boolean raw = stage.startsWith("rsa0");
        Method bridge = TramoSeatsKernel.class.getDeclaredMethod("of", RegSarimaModel.class);
        bridge.setAccessible(true);
        List<String> lines = Files.readAllLines(Path.of(args[0]));
        Set<String> seen = new HashSet<>();
        for (int i = 0; i < lines.size(); ++i) {
            if (!lines.get(i).startsWith("KER ")) continue;
            String[] header = lines.get(i).split(" ");
            String id = header[1];
            if (!(raw ? RSA0 : TEMPLATES.keySet()).contains(id)) continue;
            require(seen.add(id), id + " duplicated");
            int n = header.length;
            require(header[n-3].equals("12") && lines.get(i+1).startsWith("values "), id);
            TsPeriod start = TsPeriod.monthly(Integer.parseInt(header[n-2]), Integer.parseInt(header[n-1]));
            double[] values = Arrays.stream(lines.get(i+1).substring(7).split(" "))
                .mapToDouble(Double::parseDouble).toArray();
            TsData data = TsData.of(start, DoubleSeq.of(values));
            if (raw) rawRsa0(id, data, stage);
            else observe(bridge, id, TEMPLATES.get(id), data, args.length > 1 && args[1].equals("merge"));
        }
        require(seen.equals(raw ? RSA0 : TEMPLATES.keySet()), "incomplete corpus");
    }
}
