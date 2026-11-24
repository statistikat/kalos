// Item 21-H.2: the JVM oracle for the automatic-spec pipeline driver.
//
// docs/contracts/x13-automatic-pipeline-contract.md.  Every case in
// tests/jvm_x13_automatic.h is the decoder-equivalent JVM automatic pipeline
// (X13Kernel.of(spec, ctx).process(ts.cleanExtremities(), log) behind the
// X13AutomaticProbe observer) run on the same request the driver mirrors;
// the corpus and the double-capture protocol live in
// validation/evidence/native/x13-automatic (capture-x13-automatic.R).
//
// Tier policy (contract Envelope, measured against the sweep in
// tools/verify_x13_automatic.cpp):
//   - decisions are BIT-exact: the final description, the variable identities,
//     the differencing triples, the arma selections and panel membership, the
//     ODM picks and cv, the log-level preference scalar, the cascade remark,
//     the effective-observation count;
//   - quantities the JVM only displays through the observer (%.6g) or that
//     ride on trial-fit arithmetic carry their pinned bands: tmean 5e-3
//     (21-D trial-fit class; re-measured 3.1e-3 under the CI-FDLIBM pinned
//     transcendental semantics, see kTmeanBand), panel BIC2 1e-2
//     (measured 8.1e-3 at the
//     automatic landing, membership bit), the AICC display pair 1e-5;
//   - landing quantities from the refinement basin carry the 2e-3 basin
//     band (loglik, AICC, regression coefficients) and the ARMA landing
//     coefficients the measured 1e-2 coefficient-basin band (G2 seam);
//   - the downstream composition tables ride the measured 1e-6 relative
//     table band (landing-drift propagation through the X-11 composition;
//     the deterministic-chain cases stay bit-identical far below it).
//
// Cost policy: a detection-on case runs the full automatic search (~230 s
// single-process); the suite is the ctest twin of the 13-case sweep, so
// CMake raises its timeout like test_x13_automatic's.  argv selects case ids
// (used by the sanitizer subset to run the cheap outlier-free cases).

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <map>
#include <string>
#include <vector>

#include "kalos/x13_automatic.hpp"
#include "x13_nonlog_pseudo.hpp"

#include "jvm_x13_automatic.h"

