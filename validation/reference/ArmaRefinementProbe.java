/*
 * Public-API observer for the item 21-F pinned-reference capture: the ARMA
 * refinement and comparison machine of the pinned jdplus 3.9.0
 * jdplus.x13.base.core.x13.regarima.AutoModellingModule (rjd3x13 jars).
 *
 * Nothing here computes any decision. run() reproduces the CALL SEQUENCE of
 * AutoModellingModule.process on the real pinned classes -
 *   1. the current estimation through the public GlsSarimaComputer chain
 *      (RegArimaUtility.processor(true, eps).process), giving the model and
 *      the ConcentratedLikelihoodWithMissing the module reads;
 *   2. RegArimaUtility.linearizedData(model, cl);
 *   3. DifferencingModule.process(lin, period) on the module built exactly
 *      as X13SpecDecoder.readAutoModel builds it (the DifferencingDecision-
 *      Probe construction, reused verbatim);
 *   4. BackFilter(UnitRoots.D(d).times(UnitRoots.D(period, bd))).apply(lin)
 *      exactly as the module's bytecode performs it (DataBlock.make of
 *      length - degree, then apply);
 *   5. ArmaModule.select(z, period, d, bd) plus getPreferedModels on the
 *      decoder-built ArmaModule (the builder is package-private, reached by
 *      reflection like the InitialArmaIdentificationProbe precedent; the
 *      private arima field of a panel entry is read for observation only);
 *   6. SarimaOrders.of(best, d, bd) / SarimaOrders.airline(period) and the
 *      public equals for the Changed/Unchanged comparison.
 * Every stage failure is reported as one "STAGE|error|..." line plus the
 * pinned airline RESULT line instead of throwing, mirroring the module's
 * RuntimeException catch. All doubles print with %.17g.
 *
 * usage: java ArmaRefinementProbe <seriesfile> <userfile|-> period
 *        p q bp bq d bd mean
 */
import jdplus.toolkit.base.api.arima.SarmaOrders;
import jdplus.toolkit.base.api.arima.SarimaOrders;
import jdplus.toolkit.base.api.data.DoubleSeq;
import jdplus.toolkit.base.api.data.Doubles;
import jdplus.toolkit.base.core.data.DataBlock;
import jdplus.toolkit.base.core.math.linearfilters.BackFilter;
import jdplus.toolkit.base.core.math.polynomials.UnitRoots;
import jdplus.toolkit.base.core.regarima.IRegArimaComputer;
import jdplus.toolkit.base.core.regarima.RegArimaEstimation;
import jdplus.toolkit.base.core.regarima.RegArimaModel;
import jdplus.toolkit.base.core.regarima.RegArimaUtility;
import jdplus.toolkit.base.core.sarima.SarimaModel;
import jdplus.x13.base.api.regarima.AutoModelSpec;
import jdplus.x13.base.core.x13.regarima.DifferencingModule;
import java.io.IOException;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

public final class ArmaRefinementProbe {

  private ArmaRefinementProbe() {
  }

