import java.util.*;
import jdplus.sa.base.api.DecompositionMode;
import jdplus.toolkit.base.api.arima.SarimaSpec;
import jdplus.toolkit.base.api.modelling.TransformationType;
import jdplus.toolkit.base.api.timeseries.*;
import jdplus.toolkit.base.api.timeseries.calendars.LengthOfPeriodType;
import jdplus.toolkit.base.api.timeseries.regression.ModellingContext;
import jdplus.x13.base.api.regarima.*;
import jdplus.x13.base.api.x11.*;
import jdplus.x13.base.api.x13.X13Spec;
import jdplus.x13.base.core.x13.X13Kernel;

/** Observe stock X13Kernel finals after a non-log, pseudo-additive request. */
public final class X13NonlogPseudoProbe {
    static final List<String> TABLES = List.of("d10", "d11", "d12", "d13", "c17", "d11final",
        "d12final", "d13final", "d16", "d18", "e1", "e2", "e3", "e11");

    static void row(String key, double[] values) {
        System.out.print(key);
        for (double value : values) System.out.printf(" %016x", Double.doubleToRawLongBits(value));
        System.out.println();
    }

    static void fixed(TsData ts) {
        var model = RegArimaSpec.builder().usingAutoModel(false)
            .basic(BasicSpec.builder().preprocessing(true).build())
            .transform(TransformSpec.builder().function(TransformationType.None)
                .adjust(LengthOfPeriodType.None).outliersCorrection(false).build())
            .regression(RegressionSpec.builder().build()).outliers(OutlierSpec.DEFAULT_DISABLED)
            .arima(SarimaSpec.builder().d(1).bd(1).buildWithoutValidation()).estimate(EstimateSpec.DEFAULT).build();
        var x11 = X11Spec.builder().mode(DecompositionMode.PseudoAdditive).seasonal(true)
            .filters(new SeasonalFilterOption[]{SeasonalFilterOption.S3X5}).hendersonFilterLength(13)
            .lowerSigma(1.5).upperSigma(2.5).forecastHorizon(0).backcastHorizon(0).build();
        var fit = X13Kernel.of(X13Spec.builder().regArima(model).x11(x11).build(), new ModellingContext()).process(ts, null);
        var x = fit.getDecomposition();
        if (fit.getPreprocessing().getDescription().isLogTransformation() || x.getMode() != DecompositionMode.PseudoAdditive)
            throw new AssertionError("fixed transform or mode");
        var f = fit.getFinals();
        var tables = List.of(x.getD10(), x.getD11(), x.getD12(), x.getD13(), x.getC17(),
            f.getD11final(), f.getD12final(), f.getD13final(), f.getD16(), f.getD18(), f.getE1(), f.getE2(), f.getE3(), f.getE11());
        for (int i = 0; i < tables.size(); ++i) row(TABLES.get(i), tables.get(i).getValues().toArray());
    }

    public static void main(String[] args) {
        var lines = new ArrayList<String>();
        new Scanner(System.in).useDelimiter("\\n").forEachRemaining(lines::add);
        lines.add("mode PseudoAdditive");
        String id = lines.stream().filter(s -> s.startsWith("id ")).findFirst().orElseThrow().substring(3);
        double[] values = Arrays.stream(lines.stream().filter(s -> s.startsWith("values ")).findFirst().orElseThrow()
            .substring(7).split(";")).mapToDouble(Double::parseDouble).toArray();
        System.out.println("case " + id);
        row("input", values);
        if (args[0].equals("fixed")) {
            fixed(TsData.ofInternal(TsPeriod.monthly(1990, 1), values));
            return;
        }
        String result = X13AutomaticProbe.run(lines.toArray(String[]::new));
        if (!result.contains("\nstatus ok\n") || !result.contains("\nlog false\n") ||
            !result.contains("\nmode_effective PseudoAdditive\n")) throw new AssertionError(result);
        int count = 0;
        for (String line : result.split("\n")) {
            int space = line.indexOf(' ');
            if (space < 1 || !TABLES.contains(line.substring(0, space))) continue;
            row(line.substring(0, space), Arrays.stream(line.substring(space+1).split(";"))
                .mapToDouble(Double::parseDouble).toArray());
            ++count;
        }
        if (count != TABLES.size()) throw new AssertionError("automatic table count");
    }
}
