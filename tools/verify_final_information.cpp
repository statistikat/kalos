// Full-sweep verification: the native item 21-H.0 final-information unit
// against the pinned jdplus 3.9.0 corpus
// (validation/evidence/native/final-information; an alternative corpus
// directory may be passed as the single argument).
//
// Tier 1 (producer, bit-exact, all 27 rows) replays each case through
// kalos::final_information with a raw-bit node-table callback rebuilt
// directly from capture.csv/nodes.csv (the base node carries fx_hex, or the
// rejection for rej rows, exactly as gen_final_information_golden.py
// injects it): a lookup miss means the unit evaluated a node outside the
// captured stencil, an uncovered node means it skipped one the JVM
// evaluated, and status, fx, the post-checkepsilon epsilons, the gradient,
// the hessian, the scaled score and information, the asymptotic covariance
// and its diagonal are compared bit for bit against the capture.
//
// Tier 2 (deviance-kernel fidelity, bit-exact where the kernel is
// qualified): for est rows the point is the estimator's own converged point
// and fx_hex its own final ssq, so detail::pinned_deviances_evaluate must
// reproduce fx_hex bit for bit; for rej rows (the JVM's ssq stage threw) the
// kernel must return the matching rejection. The pinned kernel is qualified
// on estimator-converged paths; bit agreement at the JVM stencil's
// +/-sqrt(eps) perturbed nodes is NOT part of that domain, so tier 3 only
// tallies kernel/node agreement (and any disagreement at an ok/invfail
// base) as a diagnostic without gating the sweep. The producer itself is
// bit-verified by tier 1.
#include <kalos/detail/ssq_deviances.hpp>
#include <kalos/final_information.hpp>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

long checks = 0, bad = 0;
long kernel_agree = 0, kernel_drift = 0;

std::string unquote(const std::string& s) {
  if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
    return s.substr(1, s.size() - 2);
  return s;
}

std::vector<std::string> fields(const std::string& line) {
  // Split one CSV line honouring quoted cells with embedded commas.
  std::vector<std::string> out;
  std::string cell;
  bool quoted = false;
  for (std::size_t i = 0; i < line.size(); ++i) {
    const char ch = line[i];
    if (quoted) {
      if (ch == '"') {
        if (i + 1 < line.size() && line[i + 1] == '"') { cell += '"'; ++i; }
        else quoted = false;
      } else cell += ch;
    } else if (ch == '"') quoted = true;
    else if (ch == ',') { out.push_back(cell); cell.clear(); }
    else cell += ch;
  }
  out.push_back(cell);
  return out;
}

// csv -> named row map (header-indexed, immune to column reordering).
std::vector<std::map<std::string, std::string>> read_named(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open " + path);
  std::vector<std::map<std::string, std::string>> rows;
  std::vector<std::string> header;
  std::string line;
  bool first = true;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty()) continue;
    if (first) { header = fields(line); first = false; continue; }
    const std::vector<std::string> f = fields(line);
    std::map<std::string, std::string> row;
    for (std::size_t k = 0; k < header.size() && k < f.size(); ++k)
      row[header[k]] = unquote(f[k]);
    rows.push_back(row);
  }
  return rows;
}

std::vector<std::string> split(const std::string& raw, char sep) {
  std::vector<std::string> out;
  std::stringstream parts(raw);
  std::string item;
  while (std::getline(parts, item, sep)) out.push_back(item);
  if (raw.empty()) out.clear();
  return out;
}

std::uint64_t to_bits(double d) {
  std::uint64_t u = 0;
  std::memcpy(&u, &d, sizeof(u));
  return u;
}

double from_bits(std::uint64_t u) {
  double d = 0.0;
  std::memcpy(&d, &u, sizeof(d));
  return d;
}

std::uint64_t hex_bits(const std::string& tok) {
  return std::strtoull(tok.c_str(), nullptr, 16);
}

double hex_real(const std::string& tok) { return from_bits(hex_bits(tok)); }

std::vector<double> hexes(const std::string& raw, char sep) {
  std::vector<double> out;
  for (const auto& t : split(raw, sep))
    if (!t.empty()) out.push_back(hex_real(t));
  return out;
}

std::vector<std::uint64_t> hexkey(const std::string& raw, char sep) {
  std::vector<std::uint64_t> out;
  for (const auto& t : split(raw, sep))
    if (!t.empty()) out.push_back(hex_bits(t));
  return out;
}

