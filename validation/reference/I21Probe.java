import jdplus.toolkit.base.api.data.DoubleSeq;
import jdplus.toolkit.base.api.math.matrices.Matrix;
import jdplus.toolkit.base.api.processing.ProcessingLog;
import jdplus.toolkit.base.api.timeseries.TsData;
import jdplus.toolkit.base.api.timeseries.TsDomain;
import jdplus.toolkit.base.api.timeseries.TsPeriod;
import jdplus.toolkit.base.api.timeseries.calendars.DayClustering;
import jdplus.toolkit.base.api.timeseries.calendars.Easter;
import jdplus.toolkit.base.api.timeseries.calendars.GenericTradingDays;
import jdplus.toolkit.base.api.timeseries.calendars.LengthOfPeriodType;
import jdplus.toolkit.base.api.timeseries.calendars.TradingDaysType;
import jdplus.toolkit.base.api.timeseries.regression.EasterVariable;
import jdplus.toolkit.base.api.timeseries.regression.GenericTradingDaysVariable;
import jdplus.toolkit.base.api.timeseries.regression.ITradingDaysVariable;
import jdplus.toolkit.base.api.timeseries.regression.ITsVariable;
import jdplus.toolkit.base.api.timeseries.regression.ILengthOfPeriodVariable;
import jdplus.toolkit.base.api.timeseries.regression.JulianEasterVariable;
import jdplus.toolkit.base.api.timeseries.regression.LengthOfPeriod;
import jdplus.toolkit.base.api.timeseries.regression.ModellingContext;
import jdplus.toolkit.base.api.timeseries.regression.Ramp;
import jdplus.toolkit.base.api.timeseries.regression.UserVariables;
import jdplus.toolkit.base.api.timeseries.regression.Variable;
import jdplus.toolkit.base.core.math.matrices.FastMatrix;
import jdplus.toolkit.base.core.modelling.GeneralLinearModel;
import jdplus.toolkit.base.core.modelling.regression.GenericTradingDaysFactory;
import jdplus.toolkit.base.core.modelling.regression.Regression;
import jdplus.toolkit.base.core.regsarima.regular.RegSarimaModel;
import jdplus.toolkit.base.core.sarima.SarimaModel;
import jdplus.toolkit.base.core.stats.likelihood.LikelihoodStatistics;
import jdplus.tramoseats.base.api.tramo.TramoSpec;
import jdplus.tramoseats.base.core.tramo.TramoKernel;
import jdplus.x13.base.api.x13.X13Spec;
import jdplus.x13.base.r.X13;
import java.time.LocalDate;
import java.time.LocalDateTime;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * 21-I live probe (pinned jdplus 3.9.0): calendar/Easter/user-variable
 * GENERATION (TD matrices, LoP, Easter computus, ramps, user columns, names)
 * and DETECTION (X-13 rsa4/rsa5c regAIC modules; TRAMO TRfull FTEST) with raw
 * getter dumps (%.17g). Double capture: two independent processes must agree.
 * Modes: gen | x13tpl | x13run <rsa4|rsa5c> <clean|tdeff> | tramo <spec> <clean|tdeff>
 */
public class I21Probe {

  // pinned proto bytes (m4gap capture A, 248 bytes each)
  static final String HEX_RSA4
      = "0ab7010a060a0010011801120b080a1900000000000000c01a1f0a040a02414f0a040a024c530a040a025443120029666666666666e63f301e220c18012202100130013a0210012a4c080111666666666666ee3f19000000000000f03f21280af4893c49c23f29cdccccccccccf03f31d151812fae7ef03f392fbae8a28b2ef23f419a9999999999b93f4967202a1dd933f03f580132161a0a08011001300250016200220808011008180152003a0b0a001148afbc9af2d77a3e1224100122010029000000000000f83f31000000000000044038ffffffffffffffffff0160031a16100219000000000000f03f21000000000000f03f2801";
  static final String HEX_RSA5C
      = "0ab7010a060a0010011801120b080a1900000000000000c01a1f0a040a02414f0a040a024c530a040a025443120029666666666666e63f301e220c18012202100130013a0210012a4c080111666666666666ee3f19000000000000f03f21280af4893c49c23f29cdccccccccccf03f31d151812fae7ef03f392fbae8a28b2ef23f419a9999999999b93f4967202a1dd933f03f580132161a0a08061001300250016200220808011008180152003a0b0a001148afbc9af2d77a3e1224100122010029000000000000f83f31000000000000044038ffffffffffffffffff0160031a16100219000000000000f03f21000000000000f03f2801";