namespace {

int g_failures = 0;
long long g_checks = 0;

double rdbits(std::uint64_t bits) {
  double v;
  std::memcpy(&v, &bits, sizeof(v));
  return v;
}

bool bit_equal(double a, double b) {
  return std::memcmp(&a, &b, sizeof(double)) == 0;
}
bool same_nan(double a, double b) {
  return std::isnan(a) && std::isnan(b);
}

// ---- family scoreboard (the contract tier tables are built from this) -----

struct Family {
  long long checks = 0, bad = 0, exact = 0;
  double worst = 0.;
  std::string worst_where;
};
std::map<std::string, Family> g_families;

void tally(const std::string& name, bool ok, bool bit, double dev,
           const std::string& where) {
  Family& f = g_families[name];
  ++f.checks;
  ++g_checks;
  if (bit) ++f.exact;
  if (!ok) {
    ++f.bad;
    ++g_failures;
    std::printf("FAIL [%s] %s\n", name.c_str(), where.c_str());
  }
  if (dev > f.worst) {
    f.worst = dev;
    f.worst_where = where;
  }
}

void bit_d(const std::string& fam, double got, double want,
           const std::string& where) {
  const bool ok = bit_equal(got, want) || same_nan(got, want);
  tally(fam, ok, ok, 0., where);
}
void bit_i(const std::string& fam, long long got, long long want,
           const std::string& where) {
  tally(fam, got == want, got == want, got == want ? 0. : 1., where);
}
void bit_s(const std::string& fam, const std::string& got,
           const std::string& want, const std::string& where) {
  tally(fam, got == want, got == want, got == want ? 0. : 1., where);
}
void close_d(const std::string& fam, double got, double want, double band,
             const std::string& where) {
  if (bit_equal(got, want) || same_nan(got, want)) {
    tally(fam, true, true, 0., where);
    return;
  }
  const double rel =
      std::abs(want) > 0. ? std::abs(got - want) / std::abs(want) : std::abs(got - want);
  tally(fam, std::isfinite(got) && rel <= band, false, rel, where);
}

// ---- request mirror (fixed decoder-pinned scalars per the contract) -------

kalos::X13AutomaticRequest mirror_request(const golden_x13auto::Case& c) {
  kalos::X13AutomaticRequest r;
  r.input.reserve(static_cast<std::size_t>(c.n));
  for (int i = 0; i < c.n; ++i) r.input.push_back(rdbits(c.values[i]));
  r.frequency = 12;
  r.first_year = c.first_year;
  r.first_period = c.first_period;
  r.mode = static_cast<kalos::X11DecompositionMode>(c.mode);
  r.aicc_log_correction = rdbits(c.aicc_log_correction);
  r.max_d = 2;
  r.max_bd = 1;
  r.ub1 = rdbits(c.ub1);   // module-space reciprocals, raw bits
  r.ub2 = rdbits(c.ub2);
  r.cancel = rdbits(c.cancel);
  r.differencing_precision = 1e-7;
  r.models_count = 5;
  r.max_p = 2;
  r.max_q = 2;
  r.max_bp = 1;
  r.max_bq = 1;
  r.balanced = c.balanced != 0;
  r.mixed = c.mixed != 0;
  r.arma_accept_white_noise = false;
  r.arma_eps = 1e-7;
  r.outliers_used = c.outliers_used != 0;
  for (int i = 0; i < c.n_types; ++i)
    r.outlier_types.push_back(static_cast<kalos::OutlierType>(c.otypes[i]));
  r.outlier_max_round = static_cast<std::size_t>(c.maxiter);
  r.outlier_max_outliers = 30;
  r.tcrate = rdbits(c.tcrate);
  r.precision = 1e-7;
  r.mean_precision = 1e-5;
  r.va = 0.;
  r.reduce_va = rdbits(c.reduce_va);
  r.ljung_box_limit = rdbits(c.ljung_box_limit);
  r.check_mu = true;
  r.accept_airline = false;
  r.ur_limit = 0.95;
  r.mean_test_initial = 1.96;
  r.mean_test_final = 1.0;
  r.tsig = 1.0;
  r.seasonal.assign(12, kalos::X11SeasonalRequest::msr);
  r.trend_length = 0;
  r.sigma = kalos::X11SigmaPolicy::accurate;
  r.lower_sigma = rdbits(c.lower_sigma);
  r.upper_sigma = rdbits(c.upper_sigma);
  r.backcast = 0;
  r.forecast = 0;
  return r;
}

const char* mode_name(kalos::X11DecompositionMode m) {
  using M = kalos::X11DecompositionMode;
  switch (m) {
    case M::additive: return "Additive";
    case M::multiplicative: return "Multiplicative";
    case M::log_additive: return "LogAdditive";
    case M::pseudo_additive: return "PseudoAdditive";
  }
  return "?";
}

// ---- driver-side chain helpers ---------------------------------------------

const kalos::AutoDecisionRecord* stage_entry(
    const std::vector<kalos::AutoDecisionRecord>& chain, const std::string& stage,
    std::size_t occurrence) {
  std::size_t seen = 0;
  for (const auto& e : chain) {
    if (e.stage == stage && seen++ == occurrence) return &e;
  }
  return nullptr;
}

std::vector<const kalos::AutoDecisionRecord*> stage_suffix(
    const std::vector<kalos::AutoDecisionRecord>& chain, const std::string& suffix) {
  std::vector<const kalos::AutoDecisionRecord*> out;
  for (const auto& e : chain) {
    if (e.stage.size() >= suffix.size() &&
        e.stage.compare(e.stage.size() - suffix.size(), suffix.size(), suffix) == 0)
      out.push_back(&e);
  }
  return out;
}

double entry_scalar(const kalos::AutoDecisionRecord& e, const char* key) {
  for (const auto& s : e.scalars)
    if (s.first == key) return s.second;
  return std::numeric_limits<double>::quiet_NaN();
}

// ---- the per-case oracle replay ---------------------------------------------

constexpr double kTmeanBand = 5e-3;     // 21-D trial-fit-class band, re-measured
                                        // under CI-FDLIBM (2026-11-21): with the
                                        // transcendentals pinned to the JVM's
                                        // bytecode semantics and the landing on
                                        // the pinned LM machine the trial-fit
                                        // t-stat rides, the pinned corpus-era
                                        // 7.4e-4 (local glibc trajectory) moved
                                        // to 3.1e-3 (multi/diff0) deterministically
                                        // (bit-noise shims and the -AVX2_FMA
                                        // hwcaps mask are no-ops); decision bits
                                        // untouched.
constexpr double kPanelBicBand = 1e-2;  // panel BIC2 at the automatic landing
                                        // (measured 8.1e-3 worst, 21-H.3: the
                                        // trial fits run on the ODM-altered
                                        // design; membership stays bit-exact)
constexpr double kDisplayBand = 1e-5;   // observer %.6g AICC display
constexpr double kBasinBand = 2e-3;     // refinement-basin landing quantities
constexpr double kCoeffBasinBand = 1e-2;
// ARMA landing coefficients: measured 9.8e-3 worst (MA0, ao_nodet) - the
// row-4 landing basin depth from the driver's start values (G2 seam), 21-H.3.
constexpr double kTableBand = 1e-6;     // downstream tables: measured 5.0e-7
                                        // worst (multi/out), landing-drift
                                        // propagation through the X-11
                                        // composition (21-H.3)

using golden_x13auto::Case;
using golden_x13auto::Ev;

void run_case(const Case& c) {
  const std::string where = c.id;
  const std::vector<Ev> evs(c.evs, c.evs + c.n_evs);
  std::printf("== %s\n", c.id);
  const kalos::X13AutomaticRequest request = mirror_request(c);
  const kalos::X13AutomaticFitResult result = kalos::x13_automatic_fit(request);

  // ---- status and final description: bit ----------------------------------
  bit_s("model-facts", result.status, "available", where + "/status");
  if (result.status != "available") {
    std::printf("   status %s\n", result.status.c_str());
    return;
  }
  bit_i("model-facts", result.log ? 1 : 0, c.exp_log, where + "/log");
  bit_i("model-facts", result.mean ? 1 : 0, c.exp_mean, where + "/mean");
  bit_i("model-facts", result.p, c.exp_p, where + "/p");
  bit_i("model-facts", result.d, c.exp_d, where + "/d");
  bit_i("model-facts", result.q, c.exp_q, where + "/q");
  bit_i("model-facts", result.bp, c.exp_bp, where + "/bp");
  bit_i("model-facts", result.bd, c.exp_bd, where + "/bd");
  bit_i("model-facts", result.bq, c.exp_bq, where + "/bq");
  bit_s("model-facts", mode_name(result.fit.mode_effective),
        mode_name(static_cast<kalos::X11DecompositionMode>(c.exp_mode)),
        where + "/mode_effective");

  // ---- variables: identity bit, landing coefficients banded ----------------
  {
    bit_i("model-facts", static_cast<long long>(result.variables.size()),
          c.exp_nvars, where + "/nvars");
    struct Want {
      int position, type;
      std::string name, regeffect;
      int row;
    };
    std::vector<Want> want;
    for (int i = 0; i < c.exp_nvars; ++i) {
      Want w;
      w.position = c.var_positions[i];
      const std::string vname = c.var_names[i];
      w.type = vname.substr(0, 2) == "AO" ? 0
               : vname.substr(0, 2) == "LS" ? 1
               : vname.substr(0, 2) == "TC" ? 2 : 3;
      w.name = vname;
      w.regeffect = c.var_regeffects[i] == std::string("-") ? "" : c.var_regeffects[i];
      w.row = c.var_rows[i];
      want.push_back(w);
    }
    std::stable_sort(want.begin(), want.end(), [](const Want& a, const Want& b) {
      return std::make_pair(a.position, a.type) < std::make_pair(b.position, b.type);
    });
    std::vector<kalos::AutoVariableRecord> got = result.variables;
    std::stable_sort(got.begin(), got.end(),
                     [](const kalos::AutoVariableRecord& a,
                        const kalos::AutoVariableRecord& b) {
                       return std::make_pair(a.position, static_cast<int>(a.type)) <
                              std::make_pair(b.position, static_cast<int>(b.type));
                     });
    for (std::size_t i = 0; i < got.size() && i < want.size(); ++i) {
      const std::string tag = where + "/var" + std::to_string(i);
      bit_i("variables", static_cast<long long>(got[i].position), want[i].position,
            tag + "/position");
      bit_i("variables", static_cast<long long>(got[i].type), want[i].type,
            tag + "/type");
      bit_s("variables", got[i].name, want[i].name, tag + "/name");
      bit_s("variables", got[i].regeffect, want[i].regeffect, tag + "/regeffect");
      // The regcoef vector is the JVM final-design list; the row index
      // carries the const slot (row 0 when the mean is on).
      if (want[i].row >= 0 && want[i].row < c.n_regcoef)
        close_d("landing-coefficients", got[i].coefficient,
                rdbits(c.regcoef[want[i].row]), kBasinBand, tag + "/coef");
    }
    std::vector<double> flat;
    for (const auto& block : result.coefficients)
      flat.insert(flat.end(), block.begin(), block.end());
    bit_i("model-facts", static_cast<long long>(flat.size()), c.n_arma,
          where + "/narma");
    for (std::size_t i = 0; i < flat.size() && static_cast<int>(i) < c.n_arma; ++i)
      close_d("landing-coefficients", flat[i], rdbits(c.arma[i]), kCoeffBasinBand,
              where + "/arma" + std::to_string(i));
  }

  // ---- landing statistics ---------------------------------------------------
  bit_i("landing-stats", static_cast<long long>(result.effective_observations),
        c.neff, where + "/neff");
  close_d("landing-stats", result.log_likelihood, rdbits(c.loglik), kBasinBand,
          where + "/loglik");
  close_d("landing-stats", result.aicc, rdbits(c.aicc), kBasinBand,
          where + "/aicc");

  // ---- decision-chain projection (golden events in observer order) ----------
  const auto& chain = result.chain;

  // kind 1: the log-level test, %.6g display pair + preference scalar.
  {
    std::vector<const Ev*> aicc;
    for (const Ev& e : evs)
      if (e.kind == 1) aicc.push_back(&e);
    const auto trans = stage_suffix(chain, "transformation");
    bit_i("chain-counts", static_cast<long long>(aicc.size()),
          static_cast<long long>(trans.size()), where + "/aicc-count");
    for (std::size_t i = 0; i < aicc.size() && i < trans.size(); ++i) {
      const Ev& e = *aicc[i];
      const std::string tag = where + "/aicc" + std::to_string(i);
      if (!trans[i]->transformation) {
        bit_i("display-aicc", -1, 0, tag + "/payload");
        continue;
      }
      const kalos::TransformationDecisionResult& t = *trans[i]->transformation;
      close_d("display-aicc", t.aicc_log, rdbits(e.d0), kDisplayBand,
              tag + "/logs");
      close_d("display-aicc", t.aicc_level, rdbits(e.d1), kDisplayBand,
              tag + "/levels");
      bit_d("chain-scalars", rdbits(e.d2), request.aicc_log_correction,
            tag + "/pref");
    }
  }

  // kinds 2/3: differencing and arma selection, one pair per refine entry.
  {
    std::vector<const Ev*> diffs, armas;
    for (const Ev& e : evs) {
      if (e.kind == 2) diffs.push_back(&e);
      if (e.kind == 3) armas.push_back(&e);
    }
    const auto refines = stage_suffix(chain, ".refine");
    bit_i("chain-counts", static_cast<long long>(diffs.size()),
          static_cast<long long>(refines.size()), where + "/diff-count");
    bit_i("chain-counts", static_cast<long long>(armas.size()),
          static_cast<long long>(refines.size()), where + "/arma-count");
    for (std::size_t i = 0; i < diffs.size() && i < refines.size(); ++i) {
      const Ev& e = *diffs[i];
      const std::string tag = where + "/diff" + std::to_string(i);
      if (!refines[i]->refine || !refines[i]->refine->differencing) {
        bit_i("diff-decisions", -1, 0, tag + "/payload");
        continue;
      }
      const kalos::DifferencingDecision& got = *refines[i]->refine->differencing;
      bit_i("diff-decisions", got.d, e.i0, tag + "/d");
      bit_i("diff-decisions", got.seasonal_d, e.i1, tag + "/bd");
      bit_i("diff-decisions", got.mean_correction ? 1 : 0, e.i2, tag + "/mean");
      close_d("diff-tmean", got.t_mean, rdbits(e.d0), kTmeanBand, tag + "/tmean");
    }
    for (std::size_t i = 0; i < armas.size() && i < refines.size(); ++i) {
      const Ev& e = *armas[i];
      const std::string tag = where + "/arma" + std::to_string(i);
      if (!refines[i]->refine) {
        bit_i("arma-decisions", -1, 0, tag + "/payload");
        continue;
      }
      const kalos::ArmaRefinement& got = *refines[i]->refine;
      bit_i("arma-decisions", got.p, e.i0, tag + "/sel-p");
      bit_i("arma-decisions", got.q, e.i1, tag + "/sel-q");
      bit_i("arma-decisions", got.bp, e.i2, tag + "/sel-bp");
      bit_i("arma-decisions", got.bq, e.i3, tag + "/sel-bq");
      bit_i("arma-panel", static_cast<long long>(got.preferred_models.size()),
            e.n_panel, tag + "/panel-size");
      for (int j = 0; j < e.n_panel &&
                      j < static_cast<int>(got.preferred_models.size()); ++j) {
        const std::string ptag = tag + "/panel" + std::to_string(j);
        const kalos::ArmaPreferredModel& m = got.preferred_models[j];
        bit_i("arma-panel", m.p, e.panel[j].p, ptag + "/p");
        bit_i("arma-panel", m.q, e.panel[j].q, ptag + "/q");
        bit_i("arma-panel", m.bp, e.panel[j].bp, ptag + "/bp");
        bit_i("arma-panel", m.bq, e.panel[j].bq, ptag + "/bq");
        close_d("arma-panel", m.bic, rdbits(e.panel[j].bic), kPanelBicBand,
                ptag + "/bic");
      }
    }
  }

  // kind 4: ODM rounds, one entry per JVM ODM Info.
  {
    std::vector<const Ev*> odms;
    for (const Ev& e : evs)
      if (e.kind == 4) odms.push_back(&e);
    std::vector<const kalos::AutoDecisionRecord*> entries;
    for (std::size_t o = 0;; ++o) {
      const auto* e = stage_entry(chain, "outliers0", o);
      if (e == nullptr) break;
      entries.push_back(e);
    }
    for (int round = 1;; ++round) {
      const std::string stage = "round" + std::to_string(round) + ".outliers";
      bool any = false;
      for (std::size_t o = 0;; ++o) {
        const auto* e = stage_entry(chain, stage, o);
        if (e == nullptr) break;
        entries.push_back(e);
        any = true;
      }
      if (!any) break;
    }
    bit_i("chain-counts", static_cast<long long>(odms.size()),
          static_cast<long long>(entries.size()), where + "/odm-count");
    for (std::size_t i = 0; i < odms.size() && i < entries.size(); ++i) {
      const Ev& e = *odms[i];
      const std::string tag = where + "/odm" + std::to_string(i);
      if (!entries[i]->outliers) {
        bit_i("odm-decisions", -1, 0, tag + "/payload");
        continue;
      }
      std::vector<std::pair<int, int>> got;
      for (const auto& o : entries[i]->outliers->outliers)
        got.emplace_back(static_cast<int>(o.position), static_cast<int>(o.type));
      std::sort(got.begin(), got.end());
      std::vector<std::pair<int, int>> want;
      for (int j = 0; j < e.n_picks; ++j)
        want.emplace_back(e.picks[j].position, e.picks[j].type);
      std::sort(want.begin(), want.end());
      bit_i("odm-decisions", static_cast<long long>(got.size()),
            static_cast<long long>(want.size()), tag + "/count");
      for (std::size_t j = 0; j < got.size() && j < want.size(); ++j) {
        bit_i("odm-decisions", got[j].first, want[j].first, tag + "/pos");
        bit_i("odm-decisions", got[j].second, want[j].second, tag + "/type");
      }
      // The cv the JVM ODM Info carries is the module's incoming threshold:
      // the driver's entry scalar. Bit-identical arithmetic on both sides.
      bit_d("odm-decisions", entry_scalar(*entries[i], "cv"), rdbits(e.d0),
            tag + "/cv");
    }
  }

  // kind 5: the cascade reduced-spec remark equals the final orders.
  for (int i = 0, k = 0; k < c.n_evs; ++k) {
    const Ev& e = c.evs[k];
    if (e.kind != 5) continue;
    const std::string tag = where + "/cascade" + std::to_string(i++);
    bit_i("model-facts", result.p, e.i0, tag + "/p");
    bit_i("model-facts", result.d, e.i1, tag + "/d");
    bit_i("model-facts", result.q, e.i2, tag + "/q");
    bit_i("model-facts", result.bp, e.i3, tag + "/bp");
    bit_i("model-facts", result.bd, static_cast<long long>(e.d0), tag + "/bd");
    bit_i("model-facts", result.bq, static_cast<long long>(e.d1), tag + "/bq");
  }

  // ---- downstream composition tables ----------------------------------------
  {
    const std::pair<const char*, const std::vector<double>*> tables[] = {
        {"out", &result.fit.preadjustment.out},
        {"d11final", &result.fit.finals.d11final},
        {"e11", &result.fit.finals.e11}};
    const std::uint64_t* wants[] = {c.out, c.d11final, c.e11};
    for (std::size_t t = 0; t < 3; ++t) {
      const std::vector<double>& got = *tables[t].second;
      bit_i("tables", static_cast<long long>(got.size()), c.n,
            where + "/" + tables[t].first + "/length");
      for (int i = 0; i < c.n && static_cast<int>(got.size()) > i; ++i)
        close_d("tables", got[i], rdbits(wants[t][i]), kTableBand,
                where + "/" + tables[t].first + "[" + std::to_string(i) + "]");
    }
  }
  std::printf("   ok\n");
}

}  // namespace

