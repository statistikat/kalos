import java.util.HexFormat;
import jdplus.toolkit.base.core.math.linearfilters.SymmetricFilter;

/**
 * Frozen capture: the two adversarial corpus cases of the M6 factorizer fuzz
 * campaign (indices 144 and 304), offered to each installed factorizer tier and
 * to the standard chain of the pinned JVM. Both filters drive the robust tier's
 * SymmetricMullerNewtonSolver through +-0.0-sensitive branch sequences: on the
 * first initialize() division the quotient is cart(-2.0, -0.0), which
 * api.math.Complex normalizes to the NEG_TWO singleton (-2.0, +0.0), and that
 * sign decides the square-root branch of the parabola root two steps later.
 * Both cases therefore resolve on "robust ok" here while a port that builds
 * Complex values without the cart normalization throws
 * ArrayIndexOutOfBoundsException from the solver iteration guard.
 *
 * The cases are drawn from a splitmix64 stream derived from the case index;
 * stdout uses the line format of the differential fuzz harness of the M6
 * campaign: one "case" input row and one "res" row per tier.
 */
public class SeatsWkMullerAdversarialProbe {
    static long state;

    static void seed(int idx) {
        state = 0x9E3779B97F4A7C15L * (idx + 1);
        for (int i = 0; i < 8; ++i)
            next();
    }

    static long next() {
        state += 0x9E3779B97F4A7C15L;
        long z = state;
        z = (z ^ (z >>> 30)) * 0xBF58476D1CE4E5B9L;
        z = (z ^ (z >>> 27)) * 0x94D049BB133111EBL;
        return z ^ (z >>> 31);
    }

    static String bits(double value) {
        return HexFormat.of().toHexDigits(Double.doubleToRawLongBits(value));
    }

    static void runCase(int idx) throws Exception {
        seed(idx);
        int n = 1 + (int) Long.remainderUnsigned(next(), 30);
        int regime = (int) Long.remainderUnsigned(next(), 6);
        double[] w = new double[n];
        for (int i = 0; i < n; ++i) {
            long raw = next();
            double unit = (double) Long.remainderUnsigned(raw, 1000000L) / 1000000.0;
            int sign = (int) Long.remainderUnsigned(raw >>> 20, 2) == 0 ? 1 : -1;
            switch (regime) {
                case 0 -> w[i] = sign * (0.05 + 0.9 * unit) / n;
                case 1 -> {
                    long e = 500 + Long.remainderUnsigned(next(), 15000);
                    long m = next() & 0xFFFFFFFFFFFFFL;
                    w[i] = sign * Double.longBitsToDouble((e << 52) | m);
                }
                case 2 -> w[i] = i == 0 ? 1.0 : sign * unit / n;
                case 3 -> w[i] = sign * (i == 0 ? 1e-300 : unit * 1e-300);
                case 4 -> w[i] = sign * (i == 0 ? 1e150 : unit * 1e150);
                default -> w[i] = sign * (0.25 + 0.5 * unit) * (1.0 + 0.125 * i);
            }
        }
        StringBuilder head = new StringBuilder("case ").append(idx).append(' ').append(n)
            .append(' ').append(regime);
        for (double value : w)
            head.append(' ').append(bits(value));
        System.out.println(head);
        System.out.flush();
        SymmetricFilter filter = SymmetricFilter.ofInternal(w.clone());
        Class<?> algorithms =
            Class.forName("internal.toolkit.base.core.math.linearfilters.SymmetricFilterAlgorithms");
        String[] names = {"ev2", "ev1", "fast", "robust", "chain"};
        String[] getters = {"evFactorizer2", "evFactorizer", "fastFactorizer", "robustFactorizer",
            "factorizer"};
        java.lang.reflect.Method factorize = Class.forName(
            "jdplus.toolkit.base.core.math.linearfilters.SymmetricFilter$Factorizer")
            .getMethod("factorize", SymmetricFilter.class);
        // The chain row uses the consumer seam: SymmetricFilter.factorize() =
        // degree-0 shortcut plus the default (ev2->ev1->fast->robust) chain,
        // exactly what the kalos factorize_sf mirror calls. The bare
        // factorizer() lambda omits the shortcut and is NOT the seam.
        java.lang.reflect.Method defaultSeam = SymmetricFilter.class.getMethod("factorize");
        for (int t = 0; t < 5; ++t) {
            StringBuilder line = new StringBuilder("res ").append(idx).append(' ')
                .append(names[t]);
            try {
                Object out = t < 4
                    ? factorize.invoke(algorithms.getMethod(getters[t]).invoke(null), filter)
                    : defaultSeam.invoke(filter);
                if (out == null) {
                    System.out.println(line.append(" null"));
                    continue;
                }
                Object bf = out.getClass().getField("factor").get(out);
                double scaling = out.getClass().getField("scaling").getDouble(out);
                int length = (int) bf.getClass().getMethod("length").invoke(bf);
                line.append(" ok ").append(bits(scaling)).append(' ').append(length);
                for (int j = 0; j < length; ++j)
                    line.append(' ').append(bits((double) bf.getClass().getMethod("get", int.class)
                        .invoke(bf, j)));
                System.out.println(line);
            } catch (Throwable error) {
                System.out.println(line.append(" threw"));
            }
            System.out.flush();
        }
    }

    public static void main(String[] args) throws Exception {
        for (int idx : new int[]{144, 304})
            runCase(idx);
    }
}
