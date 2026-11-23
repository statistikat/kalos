import jdplus.toolkit.base.api.timeseries.TsData;
import jdplus.toolkit.base.api.timeseries.TsPeriod;
import jdplus.toolkit.base.api.timeseries.regression.ModellingContext;
import jdplus.x13.base.api.x13.X13Spec;
import jdplus.x13.base.core.x13.X13Kernel;
import jdplus.x13.base.r.X13;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;

public class X13RefusalDemandProbe {
    public static void main(String[] args) throws Exception {
        Path input = Path.of(args[0]), output = Path.of(args[1]);
        for (String template : List.of("rsa0", "rsa1", "rsa2c")) {
            for (String series : List.of("air", "ao", "seas")) {
                var lines = Files.readAllLines(input.resolve(series + ".csv"));
                String[] start = lines.getFirst().split(",");
                var data = TsData.ofInternal(TsPeriod.monthly(Integer.parseInt(start[0]),
                        Integer.parseInt(start[1])), lines.stream().skip(1).mapToDouble(Double::parseDouble).toArray());
                var result = X13Kernel.of(X13Spec.fromString(template), new ModellingContext()).process(data, null);
                String id = template + "-" + series;
                Files.write(output.resolve(id + ".pb"), X13.toBuffer(result));
                Files.writeString(output.resolve(id + ".nf"), String.valueOf(result.getData("y_f", TsData.class).length()));
            }
        }
    }
}
