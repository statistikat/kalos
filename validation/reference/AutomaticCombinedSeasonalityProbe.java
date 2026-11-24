import java.nio.file.*;
import java.util.*;
import jdplus.sa.base.api.DecompositionMode;
import jdplus.toolkit.base.api.timeseries.*;
import jdplus.toolkit.base.api.timeseries.regression.ModellingContext;
import jdplus.x13.base.core.x13.X13Kernel;

/** Compare the public automatic result getter with D8 on the observed domain. */
public final class AutomaticCombinedSeasonalityProbe {
    interface Observer {
        void accept(String id, TsData input, jdplus.x13.base.core.x13.X13Results fit);
    }

    static void publicProducts(String[] args, Observer observer) throws Exception {
        for (String path : args) {
            var lines = Files.readAllLines(Path.of(path));
            var settings = lines.remove(0).split(" ");
            var spec = X13AutomaticProbe.specification(lines.toArray(String[]::new));
            var x11 = spec.getX11().toBuilder().mode(DecompositionMode.valueOf(settings[1]))
                .backcastHorizon(Integer.parseInt(settings[2])).forecastHorizon(Integer.parseInt(settings[3])).build();
            double[] values = Arrays.stream(lines.stream().filter(s -> s.startsWith("values ")).findFirst().orElseThrow()
                .substring(7).split(";")).mapToDouble(Double::parseDouble).toArray();
            var start = lines.stream().filter(s -> s.startsWith("start ")).findFirst().orElseThrow().substring(6).split(":");
            var data = TsData.ofInternal(TsPeriod.monthly(Integer.parseInt(start[0]), Integer.parseInt(start[1])), values);
            var fit = X13Kernel.of(spec.toBuilder().x11(x11).build(), new ModellingContext()).process(data, null);
            observer.accept(String.join("_", settings), data, fit);
        }
    }

    public static void main(String[] args) throws Exception {
        publicProducts(args, (id, data, fit) -> {
            var result = fit.getDecomposition();
            var si = TsData.fitToDomain(result.getD8(), data.getDomain());
            var verdict = CombinedSeasonalityProbe.observe(id, si,
                result.getMode().isMultiplicative(), "automatic_d8");
            CombinedSeasonalityProbe.compare(fit.getData(CombinedSeasonalityProbe.KEY, String.class), verdict, id);
        });
    }
}
