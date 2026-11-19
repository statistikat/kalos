package jdplus.toolkit.base.core.math.linearfilters;

import java.io.PrintWriter;

/**
 * Item 21-H golden generator for FilterUtility.checkRoots(DoubleSeq, double)
 * over the degrees the qualified corpus reaches (effective degree <= 2;
 * degree >= 3 runs the Random-seeded companion solver and is out of scope).
 * Emits "cr len limitHex c0..c3Hex resHex" rows; the padded zeros never
 * change the effective degree. Consumed by
 * tools/gen_check_roots_golden.py -> tests/jvm_check_roots.h.
 *
 * Capture recipe (intrinsic-disabled):
 *   javac -cp "$CP" -d goldenclasses CheckRootsGolden.java
 *   _JAVA_OPTIONS="-XX:+UnlockDiagnosticVMOptions -XX:DisableIntrinsic=_dexp \
 *       -XX:DisableIntrinsic=_dlog" \
 *   java -cp "goldenclasses:$CP" jdplus.toolkit.base.core.math.linearfilters.CheckRootsGolden GOLDEN.txt
 */
public class CheckRootsGolden {

    static PrintWriter out;

    static String hex(double d) {
        return String.format("0x%016x", Double.doubleToRawLongBits(d));
    }

    static void probe(double[] c, double limit) {
        double[] padded = new double[4];
        int len = Math.min(c.length, 4);
        for (int i = 0; i < len; ++i) {
            padded[i] = c[i];
        }
        double res;
        boolean threw = false;
        try {
            res = FilterUtility.checkRoots(
                    jdplus.toolkit.base.api.data.DoubleSeq.of(c), limit) ? 1.0 : 0.0;
        } catch (RuntimeException e) {
            threw = true;
            res = 0.0;
        }
        out.println((threw ? "crthrow " : "cr ") + c.length + " " + hex(limit)
                + " " + hex(padded[0]) + " " + hex(padded[1]) + " " + hex(padded[2])
                + " " + hex(padded[3]) + " " + hex(res));
    }

    public static void main(String[] args) throws Exception {
        out = new PrintWriter(args[0]);
        double[] limits = {1.0 / 0.95, 1.0, 0.99999, 2.0, 0.5, -1.0};
        double[] c1 = {0.0, -0.0, 0.5, -0.5, 0.95, -0.95, 1.05, -1.05, 1e-300,
            1e300, -1e300};
        for (double limit : limits) {
            for (double a : c1) {
                probe(new double[]{a}, limit);
            }
            double[] a2 = {0.0, 0.1, -0.1, 0.5, -0.5, 1.1, -1.1, 1.9, -1.9};
            double[] b2 = {0.04, 0.25, 0.81, 1.1, -0.25, -0.5, 0.0, 4.0, -4.0};
            for (double a : a2) {
                for (double b : b2) {
                    probe(new double[]{a, b}, limit);
                }
            }
            // Trailing zeros shrink the effective degree.
            probe(new double[]{0.5, 0.0}, limit);
            probe(new double[]{0.5, 0.0, 0.0}, limit);
            probe(new double[]{0.0, 0.0}, limit);
            probe(new double[]{-0.9, 0.2, 0.0}, limit);
            probe(new double[]{1.2, -0.4, 0.0}, limit);
            // A negative limit short-circuits the >= 3 branch (guard) before
            // the Random-seeded solver; deterministic, so pinnable.
            if (limit < 0.0) {
                probe(new double[]{0.1, 0.2, 0.3}, limit);
                probe(new double[]{0.1, 0.2, 0.3, 0.4}, limit);
            }
        }
        out.flush();
        out.close();
        System.out.println("wrote " + args[0]);
    }
}
