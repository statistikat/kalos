package jdplus.toolkit.base.core.dstats;

import java.io.PrintWriter;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.List;

/**
 * Item 23-C.2 golden generator for the noncentral-beta-free F kernel of the
 * pinned stack: SpecialFunctions.incBetaCf (modified-Lentz continued
 * fraction), SpecialFunctions.incBeta and SpecialFunctions.FProbability -
 * the machine behind the SeasonalFTest p-values consumed by the TRAMO
 * differencing battery (docs/contracts/tramo-differencing-decision-contract.
 * md). Lives in the SpecialFunctions package to reach the package-private
 * incBetaCf. Emits rows at RAW BIT level (Double.doubleToRawLongBits):
 *   cf <a> <b> <x> <out>          incBetaCf(a, b, x)
 *   beta <a> <b> <x> <out>        incBeta(a, b, x)
 *   fprob <v> <k1> <k2> <out>     FProbability(v, k1, k2)
 * and, for every line "id v k1 k2" of the optional corpus file (the
 * (value, df1, df2) triples recorded by the battery capture),
 *   corpus <id> <v> <k1> <k2> <out>.
 * A "-throw" suffix on the tag marks a DStatException (guard or the 1000
 * iteration bound); the out field is then 0.
 *
 * Capture recipe (mirrors SpecialFunctionsGolden, intrinsic-disabled G2):
 *   javac --release 21 -cp "$CP" -d classes FcdfGolden.java
 *   _JAVA_OPTIONS="-XX:+UnlockDiagnosticVMOptions -XX:DisableIntrinsic=_dexp \
 *       -XX:DisableIntrinsic=_dlog" \
 *   java -cp "classes:$CP" jdplus.toolkit.base.core.dstats.FcdfGolden \
 *       GOLDEN.txt [triples.txt]
 * Run twice as separate processes and compare bytes: the output must be
 * byte-identical (validation/reference/capture-tramo-differencing-battery.R
 * records the pair and the byte-identity check).
 */
public class FcdfGolden {

    static PrintWriter out;

    static String hex(double d) {
        return String.format("0x%016x", Double.doubleToRawLongBits(d));
    }

    static double d(String s) {
        switch (s.toLowerCase()) {
            case "nan":
                return Double.NaN;
            case "-nan":
                return -Double.NaN;
            case "inf":
            case "infinity":
                return Double.POSITIVE_INFINITY;
            case "-inf":
            case "-infinity":
                return Double.NEGATIVE_INFINITY;
            case "-0":
                return -0.0;
            default:
                return Double.parseDouble(s);
        }
    }

    static void cf(String a, String b, String x) {
        double da = d(a), db = d(b), dx = d(x);
        try {
            out.println("cf " + hex(da) + " " + hex(db) + " " + hex(dx) + " "
                    + hex(SpecialFunctions.incBetaCf(da, db, dx)));
        } catch (Throwable t) {
            out.println("cf-throw " + hex(da) + " " + hex(db) + " " + hex(dx) + " " + hex(0.0));
        }
    }

    static void beta(String a, String b, String x) {
        double da = d(a), db = d(b), dx = d(x);
        try {
            out.println("beta " + hex(da) + " " + hex(db) + " " + hex(dx) + " "
                    + hex(SpecialFunctions.incBeta(da, db, dx)));
        } catch (Throwable t) {
            out.println("beta-throw " + hex(da) + " " + hex(db) + " " + hex(dx) + " " + hex(0.0));
        }
    }

    static void fprob(String v, String k1, String k2) {
        double dv = d(v), dk1 = d(k1), dk2 = d(k2);
        try {
            out.println("fprob " + hex(dv) + " " + hex(dk1) + " " + hex(dk2) + " "
                    + hex(SpecialFunctions.FProbability(dv, dk1, dk2)));
        } catch (Throwable t) {
            out.println("fprob-throw " + hex(dv) + " " + hex(dk1) + " " + hex(dk2) + " " + hex(0.0));
        }
    }

