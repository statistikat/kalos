import java.lang.reflect.Method;
import java.util.List;
import java.util.Map;
import jdplus.toolkit.base.api.data.DoubleSeq;
import jdplus.toolkit.base.api.timeseries.*;
import jdplus.toolkit.base.api.timeseries.calendars.DayClustering;
import jdplus.toolkit.base.api.timeseries.regression.*;
import jdplus.toolkit.base.core.regarima.RegArimaModel;
import jdplus.toolkit.base.core.regsarima.regular.*;
import jdplus.tramoseats.base.api.tramo.TramoSpec;
import jdplus.tramoseats.base.core.tramo.TramoKernel;
import jdplus.tramoseats.base.core.tramo.internal.OutliersDetectionModule;

/** First scan on the actual detector and final public outliers, conditional on calendar columns. */
public final class TramoCalendarOutliersProbe {
    static void vector(String name, DoubleSeq values) {
        var row = new StringBuilder(name);
        for (double value : values.toArray()) row.append(" ").append(CalendarFTestProbe.bits(value));
        System.out.println(row);
    }

    public static void main(String[] args) throws Exception {
        var kernel = TramoKernel.of(TramoSpec.fromString("TRfull"), new ModellingContext());
        var factory = TramoKernel.class.getDeclaredMethod("regressionModule", boolean.class);
        factory.setAccessible(true);
        var regression = factory.invoke(kernel, false);
        var lpField = regression.getClass().getDeclaredField("lp");
        var easterField = regression.getClass().getDeclaredField("easter");
        lpField.setAccessible(true);
        easterField.setAccessible(true);
        Scratch23g.initRefl();
        Method make = OutliersDetectionModule.class.getDeclaredMethod("make", ModelDescription.class, double.class);
        make.setAccessible(true);
        for (int i = 0; i < 16; ++i) {
            int scheme = i % 4;
            boolean mean = (i & 4) != 0, log = (i & 8) != 0, ml = i % 3 == 1;
            var start = TsPeriod.monthly(1991 + i, 1 + i % 12);
            double[] y = CalendarFTestProbe.series(scheme, 120 + i);
            if (i % 4 != 0) y[33] += 42;
            if (i % 4 >= 2) for (int t = 62; t < y.length; ++t) y[t] += 23;
            if (i % 4 == 3) for (int t = 85; t < y.length; ++t) y[t] += 62 * StrictMath.pow(.7, t-85);
            if (log) for (int t = 0; t < y.length; ++t) y[t] = StrictMath.exp(y[t]/180);
            var desc = new ModelDescription(TsData.of(start, DoubleSeq.of(y)), null);
            desc.setAirline(true);
            desc.setMean(mean);
            desc.setLogTransformation(log);
            desc.addVariable(Variable.variable("td", new GenericTradingDaysVariable(
                scheme % 2 == 0 ? DayClustering.TD2 : DayClustering.TD7), Map.of()));
            if (scheme >= 2) desc.addVariable(Variable.variable("lp", (ILengthOfPeriodVariable)lpField.get(regression), Map.of()));
            if (scheme == 3) desc.addVariable(Variable.variable("easter", (IEasterVariable)easterField.get(regression), Map.of()));
            System.out.printf("case %d %d %d %d%n", i, mean ? 1 : 0, log ? 1 : 0, ml ? 1 : 0);
            vector("input", DoubleSeq.of(y));
            var model = desc.regarima();
            for (var col : model.getX()) vector("column", col);
            var odm = OutliersDetectionModule.builder().ao(true).ls(true).tc(true).so(false)
                .tcrate(.7).precision(1e-7).maximumLikelihood(ml).build();
            double cv = 3.5;
            var driver = make.invoke(odm, desc, cv);
            Scratch23g.M_clear.invoke(driver);
            Scratch23g.F_regarima.set(driver, model);
            if (!(boolean)Scratch23g.M_estimateModel.invoke(driver, desc.mapping())) throw new AssertionError(i);
            var sod = Scratch23g.F_sod.get(driver);
            if (!(boolean)sod.getClass().getMethod("process", RegArimaModel.class)
                    .invoke(sod, Scratch23g.F_regarima.get(driver))) throw new AssertionError(i);
            int pos = (int)Scratch23g.M_getMaxPos.invoke(sod), kind = (int)Scratch23g.M_getMaxType.invoke(sod);
            double t = (double)Scratch23g.M_getMaxTStat.invoke(sod);
            double coefficient = (double)Scratch23g.M_coefficient.invoke(sod, pos, kind);
            System.out.printf("scan %d %d %s %s%n", pos, kind, CalendarFTestProbe.bits(t), CalendarFTestProbe.bits(coefficient));
            var result = odm.process(RegSarimaModelling.of(desc), cv);
            System.out.print("outliers " + result);
            desc.variables().forEach(v -> {
                if (v.getCore() instanceof IOutlier o) System.out.printf(" %d %d", start.until(start.withDate(o.getPosition())),
                    List.of("AO","LS","TC").indexOf(o.getCode()));
            });
            System.out.println();
        }
    }
}
