/*
 * Public-API observer for the item 21-E pinned-reference capture: the initial
 * ARMA identification machine of the pinned jdplus 3.9.0
 * jdplus.x13.base.core.x13.regarima.ArmaModule (rjd3x13 jars).
 *
 * Nothing here computes any decision: the observer calls the real pinned
 * classes - RegArimaUtility.olsResiduals for the cursor, the ArmaModule
 * RegArmaBic sort/cmp/merge/select machinery for the candidates and the
 * refinement, HannanRissanen (the initializer's engine) for the start point,
 * and RegArimaUtility.processor(true, eps) (the machine the RegArmaBic
 * constructor builds) for the likelihood statistics - and prints what they
 * return. The module itself is assembled through the pinned package-private
 * ArmaModule Builder by reflection, the same object
 * X13SpecDecoder.readAutoModel builds; reflection is used only to reach that
 * constructor, the builder setters, the RegArmaBic factory and comparison
 * entry points and the private SarimaModel of a RegArmaBic entry (the
 * DifferencingDecisionProbe reflection precedent - no rule is
 * re-implemented). All doubles print with %.17g. Failures report one
 * "error|..." line instead of throwing.
 *
 * usage: java InitialArmaIdentificationProbe <method> <seriesfile> [ints...]
 *   ols    period d bd mean            - RegArimaUtility.olsResiduals cursor
 *   dmodel period d bd mean            - the differenced linear model parts
 *   hr     period p q bp bq            - HannanRissanen start on the cursor
 *   bic    period p q bp bq eps        - one sort() candidate (statistics)
 *   landing period p q bp bq eps       - the processor route (fit) statistics
 *   grid   period maxP maxQ maxBp maxBq mixed eps
 *                                      - sort() over the full stage grid
 *   select period d bd nmod maxP maxQ maxBp maxBq balanced mixed wn eps
 *                                      - the full ArmaModule.select pipeline
 */
import jdplus.toolkit.base.api.arima.SarmaOrders;
import jdplus.toolkit.base.api.arima.SarimaOrders;
import jdplus.toolkit.base.api.data.DoubleSeq;
import jdplus.toolkit.base.api.data.Doubles;
import jdplus.toolkit.base.core.regarima.IRegArimaComputer;
import jdplus.toolkit.base.core.regarima.RegArimaEstimation;
import jdplus.toolkit.base.core.regarima.RegArimaModel;
import jdplus.toolkit.base.core.regarima.RegArimaUtility;
import jdplus.toolkit.base.core.sarima.SarimaModel;
import jdplus.toolkit.base.core.sarima.estimation.HannanRissanen;
import jdplus.toolkit.base.core.stats.likelihood.LikelihoodStatistics;
import jdplus.toolkit.base.core.stats.linearmodel.LinearModel;
import jdplus.toolkit.base.core.stats.linearmodel.Ols;
import java.io.IOException;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

public final class InitialArmaIdentificationProbe {

  private InitialArmaIdentificationProbe() {
  }

  private static String fmt(double v) {
    return String.format(Locale.ROOT, "%.17g", v);
  }

  private static String error(Throwable e) {
    String msg = e.getMessage();
    return "error|" + e.getClass().getName() + "|" + (msg == null ? "" : msg);
  }

  private static String seq(DoubleSeq d) {
    StringBuilder sb = new StringBuilder();
    for (int i = 0; i < d.size(); ++i) {
      if (i != 0) {
        sb.append(';');
      }
      sb.append(fmt(d.get(i)));
    }
    return sb.toString();
  }

  private static SarimaModel stationaryModel(SarmaOrders orders) {
    return SarimaModel.builder(orders).setDefault().build();
  }

  private static SarmaOrders orders(int period, int p, int q, int bp, int bq) {
    SarmaOrders orders = new SarmaOrders(period);
    orders.setP(p);
    orders.setQ(q);
    orders.setBp(bp);
    orders.setBq(bq);
    return orders;
  }

