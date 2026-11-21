// Item 24-A replay: drives kalos::validate_tramo_seats_model from the
// captured golden (tests/jvm_tramo_seats_model.h) and checks every 24-A
// assertable fact against the unit's result. Doubles are compared bit for
// bit: the %.17g capture round-trips through strtod exactly, so equality on
// the re-parsed values is equality on the captured IEEE-754 payloads (a
// formatter-independent comparison, stronger than rendering both sides).
//
// The kevt comparison is EXACT for every case whose live run stayed inside
// this unit's scope and exact-PREFIX for the Legacy approximation cases
// (legacy_approx: the live kernel continued into DefaultModelApproximator,
// register D6, which the unit never re-implements).
#ifndef KALOS_TESTS_TRAMO_SEATS_MODEL_REPLAY_H
#define KALOS_TESTS_TRAMO_SEATS_MODEL_REPLAY_H

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#include "jvm_tramo_seats_model.h"
#include "kalos/tramo_seats_model_validator.hpp"

namespace replay_tsm {

inline std::uint64_t bits(double v) {
  std::uint64_t u;
  std::memcpy(&u, &v, sizeof(u));
  return u;
}

inline std::vector<std::string> tokens(const std::string& line) {
  std::vector<std::string> out;
  std::istringstream is(line);
  std::string w;
  while (is >> w) out.push_back(w);
  return out;
}

inline std::vector<double> num_list(const std::vector<std::string>& toks,
                                    std::size_t from) {
  std::vector<double> v;
  for (std::size_t i = from; i < toks.size(); ++i) v.push_back(std::strtod(toks[i].c_str(), nullptr));
  return v;
}

struct RequestError {
  std::string msg;
};

inline bool parse_request(const golden_tsm::Case& c, kalos::tramo_seats_model_request& r,
                          std::string& err) {
  std::size_t p_ = 0, q_ = 0, bp_ = 0, bq_ = 0;
  for (std::size_t i = 0; i < c.nreq; ++i) {
    std::vector<std::string> t = tokens(c.req[i]);  // t[0] == "req"
    if (t.size() < 2) {
      err = std::string("short req line in ") + c.id;
      return false;
    }
    const std::string& k = t[1];
    if (k == "spec") {  // p d q bp bd bq period (orders == vector lengths)
      if (t.size() != 9) {
        err = "bad spec line";
        return false;
      }
      r.period = std::atoi(t[8].c_str());
      r.frequency = r.period;  // the corpus always pairs them (12/12, 4/4)
      r.d = std::atoi(t[3].c_str());
      r.bd = std::atoi(t[6].c_str());
      p_ = std::atoi(t[2].c_str());
      q_ = std::atoi(t[4].c_str());
      bp_ = std::atoi(t[5].c_str());
      bq_ = std::atoi(t[7].c_str());
    } else if (k == "phi") {
      r.phi = num_list(t, 2);
    } else if (k == "bphi") {
      r.bphi = num_list(t, 2);
    } else if (k == "theta") {
      r.theta = num_list(t, 2);
    } else if (k == "btheta") {
      r.btheta = num_list(t, 2);
    } else if (k == "log") {  // log L mean M var V
      if (t.size() != 7) {
        err = "bad log line";
        return false;
      }
      r.log = std::atoi(t[2].c_str()) != 0;
      r.mean_correction = std::atoi(t[4].c_str()) != 0;
      r.innovation_variance = std::strtod(t[6].c_str(), nullptr);
    } else if (k == "knobs") {  // xl epsphi rmod smod smodPi mode ...
      if (t.size() < 8) {
        err = "bad knobs line";
        return false;
      }
      r.xl = std::strtod(t[2].c_str(), nullptr);
      r.epsphi = std::strtod(t[3].c_str(), nullptr);
      r.rmod = std::strtod(t[4].c_str(), nullptr);
      r.smod = std::strtod(t[5].c_str(), nullptr);
      r.smod_pi = std::strtod(t[6].c_str(), nullptr);
      const std::string m = t[7];
      if (m == "None")
        r.mode = kalos::seats_approx_mode::none;
      else if (m == "Legacy")
        r.mode = kalos::seats_approx_mode::legacy;
      else if (m == "Noisy")
        r.mode = kalos::seats_approx_mode::noisy;
      else {
        err = "unknown mode " + m;
        return false;
      }
    }
  }
  if (p_ != r.phi.size() || bp_ != r.bphi.size() || q_ != r.theta.size() ||
      bq_ != r.btheta.size()) {
    err = "spec orders disagree with the parameter vectors";
    return false;
  }
  return true;
}

struct Checker {
  const golden_tsm::Case& c;
  const kalos::tramo_seats_model_result& r;
  int checks = 0;
  int failures = 0;
  std::vector<std::string>* log = nullptr;

