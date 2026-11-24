import java.nio.file.Files;
import java.nio.file.Path;
import java.util.HexFormat;
import java.util.List;
import jdplus.sa.base.api.ComponentType;
import jdplus.sa.base.api.SeriesDecomposition;
import jdplus.toolkit.base.api.data.DoubleSeq;
import jdplus.toolkit.base.api.modelling.ComponentInformation;
import jdplus.toolkit.base.api.processing.ProcessingLog;
import jdplus.toolkit.base.api.timeseries.TsData;
import jdplus.toolkit.base.api.timeseries.TsPeriod;
import jdplus.toolkit.base.api.timeseries.regression.ModellingContext;
import jdplus.toolkit.base.core.regsarima.regular.RegSarimaModel;
import jdplus.tramoseats.base.api.tramoseats.TramoSeatsSpec;
import jdplus.tramoseats.base.core.tramo.TramoKernel;
import jdplus.tramoseats.base.core.tramoseats.TramoSeatsKernel;

/**
 * Reference capture for the M6 SEATS Wiener-Kolmogorov factorizer slice. The input is the
 * x13-refusal-jvm-profiles seas.csv monthly series, the same profile whose WK extraction the
 * native mirror refused. For each of the RSA3 and RSAfull presets the probe echoes the TRAMO
 * fit facts (order, parameters, likelihood, residuals), the ucarima model of the decomposition
 * and its compact form (per-component MA, innovation variance, symmetric MA and the four
 * installed factorizer tiers of SymmetricFilterAlgorithms), and then every
 * (ComponentType x ComponentInformation) series of the final decomposition and of the merged
 * finals. All values are raw binary64 bit patterns; the tier rows record which decomposer
 * resolves each filter and with which factor and scaling. The probe reads only the public
 * jdplus 3.9.0 API plus the public tier factories of
 * internal.toolkit.base.core.math.linearfilters.SymmetricFilterAlgorithms, the accessor pair
 * the JVM itself uses to build its default factorizer chain.
 */
public class SeatsWkFactorizerProbe {
    private static String bits(double value) {
        return HexFormat.of().toHexDigits(Double.doubleToRawLongBits(value));
    }