  static String g(double d) {
    return String.format("%.17g", d);
  }

  static byte[] unhex(String h) {
    byte[] b = new byte[h.length() / 2];
    for (int i = 0; i < b.length; ++i) {
      b[i] = (byte) Integer.parseInt(h.substring(2 * i, 2 * i + 2), 16);
    }
    return b;
  }

  static String hex(byte[] b) {
    StringBuilder sb = new StringBuilder();
    for (byte x : b) {
      sb.append(String.format("%02x", x));
    }
    return sb.toString();
  }

  // -- fixed inputs (StrictMath only; intrinsic-independent) -------------------
  static double[] clean(int n) {
    double[] y = new double[n];
    for (int t = 0; t < n; ++t) {
      double k = t + 1;
      y[t] = 50 + 0.05 * k + 5 * StrictMath.sin(2 * StrictMath.PI * k / 12)
          + 3 * StrictMath.cos(4 * StrictMath.PI * k / 12)
          + 0.5 * StrictMath.sin(14 * StrictMath.PI * k / 12);
    }
    return y;
  }

  // monthly totals of a daily process with strong weekday effects (Mon..Sun
  // weights) + deterministic pseudo-noise + a fixed-date (not Easter) spike;
  // the trading-day signal is REAL, so the FTEST/AICc machines must keep it.
  static double[] tdeff(int n) {
    double[] y = new double[n];
    int year = 1990, month = 1;
    int k = 0;
    for (int t = 0; t < n; ++t) {
      int len = LocalDate.of(year, month, 1).lengthOfMonth();
      double s = 0;
      for (int d = 1; d <= len; ++d) {
        LocalDate dt = LocalDate.of(year, month, d);
        int wd = dt.getDayOfWeek().getValue() - 1; // 0=Mon..6=Sun
        double w;
        switch (wd) {
          case 0: w = 0.9; break;
          case 1: w = 1.15; break;
          case 2: w = 0.8; break;
          case 3: w = 1.25; break;
          case 4: w = 0.6; break;
          default: w = 0.0; break; // Sat/Sun: closed
        }
        double noise = 0.15 * StrictMath.sin(k * 0.7 + 1) + 0.1 * StrictMath.sin(k * 2.3);
        s += (20.0 + 0.02 * k + noise) * w;
        ++k;
      }
      y[t] = s;
      ++month;
      if (month == 13) {
        month = 1;
        ++year;
      }
    }
    return y;
  }

  static TsData ts(String kind) {
    if (kind.equals("clean")) {
      return TsData.of(TsPeriod.monthly(1990, 1), DoubleSeq.of(clean(96)));
    }
    return TsData.of(TsPeriod.monthly(1990, 1), DoubleSeq.of(tdeff(156)));
  }

  static void dumpSeries(String tag, double[] y) {
    System.out.println("ser " + tag + " n=" + y.length);
    for (int i = 0; i < y.length; ++i) {
      System.out.println("val " + i + " " + g(y[i]));
    }
  }

  // -- raw column dump through the SAME machine the engines use ---------------
  static void dumpMatrix(String tag, ITsVariable v, TsDomain dom) {
    FastMatrix m = Regression.matrix(dom, new ITsVariable[]{v});
    System.out.println("mat " + tag + " rows=" + m.getRowsCount() + " cols=" + m.getColumnsCount());
    for (int i = 0; i < m.getRowsCount(); ++i) {
      StringBuilder sb = new StringBuilder();
      sb.append("row ").append(tag).append(' ').append(i);
      for (int j = 0; j < m.getColumnsCount(); ++j) {
        sb.append(' ').append(g(m.get(i, j)));
      }
      System.out.println(sb);
    }
  }

