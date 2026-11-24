import java.io.PrintStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.HexFormat;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;import jdplus.toolkit.base.api.data.DoubleSeq;
import jdplus.toolkit.base.api.processing.ProcessingLog;
import jdplus.toolkit.base.api.timeseries.TsData;
import jdplus.toolkit.base.api.timeseries.TsPeriod;
import jdplus.toolkit.base.api.timeseries.regression.ModellingContext;
import jdplus.toolkit.base.core.math.linearfilters.BackFilter;
import jdplus.toolkit.base.core.math.linearfilters.SymmetricFilter;
import jdplus.tramoseats.base.api.tramoseats.TramoSeatsSpec;
import jdplus.tramoseats.base.core.tramoseats.TramoSeatsKernel;

/**
 * Reference capture for the M6 SEATS Wiener-Kolmogorov factorizer slice: the ledger of every
 * SymmetricFilter.factorize() call the pinned JVM performs while running the RSA3 preset on the
 * x13-refusal-jvm-profiles seas.csv series. The probe temporarily installs a logging decorator
 * as the default factorizer. The decorator only delegates to the standard chain captured before
 * installation, so the run and its products are the untouched pinned ones; the two trailing
 * summary lines confirm the product and its unchanged decomposition. Each fac row records the
 * one-sided filter coefficients and the six innermost caller frames, the resolved factor and
 * scaling of the standard chain, and one tier row per installed decomposer of
 * internal.toolkit.base.core.math.linearfilters.SymmetricFilterAlgorithms (ev2, ev1, fast,
 * robust), null when the tier declines. All values are raw binary64 bit patterns. The total
 * error variance filter the native mirror refused appears as calls 3 and 4 (the Burman stdev
 * estimates and forecasts); it resolves on robust alone.
 */
public class SeatsWkFactorizeCallsProbe {
    static final AtomicInteger seq = new AtomicInteger();
    static PrintStream log = System.out;
    static SymmetricFilter.Factorizer standard;
    static Class<?> TIERS;
    static boolean active;

    static String bits(double value) {
        return HexFormat.of().toHexDigits(Double.doubleToRawLongBits(value));
    }

    static String describe(SymmetricFilter filter) {
        StringBuilder line = new StringBuilder();
        for (double value : filter.coefficientsAsPolynomial().toArray()) {
            line.append(' ').append(bits(value));
        }
        return line.toString();
    }

    public static void main(String[] args) throws Exception {
        Class<?> algorithms = Class.forName("internal.toolkit.base.core.math.linearfilters.SymmetricFilterAlgorithms");
        TIERS = algorithms;
        standard = (SymmetricFilter.Factorizer) algorithms.getMethod("factorizer").invoke(null);
        SymmetricFilter.Factorizer logging = filter -> {
            if (!active) return standard.factorize(filter);
            int id = seq.incrementAndGet();
            int n = filter.coefficientsAsPolynomial().degree() + 1;
            StringBuilder entry = new StringBuilder("fac ").append(id).append(" n=").append(n)
                .append(" poly").append(describe(filter));
            entry.append((String) StackWalker.getInstance().walk(frames -> frames
                .skip(2)
                .limit(6)
                .map(frame -> "\n    at " + frame)
                .reduce("", (left, right) -> left + right)));
            SymmetricFilter.Factorization out = null;
            Throwable error = null;
            try {
                out = standard.factorize(filter);
            } catch (Throwable thrown) {
                error = thrown;
            }
            if (error != null) {
                log.println(entry + " threw " + error);
            } else if (out == null) {
                log.println(entry + " -> null");
            } else {
                BackFilter bf = out.factor;
                StringBuilder res = new StringBuilder(" scaling=").append(bits(out.scaling))
                    .append(" factor n=").append(bf.length());
                for (int i = 0; i < bf.length(); ++i) res.append(' ').append(bits(bf.get(i)));
                log.println(entry + " ->" + res);
                for (String tier : new String[]{"ev2", "ev1", "fast", "robust"}) {
                    try {
                        String method = tier.equals("ev2") ? "evFactorizer2"
                            : tier.equals("ev1") ? "evFactorizer" : tier + "Factorizer";
                        SymmetricFilter.Factorizer f = (SymmetricFilter.Factorizer) TIERS
                            .getMethod(method).invoke(null);
                        SymmetricFilter.Factorization t = f.factorize(filter);
                        if (t == null) {
                            log.println("  tier " + id + " " + tier + " null");
                        } else {
                            StringBuilder tr = new StringBuilder("  tier ").append(id).append(' ')
                                .append(tier).append(" scaling=").append(bits(t.scaling))
                                .append(" factor n=").append(t.factor.length());
                            for (int i = 0; i < t.factor.length(); ++i)
                                tr.append(' ').append(bits(t.factor.get(i)));
                            log.println(tr);
                        }
                    } catch (Throwable thrown) {
                        Throwable cause = thrown.getCause() == null ? thrown : thrown.getCause();
                        log.println("  tier " + id + " " + tier + " threw " + cause);
                    }
                }
            }
            return out;
        };
        SymmetricFilter.setDefaultFactorizer(logging);

        List<String> lines = Files.readAllLines(Path.of(args[0]));
        String[] start = lines.get(0).split(",");
        double[] values = new double[lines.size() - 1];
        for (int i = 1; i < lines.size(); ++i) values[i - 1] = Double.parseDouble(lines.get(i).trim());
        TsData data = TsData.of(TsPeriod.monthly(Integer.parseInt(start[0].trim()),
            Integer.parseInt(start[1].trim())), DoubleSeq.of(values));

        active = true;
        var sa = TramoSeatsKernel.of(TramoSeatsSpec.RSA3, new ModellingContext())
            .process(data, new ProcessingLog() { });
        active = false;
        SymmetricFilter.setDefaultFactorizer(standard);
        log.println("fac total " + seq.get() + " product " + (sa != null));
        if (sa != null) {
            var decomp = sa.getDecomposition();
            log.println("fac final uc changed " + decomp.isModelChanged());
        }
    }
}