void fail(const std::string& id, const std::string& what) {
  std::printf("  %s: FAIL %s\n", id.c_str(), what.c_str());
  ++bad;
}

std::string u64(std::uint64_t u) {
  char buf[24];
  std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(u));
  return buf;
}

void bit(const std::string& id, const std::string& what, double got, double want) {
  ++checks;
  if (to_bits(got) != to_bits(want)) {
    fail(id, what + " got " + u64(to_bits(got)) + " want " + u64(to_bits(want)));
  }
}

void bitvec(const std::string& id, const std::string& what,
            const std::vector<double>& got, const std::vector<double>& want) {
  ++checks;
  if (got.size() != want.size()) {
    fail(id, what + " size " + std::to_string(got.size()) + " != " +
             std::to_string(want.size()));
    return;
  }
  for (std::size_t k = 0; k < got.size(); ++k) {
    if (to_bits(got[k]) != to_bits(want[k])) {
      fail(id, what + "[" + std::to_string(k) + "] got " + u64(to_bits(got[k])) +
               " want " + u64(want[k]));
      return;
    }
  }
}

struct Kernel {
  const std::vector<double>* y;
  int p, bp, q, bq, freq;
  std::optional<double> operator()(const std::vector<double>& x) const {
    const auto m = kalos::detail::pinned_deviances_evaluate(
        *y, static_cast<std::size_t>(p), static_cast<std::size_t>(bp),
        static_cast<std::size_t>(q), static_cast<std::size_t>(bq), freq, {}, x);
    if (!m.has_value()) return std::nullopt;
    return m->ssq_e;
  }
};

} // namespace