  // -- GEN mode ----------------------------------------------------------------
  static void gen() {
    DayClustering[] cls = {
      DayClustering.TD2, DayClustering.TD2c, DayClustering.TD2d, DayClustering.TD3,
      DayClustering.TD3c, DayClustering.TD4, DayClustering.TD4c, DayClustering.TD6,
      DayClustering.TD7
    };
    System.out.println("== DayClustering inventory");
    for (DayClustering c : cls) {
      System.out.println("dcl " + c.getType().name() + " groups=" + c.getGroupsCount()
          + " def=" + Arrays.toString(c.getGroupsDefinition())
          + " groupCounts=" + Arrays.toString(
              java.util.stream.IntStream.range(0, c.getGroupsCount()).map(c::getGroupCount).toArray()));
    }
    System.out.println("== TradingDaysType inventory");
    for (TradingDaysType t : TradingDaysType.values()) {
      System.out.println("tdt " + t + " nvar=" + t.getVariablesCount()
          + " names=" + Arrays.toString(t.names())
          + " contrastNames=" + Arrays.toString(t.contrastNames())
          + " desc=" + t.description());
    }
    System.out.println("== names");
    System.out.println("const ITradingDaysVariable.NAME=" + ITradingDaysVariable.NAME);
    System.out.println("const ILengthOfPeriodVariable.NAME=" + ILengthOfPeriodVariable.NAME);
    for (int k = 1; k <= 8; ++k) {
      System.out.println("defaultName(" + k + ")=" + ITradingDaysVariable.defaultName(k));
    }
    for (DayClustering c : cls) {
      System.out.println("nameOf " + c.getType().name() + " = " + GenericTradingDaysVariable.nameOf(c));
    }

    TsData m12 = TsData.of(TsPeriod.monthly(1990, 1), DoubleSeq.of(clean(108)));
    TsDomain dom = m12.getDomain();
    System.out.println("== raw tdCount " + dom.getInterval() + ".." + dom.getLastPeriod());
    int[][] td = GenericTradingDaysFactory.tdCount(dom);
    for (int i = 0; i < td.length; ++i) {
      System.out.println("tdc " + dom.get(i) + " " + Arrays.toString(td[i]));
    }
    System.out.println("== meanDays");
    System.out.println("mean " + Arrays.toString(GenericTradingDaysFactory.meanDays(dom)));
    System.out.println("mean12 " + Arrays.toString(GenericTradingDaysFactory.meanDays(12)));

    System.out.println("== contrast + raw matrices per clustering");
    for (DayClustering c : cls) {
      String nm = c.getType().name();
      GenericTradingDays gtd = GenericTradingDays.contrasts(c);
      GenericTradingDaysVariable v = new GenericTradingDaysVariable(gtd);
      System.out.println("gtd " + nm + " dim=" + v.dim() + " type=" + v.getTradingDaysType()
          + " desc=" + v.description(dom));
      dumpMatrix("c_" + nm, v, dom);
      GenericTradingDays rawv = GenericTradingDays.raw(c);
      GenericTradingDaysVariable rv = new GenericTradingDaysVariable(rawv);
      System.out.println("raw " + nm + " dim=" + rv.dim() + " desc=" + rv.description(dom));
      dumpMatrix("r_" + nm, rv, dom);
    }

    System.out.println("== LengthOfPeriod columns");
    for (LengthOfPeriodType t : LengthOfPeriodType.values()) {
      LengthOfPeriod lp = new LengthOfPeriod(t);
      System.out.println("lop " + t + " dim=" + lp.dim() + " desc=" + lp.description(dom));
      if (t != LengthOfPeriodType.None) {
        dumpMatrix("lop_" + t, lp, dom);
      }
    }

    System.out.println("== Easter computus 1985..2015");
    for (int y = 1985; y <= 2015; ++y) {
      System.out.println("eas " + y
          + " easter=" + Easter.easter(y)
          + " easter2=" + Easter.easter2(y)
          + " jul(true)=" + Easter.julianEaster(y, true)
          + " jul(false)=" + Easter.julianEaster(y, false)
          + " probJul " + g(Easter.probJulianEaster(y))
          + " prob " + g(Easter.probEaster(y)));
    }
    System.out.println("== EasterVariable columns");
    int[] durs = {1, 5, 8, 15};
    for (int d : durs) {
      EasterVariable ev = EasterVariable.builder().duration(d).build();
      System.out.println("eastvar d=" + d + " corr=" + ev.getMeanCorrection()
          + " end=" + ev.getEndPosition() + " dim=" + ev.dim() + " desc=" + ev.description(dom));
      dumpMatrix("eas_" + d, ev, dom);
      EasterVariable mv = EasterVariable.builder().duration(d)
          .meanCorrection(EasterVariable.Correction.Simple).build();
      System.out.println("eastvarSimple d=" + d + " corr=" + mv.getMeanCorrection());
      dumpMatrix("easS_" + d, mv, dom);
      JulianEasterVariable je = new JulianEasterVariable(d, true);
      System.out.println("julas " + je + " dim=" + je.dim() + " desc=" + je.description(dom));
      dumpMatrix("jul_" + d, je, dom);
    }

    System.out.println("== Ramp");
    Ramp r = new Ramp(LocalDateTime.of(1992, 3, 1, 0, 0), LocalDateTime.of(1994, 6, 1, 0, 0));
    System.out.println("ramp dim=" + r.dim() + " desc=" + r.description(dom));
    dumpMatrix("ramp", r, dom);
    Ramp r2 = new Ramp(LocalDateTime.of(1994, 6, 1, 0, 0), LocalDateTime.of(1992, 3, 1, 0, 0));
    System.out.println("ramp2 desc=" + r2.description(dom));
    dumpMatrix("ramp2", r2, dom);

    System.out.println("== UserVariables");
    TsData ud = TsData.of(TsPeriod.monthly(1992, 1), DoubleSeq.of(clean(48))); // SHORTER domain
    UserVariables uv = new UserVariables("grp", new String[]{"u1"}, new TsData[]{ud});
    System.out.println("uv dim=" + uv.dim() + " desc=" + uv.description(dom));
    try {
      dumpMatrix("user_short", uv, dom);
      System.out.println("user_short fill=OK");
    } catch (Exception ex) {
      System.out.println("user_short fill THROWS " + ex.getClass().getName() + " " + ex.getMessage());
    }
    TsData ud2 = TsData.of(TsPeriod.monthly(1990, 1), DoubleSeq.of(clean(108)));
    UserVariables uv2 = new UserVariables("grp", new String[]{"u1"}, new TsData[]{ud2});
    dumpMatrix("user_aligned", uv2, dom);
    System.out.println("user_aligned desc=" + uv2.description(dom));

    System.out.println("== quarterly record");
    TsData q = TsData.of(TsPeriod.quarterly(1990, 1), DoubleSeq.of(clean(36)));
    TsDomain qdom = q.getDomain();
    GenericTradingDaysVariable qv = new GenericTradingDaysVariable(GenericTradingDays.contrasts(DayClustering.TD2));
    System.out.println("q desc=" + qv.description(qdom));
    dumpMatrix("q_td2", qv, qdom);
    dumpMatrix("q_eas8", EasterVariable.builder().duration(8).build(), qdom);
  }

