// Item 21-I.1: replay the pinned jdplus 3.9.0 calendar-generation corpus
// (tests/jvm_calendar_generation.h, generated from the byte-identical double
// JVM captures in validation/evidence/native/calendar-generation/) through
// the public kalos::calendar_generation API. Every matrix value is compared
// BIT for BIT against the captured IEEE-754 pattern; the computus, the
// probability tables and the name/description conventions replay as separate
// claims; the envelope refusals (annual TD/Easter domains, unaligned domains
// or user data, duration bounds) must throw std::invalid_argument.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

#include "jvm_calendar_generation.h"
#include "kalos/calendar_generation.hpp"

namespace {

std::size_t g_checks = 0;
std::size_t g_failures = 0;
std::vector<std::string> g_problems;
bool g_verbose = false;

void fail(const std::string& id, const std::string& what) {
  ++g_failures;
  g_problems.push_back(id + ": " + what);
}

double from_bits(std::uint64_t u) {
  double d;
  std::memcpy(&d, &u, 8);
  return d;
}

// BIT comparison: sign included, NaN == NaN.
void chk_bits(const std::string& id, double got, std::uint64_t want) {
  ++g_checks;
  std::uint64_t g;
  std::memcpy(&g, &got, 8);
  if (g == want) return;
  if (std::isnan(got) && std::isnan(from_bits(want))) return;
  char buf[128];
  std::snprintf(buf, sizeof(buf), "bit %016llx != %016llx (got %.17g want %.17g)",
                static_cast<unsigned long long>(g), static_cast<unsigned long long>(want),
                got, from_bits(want));
  fail(id, buf);
}

// Flatten kalos output into the golden row-major layout and compare.
void chk_matrix(const std::string& id, const std::vector<double>& got, int start, int cols) {
  ++g_checks;
  if (static_cast<int>(got.size()) != cols) {
    fail(id, "width " + std::to_string(got.size()) + " != " + std::to_string(cols));
    return;
  }
  for (int i = 0; i < cols; ++i) chk_bits(id, got[static_cast<std::size_t>(i)],
                                          kalos_golden::VALS[static_cast<std::size_t>(start + i)]);
}

const kalos::day_clustering CL[9] = {
    kalos::day_clustering::td2,  kalos::day_clustering::td2c, kalos::day_clustering::td2d,
    kalos::day_clustering::td3,  kalos::day_clustering::td3c, kalos::day_clustering::td4,
    kalos::day_clustering::td4c, kalos::day_clustering::td6,  kalos::day_clustering::td7};

std::vector<double> user_data(const kalos_golden::Case& c) {
  for (int u = 0; u < kalos_golden::NUDATA; ++u) {
    if (std::strcmp(kalos_golden::UDATA_TAB[u].name, c.udata) == 0) {
      const int s = kalos_golden::UDATA_TAB[u].start;
      std::vector<double> d;
      for (int i = 0; i < c.ulen; ++i)
        d.push_back(from_bits(kalos_golden::UDATA[static_cast<std::size_t>(s + c.uoff + i)]));
      return d;
    }
  }
  fail(c.id, std::string("user data ") + c.udata + " not in table");
  return {};
}

void run_matrix(const kalos_golden::Case& c) {
  const std::string id = c.id;
  const kalos::calendar_domain dom{kalos_golden::DOMAINS[c.dom][0],
                                   kalos_golden::DOMAINS[c.dom][1],
                                   kalos_golden::DOMAINS[c.dom][2],
                                   kalos_golden::DOMAINS[c.dom][3]};
  const int start = c.start;
  switch (c.kind) {
    case kalos_golden::K_TDC: {
      auto got = kalos::td_count(dom);
      ++g_checks;
      if (static_cast<int>(got.size()) != c.rows) {
        fail(id, "td_count rows");
        return;
      }
      for (int w = 0; w < c.rows; ++w)
        for (int i = 0; i < c.cols; ++i)
          chk_bits(id + "[" + std::to_string(w) + "][" + std::to_string(i) + "]",
                   static_cast<double>(got[static_cast<std::size_t>(w)]
                                           [static_cast<std::size_t>(i)]),
                   kalos_golden::VALS[static_cast<std::size_t>(start + w * c.cols + i)]);
      return;
    }
    case kalos_golden::K_MEAN:
      chk_matrix(id, kalos::mean_days(dom), start, c.cols);
      return;
    case kalos_golden::K_MEAN12:
      chk_matrix(id, kalos::mean_days_yearly(12), start, c.cols);
      return;
    case kalos_golden::K_CONTRAST: {
      auto got = kalos::trading_days_contrast(CL[c.p0], dom);
      ++g_checks;
      if (static_cast<int>(got.size()) != c.cols) {
        fail(id, "contrast width " + std::to_string(got.size()) + " != " +
                     std::to_string(c.cols));
        return;
      }
      for (int i = 0; i < c.rows; ++i)
        for (int j = 0; j < c.cols; ++j)
          chk_bits(id + "[" + std::to_string(i) + "][" + std::to_string(j) + "]",
                   got[static_cast<std::size_t>(j)][static_cast<std::size_t>(i)],
                   kalos_golden::VALS[static_cast<std::size_t>(start + i * c.cols + j)]);
      return;
    }
    case kalos_golden::K_LOP: {
      auto got = kalos::length_of_period_column(
          c.p0 == 1 ? kalos::calendar_lop::leap_year
                    : kalos::calendar_lop::length_of_period,
          dom);
      ++g_checks;
      if (static_cast<int>(got.size()) != c.rows) {
        fail(id, "lop length");
        return;
      }
      for (int i = 0; i < c.rows; ++i)
        chk_bits(id + "[" + std::to_string(i) + "]", got[static_cast<std::size_t>(i)],
                 kalos_golden::VALS[static_cast<std::size_t>(start + i)]);
      return;
    }
    case kalos_golden::K_EASTER: {
      kalos::easter_correction corr = kalos::easter_correction::simple;
      switch (c.p1) {
        case 0: corr = kalos::easter_correction::none; break;
        case 1: corr = kalos::easter_correction::simple; break;
        case 2: corr = kalos::easter_correction::pre_computed; break;
        case 3: corr = kalos::easter_correction::theoretical; break;
      }
      auto got = kalos::easter_column(c.p0, corr, c.p2, dom);
      ++g_checks;
      if (static_cast<int>(got.size()) != c.rows) {
        fail(id, "easter length");
        return;
      }
      for (int i = 0; i < c.rows; ++i)
        chk_bits(id + "[" + std::to_string(i) + "]", got[static_cast<std::size_t>(i)],
                 kalos_golden::VALS[static_cast<std::size_t>(start + i)]);
      return;
    }
    case kalos_golden::K_JULIAN: {
      auto got = kalos::julian_easter_column(c.p0, c.p1 == 1, dom);
      ++g_checks;
      if (static_cast<int>(got.size()) != c.rows) {
        fail(id, "julian easter length");
        return;
      }
      for (int i = 0; i < c.rows; ++i)
        chk_bits(id + "[" + std::to_string(i) + "]", got[static_cast<std::size_t>(i)],
                 kalos_golden::VALS[static_cast<std::size_t>(start + i)]);
      return;
    }
    case kalos_golden::K_RAMP: {
      auto got = kalos::ramp_column(c.p0, c.p1, c.p2, c.p3, dom);
      ++g_checks;
      if (static_cast<int>(got.size()) != c.rows) {
        fail(id, "ramp length");
        return;
      }
      for (int i = 0; i < c.rows; ++i)
        chk_bits(id + "[" + std::to_string(i) + "]", got[static_cast<std::size_t>(i)],
                 kalos_golden::VALS[static_cast<std::size_t>(start + i)]);
      return;
    }
    case kalos_golden::K_USER: {
      auto got = kalos::user_column(user_data(c), c.p0, c.p1, dom);
      ++g_checks;
      if (static_cast<int>(got.size()) != c.rows) {
        fail(id, "user length");
        return;
      }
      for (int i = 0; i < c.rows; ++i)
        chk_bits(id + "[" + std::to_string(i) + "]", got[static_cast<std::size_t>(i)],
                 kalos_golden::VALS[static_cast<std::size_t>(start + i)]);
      return;
    }
  }
}

}  // namespace

