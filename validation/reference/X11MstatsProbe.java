import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;
import jdplus.toolkit.base.api.data.DoubleSeq;
import jdplus.toolkit.base.api.timeseries.TsData;
import jdplus.toolkit.base.api.timeseries.TsPeriod;
import jdplus.toolkit.base.api.timeseries.regression.ModellingContext;
import jdplus.x13.base.api.x11.MsrTable;
import jdplus.x13.base.api.x13.X13Spec;
import jdplus.x13.base.core.x11.X11Results;
import jdplus.x13.base.core.x13.Mstatistics;
import jdplus.x13.base.core.x13.X13Finals;
import jdplus.x13.base.core.x13.X13Kernel;
import jdplus.x13.base.core.x13.X13Preadjustment;
import jdplus.x13.base.core.x13.X13Results;
import jdplus.x13.base.core.x13.diagnostics.MDiagnostics;
import jdplus.x13.base.core.x13.diagnostics.MDiagnosticsConfiguration;

/**
 * Emits the inputs and published values of the X-11 M-statistics battery for the slice-P4
 * evidence pack. Each stock profile is fitted directly through the X-13 kernel (RSA3 is the
 * automatic profile; the fixed templates go through X13Spec.fromString) and the series that
 * feed Mstatistics.of are dumped bit-exactly: A1, A8t/s/i, B1, D8, D10-D13, E1-E3, D18, plus
 * the ICRatio, the globally selected MSR value and the D9 choice that drive the M3 and M6
 * presets. Then the published m1..m11 words, q and qm2 from Mstatistics and the
 * MDiagnostics q/q2 grades follow per case. Line breaks are (char) 10 by construction; the
 * JVM runs with the bridge flags so the exp/log bit-patterns match the kalos lanes.
 *
 * Usage: java X11MstatsProbe <profiles-dir>
 */
public class X11MstatsProbe {

    static final char LF = (char) 10;
    static final StringBuilder OUT = new StringBuilder();

    public static void main(String[] args) throws Exception {
        Path dir = Path.of(args[0]);
        for (String stem : List.of("air", "ao", "seas")) {
            report("stock_rsa3_" + stem,
                    X13Kernel.of(X13Spec.RSA3, new ModellingContext()).process(read(dir.resolve(stem + ".csv")), null));
        }
        for (String spec : List.of("rsa0", "rsa1", "rsa2c")) {
            for (String stem : List.of("air", "ao", "seas")) {
                X13Results results = X13Kernel.of(X13Spec.fromString(spec), new ModellingContext())
                        .process(read(dir.resolve(stem + ".csv")), null);
                if (!results.isValid()) {
                    throw new AssertionError("invalid fit for " + spec + " on " + stem);
                }
                report("stock_" + spec + "_" + stem, results);
            }
        }
        System.out.print(OUT);
    }

    static TsData read(Path path) throws Exception {
        List<String> lines = Files.readAllLines(path);
        String[] date = lines.get(0).split(",");
        TsPeriod start = TsPeriod.monthly(Integer.parseInt(date[0]), Integer.parseInt(date[1]));
        double[] values = lines.subList(1, lines.size()).stream().mapToDouble(Double::parseDouble).toArray();
        return TsData.ofInternal(start, values);
    }

    static void report(String id, X13Results results) {
        OUT.append("case ").append(id).append(LF);
        X13Preadjustment pre = results.getPreadjustment();
        X11Results tables = results.getDecomposition();
        X13Finals finals = results.getFinals();
        OUT.append("info ").append(tables.getMode())
                .append(' ').append(tables.getNbackcasts())
                .append(' ').append(tables.getNforecasts()).append(LF);
        OUT.append("icr ").append(bits(tables.getICRatio())).append(LF);
        MsrTable msr = tables.getD9Msr();
        OUT.append("msr ").append(msr == null ? "absent" : bits(msr.getGlobalMsr())).append(LF);
        OUT.append("d9 ").append(tables.getD9filter()).append(' ').append(tables.isD9default()).append(LF);
        series("a1", pre.getA1());
        series("a8", pre.getA8());
        series("a8t", pre.getA8t());
        series("a8s", pre.getA8s());
        series("a8i", pre.getA8i());
        series("b1", tables.getB1());
        series("d8", tables.getD8());
        series("d10", tables.getD10());
        series("d11", tables.getD11());
        series("d12", tables.getD12());
        series("d13", tables.getD13());
        series("e1", finals.getE1());
        series("e2", finals.getE2());
        series("e3", finals.getE3());
        series("d18", finals.getD18());
        Mstatistics m = results.getDiagnostics().getMstatistics();
        StringBuilder sb = new StringBuilder("mstats");
        for (int i = 1; i <= 11; ++i) {
            sb.append(' ').append(bits(m.getM(i)));
        }
        sb.append(' ').append(bits(m.getQ())).append(' ').append(bits(m.getQm2()));
        OUT.append(sb).append(LF);
        MDiagnostics diagnostics = MDiagnostics.of(MDiagnosticsConfiguration.builder().build(), results);
        OUT.append("grade ").append(diagnostics.getDiagnostic("q"))
                .append(' ').append(diagnostics.getDiagnostic("q2")).append(LF);
    }

    static void series(String tag, TsData series) {
        TsPeriod period = series.getStart();
        double[] values = series.getValues().toArray();
        OUT.append(tag).append(' ').append(period.year())
                .append(' ').append(period.annualPosition() + 1)
                .append(' ').append(values.length);
        for (double value : values) {
            OUT.append(' ').append(bits(value));
        }
        OUT.append(LF);
    }

    static String bits(double value) {
        return String.format("%016x", Double.doubleToRawLongBits(value));
    }
}