  // -- GEN2 mode: envelope-shape oracle (start offsets, corrections, endPos,
  // f in {12,4,1}, boundary durations, user alignments, ramp clamps, prob tables)
  static TsDomain domOf(int y, int m, int f, int n) {
    TsPeriod p;
    if (f == 12) {
      p = TsPeriod.monthly(y, m);
    } else if (f == 4) {
      p = TsPeriod.quarterly(y, (m - 1) / 3 + 1);
    } else {
      p = TsPeriod.yearly(y);
    }
    return TsDomain.of(p, n);
  }

  static void dumpAll(String t, TsDomain dom, int f) {
    System.out.println("mean " + t + " " + Arrays.toString(GenericTradingDaysFactory.meanDays(dom)));
    if (f > 3) {
      dumpMatrix("c2_" + t, new GenericTradingDaysVariable(GenericTradingDays.contrasts(DayClustering.TD2)), dom);
      dumpMatrix("c7_" + t, new GenericTradingDaysVariable(GenericTradingDays.contrasts(DayClustering.TD7)), dom);
      dumpMatrix("cu_" + t, new GenericTradingDaysVariable(GenericTradingDays.contrasts(DayClustering.TD2d)), dom);
      dumpMatrix("c3_" + t, new GenericTradingDaysVariable(GenericTradingDays.contrasts(DayClustering.TD3c)), dom);
      int[][] td = GenericTradingDaysFactory.tdCount(dom);
      for (int i = 0; i < td.length; ++i) {
        System.out.println("tdc " + t + " " + Arrays.toString(td[i]));
      }
    }
    dumpMatrix("ly_" + t, new LengthOfPeriod(LengthOfPeriodType.LeapYear), dom);
    dumpMatrix("lp_" + t, new LengthOfPeriod(LengthOfPeriodType.LengthOfPeriod), dom);
    if (f > 3) {
      dumpMatrix("eas_" + t, EasterVariable.builder().duration(8).build(), dom);
      dumpMatrix("jul_" + t, new JulianEasterVariable(8, true), dom);
    }
    dumpMatrix("rmp_" + t, new Ramp(LocalDateTime.of(1992, 3, 1, 0, 0), LocalDateTime.of(1994, 6, 1, 0, 0)), dom);
  }