  /** The OLS residual cursor of RegArimaUtility.olsResiduals (k = 0 user columns). */
  public static String ols(int period, int d, int bd, boolean mean, double[] y,
      double[] userFlat, int k) {
    try {
      SarimaOrders orders = new SarimaOrders(period);
      orders.setD(d);
      orders.setBd(bd);
      RegArimaModel.Builder builder = RegArimaModel.builder()
          .y(Doubles.of(y))
          .arima(SarimaModel.builder(orders).build())
          .meanCorrection(mean);
      for (int c = 0; c < k; ++c) {
        double[] column = new double[y.length];
        System.arraycopy(userFlat, c * y.length, column, 0, y.length);
        builder = builder.addX(Doubles.of(column));
      }
      DoubleSeq r = RegArimaUtility.olsResiduals(builder.build());
      if (r == null) {
        return "error|ols-null|degenerate response";
      }
      return "ok|" + r.size() + "|" + seq(r);
    } catch (Throwable e) {
      return error(e);
    }
  }

  /** Intermediates of the pinned differenced linear model (response, design, Ols). */
  public static String dmodel(int period, int d, int bd, boolean mean, double[] y,
      double[] userFlat, int k) {
    try {
      SarimaOrders orders = new SarimaOrders(period);
      orders.setD(d);
      orders.setBd(bd);
      RegArimaModel.Builder builder = RegArimaModel.builder()
          .y(Doubles.of(y))
          .arima(SarimaModel.builder(orders).build())
          .meanCorrection(mean);
      for (int c = 0; c < k; ++c) {
        double[] column = new double[y.length];
        System.arraycopy(userFlat, c * y.length, column, 0, y.length);
        builder = builder.addX(Doubles.of(column));
      }
      LinearModel lm = builder.build().differencedModel().asLinearModel();
      StringBuilder sb = new StringBuilder();
      sb.append(seq(lm.getY()));
      sb.append('/').append(lm.isMeanCorrection() ? "m" : "-");
      for (int i = 0; i < lm.getXCount(); ++i) {
        sb.append('/').append(seq(lm.X(i)));
      }
      if (lm.getXCount() > 0) {
        sb.append('/').append(seq(Ols.compute(lm).getCoefficients()));
      }
      return "ok|" + sb;
    } catch (Throwable e) {
      return error(e);
    }
  }

  /**
   * The HannanRissanen engine (the pinned initializer's estimator) on the
   * cursor: success flag plus the fitted model's raw [phi, bphi, theta,
   * btheta] blocks. A failed run reports process=false.
   */
  public static String hr(double[] cursor, int period, int p, int q, int bp, int bq) {
    try {
      HannanRissanen hr = HannanRissanen.builder().build();
      boolean ok = hr.process(Doubles.of(cursor), orders(period, p, q, bp, bq));
      if (!ok) {
        return "false";
      }
      SarimaModel m = hr.getModel();
      return "ok|" + seq(m.getPhi()) + "|" + seq(m.getBphi()) + "|" + seq(m.getTheta())
          + "|" + seq(m.getBtheta());
    } catch (Throwable e) {
      return error(e);
    }
  }

  /**
   * The pinned fit route (the machine the RegArmaBic constructor builds):
   * processor(true, eps).process on the design-free ARMA model over the
   * cursor. Likelihood statistics plus the landing parameters in the raw
   * SarimaModel convention.
   */
  public static String landing(double[] cursor, int period, int p, int q, int bp,
      int bq, double eps) {
    try {
      IRegArimaComputer<SarimaModel> computer = RegArimaUtility.processor(true, eps);
      RegArimaEstimation<SarimaModel> est = computer.process(
          RegArimaModel.<SarimaModel>builder().y(Doubles.of(cursor))
              .arima(stationaryModel(orders(period, p, q, bp, bq))).build(),
          null);
      if (est == null) {
        return "error|null|failed";
      }
      LikelihoodStatistics st = est.statistics();
      DoubleSeq par = est.getMax().getParameters();
      return "ok|" + fmt(st.getLogLikelihood()) + "|" + fmt(st.getBIC2()) + "|"
          + fmt(st.getSsqErr()) + "|" + fmt(st.getEstimatedParametersCount()) + "|"
          + fmt(st.getEffectiveObservationsCount()) + "|" + seq(par);
    } catch (Throwable e) {
      return error(e);
    }
  }

