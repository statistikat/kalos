import java.nio.file.*;
import java.util.*;
import jdplus.toolkit.base.api.timeseries.*;
import jdplus.toolkit.base.api.timeseries.regression.ModellingContext;
import jdplus.x13.base.api.x13.X13Spec;
import jdplus.x13.base.core.x13.*;

/** Public preprocessing state and forecast/diagnostic getters on X-13 automatic fits. */
public final class X13AutomaticDiagnosticsProbe {
    static void report(String id, TsData observed, X13Results fit) {
        var model = fit.getPreprocessing();
        var orders = model.arima().orders();
        var stats = model.getEstimation().getStatistics();
        System.out.println("case " + id);
        System.out.printf("model %b %b %d %d %d %d %d %d%n", model.getDescription().isLogTransformation(),
            model.isMeanCorrection(), orders.getP(), orders.getD(), orders.getQ(), orders.getBp(), orders.getBd(), orders.getBq());
        X13FixedTemplateProbe.vector("arma", model.arima().parameters().toArray());
        X13FixedTemplateProbe.vector("coefficients", model.getEstimation().getCoefficients().toArray());
        System.out.printf("likelihood %d %d %s %s%n", stats.getEffectiveObservationsCount(),
            stats.getEstimatedParametersCount(), X13FixedTemplateProbe.bits(stats.getLogLikelihood()),
            X13FixedTemplateProbe.bits(stats.getSsqErr()));
        X13FixedTemplateProbe.series("residuals", model.fullResiduals());
        X13FixedTemplateProbe.diagnostics(fit);
    }

    public static void main(String[] args) throws Exception {
        for (String id : List.of("air", "ao", "seas")) {
            var rows = Files.readAllLines(Path.of(args[0], id + ".csv"));
            var start = rows.remove(0).split(",");
            var values = rows.stream().mapToDouble(Double::parseDouble).toArray();
            var observed = TsData.ofInternal(TsPeriod.monthly(Integer.parseInt(start[0]), Integer.parseInt(start[1])), values);
            var result = X13Kernel.of(X13Spec.RSA3, new ModellingContext()).process(observed, null);
            report("stock_rsa3_" + id, observed, result);
        }
        AutomaticCombinedSeasonalityProbe.publicProducts(Arrays.copyOfRange(args, 1, args.length),
                                                         X13AutomaticDiagnosticsProbe::report);
    }
}