  static void dumpUser(String t, TsDomain dom, TsPeriod dataStart, int len, double[] v) {
    TsData d = TsData.of(dataStart, DoubleSeq.of(v));
    UserVariables uv = new UserVariables("grp", new String[]{"u1"}, new TsData[]{d});
    try {
      dumpMatrix("u_" + t, uv, dom);
      System.out.println("u_" + t + " fill=OK");
    } catch (Exception ex) {
      System.out.println("u_" + t + " fill THROWS " + ex.getClass().getName());
    }
    TsData d2 = TsData.of(dataStart, DoubleSeq.of(v));
    dumpMatrix("ua_" + t, new UserVariables("grp", new String[]{"u1"}, new TsData[]{d2}), d2.getDomain());
  }

  static void gen2() {
    TsDomain domA = domOf(1990, 1, 12, 108);
    dumpAll("A", domA, 12);
    dumpAll("M1", domOf(1989, 4, 12, 90), 12);
    dumpAll("M2", domOf(1990, 3, 12, 48), 12);
    dumpAll("Q1", domOf(1990, 2, 4, 28), 4);
    dumpAll("Q2", domOf(1991, 5, 4, 30), 4);
    dumpAll("Y1", domOf(1990, 1, 1, 20), 1);
    System.out.println("== M3 (leapless)");
    TsDomain m3 = domOf(1991, 2, 12, 11);
    try {
      dumpMatrix("ly_M3", new LengthOfPeriod(LengthOfPeriodType.LeapYear), m3);
      System.out.println("ly_M3 fill=OK");
    } catch (Exception ex) {
      System.out.println("ly_M3 fill THROWS " + ex.getClass().getName());
    }
    dumpMatrix("lp_M3", new LengthOfPeriod(LengthOfPeriodType.LengthOfPeriod), m3);

    System.out.println("== easter variants (A)");
    Object[][] evs = {
      {"e13n", 13, 0, EasterVariable.Correction.None},
      {"e28s", 28, 0, EasterVariable.Correction.Simple},
      {"e10p", 10, 0, EasterVariable.Correction.PreComputed},
      {"e25p", 25, 0, EasterVariable.Correction.PreComputed},
      {"e20t", 20, 0, EasterVariable.Correction.Theoretical},
      {"e8pm", 8, 1, EasterVariable.Correction.Simple},
      {"e8nm", 8, -1, EasterVariable.Correction.Simple},
      {"e1pm", 1, 1, EasterVariable.Correction.None},
      {"e15nm", 15, -1, EasterVariable.Correction.Simple},};
    for (Object[] e : evs) {
      EasterVariable v = EasterVariable.builder().duration((Integer) e[1]).endPosition((Integer) e[2])
          .meanCorrection((EasterVariable.Correction) e[3]).build();
      System.out.println("vdesc " + e[0] + " corr=" + v.getMeanCorrection() + " end=" + v.getEndPosition()
          + " desc=" + v.description(domA));
      dumpMatrix((String) e[0], v, domA);
    }
    TsDomain m4 = domOf(2006, 1, 12, 36);
    EasterVariable v4 = EasterVariable.builder().duration(8).endPosition(-1)
        .meanCorrection(EasterVariable.Correction.Simple).build();
    dumpMatrix("e8nm_D", v4, m4);
    dumpMatrix("jul15f", new JulianEasterVariable(15, false), domA);
    dumpMatrix("jul28", new JulianEasterVariable(28, true), domA);
    dumpMatrix("jul1f", new JulianEasterVariable(1, false), domA);
    dumpMatrix("jul8_Q1", new JulianEasterVariable(8, true), domOf(1990, 2, 4, 28));
    dumpMatrix("jul15_Q2", new JulianEasterVariable(15, true), domOf(1991, 5, 4, 30));

    System.out.println("== user variants (A)");
    dumpUser("S", domA, TsPeriod.monthly(1992, 1), 48, clean(48));
    dumpUser("PRE", domA, TsPeriod.monthly(1988, 1), 40, clean(40));
    dumpUser("POST", domA, TsPeriod.monthly(1997, 6), 40, clean(40));
    dumpUser("BEF", domA, TsPeriod.monthly(1985, 1), 24, clean(24));
    dumpUser("AFT", domA, TsPeriod.monthly(1999, 1), 24, clean(24));
    dumpUser("COV", domA, TsPeriod.monthly(1989, 1), 200, clean(200));
    dumpUser("Q", domOf(1990, 2, 4, 28), TsPeriod.quarterly(1991, 1), 10, clean(10));
    dumpUser("Y", domOf(1990, 1, 1, 20), TsPeriod.yearly(1993), 10, clean(10));

    System.out.println("== ramp clamps (A)");
    dumpMatrix("rmpBEF", new Ramp(LocalDateTime.of(1985, 1, 1, 0, 0), LocalDateTime.of(1986, 1, 1, 0, 0)), domA);
    dumpMatrix("rmpAFT", new Ramp(LocalDateTime.of(2000, 1, 1, 0, 0), LocalDateTime.of(2001, 1, 1, 0, 0)), domA);
    dumpMatrix("rmpEQ", new Ramp(LocalDateTime.of(1992, 3, 1, 0, 0), LocalDateTime.of(1992, 3, 1, 0, 0)), domA);
    dumpMatrix("rmpQ", new Ramp(LocalDateTime.of(1992, 3, 1, 0, 0), LocalDateTime.of(1994, 6, 1, 0, 0)), domOf(1990, 2, 4, 28));
    dumpMatrix("rmpY", new Ramp(LocalDateTime.of(1993, 1, 1, 0, 0), LocalDateTime.of(1997, 1, 1, 0, 0)), domOf(1990, 1, 1, 20));

    System.out.println("== prob tables");
    for (int d = 0; d <= 44; ++d) {
      System.out.println("pjul " + d + " " + g(Easter.probJulianEaster(d)));
    }
    for (int d = 0; d <= 36; ++d) {
      System.out.println("prob " + d + " " + g(Easter.probEaster(d)));
    }
  }

