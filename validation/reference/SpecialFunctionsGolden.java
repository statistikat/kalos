package jdplus.toolkit.base.core.dstats;

import java.io.PrintWriter;

/**
 * Item 21-H golden generator for the incomplete-gamma machine behind the
 * Ljung-Box Chi-square p-values (SpecialFunctions.logGamma/IGSeries/
 * IGContFract/incGamma/chiSquare + Chi2.getProbability(x, Upper)). Emits
 * rows "tag aHex bHex vHex" at raw bit level for tests/jvm_special_
 * functions.h (via tools/gen_special_golden_header.py). Lives in the
 * SpecialFunctions package to reach the package-private IGSeries /
 * IGContFract.
 *
 * Capture recipe (mirrors the item 21-D..G convention, intrinsic-disabled):
 *   JARS=(see capture-x13-fit.R); CP=$(IFS=:; echo "${JARS[*]}")
 *   javac -cp "$CP" -d classes SpecialFunctionsGolden.java
 *   _JAVA_OPTIONS="-XX:+UnlockDiagnosticVMOptions -XX:DisableIntrinsic=_dexp \
 *       -XX:DisableIntrinsic=_dlog" \
 *   java -cp "classes:$CP" jdplus.toolkit.base.core.dstats.SpecialFunctionsGolden GOLDEN.txt
 */
public class SpecialFunctionsGolden {

    static PrintWriter out;

    static String hex(double d) {
        return String.format("0x%016x", Double.doubleToRawLongBits(d));
    }

    static void emit(String tag, double a, double b, double v) {
        out.println(tag + " " + hex(a) + " " + hex(b) + " " + hex(v));
    }

    public static void main(String[] args) throws Exception {
        out = new PrintWriter(args[0]);
        // logGamma: Lanczos knots, sub-unit reflection zone, integer poles
        // and the df/2 half-integers used by the Ljung-Box chain.
        double[] la = {1e-8, 1e-6, 1e-4, 0.01, 0.05, 0.125, 0.25, 0.5,
            0.75, 1.0, 1.25, 1.5, 1.75, 2.0, 2.5, 3.0, 3.5, 4.0, 4.5,
            5.0, 5.5, 6.0, 6.5, 7.0, 7.5, 8.0, 8.5, 9.5, 10.5, 12.5,
            16.5, 20.5, 33.5, 50.25, 77.77, 123.456, 500.5};
        for (double a : la) {
            emit("loggamma", a, 0.0, SpecialFunctions.logGamma(a));
        }
        // IGSeries / IGContFract / incGamma grid over the (a, x) plane the
        // Chi-square chain walks (a = df/2 in {0.25..20.5}, x = stat/2 in
        // {0 .. 150}); x = 0 rows pin the divergent-CF throw surface.
        double[] as = {0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 2.0, 2.5, 3.0,
            3.5, 4.0, 5.0, 5.5, 6.0, 6.5, 8.5, 10.5, 12.5, 16.5, 20.5};
        double[] xs = {0.0, 1e-12, 1e-6, 0.05, 0.25, 0.5, 1.0, 1.5, 2.0,
            3.0, 4.0, 5.5, 7.0, 9.0, 12.0, 16.0, 24.0, 40.0, 75.0, 150.0};
        for (double a : as) {
            for (double x : xs) {
                try {
                    emit("igseries", a, x, SpecialFunctions.IGSeries(a, x));
                } catch (Exception e) {
                    emit("igseries-throw", a, x, 0.0);
                }
                try {
                    emit("igcf", a, x, SpecialFunctions.IGContFract(a, x));
                } catch (Exception e) {
                    emit("igcf-throw", a, x, 0.0);
                }
                emit("incgamma", a, x, SpecialFunctions.incGamma(a, x));
            }
        }
        // Chi-square upper-tail p-values over the realistic (df, statistic)
        // rectangle of the pinned Ljung-Box acceptance test.
        double[] dfs = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0,
            11.0, 12.0, 13.0, 14.0, 15.0, 16.0, 17.0, 18.0, 19.0, 20.0,
            22.0, 24.0, 26.0, 28.0, 30.0, 33.0, 36.0, 40.0, 0.5, 12.5,
            20.5, 100.0};
        double[] stats = {0.0, 1e-12, 0.0125, 0.05, 0.2, 0.45, 0.8, 1.25,
            1.8, 2.45, 3.2, 4.05, 5.0, 6.05, 7.2, 8.45, 9.8, 11.25, 12.8,
            14.45, 16.2, 18.05, 20.0, 25.0, 30.0, 35.0, 40.0, 50.0, 65.0,
            80.0, 100.0, 150.0};
        for (double df : dfs) {
            for (double st : stats) {
                try {
                    double p = new Chi2(df).getProbability(st,
                            jdplus.toolkit.base.api.stats.ProbabilityType.Upper);
                    emit("chi2upper", df, st, p);
                } catch (Throwable t) {
                    emit("chi2-throw", df, st, 0.0);
                }
            }
        }
        out.flush();
        out.close();
        System.out.println("wrote " + args[0]);
    }
}
