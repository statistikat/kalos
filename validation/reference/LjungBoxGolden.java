package jdplus.toolkit.base.core.stats.tests;

import java.io.PrintWriter;
import java.lang.reflect.Method;
import java.util.function.IntToDoubleFunction;

/**
 * Item 21-H golden generator for the automatic-model diagnostics chain:
 * AutoCovariances.autoCorrelationFunction, the LjungBox(seq, false) statistic
 * and Chi2 upper-tail p-value (ModelController.calcLb machine), the private
 * ModelController.calcLbLength / calcResStat rules (reached by reflection).
 * Emits "tag ints... valueHex" rows for tests/jvm_arma_diagnostics.h (via
 * tools/gen_arma_diagnostics_golden.py).
 *
 * Capture recipe (intrinsic-disabled, same jars as SpecialFunctionsGolden):
 *   javac -cp "$CP" -d goldenclasses LjungBoxGolden.java
 *   _JAVA_OPTIONS="-XX:+UnlockDiagnosticVMOptions -XX:DisableIntrinsic=_dexp \
 *       -XX:DisableIntrinsic=_dlog" \
 *   java -cp "goldenclasses:$CP" jdplus.toolkit.base.core.stats.tests.LjungBoxGolden GOLDEN.txt
 *
 * Series are exact-arithmetic recurrences (no transcendentals, no RNG): the
 * C++ test replays identical inputs bit-exactly.
 */
public class LjungBoxGolden {

    static PrintWriter out;

    static String hex(double d) {
        return String.format("0x%016x", Double.doubleToRawLongBits(d));
    }

    // Exact recurrences / rational constructions per case id.
    static double[] series(int id) {
        switch (id) {
            case 0: { // white-noise-like xorshift mapped to rationals in [-1, 1)
                double[] x = new double[120];
                long s = 88172645463325252L;
                for (int i = 0; i < x.length; ++i) {
                    s ^= s << 13; s ^= s >>> 7; s ^= s << 17;
                    x[i] = (double) ((s >>> 11) % 1000000L) / 500000.0 - 1.0;
                }
                return x;
            }
            case 1: { // AR(1) recursion phi = 1/2 over case-0 innovations
                double[] e = series(0);
                double[] x = new double[e.length];
                x[0] = e[0];
                for (int i = 1; i < x.length; ++i) {
                    x[i] = 0.5 * x[i - 1] + e[i];
                }
                return x;
            }
            case 2: { // linear ramp plus alternating sign (trend + cycle)
                double[] x = new double[144];
                for (int i = 0; i < x.length; ++i) {
                    x[i] = (double) i / 7.0 + ((i & 1) == 0 ? 1.0 : -1.0) * ((double) (i % 12) / 5.0);
                }
                return x;
            }
            case 3: { // constant
                double[] x = new double[60];
                for (int i = 0; i < x.length; ++i) {
                    x[i] = 3.5;
                }
                return x;
            }
            case 4: { // near-constant: variance under the 1e-38 floor
                double[] x = new double[60];
                for (int i = 0; i < x.length; ++i) {
                    x[i] = 1.0 + ((i & 3) - 1) * 1e-25;
                }
                return x;
            }
            case 5: { // MA(1)-style recursion with seasonal alternation, short
                double[] e = series(0);
                double[] x = new double[40];
                for (int i = 0; i < x.length; ++i) {
                    x[i] = e[i] + 0.25 * (i > 0 ? e[i - 1] : 0.0);
                }
                return x;
            }
            case 6: { // alternating growth/decay recursion (unstable-ish)
                double[] x = new double[100];
                x[0] = 1.0;
                x[1] = -0.5;
                for (int i = 2; i < x.length; ++i) {
                    x[i] = 0.9 * x[i - 1] + 0.4 * x[i - 2];
                }
                return x;
            }
            case 7: { // sparse impulses
                double[] x = new double[96];
                for (int i = 0; i < x.length; ++i) {
                    x[i] = (i % 12 == 0) ? 4.0 : 0.0;
                }
                return x;
            }
            case 8: { // tiny length (exercises calcLbLength halving / k >= n)
                double[] x = new double[10];
                for (int i = 0; i < x.length; ++i) {
                    x[i] = (double) (i * i % 7) / 3.0 - 1.0;
                }
                return x;
            }
            default:
                throw new IllegalArgumentException("bad case " + id);
        }
    }