  // -- TPL mode ----------------------------------------------------------------
  static void tpl() throws Exception {
    for (String nm : new String[]{"rsa0", "rsa1", "rsa2", "rsa2c", "rsa3", "rsa4", "rsa5", "rsa5c"}) {
      X13Spec s = X13Spec.fromString(nm);
      byte[] b = X13.toBuffer(s);
      byte[] back = X13.toBuffer(X13.specOf(b));
      System.out.println("fromString " + nm + " bytes=" + b.length + " roundtrip="
          + Arrays.equals(b, back) + " " + hex(b));
    }
    for (java.lang.reflect.Field f : X13Spec.class.getDeclaredFields()) {
      if (java.lang.reflect.Modifier.isStatic(f.getModifiers())
          && java.lang.reflect.Modifier.isPublic(f.getModifiers())
          && f.getType() == X13Spec.class) {
        X13Spec s = (X13Spec) f.get(null);
        byte[] b = X13.toBuffer(s);
        System.out.println("static " + f.getName() + " bytes=" + b.length + " " + hex(b));
      }
    }
    String[][] pin = {{"rsa4", HEX_RSA4}, {"rsa5c", HEX_RSA5C}};
    for (String[] p : pin) {
      byte[] raw = unhex(p[1]);
      X13Spec s = X13.specOf(raw);
      byte[] back = X13.toBuffer(s);
      System.out.println("roundtrip " + p[0] + " lenIn=" + raw.length + " lenOut=" + back.length
          + " identity=" + Arrays.equals(raw, back));
      System.out.println("ptext " + p[0]);
      System.out.println(jdplus.x13.base.protobuf.Spec.parseFrom(back).toString());
      System.out.println("end " + p[0]);
    }
  }