  void fail(const std::string& what, const std::string& want,
            const std::string& got) {
    ++failures;
    if (log != nullptr) {
      std::ostringstream os;
      os << c.id << ": " << what << " want [" << want << "] got [" << got << "]";
      log->push_back(os.str());
    }
  }

  static std::string hex_list(const std::vector<double>& v) {
    std::ostringstream os;
    for (std::size_t i = 0; i < v.size(); ++i) os << std::hex << bits(v[i]) << " ";
    return os.str();
  }

  bool eq_bits(const char* what, const std::string& want_raw, const std::vector<double>& got,
               std::size_t from) {
    std::vector<double> want = num_list(tokens(want_raw), from);
    ++checks;
    if (want.size() != got.size()) {
      fail(what, want_raw, hex_list(got));
      return false;
    }
    for (std::size_t i = 0; i < want.size(); ++i) {
      if (bits(want[i]) != bits(got[i])) {
        fail(what, want_raw, hex_list(got));
        return false;
      }
    }
    return true;
  }

  void eq_str(const char* what, const std::string& want, const std::string& got) {
    ++checks;
    if (want != got) fail(what, want, got);
  }
};

inline void check_facts(Checker& k) {
  const kalos::tramo_seats_model_result& r = k.r;
  for (std::size_t i = 0; i < k.c.nfacts; ++i) {
    const std::string line = k.c.facts[i];
    const std::string key = line.substr(0, line.find(' '));
    if (key == "v.changed") {
      k.eq_str("v.changed", line.substr(10), r.parameters_cut_off ? "1" : "0");
    } else if (key == "v.xl") {
      std::vector<std::string> t = tokens(line);
      ++k.checks;
      if (bits(std::strtod(t[1].c_str(), nullptr)) != bits(r.xl))
        k.fail("v.xl", t[1], "unit xl differs");
    } else if (key == "v.orders") {
      std::ostringstream os;
      os << r.orders_p << " " << r.orders_d << " " << r.orders_q << " " << r.orders_bp
         << " " << r.orders_bd << " " << r.orders_bq;
      std::string want = line.substr(9);  // "v.orders " prefix
      while (!want.empty() && want[want.size() - 1] == ' ') want.erase(want.size() - 1);
      k.eq_str("v.orders", want, os.str());
    } else if (key == "v.phi") {
      k.eq_bits("v.phi", line, r.phi, 1);
    } else if (key == "v.bphi") {
      k.eq_bits("v.bphi", line, r.bphi, 1);
    } else if (key == "v.theta") {
      k.eq_bits("v.theta", line, r.theta, 1);
    } else if (key == "v.btheta") {
      k.eq_bits("v.btheta", line, r.btheta, 1);
    } else if (key == "r.decompose") {
      k.eq_str("r.decompose", line.substr(12), r.decomposition_threw ? "null" : "ok");
    } else if (key == "r.count") {
      std::ostringstream os;
      os << r.raw_components.size();
      std::string want = line.substr(8);  // "r.count " prefix
      while (!want.empty() && want[want.size() - 1] == ' ') want.erase(want.size() - 1);
      k.eq_str("r.count", want, os.str());
    } else if (key == "d.decompose") {
      k.eq_str("d.decompose", line.substr(12), r.variance_max_ok ? "ok" : "null");
    } else if (key == "k.cutOff") {
      k.eq_str("k.cutOff", line.substr(9), r.parameters_cut_off ? "1" : "0");
    } else if (key == "k.modelChanged") {
      // Record-only for the Legacy approximation cases: the captured 1 comes
      // from the D6 continuation (SeatsModel.setModelChanged after
      // approximate()), which this unit deliberately does not model.
      if (k.c.legacy_approx != 0) continue;
      k.eq_str("k.modelChanged", line.substr(15), r.model_changed ? "1" : "0");
    } else if (key == "r") {
      // "r c{i} <sar|ur|sfd> ..."
      std::vector<std::string> t = tokens(line);
      if (t.size() < 3) {
        ++k.checks;
        k.fail("r", line, "malformed");
        continue;
      }
      std::size_t idx = static_cast<std::size_t>(std::atoi(t[1].c_str() + 1));
      ++k.checks;
      if (idx >= r.raw_components.size()) {
        k.fail("r", line, "component missing");
        continue;
      }
      const kalos::seats_raw_component& comp = r.raw_components[idx];
      if (t[2] == "sar")
        k.eq_bits(line.c_str(), line, comp.sar, 3);
      else if (t[2] == "ur")
        k.eq_bits(line.c_str(), line, comp.ur, 3);
      else if (t[2] == "sfd")
        k.eq_bits(line.c_str(), line, comp.sfd, 3);
    } else {
      ++k.checks;
      k.fail("unknown fact key", line, "");
    }
  }
}

inline void check_flags_and_events(Checker& k) {
  const golden_tsm::Case& c = k.c;
  const kalos::tramo_seats_model_result& r = k.r;
  const std::string status = c.status;
  ++k.checks;
  if (r.seats_exception != (status.find("SeatsException") != std::string::npos))
    k.fail("seats_exception", status, r.seats_exception ? "1" : "0");
  ++k.checks;
  if (r.approximation_required != (c.legacy_approx != 0))
    k.fail("approximation_required", c.legacy_approx ? "1" : "0",
           r.approximation_required ? "1" : "0");
  // The noisy warning rides the golden event stream (its capture never
  // reached the k.* facts); the unit must agree with the kernel frame.
  bool golden_noisy = false;
  for (std::size_t i = 0; i < c.nevents; ++i)
    if (std::strstr(c.events[i], "warning 2 noisy model used") != nullptr) golden_noisy = true;
  ++k.checks;
  if (r.noisy_model_used != golden_noisy)
    k.fail("noisy_model_used", golden_noisy ? "1" : "0", r.noisy_model_used ? "1" : "0");
  ++k.checks;
  // Outside the D6 continuation the kernel only flags the model changed when
  // setVarianceMax replaced it (the noisy branch), exactly the warning above.
  if (c.legacy_approx == 0 && r.model_changed != golden_noisy)
    k.fail("model_changed", golden_noisy ? "1" : "0", r.model_changed ? "1" : "0");
  // Kernel frame: exact for every in-scope run, exact-prefix for the Legacy
  // approximation cases (the live continuation is D6 territory).
  std::vector<std::string> mine;
  for (std::size_t i = 0; i < r.events.size(); ++i) {
    std::ostringstream os;
    os << r.events[i].kind << " " << r.events[i].depth << " " << r.events[i].msg;
    mine.push_back(os.str());
  }
  ++k.checks;
  bool ok = mine.size() <= c.nevents;
  for (std::size_t i = 0; ok && i < mine.size(); ++i)
    if (mine[i] != c.events[i]) ok = false;
  if (c.legacy_approx == 0 && mine.size() != c.nevents) ok = false;
  if (c.legacy_approx != 0 && mine.size() >= c.nevents) ok = false;
  if (!ok) {
    std::ostringstream os;
    for (std::size_t i = 0; i < mine.size(); ++i) os << mine[i] << "; ";
    k.fail("kevt prefix", "captured stream", os.str());
  }
}

struct Summary {
  std::size_t cases = 0;
  int checks = 0;
  int failures = 0;
  std::vector<std::string> problems;
};

inline Summary replay_all(bool verbose) {
  Summary s;
  std::size_t n = 0;
  const golden_tsm::Case* table = golden_tsm::table(n);
  for (std::size_t i = 0; i < n; ++i) {
    const golden_tsm::Case& c = table[i];
    ++s.cases;
    kalos::tramo_seats_model_request req;
    std::string err;
    if (!parse_request(c, req, err)) {
      ++s.failures;
      s.problems.push_back(std::string(c.id) + ": request parse: " + err);
      continue;
    }
    kalos::tramo_seats_model_result res;
    if (c.reject != 0) {
      // QR-cascade record case (D11): the JVM entered the
      // FastEigenValuesSolver / seasonal root-zone reconstruction path;
      // the unit must refuse the request fail-closed.
      ++s.checks;
      bool threw = false;
      try {
        kalos::validate_tramo_seats_model(req);
      } catch (const std::exception&) {
        threw = true;
      }
      if (!threw) {
        ++s.failures;
        s.problems.push_back(std::string(c.id) + ": refusal expected, unit accepted");
      }
      continue;
    }
    try {
      res = kalos::validate_tramo_seats_model(req);
    } catch (const std::exception& e) {
      ++s.failures;
      s.problems.push_back(std::string(c.id) + ": unit refused: " + e.what());
      continue;
    }
    Checker k{c, res, 0, 0, &s.problems};
    check_facts(k);
    check_flags_and_events(k);
    s.checks += k.checks;
    s.failures += k.failures;
    if (verbose) {
      std::ostringstream os;
      os << c.id << ": " << k.checks << " checks, " << k.failures << " failures"
         << ", floor " << res.variance_floor << " (bits 0x" << std::hex
         << bits(res.variance_floor) << std::dec << ")";
      s.problems.push_back(os.str());
    }
  }
  return s;
}

}  // namespace replay_tsm

#endif