int main(int argc, char** argv) {
  g_verbose = argc > 1 && std::strcmp(argv[1], "-v") == 0;

  for (int i = 0; i < kalos_golden::NCASES; ++i) {
    const kalos_golden::Case& c = kalos_golden::CASES[i];
    try {
      run_matrix(c);
    } catch (const std::exception& e) {
      fail(c.id, std::string("escaped exception: ") + e.what());
    }
    if (g_verbose)
      std::printf("%-22s %d x %d %s\n", c.id, c.rows, c.cols,
                  g_failures == 0 ? "ok" : "FAIL");
  }

  // computus 1985..2015 (easter == easter2 pinned; both julian variants;
  // the out-of-range probability calls the probe made with YEAR arguments)
  for (int i = 0; i < kalos_golden::NCOMP; ++i) {
    const auto& r = kalos_golden::COMP[i];
    const std::string y = "y" + std::to_string(r.y);
    auto e = kalos::easter_date(r.y);
    ++g_checks;
    if (e.month * 100 + e.day != r.e) fail(y, "easter computus");
    auto jt = kalos::julian_easter_date(r.y, true);
    ++g_checks;
    if (jt.month * 100 + jt.day != r.jt) fail(y, "julian easter (gregorian)");
    auto jf = kalos::julian_easter_date(r.y, false);
    ++g_checks;
    if (jf.month * 100 + jf.day != r.jf) fail(y, "julian easter (julian)");
    chk_bits(y + " pjul", kalos::prob_julian_easter(r.y), r.pjul);
    chk_bits(y + " prob", kalos::prob_easter(r.y), r.prob);
  }

  // probability tables incl. the pinned out-of-range guards
  for (int i = 0; i < kalos_golden::NPJUL; ++i)
    chk_bits("pjul" + std::to_string(kalos_golden::PJUL[i].d),
             kalos::prob_julian_easter(kalos_golden::PJUL[i].d), kalos_golden::PJUL[i].v);
  for (int i = 0; i < kalos_golden::NPROB; ++i)
    chk_bits("prob" + std::to_string(kalos_golden::PROB[i].d),
             kalos::prob_easter(kalos_golden::PROB[i].d), kalos_golden::PROB[i].v);
  for (int d : {-1, 43, 44, 100}) {
    ++g_checks;
    if (kalos::prob_julian_easter(d) != 0.0) fail("pjul guard", std::to_string(d));
  }
  for (int d : {-1, 35, 36, 100}) {
    ++g_checks;
    if (kalos::prob_easter(d) != 0.0) fail("prob guard", std::to_string(d));
  }

  // name conventions
  for (int i = 0; i < 9; ++i) {
    ++g_checks;
    if (std::strcmp(kalos::trading_days_tag(CL[i]), kalos_golden::NAMEOF[i].tag) != 0)
      fail("tag", kalos_golden::NAMEOF[i].tag);
    ++g_checks;
    if (std::strcmp(kalos::trading_days_name(CL[i]), kalos_golden::NAMEOF[i].name) != 0)
      fail("nameOf", kalos_golden::NAMEOF[i].tag);
    std::string gtd(kalos_golden::GTD[i].line);
    ++g_checks;
    if (gtd.find(std::string("gtd ") + kalos::trading_days_tag(CL[i])) != 0 ||
        gtd.find(std::string(" type=") + kalos::trading_days_tag(CL[i])) == std::string::npos ||
        gtd.find(std::string(" desc=") + kalos::trading_days_name(CL[i])) == std::string::npos)
      fail("gtd line", gtd);
  }
  for (int k = 1; k <= 8; ++k) {
    ++g_checks;
    if (std::strcmp(kalos::trading_days_default_name(k), "td") != 0)
      fail("defaultName", std::to_string(k));
  }
  ++g_checks;
  if (std::strcmp(kalos::trading_days_variable_name(), "td") != 0 ||
      std::strcmp(kalos::length_of_period_variable_name(), "lp") != 0)
    fail("const NAME", "td/lp");
  {
    const auto& types = kalos::trading_days_types();
    ++g_checks;
    if (types.size() != 11 || sizeof(kalos_golden::TDT) / sizeof(kalos_golden::TDT[0]) != 11)
      fail("tdt table size", std::to_string(types.size()));
    // capture order differs from the jar values() order: match rows by tag
    for (const auto& g : kalos_golden::TDT) {
      bool found = false;
      for (const auto& t : types) {
        if (std::strcmp(t.tag, g.tag) != 0) continue;
        found = true;
        ++g_checks;
        if (t.nvar != g.nvar || std::strcmp(t.names, g.names) != 0 ||
            std::strcmp(t.contrast_names, g.contrast_names) != 0 ||
            std::strcmp(t.description, g.desc) != 0)
          fail("tdt row", g.tag);
        break;
      }
      if (!found) fail("tdt row missing", g.tag);
    }
  }
  // descriptions
  for (int i = 0; i < 4; ++i) {
    ++g_checks;
    if (kalos::julian_easter_description(kalos_golden::EAS_DUR[i]) !=
        kalos_golden::EAS_DESC[i].julian_desc)
      fail("julian easter desc", std::to_string(kalos_golden::EAS_DUR[i]));
  }
  for (int i = 0; i < 9; ++i) {
    const auto& v = kalos_golden::VDESC[i];
    ++g_checks;
    if (kalos::easter_description(v.dur) != v.desc)
      fail("easter desc", std::to_string(v.dur));
  }
  ++g_checks;
  if (std::string(kalos_golden::DESC_TD_NAME) !=
      std::string("const ITradingDaysVariable.NAME=") + kalos::trading_days_variable_name())
    fail("desc", "td name line");
  ++g_checks;
  if (std::string(kalos_golden::DESC_LP_NAME) !=
      std::string("const ILengthOfPeriodVariable.NAME=") + kalos::length_of_period_variable_name())
    fail("desc", "lp name line");
  ++g_checks;
  if (std::string(kalos_golden::DESC_Q) !=
      std::string("q desc=") + kalos::trading_days_name(kalos::day_clustering::td2))
    fail("desc", "q line");
  ++g_checks;
  if (std::string(kalos_golden::DESC_UV) != "uv dim=1 desc=grp" ||
      std::string(kalos_golden::DESC_USER_ALIGNED) != "user_aligned desc=grp")
    fail("desc", "user lines");
  ++g_checks;
  if (std::string(kalos_golden::DESC_RAMP) !=
      "ramp dim=1 desc=" + kalos::ramp_description(1992, 3, 1994, 6, 12))
    fail("desc", "ramp line");
  ++g_checks;
  if (std::string(kalos_golden::DESC_RAMP2) !=
      "ramp2 desc=" + kalos::ramp_description(1994, 6, 1992, 3, 12))
    fail("desc", "ramp2 line");
  ++g_checks;
  if (std::string(kalos_golden::DESC_LOP_NONE) !=
      std::string("lop None dim=1 desc=") +
          kalos::length_of_period_description(kalos::calendar_lop::none))
    fail("desc", "lop None");
  ++g_checks;
  if (std::string(kalos_golden::DESC_LOP_LEAP) !=
      std::string("lop LeapYear dim=1 desc=") +
          kalos::length_of_period_description(kalos::calendar_lop::leap_year))
    fail("desc", "lop LeapYear");
  ++g_checks;
  if (std::string(kalos_golden::DESC_LOP_PERIOD) !=
      std::string("lop LengthOfPeriod dim=1 desc=") +
          kalos::length_of_period_description(kalos::calendar_lop::length_of_period))
    fail("desc", "lop LengthOfPeriod");

  // envelope refusals (fail-closed where the jar refuses or silently leaves
  // an unprobed zero column)
  {
    const kalos::calendar_domain ann{1990, 1, 1, 20};
    const kalos::calendar_domain unaligned{1990, 2, 4, 12};
    const kalos::calendar_domain monthly{1990, 1, 12, 108};
    auto refuse = [&](const char* what, const std::function<void()>& fn) {
      ++g_checks;
      bool threw = false;
      try {
        fn();
      } catch (const std::invalid_argument&) {
        threw = true;
      }
      if (!threw) fail("envelope", std::string("no refusal: ") + what);
    };
    refuse("annual td_count", [&] { kalos::td_count(ann); });
    refuse("annual contrast",
           [&] { kalos::trading_days_contrast(kalos::day_clustering::td2, ann); });
    refuse("annual easter", [&] {
      kalos::easter_column(8, kalos::easter_correction::simple, 0, ann);
    });
    refuse("annual julian", [&] { kalos::julian_easter_column(8, true, ann); });
    refuse("unaligned domain", [&] { kalos::td_count(unaligned); });
    refuse("zero length", [&] { kalos::td_count(kalos::calendar_domain{1990, 1, 12, 0}); });
    refuse("bad frequency", [&] { kalos::td_count(kalos::calendar_domain{1990, 1, 2, 108}); });
    refuse("easter dur 0",
           [&] { kalos::easter_column(0, kalos::easter_correction::simple, 0, monthly); });
    refuse("easter dur 29",
           [&] { kalos::easter_column(29, kalos::easter_correction::simple, 0, monthly); });
    refuse("PreComputed 26", [&] {
      kalos::easter_column(26, kalos::easter_correction::pre_computed, 0, monthly);
    });
    refuse("julian dur 29", [&] { kalos::julian_easter_column(29, true, monthly); });
    refuse("user misaligned", [&] {
      kalos::user_column(std::vector<double>(10, 1.0), 1990, 2, unaligned);
    });
    refuse("user empty",
           [&] { kalos::user_column(std::vector<double>{}, 1990, 1, monthly); });
    refuse("mean_days_yearly(4)", [&] { kalos::mean_days_yearly(4); });
    refuse("ramp_description quarterly", [&] { kalos::ramp_description(1992, 3, 1994, 6, 4); });
  }

  std::printf("test_calendar_generation: %zu checks, %zu failures, %d cases\n", g_checks,
              g_failures, kalos_golden::NCASES);
  for (std::size_t i = 0; i < g_problems.size() && i < 40; ++i)
    std::printf("  %s\n", g_problems[i].c_str());
  return g_failures == 0 ? 0 : 1;
}