  /**
   * One candidate through the pinned ArmaModule$RegArmaBic.sort over a
   * one-element grid (the full constructor: HR initialization through the
   * processor's initializer, the pinned cmp BIC formula), plus the processor
   * route for the landing statistics. The two BIC2 values must agree bit for
   * bit; a mismatch is reported as an error.
   */
  public static String bic(double[] cursor, int period, int p, int q, int bp, int bq,
      double eps) {
    try {
      DoubleSeq c = Doubles.of(cursor);
      SarmaOrders orders = orders(period, p, q, bp, bq);
      Class<?> bicClass = Class.forName(
          "jdplus.x13.base.core.x13.regarima.ArmaModule$RegArmaBic");
      Method sort = bicClass.getMethod("sort", DoubleSeq.class, SarmaOrders[].class,
          double.class);
      Object[] sorted = (Object[]) sort.invoke(null, c, new SarmaOrders[]{orders}, eps);
      if (sorted.length != 1) {
        return "error|dropped|sentinel";
      }
      Method getBic = bicClass.getMethod("getBIC");
      Method getSpec = bicClass.getMethod("getSpecification");
      double bic = (Double) getBic.invoke(sorted[0]);
      SarmaOrders spec = (SarmaOrders) getSpec.invoke(sorted[0]);
      Field arimaField = bicClass.getDeclaredField("arima");
      arimaField.setAccessible(true);
      SarimaModel fitted = (SarimaModel) arimaField.get(sorted[0]);
      if (fitted == null) {
        return "error|dropped|no-arima";
      }
      LikelihoodStatistics st = null;
      RegArimaEstimation<SarimaModel> est = null;
      try {
        IRegArimaComputer<SarimaModel> computer = RegArimaUtility.processor(true, eps);
        est = computer.process(
            RegArimaModel.<SarimaModel>builder().y(c).arima(stationaryModel(orders))
                .build(), null);
        if (est != null) {
          st = est.statistics();
        }
      } catch (Throwable ignored) {
        // Statistics are observational; the sort BIC stands alone.
      }
      double ll = Double.NaN;
      double k = Double.NaN;
      double neff = Double.NaN;
      double bic2 = bic;
      if (st != null) {
        ll = st.getLogLikelihood();
        k = st.getEstimatedParametersCount();
        neff = st.getEffectiveObservationsCount();
        bic2 = st.getBIC2();
      }
      if (bic != bic2) {
        return "error|bic-mismatch|" + fmt(bic) + "|" + fmt(bic2);
      }
      return "ok|" + spec.getP() + "|" + spec.getQ() + "|" + spec.getBp() + "|"
          + spec.getBq() + "|" + fmt(bic) + "|" + fmt(ll) + "|" + fmt(k) + "|"
          + fmt(neff) + "|" + seq(fitted.getPhi()) + "|" + seq(fitted.getBphi()) + "|"
          + seq(fitted.getTheta()) + "|" + seq(fitted.getBtheta());
    } catch (Throwable e) {
      return error(e);
    }
  }

  /**
   * sort() over the full (p, q, bp, bq) grid (every combination, bp outer):
   * one line per candidate in sort order, plus the winner line.
   */
  public static String grid(double[] cursor, int period, int maxP, int maxQ, int maxBp,
      int maxBq, boolean mixed, double eps) {
    try {
      List<SarmaOrders> grid = new ArrayList<>();
      for (int bp = 0; bp <= maxBp; ++bp) {
        for (int bq = 0; bq <= maxBq; ++bq) {
          for (int p = 0; p <= maxP; ++p) {
            for (int q = 0; q <= maxQ; ++q) {
              if (!mixed && (p > 0 && q > 0 || bp > 0 && bq > 0)) {
                continue;
              }
              grid.add(orders(period, p, q, bp, bq));
            }
          }
        }
      }
      Class<?> bicClass = Class.forName(
          "jdplus.x13.base.core.x13.regarima.ArmaModule$RegArmaBic");
      Method sort = bicClass.getMethod("sort", DoubleSeq.class, SarmaOrders[].class,
          double.class);
      Object[] sorted = (Object[]) sort.invoke(null, Doubles.of(cursor),
          grid.toArray(new SarmaOrders[0]), eps);
      Method getBic = bicClass.getMethod("getBIC");
      Method getSpec = bicClass.getMethod("getSpecification");
      Field arimaField = bicClass.getDeclaredField("arima");
      arimaField.setAccessible(true);
      StringBuilder sb = new StringBuilder();
      for (Object entry : sorted) {
        SarmaOrders spec = (SarmaOrders) getSpec.invoke(entry);
        SarimaModel fitted = (SarimaModel) arimaField.get(entry);
        sb.append("G|").append(spec.getP()).append('|').append(spec.getQ()).append('|')
            .append(spec.getBp()).append('|').append(spec.getBq()).append('|')
            .append(fmt((Double) getBic.invoke(entry)));
        sb.append('|').append(seq(fitted.getPhi())).append('|')
            .append(seq(fitted.getBphi())).append('|').append(seq(fitted.getTheta()))
            .append('|').append(seq(fitted.getBtheta()));
        sb.append('\n');
      }
      return sb.toString();
    } catch (Throwable e) {
      return error(e);
    }
  }