int main(int argc, char** argv) {
  const std::string root =
      argc > 1 ? argv[1] : "validation/evidence/native/final-information/";
  const auto caps = read_named(root + "capture.csv");
  const auto nodes = read_named(root + "nodes.csv");
  std::map<std::string, std::map<std::string, std::string>> cases;
  for (const auto& r : read_named(root + "cases.csv")) cases[r.at("id")] = r;
  std::map<std::string, std::vector<std::map<std::string, std::string>>> byid;
  for (const auto& r : nodes) byid[r.at("id")].push_back(r);

  for (const auto& cap : caps) {
    const std::string id = cap.at("id");
    const auto& cs = cases.at(id);
    const int dim = std::stoi(cap.at("dim"));
    const std::string status = cap.at("status");
    const bool rejected = status.rfind("rej", 0) == 0;
    const bool est_row = cs.at("mode") == "est";
    const std::vector<double> point = hexes(cap.at("point_hex"), ' ');
    const double fx = rejected ? 0.0 : hex_real(cap.at("fx_hex"));

    // Tier 1: the producer through the captured node table.
    struct Replay {
      std::map<std::vector<std::uint64_t>, std::pair<std::uint64_t, int>> table;
      std::set<std::vector<std::uint64_t>> todo;
      int misses = 0;
    } rp;
    if (rejected) {
      rp.table[hexkey(cap.at("point_hex"), ' ')] = {0, 1};
      rp.todo.insert(hexkey(cap.at("point_hex"), ' '));
    } else {
      for (const auto& nd : byid[id]) {
        rp.table[hexkey(nd.at("node"), ';')] = {hex_bits(nd.at("value")),
                                                nd.at("value") == "THROW"};
        rp.todo.insert(hexkey(nd.at("node"), ';'));
      }
      const auto pk = hexkey(cap.at("point_hex"), ' ');
      if (!rp.table.count(pk)) {
        rp.table[pk] = {to_bits(fx), 0}; // the base node (gen-injected)
      }
      rp.todo.insert(pk);
    }
    kalos::final_information_request req;
    req.orders.p = std::stoi(cs.at("p"));
    req.orders.bp = std::stoi(cs.at("bp"));
    req.orders.q = std::stoi(cs.at("q"));
    req.orders.bq = std::stoi(cs.at("bq"));
    req.point = point;
    req.nobs = std::stoi(cap.at("nobs"));
    req.check_all = std::stoi(cap.at("checkall_eff")) != 0;
    req.ar_limit = hex_real(cap.at("arlimit_hex"));
    req.ma_limit = hex_real(cap.at("malimit_hex"));
    req.epsilon = hex_real(cap.at("epsilon_hex"));
    req.deviance = [&](const std::vector<double>& x) -> std::optional<double> {
      std::vector<std::uint64_t> key(x.size());
      for (std::size_t k = 0; k < x.size(); ++k) key[k] = to_bits(x[k]);
      const auto it = rp.table.find(key);
      if (it == rp.table.end()) { ++rp.misses; return std::nullopt; }
      rp.todo.erase(key);
      if (it->second.second) return std::nullopt;
      return from_bits(it->second.first);
    };
    const kalos::final_information_result r = kalos::final_information(req);

    int want_status;
    if (rejected) want_status = 2;
    else if (status == "ok") want_status = 0;
    else want_status = 1;
    const int got_status =
        r.status == kalos::final_information_status::available
            ? 0
            : (r.status == kalos::final_information_status::inverse_failed ? 1 : 2);
    ++checks;
    if (got_status != want_status) fail(id, "status " + std::to_string(got_status));
    ++checks;
    if (rp.misses != 0)
      fail(id, std::to_string(rp.misses) + " lookups outside the node table");
    ++checks;
    if (!rp.todo.empty())
      fail(id, std::to_string(rp.todo.size()) + " captured nodes never evaluated");

    if (rejected) {
      ++checks;
      if (!r.gradient.empty() || !r.information.empty() ||
          !r.asymptotic_covariance.empty())
        fail(id, "rejected case produced output");
    } else {
      bit(id, "fx", r.fx, fx);
      bitvec(id, "eps", r.eps, hexes(cap.at("eps_hex"), ' '));
      bitvec(id, "grad", r.gradient, hexes(cap.at("grad_hex"), ' '));
      bitvec(id, "score", r.score, hexes(cap.at("score_hex"), ' '));
      bitvec(id, "hess", r.hessian, hexes(cap.at("hess_hex"), ' '));
      bitvec(id, "info", r.information, hexes(cap.at("info_hex"), ' '));
      if (want_status == 0) {
        bitvec(id, "asy", r.asymptotic_covariance, hexes(cap.at("asy_hex"), ' '));
        bitvec(id, "diag", r.asymptotic_covariance_diagonal,
               hexes(cap.at("diag_hex"), ' '));
      } else {
        ++checks;
        if (!r.asymptotic_covariance.empty()) fail(id, "invfail produced covariance");
      }
    }

    // Tier 2/3: the qualified deviance kernel.
    Kernel kernel;
    std::vector<double> yd = rejected ? std::vector<double>()
                                      : [&] {
                                          std::vector<double> v;
                                          for (const auto& t : split(cap.at("yd"), ';'))
                                            if (!t.empty())
                                              v.push_back(std::strtod(t.c_str(), nullptr));
                                          return v;
                                      }();
    kernel.y = &yd;
    kernel.p = req.orders.p;
    kernel.bp = req.orders.bp;
    kernel.q = req.orders.q;
    kernel.bq = req.orders.bq;
    kernel.freq = std::stoi(cs.at("freq"));
    const std::optional<double> at_point = kernel(point);
    if (rejected) {
      ++checks;
      if (at_point.has_value()) fail(id, "kernel accepted a rejected point");
      continue;
    }
    if (est_row) {
      ++checks;
      if (!at_point.has_value()) fail(id, "kernel rejected the converged point");
      else if (to_bits(*at_point) != to_bits(fx))
        fail(id, "kernel at the converged point got " + u64(to_bits(*at_point)) +
                     " want " + u64(to_bits(fx)));
    } else {
      if (!at_point.has_value() || to_bits(*at_point) != to_bits(fx)) ++kernel_drift;
      else ++kernel_agree;
    }
    for (const auto& nd : byid[id]) {
      const std::vector<double> key = hexes(nd.at("node"), ';');
      const std::optional<double> v = kernel(key);
      const bool want_throw = nd.at("value") == "THROW";
      if (want_throw ? !v.has_value()
                     : (v.has_value() && to_bits(*v) == to_bits(hex_real(nd.at("value")))))
        ++kernel_agree;
      else
        ++kernel_drift;
    }
  }

  std::printf("final-information sweep: %ld corpus checks, %ld failures | "
              "%zu cases, %zu nodes | kernel-qualified domain, drift outside: "
              "%ld agree, %ld outside-domain\n",
              checks, bad, caps.size(), nodes.size(), kernel_agree, kernel_drift);
  return bad == 0 ? 0 : 1;
}