  // -- shared result facts ------------------------------------------------------
  static void modelFacts(String pfx, RegSarimaModel m) {
    GeneralLinearModel.Description<?> d = m.getDescription();
    System.out.println("fact " + pfx + " log " + d.isLogTransformation());
    Variable<?>[] vars = d.getVariables();
    System.out.println("fact " + pfx + " variables " + vars.length);
    for (int i = 0; i < vars.length; ++i) {
      Variable<?> v = vars[i];
      StringBuilder co = new StringBuilder();
      co.append('[');
      for (int j = 0; j < v.getCoefficients().length; ++j) {
        if (j > 0) {
          co.append(',');
        }
        co.append(v.getCoefficients()[j].getType())
            .append(':').append(g(v.getCoefficients()[j].getValue()));
      }
      co.append(']');
      StringBuilder at = new StringBuilder();
      v.getAttributes().forEach((k, val) -> at.append(k).append('=').append(val).append(';'));
      System.out.println("var " + pfx + " " + i + " name=" + v.getName() + " dim=" + v.dim()
          + " core=" + v.getCore().getClass().getSimpleName()
          + " coef=" + co + " attr=" + at);
    }
    SarimaModel ar = m.arima();
    if (ar != null) {
      DoubleSeq pars = ar.parameters();
      StringBuilder ps = new StringBuilder();
      for (int i = 0; i < pars.length(); ++i) {
        if (i > 0) {
          ps.append(' ');
        }
        ps.append(g(pars.get(i)));
      }
      System.out.println("fact " + pfx + " armapars " + ps);
      System.out.println("fact " + pfx + " orders " + ar.orders().getP() + " " + ar.orders().getD()
          + " " + ar.orders().getQ() + " " + ar.orders().getBp() + " " + ar.orders().getBd()
          + " " + ar.orders().getBq());
    }
    GeneralLinearModel.Estimation e = m.getEstimation();
    if (e != null) {
      LikelihoodStatistics ls = e.getStatistics();
      System.out.println("fact " + pfx + " stats " + ls.getObservationsCount() + ' '
          + ls.getEffectiveObservationsCount() + ' ' + ls.getEstimatedParametersCount());
      System.out.println("fact " + pfx + " ll " + g(ls.getLogLikelihood()));
      System.out.println("fact " + pfx + " adjll " + g(ls.getAdjustedLogLikelihood()));
      System.out.println("fact " + pfx + " aicc " + g(ls.getAICC()));
      System.out.println("fact " + pfx + " bic2 " + g(ls.getBIC2()));
      System.out.println("fact " + pfx + " ssq " + g(ls.getSsqErr()));
      DoubleSeq co = e.getCoefficients();
      StringBuilder cs = new StringBuilder();
      for (int i = 0; i < co.length(); ++i) {
        if (i > 0) {
          cs.append(' ');
        }
        cs.append(g(co.get(i)));
      }
      System.out.println("fact " + pfx + " coefs " + cs);
    }
  }

  // -- raw dump of every Info class the calendar surfaces emit ------------------
  static void dumpInfo(Object d) {
    if (d == null) {
      System.out.print("det -");
      return;
    }
    if (d instanceof jdplus.sa.base.core.regarima.AutomaticTradingRegressionModule.Info) {
      jdplus.sa.base.core.regarima.AutomaticTradingRegressionModule.Info i
          = (jdplus.sa.base.core.regarima.AutomaticTradingRegressionModule.Info) d;
      System.out.print("det ATD names=" + Arrays.toString(i.getNames()) + " best=" + i.getBest());
      LikelihoodStatistics[] ll = i.getLl();
      for (int k = 0; k < ll.length; ++k) {
        System.out.print(" ll[" + k + "] n=" + ll[k].getObservationsCount()
            + " eff=" + ll[k].getEffectiveObservationsCount()
            + " np=" + ll[k].getEstimatedParametersCount()
            + " ssq=" + g(ll[k].getSsqErr())
            + " bic2=" + g(ll[k].getBIC2())
            + " bicc=" + g(ll[k].getBICC()));
      }
      return;
    }
    if (d instanceof jdplus.sa.base.core.regarima.EasterDetectionModule.Info) {
      jdplus.sa.base.core.regarima.EasterDetectionModule.Info i
          = (jdplus.sa.base.core.regarima.EasterDetectionModule.Info) d;
      System.out.print("det EAS names=" + Arrays.toString(i.getNames()) + " best=" + i.getBest());
      System.out.print(" ll0 n=" + i.getLl0().getObservationsCount()
          + " ssq=" + g(i.getLl0().getSsqErr())
          + " bic2=" + g(i.getLl0().getBIC2())
          + " bicc=" + g(i.getLl0().getBICC()));
      LikelihoodStatistics[] ll = i.getLl();
      for (int k = 0; k < ll.length; ++k) {
        System.out.print(" ll[" + k + "] n=" + ll[k].getObservationsCount()
            + " eff=" + ll[k].getEffectiveObservationsCount()
            + " np=" + ll[k].getEstimatedParametersCount()
            + " ssq=" + g(ll[k].getSsqErr())
            + " bic2=" + g(ll[k].getBIC2())
            + " bicc=" + g(ll[k].getBICC()));
      }
      return;
    }
    if (d instanceof jdplus.tramoseats.base.core.tramo.AutomaticFRegressionTest.Info) {
      jdplus.tramoseats.base.core.tramo.AutomaticFRegressionTest.Info i
          = (jdplus.tramoseats.base.core.tramo.AutomaticFRegressionTest.Info) d;
      System.out.print("det FT nhp=" + i.getNhp()
          + " ss0=" + g(i.getSs0()) + " ss1=" + g(i.getSs1()) + " ss6=" + g(i.getSs6())
          + " ssmc1=" + g(i.getSsmc1()) + " ssmc6=" + g(i.getSsmc6())
          + " pftd1=" + g(i.getPftd1()) + " pftd6=" + g(i.getPftd6()));
      if (i.getLl0() != null) {
        System.out.print(" dof0=" + i.getLl0().degreesOfFreedom());
      }
      if (i.getLl1() != null) {
        System.out.print(" dof1=" + i.getLl1().degreesOfFreedom());
      }
      if (i.getLl6() != null) {
        System.out.print(" dof6=" + i.getLl6().degreesOfFreedom());
      }
      return;
    }
    if (d instanceof jdplus.tramoseats.base.core.tramo.ModelStatistics) {
      jdplus.tramoseats.base.core.tramo.ModelStatistics i
          = (jdplus.tramoseats.base.core.tramo.ModelStatistics) d;
      System.out.print("det MST lbP=" + g(i.getLjungBoxPvalue()) + " se=" + g(i.getSe()));
      return;
    }
    if (d instanceof Double) {
      System.out.print("det num " + g((Double) d));
      return;
    }
    if (d instanceof Object[]) {
      System.out.print("det arr " + Arrays.deepToString((Object[]) d));
      return;
    }
    System.out.print("det obj " + String.valueOf(d));
  }