    private static void series(String label, TsData data) {
        TsPeriod start = data.getStart();
        StringBuilder line = new StringBuilder(label).append(' ').append(start.year())
            .append(' ').append(start.annualPosition() + 1).append(' ').append(data.length());
        for (double value : data.getValues().toArray()) line.append(' ').append(bits(value));
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

    private static void tierProbe(String tag, int index, Object comp) {
        Object sym;
        try {
            sym = comp.getClass().getMethod("symmetricMa").invoke(comp);
        } catch (Throwable error) {
            System.out.println("sma " + tag + " " + index + " unavailable " + error);
            return;
        }
        try {
            Object poly = sym.getClass().getMethod("coefficientsAsPolynomial").invoke(sym);
            int degree = (int) poly.getClass().getMethod("degree").invoke(poly);
            StringBuilder line = new StringBuilder("sma " + tag + " " + index + " " + (degree + 1));
            for (int j = 0; j <= degree; ++j)
                line.append(' ').append(bits((double) poly.getClass().getMethod("get", int.class)
                    .invoke(poly, j)));
            System.out.println(line);
            Class<?> filterClass = Class.forName("jdplus.toolkit.base.core.math.linearfilters.SymmetricFilter");
            Class<?> factorizerClass =
                Class.forName("jdplus.toolkit.base.core.math.linearfilters.SymmetricFilter$Factorizer");
            Class<?> algorithms =
                Class.forName("internal.toolkit.base.core.math.linearfilters.SymmetricFilterAlgorithms");
            java.lang.reflect.Method factorize = factorizerClass.getMethod("factorize", filterClass);
            for (String tier : new String[]{"ev2", "ev1", "fast", "robust"}) {
                String method = tier.equals("ev2") ? "evFactorizer2" : tier.equals("ev1") ? "evFactorizer"
                    : tier + "Factorizer";
                try {
                    Object factorization = factorize.invoke(algorithms.getMethod(method).invoke(null), sym);
                    if (factorization == null) {
                        System.out.println("tier " + tag + " " + index + " " + tier + " null");
                        continue;
                    }
                    Object bf = factorization.getClass().getField("factor").get(factorization);
                    double scaling = factorization.getClass().getField("scaling").getDouble(factorization);
                    int length = (int) bf.getClass().getMethod("length").invoke(bf);
                    StringBuilder out = new StringBuilder("tier " + tag).append(' ').append(index)
                        .append(' ').append(tier).append(' ').append(length).append(' ').append(bits(scaling));
                    for (int j = 0; j < length; ++j)
                        out.append(' ').append(bits((double) bf.getClass().getMethod("get", int.class)
                            .invoke(bf, j)));
                    System.out.println(out);
                } catch (Throwable error) {
                    Throwable cause = error.getCause() == null ? error : error.getCause();
                    System.out.println("tier " + tag + " " + index + " " + tier + " threw " + cause);
                }
            }
        } catch (Throwable error) {
            System.out.println("sma " + tag + " " + index + " failed " + error);
        }
    }

    private static void ucDump(String tag, Object uc) {
        try {
            int count = (int) uc.getClass().getMethod("getComponentsCount").invoke(uc);
            System.out.println("uc " + tag + " components " + count);
            for (int i = 0; i < count; ++i) {
                Object comp = uc.getClass().getMethod("getComponent", int.class).invoke(uc, i);
                StringBuilder line = new StringBuilder("uc " + tag + " " + i);
                try {
                    Object ma = comp.getClass().getMethod("getMa").invoke(comp);
                    int length = (int) ma.getClass().getMethod("length").invoke(ma);
                    line.append(" ma ").append(length);
                    for (int j = 0; j < length; ++j)
                        line.append(' ').append(bits((double) ma.getClass().getMethod("get", int.class)
                            .invoke(ma, j)));
                } catch (java.lang.reflect.InvocationTargetException error) {
                    line.append(" maerr ").append(error.getCause());
                }
                try {
                    line.append(" var ").append(bits((double) comp.getClass()
                        .getMethod("getInnovationVariance").invoke(comp)));
                } catch (java.lang.reflect.InvocationTargetException error) {
                    line.append(" varr ").append(error.getCause());
                }
                System.out.println(line);
                tierProbe(tag, i, comp);
            }
        } catch (Throwable error) {
            System.out.println("uc " + tag + " unavailable " + error);
        }
    }

    private static void ucarimaModels(Object decomposition) {
        try {
            ucDump("initial", decomposition.getClass().getMethod("getUcarimaModel").invoke(decomposition));
            ucDump("compact", decomposition.getClass().getMethod("getCompactUcarimaModel").invoke(decomposition));
        } catch (Throwable error) {
            System.out.println("uc unavailable " + error);
        }
    }

    private static void run(String name, TramoSeatsSpec preset, TsData data) {
        System.out.println("== spec " + name);
        try {
            RegSarimaModel model = TramoKernel.of(preset.getTramo(), new ModellingContext())
                .process(data, new ProcessingLog() { });
            if (model == null) {
                System.out.println("preprocessing absent");
            } else {
                var arima = model.arima();
                System.out.printf("model log %b mean %b order %d %d %d %d %d %d%n",
                    model.getDescription().isLogTransformation(), model.isMeanCorrection(),
                    arima.getP(), arima.getD(), arima.getQ(), arima.getBp(), arima.getBd(), arima.getBq());
                StringBuilder pars = new StringBuilder("parameters");
                for (var block : List.of(arima.getPhi(), arima.getBphi(), arima.getTheta(), arima.getBtheta()))
                    for (double value : block.toArray()) pars.append(' ').append(bits(value));
                System.out.println(pars);
                var stats = model.getEstimation().getStatistics();
                System.out.printf("likelihood %s %s %s %s %d %d%n", bits(stats.getLogLikelihood()),
                    bits(stats.getTransformationAdjustment()), bits(stats.getAdjustedLogLikelihood()),
                    bits(stats.getSsqErr()), stats.getEffectiveObservationsCount(),
                    stats.getEstimatedParametersCount());
                series("residuals", model.fullResiduals());
            }
        } catch (Throwable error) {
            System.out.println("preprocessing exception " + error);
        }
        try {
            var sa = TramoSeatsKernel.of(preset, new ModellingContext()).process(data, new ProcessingLog() { });
            if (sa == null) {
                System.out.println("product absent");
                return;
            }
            System.out.printf("product present changed %b%n", sa.getDecomposition().isModelChanged());
            ucarimaModels(sa.getDecomposition());
            components("wk", sa.getDecomposition().getFinalComponents());
            components("merged", sa.getFinals());
        } catch (Throwable error) {
            System.out.println("decomposition exception " + error);
        }
    }

    public static void main(String[] args) throws Exception {
        List<String> lines = Files.readAllLines(Path.of(args[0]));
        String[] start = lines.get(0).split(",");
        double[] values = new double[lines.size() - 1];
        for (int i = 1; i < lines.size(); ++i) values[i - 1] = Double.parseDouble(lines.get(i).trim());
        TsData data = TsData.of(TsPeriod.monthly(Integer.parseInt(start[0].trim()),
            Integer.parseInt(start[1].trim())), DoubleSeq.of(values));
        System.out.println("case seas_input");
        series("input", data);
        run("rsa3", TramoSeatsSpec.RSA3, data);
        run("rsafull", TramoSeatsSpec.RSAfull, data);
    }
}