    public static void main(String[] args) throws Exception {
        out = new PrintWriter(args[0]);
        Class<?> mc = Class.forName("jdplus.x13.base.core.x13.regarima.ModelController");
        Method calcLbLength = mc.getDeclaredMethod("calcLbLength", int.class, int.class);
        calcLbLength.setAccessible(true);
        Method calcResStat = mc.getDeclaredMethod("calcResStat",
                jdplus.toolkit.base.core.data.DataBlock.class);
        calcResStat.setAccessible(true);
        Method getRTVal = mc.getDeclaredMethod("getRTval");
        Object controller = mc.getConstructor(double.class).newInstance(0.95);

        int nc = 9;
        for (int id = 0; id < nc; ++id) {
            double[] x = series(id);
            StringBuilder sb = new StringBuilder();
            sb.append("series ").append(id).append(' ').append(x.length);
            for (double v : x) {
                sb.append(' ').append(hex(v));
            }
            out.println(sb);
        }

        // acf rows: mean flags 0 (chain) and 1 (average(), the other ctor).
        for (int id = 0; id < nc; ++id) {
            double[] x = series(id);
            for (int flag = 0; flag <= 1; ++flag) {
                double mean = flag == 0 ? 0.0 : seq(x).average();
                IntToDoubleFunction acf = jdplus.toolkit.base.api.stats.AutoCovariances
                        .autoCorrelationFunction(seq(x), mean);
                int maxLag = Math.min(30, x.length - 1);
                for (int lag = 1; lag <= maxLag; ++lag) {
                    out.println("acf " + id + " " + flag + " " + lag + " "
                            + hex(acf.applyAsDouble(lag)));
                }
            }
        }

        // LjungBox machine: new LjungBox(seq).autoCorrelationsCount(k)
        // .hyperParametersCount(nhp).build() (lag = 1, sign = 0).
        int[][] ks = {{6, 8, 12, 18, 24}, {6, 12}, {12, 24}, {6, 8, 24}};
        for (int id = 0; id < nc; ++id) {
            double[] x = series(id);
            for (int k : ks[id % ks.length]) {
                int[] nhps = {0, 1, 3, Math.max(0, k - 1), k, k + 1};
                for (int nhp : nhps) {
                    try {
                        jdplus.toolkit.base.api.stats.StatisticalTest test =
                                new LjungBox(seq(x))
                                .autoCorrelationsCount(k)
                                .hyperParametersCount(nhp)
                                .build();
                        out.println("lb " + id + " " + k + " " + nhp + " 0 "
                                + hex(test.getValue()));
                        out.println("lb " + id + " " + k + " " + nhp + " 1 "
                                + hex(test.getPvalue()));
                    } catch (RuntimeException e) {
                        out.println("lb-throw " + id + " " + k + " " + nhp + " 0 "
                                + hex(0.0));
                    }
                }
            }
        }

        // calcLbLength: exhaustive grid.
        int[] freqs = {1, 2, 3, 4, 5, 6, 8, 10, 12, 26, 52, 260};
        int[] neffs = new int[70];
        for (int i = 0; i < 60; ++i) {
            neffs[i] = 1 + i;
        }
        neffs[60] = 65; neffs[61] = 70; neffs[62] = 80; neffs[63] = 96;
        neffs[64] = 100; neffs[65] = 120; neffs[66] = 144; neffs[67] = 200;
        neffs[68] = 300; neffs[69] = 500;
        for (int f : freqs) {
            for (int ne : neffs) {
                out.println("lbLen " + f + " " + ne + " 0 "
                        + hex((double) (int) calcLbLength.invoke(null, f, ne)));
            }
        }

        // calcResStat: rtval per case.
        for (int id = 0; id < nc; ++id) {
            double[] x = series(id);
            jdplus.toolkit.base.core.data.DataBlock db =
                    jdplus.toolkit.base.core.data.DataBlock.make(x.length);
            for (int i = 0; i < x.length; ++i) {
                db.set(i, x[i]);
            }
            calcResStat.invoke(controller, db);
            out.println("rtval " + id + " 0 0 " + hex((Double) getRTVal.invoke(controller)));
        }

        out.flush();
        out.close();
        System.out.println("wrote " + args[0]);
    }

    static jdplus.toolkit.base.api.data.DoubleSeq seq(double[] x) {
        return jdplus.toolkit.base.api.data.DoubleSeq.of(x);
    }
}
