import java.lang.reflect.Method;
import java.util.HexFormat;
import jdplus.toolkit.base.api.data.DoubleSeq;
import jdplus.toolkit.base.api.math.Complex;
import jdplus.toolkit.base.core.math.linearfilters.BackFilter;
import jdplus.toolkit.base.core.math.linearfilters.SymmetricFilter;
import jdplus.toolkit.base.core.math.polynomials.LeastSquaresDivision;
import jdplus.toolkit.base.core.math.polynomials.Polynomial;
import jdplus.toolkit.base.core.math.polynomials.UnitRoots;
import jdplus.toolkit.base.core.math.polynomials.UnitRootsSolver;

/**
 * Stage oracle for the robust factorizer tier of the M6 SEATS Wiener-Kolmogorov factorizer
 * slice. The argument is the refused total error variance filter (the one-sided weights of
 * calls 3 and 4 of SeatsWkFactorizeCallsProbe) as comma-separated raw binary64 bit patterns.
 * The probe walks the pinned RobustSymmetricFrequencyResponseDecomposer pipeline stage by
 * stage - the two-sided weights, the w0 normalization, UnitRootsSolver(0), the square-root and
 * least-squares-division route it takes when unit roots are found, the SymmetricMullerNewton
 * solver (stable and all roots, remainder, division flag) - and closes with the robust tier
 * result on the filter itself. All values are raw binary64 bit patterns.
 */
public class SeatsWkRobustStagesProbe {
    static String bits(double value) {
        return HexFormat.of().toHexDigits(Double.doubleToRawLongBits(value));
    }

    static String row(String label, double[] values) {
        StringBuilder line = new StringBuilder(label).append(' ').append(values.length);
        for (double value : values) line.append(' ').append(bits(value));
        return line.toString();
    }

    static String row(String label, Complex[] values) {
        StringBuilder line = new StringBuilder(label).append(' ').append(values.length);
        for (Complex value : values) line.append(' ').append(bits(value.getRe())).append(bits(value.getIm()));
        return line.toString();
    }

    static String poly(String label, Polynomial p) {
        return row(label, p.toArray());
    }

    public static void main(String[] args) throws Exception {
        String[] hex = args[0].split(",");
        double[] w = new double[hex.length];
        for (int i = 0; i < hex.length; ++i) w[i] = Double.longBitsToDouble(Long.parseUnsignedLong(hex[i], 16));
        SymmetricFilter filter = new SymmetricFilter(Polynomial.ofInternal(w));
        System.out.println(row("input weights", w));
        System.out.println("w " + (filter.weights().applyAsDouble(0) == w[0] ? "matches w0" : "differs")
            + " " + bits(filter.weights().applyAsDouble(0)));
        System.out.println(row("weightsToArray", filter.weightsToArray()));
        System.out.println(row("poly", filter.coefficientsAsPolynomial().toArray()));
        double w0 = filter.weights().applyAsDouble(0);
        System.out.println("w0 " + bits(w0) + " decline " + !(w0 > 0));
        double[] arr = filter.weightsToArray();
        for (int i = 0; i < arr.length; ++i) arr[i] = arr[i] / w0;
        System.out.println(row("normalized", arr));
        Polynomial p = Polynomial.of(arr);
        System.out.println(poly("p after of", p));

        UnitRootsSolver solver = new UnitRootsSolver(0);
        boolean solved = solver.factorize(p);
        System.out.println("unitroots factorize " + solved);
        if (solved) {
            UnitRoots roots = solver.getUnitRoots();
            UnitRoots sqrt = roots.sqrt();
            System.out.println("sqrt null " + (sqrt == null));
            if (sqrt != null) {
                Polynomial p0 = sqrt.asPolynomial();
                System.out.println(poly("sqrt polynomial", p0));
                Polynomial q0 = p0.times(p0);
                System.out.println(poly("sqrt times sqrt", q0));
                LeastSquaresDivision division = new LeastSquaresDivision();
                boolean exact = division.divide(p, q0);
                System.out.println("ls exact " + exact);
                Polynomial quot = division.getQuotient();
                System.out.println(poly("ls quotient", quot));
                double[] coeff = quot.toArray();
                int degree = quot.degree();
                System.out.println("degree " + degree);
                for (int i = 0; i <= degree / 2; ++i) {
                    double v = (coeff[i] + coeff[degree - i]) / 2.0;
                    coeff[i] = v;
                    coeff[degree - i] = v;
                }
                p = Polynomial.of(coeff);
                System.out.println(poly("p after symmetrize", p));
            }
        }

        Class<?> solverClass = Class.forName("internal.toolkit.base.core.math.linearfilters.SymmetricMullerNewtonSolver");
        Object mn = solverClass.getConstructor().newInstance();
        boolean ok = (boolean) solverClass.getMethod("factorize", Polynomial.class).invoke(mn, p);
        System.out.println("muller factorize " + ok);
        Complex[] stable = (Complex[]) solverClass.getMethod("getStableRoots").invoke(mn);
        System.out.println(row("stable roots re/im", stable));
        Complex[] all = (Complex[]) solverClass.getMethod("roots").invoke(mn);
        System.out.println(row("all roots re/im", all));
        Polynomial remainder = (Polynomial) solverClass.getMethod("remainder").invoke(mn);
        System.out.println(poly("muller remainder", remainder));
        System.out.println("isLeastSquaresDivision " + solverClass.getMethod("isLeastSquaresDivision").invoke(mn));

        Class<?> algorithms = Class.forName("internal.toolkit.base.core.math.linearfilters.SymmetricFilterAlgorithms");
        SymmetricFilter.Factorizer robust = (SymmetricFilter.Factorizer) algorithms
            .getMethod("robustFactorizer").invoke(null);
        var fact = robust.factorize(filter);
        System.out.println(fact == null ? "robust null"
            : "robust scaling " + bits(fact.scaling) + row(" robust factor", bfArray(fact.factor)));
    }

    static double[] bfArray(BackFilter bf) {
        double[] out = new double[bf.length()];
        for (int i = 0; i < out.length; ++i) out[i] = bf.get(i);
        return out;
    }
}