  static final class Observer implements ProcessingLog {
    int depth = 0;
    long seq = 0;

    void emit(String kind, String msg, Object details) {
      ++seq;
      System.out.print("evt " + seq + " " + kind + " " + depth + " "
          + (msg == null ? "-" : msg.replace('\n', ' ')));
      dumpInfo(details);
      System.out.println();
    }

    public void push(String s) {
      emit("push", s, null);
      ++depth;
    }

    public void pop() {
      if (depth > 0) {
        --depth;
      }
      emit("pop", null, null);
    }

    public void error(String m, String o, Object d) {
      emit("error", m, d);
    }

    public void warning(String m, String o, Object d) {
      emit("warn", m, d);
    }

    public void remark(String m, String o, Object d) {
      emit("remark", m, d);
    }

    public void info(String m, String o, Object d) {
      emit("info", m, d);
    }

    public boolean isVerbose() {
      return true;
    }
  }

  // -- X-13 run ------------------------------------------------------------------
  static void x13run(String tpl, String kind) throws Exception {
    dumpSeries(kind, ts(kind).getValues().toArray());
    byte[] raw = unhex(tpl.equals("rsa4") ? HEX_RSA4 : HEX_RSA5C);
    X13Spec spec = X13.specOf(raw);
    System.out.println("spec " + tpl + " roundtrip=" + Arrays.equals(raw, X13.toBuffer(spec)));
    TsData data = ts(kind).cleanExtremities();
    jdplus.x13.base.core.x13.X13Kernel k =
        jdplus.x13.base.core.x13.X13Kernel.of(spec, new ModellingContext());
    Observer obs = new Observer();
    jdplus.x13.base.core.x13.X13Results r;
    try {
      r = k.process(data, obs);
    } catch (Exception ex) {
      System.out.println("run THROW " + ex.getClass().getName() + " " + ex.getMessage());
      return;
    }
    if (r == null) {
      System.out.println("run NULL results (kernel failure path)");
      return;
    }
    modelFacts("pre", r.getPreprocessing());
  }

  // -- TRAMO run ------------------------------------------------------------------
  static void tramo(String specName, String kind) {
    dumpSeries(kind, ts(kind).getValues().toArray());
    TramoSpec spec = TramoSpec.fromString(specName);
    System.out.println("spec " + specName + " resolved=" + (spec != null));
    TramoKernel k = TramoKernel.of(spec, new ModellingContext());
    Observer obs = new Observer();
    RegSarimaModel m;
    try {
      m = k.process(ts(kind), obs);
    } catch (Exception ex) {
      System.out.println("run THROW " + ex.getClass().getName() + " " + ex.getMessage());
      StackTraceElement[] st = ex.getStackTrace();
      for (int i = 0; i < st.length && i < 12; ++i) {
        System.out.println("fr " + i + " " + st[i]);
      }
      return;
    }
    modelFacts("tr", m);
  }

  public static void main(String[] args) throws Exception {
    String mode = args[0];
    switch (mode) {
      case "gen":
        gen();
        break;
      case "gen2":
        gen2();
        break;
      case "x13tpl":
        tpl();
        break;
      case "x13run":
        x13run(args[1], args[2]);
        break;
      case "tramo":
        tramo(args[1], args[2]);
        break;
      default:
        System.out.println("unknown mode " + mode);
    }
  }
}
