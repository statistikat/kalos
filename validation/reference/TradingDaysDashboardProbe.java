import jdplus.sa.base.core.diagnostics.GenericSaTests;
import jdplus.toolkit.base.api.arima.SarimaOrders;
import jdplus.toolkit.base.api.information.Explorable;
import jdplus.toolkit.base.api.stats.StatisticalTest;
import jdplus.toolkit.base.api.timeseries.TsData;
import jdplus.toolkit.base.api.timeseries.calendars.DayClustering;
import jdplus.toolkit.base.api.timeseries.calendars.GenericTradingDays;
import jdplus.toolkit.base.api.timeseries.regression.GenericTradingDaysVariable;
import jdplus.toolkit.base.core.modelling.regression.Regression;
import jdplus.toolkit.base.core.regarima.RegArimaModel;
import jdplus.toolkit.base.core.regsarima.RegSarimaComputer;
import jdplus.toolkit.base.core.sarima.SarimaModel;
import jdplus.toolkit.base.core.stats.linearmodel.JointTest;

/** Public residual trading-day dashboard tests and independent fit witnesses. */
public final class TradingDaysDashboardProbe {
    static void component(String tag, TsData data, StatisticalTest expected) {
        System.out.printf("domain %s %d %d %d%n", tag, data.getStart().start().getYear(),
            data.getStart().annualPosition() + 1, data.getAnnualFrequency());
        AdjustedSeasonalityProbe.vector("observations", data.getValues().toArray());
        var contrasts = GenericTradingDays.contrasts(DayClustering.TD7);
        var design = Regression.matrix(data.getDomain(), new GenericTradingDaysVariable(contrasts));
        var orders = new SarimaOrders(data.getAnnualFrequency());
        orders.setD(1);
        orders.setQ(1);
        var model = RegArimaModel.<SarimaModel>builder().y(data.getValues()).addX(design)
            .arima(SarimaModel.builder(orders).setDefault().build()).build();
        var fit = RegSarimaComputer.PROCESSOR.process(model, null);
        var ll = fit.getConcentratedLikelihood();
        var test = new JointTest(ll).hyperParametersCount(1).build();
        if (expected == null || Double.doubleToRawLongBits(test.getValue()) != Double.doubleToRawLongBits(expected.getValue())
                || Double.doubleToRawLongBits(test.getPvalue()) != Double.doubleToRawLongBits(expected.getPvalue()))
            throw new AssertionError(tag + " public TD joint test");
        AdjustedSeasonalityProbe.vector("parameters", ((SarimaModel) fit.getModel().arima()).parameters().toArray());
        System.out.printf("likelihood %d %s%n", ll.dim(), AdjustedSeasonalityProbe.bits(ll.ssq()));
        AdjustedSeasonalityProbe.vector("coefficients", ll.coefficients().toArray());
    }

    static void report(String id, Explorable result, GenericSaTests diagnostics) {
        var td = diagnostics.residualTradingDaysTests();
        System.out.printf("case %s %d%n", id, td.getOptions().getFlast());
        for (String key : new String[]{"td-sa-all", "td-sa-last", "td-i-all", "td-i-last"}) {
            var test = result.getData("diagnostics." + key, StatisticalTest.class);
            System.out.println(key + (test == null ? " unavailable" :
                " " + AdjustedSeasonalityProbe.bits(test.getValue()) + " " + AdjustedSeasonalityProbe.bits(test.getPvalue())));
        }
        component("sa", td.getSa(), result.getData("diagnostics.td-sa-all", StatisticalTest.class));
        component("i", td.getIrr(), result.getData("diagnostics.td-i-all", StatisticalTest.class));
    }

    public static void main(String[] args) throws Exception {
        AdjustedSeasonalityProbe.publicProducts(args[0], TradingDaysDashboardProbe::report);
    }
}
