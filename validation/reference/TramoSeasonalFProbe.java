import java.util.ArrayList;
import java.util.Map;
import jdplus.toolkit.base.api.arima.SarimaOrders;
import jdplus.toolkit.base.api.data.DoubleSeq;
import jdplus.toolkit.base.api.timeseries.*;
import jdplus.toolkit.base.api.timeseries.calendars.DayClustering;
import jdplus.toolkit.base.api.timeseries.regression.*;
import jdplus.toolkit.base.core.modelling.regression.*;
import jdplus.toolkit.base.core.regsarima.regular.ModelDescription;
import jdplus.toolkit.base.core.regsarima.regular.SeasonalFTest;

/** Public SeasonalFTest on monthly descriptions with dated regressors and different regular orders. */
public final class TramoSeasonalFProbe {
    static void vector(String name, DoubleSeq values) {
        var text = new StringBuilder(name);
        for (double v : values.toArray()) text.append(" ").append(CalendarFTestProbe.bits(v));
        System.out.println(text);
    }
    public static void main(String[] args) {
        for (int i = 0; i < 12; ++i) {
            int p = i % 3, d = i % 2, q = (i / 3) % 2;
            int n = 96 + i;
            int year = 1990 + i, month = i + 1;
            var start = TsPeriod.monthly(year, month);
            var data = TsData.of(start, DoubleSeq.of(CalendarFTestProbe.series(i % 3, n)));
            var desc = new ModelDescription(data, data.getDomain());
            var orders = new SarimaOrders(12);
            orders.setP(p); orders.setD(d); orders.setQ(q);
            orders.setBp(1); orders.setBd(1); orders.setBq(1);
            desc.setSpecification(orders);
            desc.setMean(i % 2 == 0);
            var regressors = new ArrayList<ITsVariable>();
            regressors.add(AdditiveOutlierFactory.FACTORY.make(start.plus(31).start()));
            if (i % 3 != 0) regressors.add(LevelShiftFactory.FACTORY_ZEROENDED.make(start.plus(54).start()));
            if (i % 3 == 2) regressors.add(new GenericTradingDaysVariable(DayClustering.TD2));
            int number = 0;
            System.out.printf("case %d %d %d %d %d %d%n", i, year, month, p, d, q);
            vector("series", data.getValues());
            for (var variable : regressors) {
                desc.addVariable(Variable.variable("x" + ++number, variable, Map.of()));
            }
            for (var column : desc.regarima().getX()) vector("column", column);
            var seasonal = new SeasonalFTest();
            if (!seasonal.test(desc)) throw new AssertionError("seasonal test failed in " + i);
            var f = seasonal.getFTest();
            String[] distribution = f.getDescription().split(" ");
            System.out.printf("test %s %s %s %s%n", CalendarFTestProbe.bits(f.getValue()),
                CalendarFTestProbe.bits(f.getPvalue()), distribution[2], distribution[10]);
        }
    }
}