  private static Object buildModule(int nmod, int maxP, int maxQ, int maxBp, int maxBq,
      boolean balanced, boolean mixed, boolean wn, double eps) throws Exception {
    Class<?> moduleClass = Class.forName("jdplus.x13.base.core.x13.regarima.ArmaModule");
    Class<?> builderClass = Class.forName(
        "jdplus.x13.base.core.x13.regarima.ArmaModule$Builder");
    java.lang.reflect.Constructor<?> ctor = builderClass.getDeclaredConstructor();
    ctor.setAccessible(true);
    Object b = ctor.newInstance();
    b = set(builderClass, b, "modelsCount", nmod);
    b = set(builderClass, b, "maxP", maxP);
    b = set(builderClass, b, "maxQ", maxQ);
    b = set(builderClass, b, "maxBp", maxBp);
    b = set(builderClass, b, "maxBq", maxBq);
    b = set(builderClass, b, "balanced", balanced);
    b = set(builderClass, b, "mixed", mixed);
    b = set(builderClass, b, "acceptWhiteNoise", wn);
    b = set(builderClass, b, "estimationPrecision", eps);
    Method build = builderClass.getDeclaredMethod("build");
    build.setAccessible(true);
    return build.invoke(b);
  }

  private static Object set(Class<?> cls, Object target, String name, int value)
      throws Exception {
    Method m = cls.getDeclaredMethod(name, int.class);
    m.setAccessible(true);
    return m.invoke(target, value);
  }

  private static Object set(Class<?> cls, Object target, String name, boolean value)
      throws Exception {
    Method m = cls.getDeclaredMethod(name, boolean.class);
    m.setAccessible(true);
    return m.invoke(target, value);
  }

  private static Object set(Class<?> cls, Object target, String name, double value)
      throws Exception {
    Method m = cls.getDeclaredMethod(name, double.class);
    m.setAccessible(true);
    return m.invoke(target, value);
  }

  /**
   * The full ArmaModule pipeline on the pinned module over the cursor: the
   * three-stage select with its merged estimations panel, the selected
   * specification, and the refinement-only scan over the panel.
   */
  public static String select(double[] cursor, int period, int d, int bd, int nmod,
      int maxP, int maxQ, int maxBp, int maxBq, boolean balanced, boolean mixed,
      boolean wn, double eps) {
    try {
      Class<?> moduleClass = Class.forName(
          "jdplus.x13.base.core.x13.regarima.ArmaModule");
      Object module = buildModule(nmod, maxP, maxQ, maxBp, maxBq, balanced, mixed, wn,
          eps);
      Method select = moduleClass.getMethod("select", DoubleSeq.class, int.class,
          int.class, int.class);
      SarmaOrders best = (SarmaOrders) select.invoke(module, Doubles.of(cursor),
          period, d, bd);
      Method panel = moduleClass.getMethod("getPreferedModels");
      Class<?> bicClass = Class.forName(
          "jdplus.x13.base.core.x13.regarima.ArmaModule$RegArmaBic");
      Method getBic = bicClass.getMethod("getBIC");
      Method getSpec = bicClass.getMethod("getSpecification");
      Object[] rows = (Object[]) panel.invoke(module);
      StringBuilder sb = new StringBuilder();
      sb.append("ok|").append(best.getP()).append('|').append(best.getQ()).append('|')
          .append(best.getBp()).append('|').append(best.getBq()).append('|')
          .append(rows.length);
      for (Object row : rows) {
        SarmaOrders spec = (SarmaOrders) getSpec.invoke(row);
        double bic = (Double) getBic.invoke(row);
        sb.append("|").append(spec.getP()).append(',').append(spec.getQ()).append(',')
            .append(spec.getBp()).append(',').append(spec.getBq()).append(',')
            .append(fmt(bic));
      }
      Method refine = moduleClass.getMethod("select", DoubleSeq.class, int.class,
          int.class, int.class);
      SarmaOrders sel = (SarmaOrders) refine.invoke(module, Doubles.of(cursor), period,
          d, bd);
      sb.append("|REF|").append(sel.getP()).append('|').append(sel.getQ()).append('|')
          .append(sel.getBp()).append('|').append(sel.getBq());
      return sb.toString();
    } catch (Throwable e) {
      return error(e);
    }
  }