    public static void main(String[] args) throws Exception {
        out = new PrintWriter(Files.newBufferedWriter(Paths.get(args[0])));

        // --- specials and guards -------------------------------------------
        String[] spec = {"0", "1", "0.5", "2", "-0", "nan", "-nan", "inf", "-inf",
            "1e-300", "1e-65", "-1e-65", "-1", "-0.5", "1e16", "1e308", "4.9e-324"};
        for (String a : spec) {
            for (String b : spec) {
                for (String x : new String[]{"0", "1", "0.5", "nan", "-0", "inf", "-1e-300",
                    "1.0000000000000002", "4.9e-324"}) {
                    cf(a, b, x);
                }
            }
        }
        // Degenerate pairs that stress the iteration bound (large parameters,
        // boundary x).
        String[][] edge = {
            {"1e16", "1e16", "0.5"}, {"1e16", "2", "0.5"}, {"2", "1e16", "0.5"},
            {"1e16", "1e16", "1e-16"}, {"0.001", "0.001", "0.5"}, {"0.001", "1", "0.5"},
            {"1", "0.001", "0.5"}, {"0.5", "0.5", "0.25"}, {"0.5", "0.5", "0.75"},
            {"1e-65", "1e-65", "0.5"}, {"1000", "1000", "0.0001"}, {"1000", "1000", "0.9999"},
        };
        for (String[] e : edge) {
            cf(e[0], e[1], e[2]);
            beta(e[0], e[1], e[2]);
        }
        // The dcmpg guards: x outside [0, 1], negative a or b (NaN variants
        // above; x == 0 and x == 1 skip the bt factor inside incBeta).
        for (String x : new String[]{"-0", "-1e-300", "1", "1.0000000000000002", "2", "inf",
            "-inf", "nan"}) {
            beta("2", "3", x);
            beta("0", "3", x);
            beta("2", "0", x);
            beta("-1", "3", x);
            beta("nan", "3", x);
            beta("2", "nan", x);
            beta("inf", "inf", x);
        }

        // --- dense grids -----------------------------------------------------
        String[] ab = {"0.1", "0.25", "0.5", "0.75", "1", "1.5", "2", "2.5", "3",
            "4", "5.5", "6", "8", "10", "12", "16", "20.5", "25", "30", "42",
            "50", "84", "96", "120", "250", "500.5", "1000"};
        String[] xs = {"0", "4.9e-324", "1e-65", "1e-16", "1e-8",
            "0.01", "0.05", "0.15625", "0.25", "0.4", "0.5", "0.7", "0.9",
            "0.99", "1 - 4.9e-324", "1"};
        for (String a : ab) {
            for (String b : ab) {
                for (String x : xs) {
                    double xd = x.startsWith("1 - ") ? Math.nextAfter(1.0, 0.0) : d(x);
                    String xt = x.startsWith("1 - ") ? String.valueOf(xd) : x;
                    beta(a, b, xt);
                    if (x.equals("0.5")) {
                        cf(a, b, xt); // cf grid at mid x for all pairs
                    }
                }
            }
        }

        // --- FProbability grid ------------------------------------------------
        String[] vs = {"-0", "0", "4.9e-324", "1e-300", "1e-16", "0.001", "0.01",
            "0.1", "0.5", "1", "2", "3.5", "10", "100", "1e16", "1e300",
            "inf", "nan", "-0.5"};
        String[] ks = {"0", "1", "2", "3", "4", "5", "11", "12", "23", "24", "47", "48",
            "83", "84", "95", "96", "100", "119", "120", "500", "1000", "1e6", "nan",
            "inf", "-1"};
        for (String v : vs) {
            for (String k1 : ks) {
                for (String k2 : new String[]{"1", "2", "11", "12", "84", "95", "nan"}) {
                    fprob(v, k1, k2);
                }
            }
        }

        // --- corpus triples from the battery capture --------------------------
        if (args.length > 1) {
            List<String> lines = Files.readAllLines(Paths.get(args[1]));
            for (String ln : lines) {
                String[] f = ln.trim().split(" +");
                if (f.length < 4 || f[0].isEmpty()) {
                    continue;
                }
                double dv = d(f[1]), dk1 = d(f[2]), dk2 = d(f[3]);
                try {
                    out.println("corpus " + f[0] + " " + hex(dv) + " " + hex(dk1) + " "
                            + hex(dk2) + " " + hex(SpecialFunctions.FProbability(dv, dk1, dk2)));
                } catch (Throwable t) {
                    out.println("corpus " + f[0] + " " + hex(dv) + " " + hex(dk1) + " "
                            + hex(dk2) + " throw");
                }
            }
        }
        out.flush();
        out.close();
        System.out.println("wrote " + args[0]);
    }
}
