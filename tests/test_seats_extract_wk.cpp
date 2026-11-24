// Item 24-C.1: replay the 36-case seats-wk-extraction corpus through
// kalos::seats_extract_wk. Six envelope refusals (the quarterly series, the
// two KalmanSmoother cases, innovation variance 0, the classical airline and
// the fixMa crash) must throw std::invalid_argument with the pinned reason;
// every other case replays its status/dom/init/final fact lines EXACTLY -
// line for line, double for double (%.17g round-trips through strtod, so the
// captured text is the comparison itself).
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#include "jvm_seats_wk_extraction.h"
#include "kalos/seats_extract_wk.hpp"
#include "tramo_seats_model_replay.h"

namespace {

std::size_t g_checks = 0;
std::size_t g_failures = 0;
std::vector<std::string> g_problems;
bool g_verbose = false;

void fail(const std::string& id, const std::string& what) {
  ++g_failures;
  g_problems.push_back(id + ": " + what);
}

std::vector<double> parse_values(const char* s) {
  std::vector<double> v;
  std::string line(s);
  std::size_t i = 0;
  while (i < line.size()) {
    while (i < line.size() && line[i] == ' ') ++i;
    if (i >= line.size()) break;
    char* end = nullptr;
    v.push_back(std::strtod(line.c_str() + i, &end));
    if (end == line.c_str() + i) break;
    i = static_cast<std::size_t>(end - line.c_str());
  }
  return v;
}

std::size_t component_rows = 0, component_values = 0;

void check_components(const golden_swk::Case& c, const std::string& stage,
                      const std::vector<kalos::seats_component_series>& rows) {
  constexpr std::array<const char*, 6> types = {
      "Series", "Trend", "Seasonal", "SeasonallyAdjusted", "Irregular", "CalendarEffect"};
  constexpr std::array<const char*, 6> kinds = {
      "Value", "Stdev", "Forecast", "StdevForecast", "Backcast", "StdevBackcast"};
  std::size_t index = 0;
  for (std::size_t j = 0; j < c.nfacts; ++j) {
    const std::string line(c.facts[j]);
    if (line.compare(0, stage.size() + 1, stage + " ") != 0) continue;
    if (index == rows.size()) {
      fail(c.id, stage + " missing component row");
      return;
    }
    const auto& row = rows[index++];
    std::istringstream in(line);
    std::string token, type, kind;
    std::size_t length;
    std::int64_t year;
    int position;
    if (!(in >> token >> type >> kind >> token >> length >> token >> year >> position >> token)
        || token != "vals"
        || type != types.at(static_cast<std::size_t>(row.component))
        || kind != kinds.at(static_cast<std::size_t>(row.information))
        || row.start_year != year || row.start_month != position + 1
        || row.values.size() != length) {
      fail(c.id, stage + " component key, domain or length differs: " + type + " " + kind);
      return;
    }
    std::string payload;
    std::getline(in, payload);
    const auto expected = parse_values(payload.c_str());
    if (expected.size() != length ||
        (length && std::memcmp(expected.data(), row.values.data(), length * sizeof(double)))) {
      fail(c.id, stage + " component values differ: " + type + " " + kind);
      return;
    }
    ++component_rows;
    component_values += length;
  }
  if (index != rows.size()) fail(c.id, stage + " has unexpected component rows");
}

}  // namespace

