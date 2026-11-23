// The two monthly SeasonalityTests spectrum classifiers on identical inputs.
import jdplus.sa.base.core.tests.AutoRegressiveSpectrumTest;
import jdplus.sa.base.core.tests.TukeySpectrumPeaksTest;
import jdplus.toolkit.base.api.data.DoubleSeq;

public final class TramoSeasonalPeaksProbe {
    private static String codes(int[] values) {
        StringBuilder result = new StringBuilder();
        for (int code : values) result.append(code);
        return result.toString();
    }

    public static void main(String[] args) {
        for (int n : new int[]{24, 59, 60, 61, 72, 79, 80, 81, 96, 119, 120, 121, 144, 240}) {
            for (int seed = 0; seed < 12; ++seed) {
                long state = seed + 1;
                double[] values = new double[n];
                for (int t = 0; t < n; ++t) {
                    state = (1664525 * state + 1013904223) & 0xffffffffL;
                    int month = (t + seed) % 12;
                    int seasonal = (7 * month + month * month) % 17 - 8;
                    values[t] = ((state >>> 16) - 32768) / 4096.0 + seasonal * (seed % 4) / 2.0;
                }
                DoubleSeq data = DoubleSeq.of(values);
                var ar = new AutoRegressiveSpectrumTest();
                var tukey = new TukeySpectrumPeaksTest();
                String a = ar.test(data, 12) ? codes(ar.seasonalPeaks(.9, .99)) : "-";
                String b = tukey.test(data, 12) ? codes(tukey.seasonalPeaks(.9, .99)) : "-";
                System.out.println(n + " " + seed + " " + a + " " + b);
            }
        }
    }
}