  private static String fmt(double v) {
    return String.format(Locale.ROOT, "%.17g", v);
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

  private static String seq(double[] v) {
    StringBuilder sb = new StringBuilder();
    for (int i = 0; i < v.length; ++i) {
      if (i != 0) {
        sb.append(';');
      }
      sb.append(fmt(v[i]));
    }
    return sb.toString();
  }

  private static String err(Throwable e) {
    String msg = e.getMessage();
    return "error|" + e.getClass().getName() + "|" + (msg == null ? "" : msg);
  }

  private static Object buildArmaModule() throws Exception {
    Class<?> builderClass = Class.forName(
        "jdplus.x13.base.core.x13.regarima.ArmaModule$Builder");
    java.lang.reflect.Constructor<?> ctor = builderClass.getDeclaredConstructor();
    ctor.setAccessible(true);
    Object b = ctor.newInstance();
    b = setInt(builderClass, b, "modelsCount", 5);
    b = setInt(builderClass, b, "maxP", 2);
    b = setInt(builderClass, b, "maxQ", 2);
    b = setInt(builderClass, b, "maxBp", 1);
    b = setInt(builderClass, b, "maxBq", 1);
    b = setBool(builderClass, b, "balanced", false);
    b = setBool(builderClass, b, "mixed", true);
    b = setBool(builderClass, b, "acceptWhiteNoise", false);
    b = setDbl(builderClass, b, "estimationPrecision", 1.0E-7);
    Method build = builderClass.getDeclaredMethod("build");
    build.setAccessible(true);
    return build.invoke(b);
  }

  private static Object setInt(Class<?> cls, Object target, String name, int value)
      throws Exception {
    Method m = cls.getDeclaredMethod(name, int.class);
    m.setAccessible(true);
    return m.invoke(target, value);
  }

  private static Object setBool(Class<?> cls, Object target, String name, boolean value)
      throws Exception {
    Method m = cls.getDeclaredMethod(name, boolean.class);
    m.setAccessible(true);
    return m.invoke(target, value);
  }

  private static Object setDbl(Class<?> cls, Object target, String name, double value)
      throws Exception {
    Method m = cls.getDeclaredMethod(name, double.class);
    m.setAccessible(true);
    return m.invoke(target, value);
  }

  private static int ifield(Class<?> cls, Object o, String name) throws Exception {
    Field f = cls.getDeclaredField(name);
    f.setAccessible(true);
    return f.getInt(o);
  }

  private static boolean bfield(Class<?> cls, Object o, String name) throws Exception {
    Field f = cls.getDeclaredField(name);
    f.setAccessible(true);
    return f.getBoolean(o);
  }

  private static String line(String... parts) {
    return String.join("|", parts) + "\n";
  }

  private static String airlineLine(int period, SarimaOrders spec0, boolean mean0) {
    SarimaOrders airline = SarimaOrders.airline(period);
    return result(airline, false, spec0, mean0, "fallback");
  }

  private static String result(SarimaOrders spec, boolean mean, SarimaOrders spec0,
      boolean mean0, String tag) {
    boolean changed = !spec0.equals(spec) || mean != mean0;
    return line("RESULT", tag, Integer.toString(spec.getP()),
        Integer.toString(spec.getQ()), Integer.toString(spec.getBp()),
        Integer.toString(spec.getBq()), Integer.toString(spec.getD()),
        Integer.toString(spec.getBd()), mean ? "1" : "0",
        spec0.equals(spec) ? "0" : "1", mean != mean0 ? "1" : "0",
        changed ? "1" : "0");
  }

  public static String run(int period, int p, int q, int bp, int bq, int d0, int bd0,
      boolean mean0, double[] y, double[] userFlat, int k) {
    SarimaOrders spec0 = new SarimaOrders(period);
    spec0.setP(p);
    spec0.setQ(q);
    spec0.setBp(bp);
    spec0.setBq(bq);
    spec0.setD(d0);
    spec0.setBd(bd0);
    StringBuilder sb = new StringBuilder();

    // 1. The current estimation on the pinned public chain.
    RegArimaEstimation<SarimaModel> est;
    try {
      RegArimaModel.Builder builder = RegArimaModel.builder()
          .y(Doubles.of(y))
          .arima(SarimaModel.builder(spec0).setDefault().build())
          .meanCorrection(mean0);
      for (int c = 0; c < k; ++c) {
        double[] column = new double[y.length];
        System.arraycopy(userFlat, c * y.length, column, 0, y.length);
        builder = builder.addX(Doubles.of(column));
      }
      IRegArimaComputer<SarimaModel> computer = RegArimaUtility.processor(true, 1.0E-7);
      est = computer.process(builder.build(), null);
      if (est == null) {
        return line("EST", err(new IllegalStateException("null estimation")));
      }
      sb.append(line("EST", "ok", seq(est.getConcentratedLikelihood().coefficients()),
          seq(est.getMax().getParameters())));
    } catch (Throwable e) {
      return line("EST", err(e));
    }

    // 2. linearizedData.
    DoubleSeq lin;
    try {
      lin = RegArimaUtility.linearizedData(est.getModel(), est.getConcentratedLikelihood());
      sb.append(line("LIN", "ok", Integer.toString(lin.size()), seq(lin)));
    } catch (Throwable e) {
      sb.append(line("LIN", err(e)));
      sb.append(airlineLine(period, spec0, mean0));
      return sb.toString();
    }

    // 3. DifferencingModule.process(lin, period).
    AutoModelSpec spec = AutoModelSpec.builder().build();
    DifferencingModule dm = DifferencingModule.builder()
        .cancel(spec.getCancel())
        .ub1(1.0 / spec.getUb1())
        .ub2(1.0 / spec.getUb2())
        .precision(1.0E-7)
        .build();
    int d;
    int bd;
    boolean mean;
    try {
      boolean ok = dm.process(lin, period);
      if (ok) {
        d = dm.getD();
        bd = dm.getBd();
        mean = dm.isMeanCorrection();
      } else { // DEAD in 3.9.0 (calc() always returns true); pinned remark path
        d = 1;
        bd = 1;
        mean = false;
      }
      sb.append(line("DIF", ok ? "ok" : "falseflag", Integer.toString(d),
          Integer.toString(bd), mean ? "1" : "0", fmt(dm.getTMean())));
    } catch (Throwable e) {
      sb.append(line("DIF", err(e)));
      sb.append(airlineLine(period, spec0, mean0));
      return sb.toString();
    }

    // 4. Re-differencing exactly as the module's bytecode performs it.
    DoubleSeq z = lin;
    if (d != 0 || bd != 0) {
      try {
        BackFilter bf = new BackFilter(UnitRoots.D(d).times(UnitRoots.D(period, bd)));
        DataBlock filtered = DataBlock.make(lin.size() - bf.getDegree());
        bf.apply(lin, filtered);
        z = filtered.unmodifiable();
        sb.append(line("Z", "ok", Integer.toString(z.size()), seq(z)));
      } catch (Throwable e) {
        sb.append(line("Z", err(e)));
        sb.append(airlineLine(period, spec0, mean0));
        return sb.toString();
      }
    } else {
      sb.append(line("Z", "identity"));
    }

    // 5. ArmaModule.select(z, period, d, bd) + getPreferedModels.
    SarmaOrders best;
    Object module;
    try {
      Class<?> moduleClass = Class.forName(
          "jdplus.x13.base.core.x13.regarima.ArmaModule");
      module = buildArmaModule();
      // Module scalars as the decoder-built objects actually hold them.
      sb.append(line("MODULE",
          Integer.toString(ifield(DifferencingModule.class, dm, "maxd")),
          Integer.toString(ifield(DifferencingModule.class, dm, "maxbd")),
          fmt(field(DifferencingModule.class, dm, "ub1")),
          fmt(field(DifferencingModule.class, dm, "ub2")),
          fmt(field(DifferencingModule.class, dm, "cancel")),
          fmt(field(DifferencingModule.class, dm, "eps")),
          Integer.toString(ifield(moduleClass, module, "nmod")),
          Integer.toString(ifield(moduleClass, module, "maxP")),
          Integer.toString(ifield(moduleClass, module, "maxQ")),
          Integer.toString(ifield(moduleClass, module, "maxBp")),
          Integer.toString(ifield(moduleClass, module, "maxBq")),
          bfield(moduleClass, module, "balanced") ? "1" : "0",
          bfield(moduleClass, module, "mixed") ? "1" : "0",
          bfield(moduleClass, module, "wn") ? "1" : "0",
          fmt(field(moduleClass, module, "eps"))));
      Method select = moduleClass.getMethod("select", DoubleSeq.class, int.class,
          int.class, int.class);
      best = (SarmaOrders) select.invoke(module, z, period, d, bd);
      if (best == null) {
        throw new NullPointerException("select returned null");
      }
      sb.append(line("SEL", "ok", Integer.toString(best.getP()),
          Integer.toString(best.getQ()), Integer.toString(best.getBp()),
          Integer.toString(best.getBq())));
    } catch (Throwable e) {
      sb.append(line("SEL", err(e)));
      sb.append(airlineLine(period, spec0, mean0));
      return sb.toString();
    }
    try {
      Class<?> moduleClass = Class.forName(
          "jdplus.x13.base.core.x13.regarima.ArmaModule");
      Method panel = moduleClass.getMethod("getPreferedModels");
      Class<?> bicClass = Class.forName(
          "jdplus.x13.base.core.x13.regarima.ArmaModule$RegArmaBic");
      Method getBic = bicClass.getMethod("getBIC");
      Method getSpec = bicClass.getMethod("getSpecification");
      Field arimaField = bicClass.getDeclaredField("arima");
      arimaField.setAccessible(true);
      Object[] rows = (Object[]) panel.invoke(module);
      for (Object row : rows) {
        SarmaOrders pspec = (SarmaOrders) getSpec.invoke(row);
        SarimaModel fitted = (SarimaModel) arimaField.get(row);
        List<String> blocks = new ArrayList<>();
        blocks.add(fitted == null ? "" : seq(fitted.getPhi()));
        blocks.add(fitted == null ? "" : seq(fitted.getBphi()));
        blocks.add(fitted == null ? "" : seq(fitted.getTheta()));
        blocks.add(fitted == null ? "" : seq(fitted.getBtheta()));
        sb.append(line("PANEL", Integer.toString(pspec.getP()),
            Integer.toString(pspec.getQ()), Integer.toString(pspec.getBp()),
            Integer.toString(pspec.getBq()), fmt((Double) getBic.invoke(row)),
            String.join("/", blocks)));
      }
    } catch (Throwable e) {
      sb.append(line("PANEL", err(e)));
    }

    // 6. SarimaOrders.of(best, d, bd) and the pinned comparison block.
    sb.append(result(SarimaOrders.of(best, d, bd), mean, spec0, mean0, "normal"));
    return sb.toString();
  }

  private static double field(Class<?> cls, Object o, String name) throws Exception {
    Field f = cls.getDeclaredField(name);
    f.setAccessible(true);
    return f.getDouble(o);
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
    if (path == null || path.equals("-")) {
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

  public static void main(String[] args) throws Exception {
    if (args.length < 10) {
      System.out.println("error|usage|seriesfile userfile period p q bp bq d bd mean");
      return;
    }
    double[] y = readSeries(args[0]);
    int[] k = new int[1];
    double[] users = readUsers(args[1], y.length, k);
    System.out.print(run(Integer.parseInt(args[2]), Integer.parseInt(args[3]),
        Integer.parseInt(args[4]), Integer.parseInt(args[5]), Integer.parseInt(args[6]),
        Integer.parseInt(args[7]), Integer.parseInt(args[8]),
        Integer.parseInt(args[9]) != 0, y, users, k[0]));
  }
}