  private static double[] readSeries(String path) throws IOException {
    List<String> lines = Files.readAllLines(Path.of(path));
    double[] y = new double[lines.size()];
    for (int i = 0; i < lines.size(); ++i) {
      String line = lines.get(i).trim();
      if (line.isEmpty()) {
        throw new IOException("blank line in the series file");
      }
      y[i] = Double.parseDouble(line);
    }
    return y;
  }

  /** User regressors: one column per line, values separated by ';'. */
  private static double[] readUsers(String path, int n, int[] k) throws IOException {
    if (path == null) {
      k[0] = 0;
      return new double[0];
    }
    List<String> lines = Files.readAllLines(Path.of(path));
    double[] flat = new double[lines.size() * n];
    for (int c = 0; c < lines.size(); ++c) {
      String[] parts = lines.get(c).trim().split(";");
      if (parts.length != n) {
        throw new IOException("user column " + c + " has " + parts.length + " entries");
      }
      for (int i = 0; i < n; ++i) {
        flat[c * n + i] = Double.parseDouble(parts[i]);
      }
    }
    k[0] = lines.size();
    return flat;
  }

  private static int at(String[] args, int i, int def) {
    return i < args.length ? Integer.parseInt(args[i]) : def;
  }

  public static void main(String[] args) throws Exception {
    if (args.length < 3) {
      System.out.println("error|usage|method seriesfile [ints...] [userfile]");
      return;
    }
    String method = args[0];
    double[] y = readSeries(args[1]);
    String out;
    switch (method) {
      case "ols": {
        int[] k = new int[1];
        double[] users = readUsers(args.length > 7 ? args[7] : null, y.length, k);
        out = ols(at(args, 2, 12), at(args, 3, 0), at(args, 4, 0), at(args, 5, 1) != 0,
            y, users, k[0]);
        break;
      }
      case "dmodel": {
        int[] k = new int[1];
        double[] users = readUsers(args.length > 7 ? args[7] : null, y.length, k);
        out = dmodel(at(args, 2, 12), at(args, 3, 0), at(args, 4, 0),
            at(args, 5, 1) != 0, y, users, k[0]);
        break;
      }
      case "hr":
        out = hr(y, at(args, 2, 12), at(args, 3, 0), at(args, 4, 0), at(args, 5, 0),
            at(args, 6, 0));
        break;
      case "landing":
        out = landing(y, at(args, 2, 12), at(args, 3, 0), at(args, 4, 0), at(args, 5, 0),
            at(args, 6, 0), Double.parseDouble(args.length > 7 ? args[7] : "1.0E-7"));
        break;
      case "bic":
        out = bic(y, at(args, 2, 12), at(args, 3, 0), at(args, 4, 0), at(args, 5, 0),
            at(args, 6, 0), Double.parseDouble(args.length > 7 ? args[7] : "1.0E-7"));
        break;
      case "grid":
        out = grid(y, at(args, 2, 12), at(args, 3, 2), at(args, 4, 2), at(args, 5, 1),
            at(args, 6, 1), at(args, 7, 1) != 0,
            Double.parseDouble(args.length > 8 ? args[8] : "1.0E-7"));
        break;
      case "select":
        out = select(y, at(args, 2, 12), at(args, 3, 0), at(args, 4, 0), at(args, 5, 5),
            at(args, 6, 2), at(args, 7, 2), at(args, 8, 1), at(args, 9, 1),
            at(args, 10, 0) != 0, at(args, 11, 1) != 0, at(args, 12, 0) != 0,
            Double.parseDouble(args.length > 13 ? args[13] : "1.0E-7"));
        break;
      default:
        out = "error|unknown-method|" + method;
        break;
    }
    System.out.print(out);
    if (!out.endsWith("\n")) {
      System.out.println();
    }
  }
}