int main(int argc, char** argv) {
  std::vector<std::string> only(argv + 1, argv + argc);
  std::size_t count = 0;
  const golden_x13auto::Case* cases = golden_x13auto::table(count);
  int ran = 0;
  const auto pseudo = nonlog_pseudo::cases(KALOS_PSEUDO_AUTO);
  for (std::size_t i = 0; i < count; ++i) {
    if (!only.empty() &&
        std::find(only.begin(), only.end(), std::string(cases[i].id)) == only.end())
      continue;
    ++ran;
    run_case(cases[i]);
    for (const auto& ref : pseudo) {
      if (ref.id != cases[i].id) continue;
      auto request = mirror_request(cases[i]);
      if (request.input != ref.rows.at("input")) throw std::runtime_error("pseudo-additive input mismatch");
      request.mode = kalos::X11DecompositionMode::pseudo_additive;
      nonlog_pseudo::check(ref, kalos::x13_automatic_fit(request).fit, kTableBand);
    }
  }
  if (ran == 0) {
    std::printf("no golden case matched the filter\n");
    return 1;
  }
  std::printf("\n== families ==\n%-24s %8s %8s %8s %10s  %s\n", "family", "checks",
              "bad", "exact", "worst-rel", "worst at");
  long long bad = 0, checks = 0;
  for (const auto& f : g_families) {
    std::printf("%-24s %8lld %8lld %8lld %10.3g  %s\n", f.first.c_str(),
                f.second.checks, f.second.bad, f.second.exact, f.second.worst,
                f.second.worst_where.c_str());
    bad += f.second.bad;
    checks += f.second.checks;
  }
  std::printf("\ncases %d, checks %lld, failures %d : %s\n", ran, checks,
              g_failures, bad == 0 && g_failures == 0 ? "PASS" : "FAIL");
  return bad == 0 && g_failures == 0 ? 0 : 1;
}