int main(int argc, char** argv) {
  g_verbose = argc > 1 && std::strcmp(argv[1], "-v") == 0;
  std::size_t n = 0;
  const golden_swk::Case* cs = golden_swk::table(n);
  for (std::size_t i = 0; i < n; ++i) {
    const golden_swk::Case& c = cs[i];
    kalos::tramo_seats_model_request req;
    std::string err;
    const golden_tsm::Case proxy{c.id,  "",  "", 0, 0, c.nreq, c.req,
                                 0,     nullptr, 0, nullptr};
    if (!replay_tsm::parse_request(proxy, req, err)) {
      fail(c.id, "request parse: " + err);
      continue;
    }
    ++g_checks;
    if (static_cast<int>(req.frequency) != c.freq) {
      fail(c.id, "golden freq disagrees with the req spec echo");
      continue;
    }
    // The 24-C knobs tail (forecastCount backcastCount method bias) is not
    // part of the 24-A echo grammar; read it from the captured knobs line.
    for (std::size_t j = 0; j < c.nreq; ++j) {
      const std::string ln = c.req[j];
      if (ln.compare(0, 10, "req knobs ") != 0) continue;
      const std::vector<std::string> t = replay_tsm::tokens(ln);
      if (t.size() != 12) {
        fail(c.id, "bad knobs tail");
        break;
      }
      req.forecast_count = std::atoi(t[8].c_str());
      req.backcast_count = std::atoi(t[9].c_str());
      req.components_method = t[10] == "Burman"
                                  ? kalos::seats_components_method::burman
                                  : kalos::seats_components_method::kalman_smoother;
      req.bias = t[11] == "Legacy" ? kalos::seats_bias_correction::legacy
                                   : kalos::seats_bias_correction::none;
    }
    req.series = parse_values(c.values);
    {
      const std::vector<std::string> st = replay_tsm::tokens(c.start);
      if (st.size() != 2) {
        fail(c.id, "bad start");
        continue;
      }
      req.start_year = std::atoi(st[0].c_str());
      req.start_month = std::atoi(st[1].c_str());
    }

    if (*c.reason != '\0') {
      std::string msg = "NO THROW";
      bool invalid = false;
      try {
        kalos::seats_extract_wk(req);
      } catch (const std::invalid_argument& e) {
        invalid = true;
        msg = e.what();
      } catch (const std::exception& e) {
        msg = std::string("wrong type: ") + e.what();
      }
      ++g_checks;
      if (!invalid || msg != c.reason) fail(c.id, "refusal: got [" + msg + "]");
      continue;
    }

    kalos::seats_extract_wk_result result;
    try {
      result = kalos::seats_extract_wk(req);
    } catch (const std::invalid_argument& e) {
      fail(c.id, std::string("unexpected refusal: ") + e.what());
      continue;
    } catch (const std::exception& e) {
      fail(c.id, std::string("escaped exception: ") + e.what());
      continue;
    }
    const auto& got = result.facts();
    check_components(c, "init", result.initial_components());
    check_components(c, "final", result.final_components());
    if (std::string(c.id) == "x_bc1") {
      // An actual January start and a one-month backcast straddle year zero.
      req.start_year = 0;
      req.start_month = 1;
      req.backcast_count = 1;
      const auto shifted = kalos::seats_extract_wk(req);
      if (shifted.fact("dom backcast") != "n 1 start -1 11")
        fail(c.id, "backcast domain before year zero");
      for (const auto& row : shifted.final_components()) {
        if (row.information == kalos::seats_information::backcast &&
            (row.start_year != -1 || row.start_month != 12))
          fail(c.id, "backcast component before year zero");
      }
    }
    if (got.size() != c.nfacts) {
      fail(c.id, "fact count: got " + std::to_string(got.size()) + ", want " +
                     std::to_string(c.nfacts));
    }
    std::size_t bad = 0;
    const std::size_t m = got.size() < c.nfacts ? got.size() : c.nfacts;
    for (std::size_t j = 0; j < m; ++j) {
      ++g_checks;
      if (got[j] == c.facts[j]) continue;
      ++bad;
      if (bad <= 3) {
        std::string w = "fact[" + std::to_string(j) + "]";
        if (got[j].substr(0, 40) != std::string(c.facts[j]).substr(0, 40)) {
          w += " got [" + got[j].substr(0, 60) + "]";
          w += " want [" + std::string(c.facts[j]).substr(0, 60) + "]";
        } else {
          // Same prefix: locate the first differing value cheaply.
          std::size_t k = 0;
          while (k < got[j].size() && k < std::string(c.facts[j]).size() &&
                 got[j][k] == c.facts[j][k])
            ++k;
          w += " differs at char " + std::to_string(k) + " got [" +
               got[j].substr(k, 40) + "] want [" +
               std::string(c.facts[j]).substr(k, 40) + "]";
        }
        fail(c.id, w);
      }
    }
    g_checks += (c.nfacts > m ? c.nfacts - m : 0);
    if (bad > 3) fail(c.id, "... " + std::to_string(bad) + " differing lines");
    if (g_verbose)
      std::printf("%-16s %zu facts %s\n", c.id, got.size(),
                  bad == 0 && got.size() == c.nfacts ? "ok" : "FAILED");
  }
  std::printf("test_seats_extract_wk: %zu checks, %zu failures, %zu cases\n",
              g_checks, g_failures, n);
  std::printf("structured components: %zu JVM rows, %zu binary64 values\n",
              component_rows, component_values);
  for (std::size_t i = 0; i < g_problems.size() && i < 40; ++i)
    std::printf("  %s\n", g_problems[i].c_str());
  return g_failures == 0 ? 0 : 1;
}
