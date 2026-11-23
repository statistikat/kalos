// Observe the public WK fit immediately below, at and above both monthly
// DefaultModelApproximator airline thresholds. Reuse the pinned probe's
// fixed series, specification construction and full public-kernel observer.
public final class SeatsAirlineApproximationProbe {
    static String field(String[] lines, String prefix) {
        for (String line : lines) {
            if (line.startsWith(prefix)) return line.substring(prefix.length());
        }
        throw new IllegalStateException("missing public observation " + prefix);
    }
    public static void main(String[] args) {
        System.out.println("theta,replacement_btheta,first_adjusted,last_adjusted");
        for (double root : new double[]{0, Math.nextDown(.36), .36, Math.nextUp(.36),
                .5, Math.nextDown(.7), .7, Math.nextUp(.7)}) {
            double theta = root == 0 ? 0 : -root;
            String[] request = {"id boundary", "var 0.5",
                "spec 0 1 " + (theta == 0 ? 0 : 1) + " 0 1 1",
                theta == 0 ? "theta" : "theta " + theta, "btheta 0.8"};
            String[] lines = SeatsWkExtractionProbe.run(request).split("\n");
            if (!field(lines, "d.decompose ").equals("null") || !field(lines, "status ").equals("ok")
                    || !field(lines, "k.modelChanged ").equals("1"))
                throw new IllegalStateException("expected successful retry at theta " + theta);
            String[] ma = field(lines, "k.ucar model ma ").split(" ");
            String[] adjusted = field(lines, "final SeasonallyAdjusted Value n 96 start 1990 0 vals ").split(" ");
            System.out.println(Double.toHexString(theta) + "," + Double.toHexString(Double.parseDouble(ma[12]))
                + "," + Double.toHexString(Double.parseDouble(adjusted[0]))
                + "," + Double.toHexString(Double.parseDouble(adjusted[95])));
        }
    }
}
