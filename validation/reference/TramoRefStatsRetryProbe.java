import java.lang.reflect.Method;
import java.util.ArrayList;
import jdplus.toolkit.base.core.regsarima.regular.RegSarimaModel;
import jdplus.tramoseats.base.api.seats.SeatsModelSpec;
import jdplus.tramoseats.base.core.tramoseats.TramoSeatsKernel;
import java.util.List;
import jdplus.toolkit.base.api.data.DoubleSeq;
import jdplus.toolkit.base.api.processing.ProcessingLog;
import jdplus.toolkit.base.api.timeseries.TsData;
import jdplus.toolkit.base.api.timeseries.TsPeriod;
import jdplus.toolkit.base.api.timeseries.regression.ModellingContext;
import jdplus.tramoseats.base.api.tramo.TramoSpec;
import jdplus.tramoseats.base.core.tramo.TramoKernel;

/** Public monthly models with residual correlation beyond the regular ARMA lags. */
public final class TramoRefStatsRetryProbe {
    static double[] series(int seed, int profile) {
        int n = new int[]{96, 144, 192, 240}[seed % 4], lag = 5 + seed % 7;
        double[] y = new double[n], noise = new double[n];
        long state = seed + 173;
        double level = 500;
        for (int t = 0; t < n; ++t) {
            state = (1664525 * state + 1013904223) & 0xffffffffL;
            double e = ((state >>> 16) - 32768) / 4096.0;
            noise[t] = e;
            double previous = t < lag ? 0 : noise[t-lag];
            double value = switch (profile) {
                case 0 -> e + .95 * previous;
                case 1 -> e + 1.4 * previous;
                case 2 -> e - (t < 2 * lag ? 0 : noise[t-2*lag]) + .85 * previous;
                case 4 -> e + previous + (t == n/2 ? 70 : 0);
                case 5 -> e - .95 * previous;
                default -> throw new IllegalArgumentException("unknown lag profile");
            };
            level += .03 + .01 * e;
            y[t] = level + value + ((t + seed) % 12 - 6) * (seed % 3);
        }
        return y;
    }

    static void observe(boolean wk, int seed, int profile, List<Integer> expected) throws Exception {
        var data = TsData.of(TsPeriod.monthly(1990, 1), DoubleSeq.of(series(seed, profile)));
        List<Integer> rounds = new ArrayList<>();
        var kernel = TramoKernel.of(TramoSpec.TR3, new ModellingContext());
        var model = kernel.process(data, new ProcessingLog() {
            public void step(String name, Object detail) {
                if (name.startsWith("round ")) rounds.add(Integer.valueOf(name.substring(6)));
            }
        });
        if (model == null || !rounds.equals(expected)) throw new AssertionError(rounds);
        String id = "fit_ref_retry_" + seed + "_" + profile;
        if (wk) {
            Method bridge = TramoSeatsKernel.class.getDeclaredMethod("of", RegSarimaModel.class);
            bridge.setAccessible(true);
            TramoSeatsBridgeProbe.merge(id, "TR3", model, (SeatsModelSpec)bridge.invoke(null, model), 0, -2);
        } else TramoSeatsBridgeProbe.rawAutomatic(id, data, "rsa3");
    }

    public static void main(String[] args) throws Exception {
        boolean wk = args.length == 1 && args[0].equals("wk");
        observe(wk, 0, 5, List.of(0, 1, 2, 1));
        observe(wk, 0, 1, List.of(0, 1, 2, 1, 4));
        observe(wk, 1, 5, List.of(0, 1, 2, 3));
        observe(wk, 2, 0, List.of(0, 1, 2, 3));
        observe(wk, 0, 4, List.of(0, 1, 2, 3, 1));
        observe(wk, 17, 2, List.of(0, 1, 2, 3, 4));
        observe(wk, 18, 2, List.of(0, 1, 2, 3, 1));
    }
}
