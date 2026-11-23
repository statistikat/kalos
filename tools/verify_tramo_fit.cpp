// Full-corpus verification for item 23-F F.4: replays every case of the
// generated golden table (tests/jvm_tramo_fit.h, from
// validation/evidence/native/tramo-fit/fit-capture-1.out) through
// kalos::tramo_fit - the same comparisons the ctest applies (status,
// mirrors, measured live/fact tiers, verdict, D9 bit-replay) - AND
// re-reads the raw corpus to cross-qualify the golden table against the
// captured JVM lines: the per-case identity flags, the seam-kind census
// (fitseam entry lines and post-state lines), the fitresid hand-over
// census, the rsnap count, the fitdec verdict and the fitcl/fituc D9 entry
// count. A corpus edit that moves any replayed fact therefore fails this
// sweep even before the unit replay runs. The d6 and i10 refusal rows have
// no JVM witness and are pinned with synthetic request mutations.
//
// Direct-compile lane (not part of the CMake test list): run from the
// repository root, see docs/contracts/tramo-fit-contract.md.
#include <kalos/tramo_fit.hpp>

using namespace kalos;
#include "../tests/jvm_tramo_fit.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

const char* kCorpus = "validation/evidence/native/tramo-fit/fit-capture-1.out";

int failures = 0;
long long checks = 0;

void fail(const std::string& what) {
    std::cout << "FAIL " << what << std::endl;
    ++failures;
}

#define SW_CHECK(cond, what)                                                 \
    do {                                                                     \
        ++checks;                                                            \
        if (!(cond))                                                         \
            fail(what);                                                      \
    } while (0)

std::vector<std::string> fields(const std::string& line) {
    std::vector<std::string> out;
    std::istringstream is(line);
    std::string t;
    while (is >> t)
        out.push_back(t);
    return out;
}

// One KER..identity block per case id.
struct Block {
    bool found = false;
    std::string identity;
    int fitseam = 0, fitseam_diff = 0, fitseam_arma = 0;
    int post_diff = 0, post_arma = 0, post_scontroller = 0;
    int rsnap = 0, fitresid = 0;
    long long fituc_bits = -1;
    long long fitcl_nx = -1;
    std::string verdict = "-";
    int toint = -1;
};

std::vector<Block> read_blocks(const std::vector<std::string>& ids) {
    std::vector<Block> blocks(ids.size());
    std::ifstream in(kCorpus);
    if (!in) {
        fail("cannot open the corpus");
        return blocks;
    }
    int cur = -1;
    std::string line;
    while (std::getline(in, line)) {
        const std::vector<std::string> t = fields(line);
        if (t.empty())
            continue;
        if (t[0] == "KER" && t.size() > 1) {
            cur = -1;
            for (std::size_t i = 0; i < ids.size(); ++i) {
                if (ids[i] == t[1])
                    cur = static_cast<int>(i);
            }
            if (cur >= 0)
                blocks[cur].found = true;
            continue;
        }
        if (cur < 0)
            continue;
        Block& b = blocks[cur];
        if (t[0] == "identity" && t.size() >= 8) {
            b.identity = t[3] + "/" + t[5] + "/" + t[7];
        } else if (t[0] == "fitseam") {
            ++b.fitseam;
            if (t.size() > 2 && t[2] == "diff")
                ++b.fitseam_diff;
            if (t.size() > 2 && t[2] == "arma")
                ++b.fitseam_arma;
        } else if (t[0] == "seam" && t.size() > 2) {
            if (t[2] == "diff" && t.size() > 3 && t[3] == "res")
                ++b.post_diff;
            else if (t[2] == "arma" && t.size() > 3 && t[3] == "res")
                ++b.post_arma;
            else if (t[2] == "scontroller")
                ++b.post_scontroller;
        } else if (t[0] == "rsnap") {
            ++b.rsnap;
        } else if (t[0] == "fitresid") {
            ++b.fitresid;
        } else if (t[0] == "fituc") {
            b.fituc_bits = static_cast<long long>(t.size() - 1);
        } else if (t[0] == "fitcl" && t.size() > 4) {
            b.fitcl_nx = std::stoll(t[4]);
        } else if (t[0] == "fitdec" && t.size() > 3 && t[2] == "toint") {
            b.verdict = t[1];
            b.toint = std::stoi(t[3]);
        }
    }
    return blocks;
}

void qualify(const FitExpect& e, const Block& b,
             const TramoFitRequest& rq) {
    const std::string id = e.id;
    SW_CHECK(b.found, id + ": no capture block");
    SW_CHECK(b.identity == "1/1/1", id + ": identity line not all-1");
    int diff = 0, arma = 0, scan = 0, scontroller = 0, snaps = 0;
    for (const TramoKernelSeam& s : rq.kernel.seams) {
        if (s.kind == TramoKernelSeamKind::differencing)
            ++diff;
        else if (s.kind == TramoKernelSeamKind::arma)
            ++arma;
        else if (s.kind == TramoKernelSeamKind::scan)
            ++scan;
        else if (s.kind == TramoKernelSeamKind::seasonality_controller)
            ++scontroller;
    }
    (void)scan;
    int resid = 0;
    for (const TramoFitSeamInputs& in : rq.seam_inputs)
        if (!in.ols_residuals.empty())
            ++resid;
    SW_CHECK(b.fitseam == static_cast<int>(rq.seam_inputs.size()),
             id + ": fitseam census vs seam_inputs");
    SW_CHECK(b.fitseam_diff == diff, id + ": fitseam diff census");
    SW_CHECK(b.fitseam_arma == arma, id + ": fitseam arma census");
    SW_CHECK(b.post_diff == diff, id + ": diff post-state census");
    SW_CHECK(b.post_arma == arma, id + ": arma post-state census");
    SW_CHECK(b.post_scontroller == scontroller,
             id + ": scontroller census");
    SW_CHECK(b.fitresid == resid, id + ": fitresid census");
    SW_CHECK(b.rsnap >= 0 &&
                 b.rsnap <= static_cast<int>(rq.kernel.seams.size()) + 8,
             id + ": rsnap census bound");
    SW_CHECK(e.verdict == "-" || (b.verdict == e.verdict && b.toint == e.toint),
             id + ": fitdec verdict vs golden");
    if (b.fituc_bits >= 0) {
        SW_CHECK(b.fitcl_nx * (b.fitcl_nx + 1) / 2 == b.fituc_bits,
                 id + ": fituc length != nx(nx+1)/2");
        SW_CHECK(b.fituc_bits == e.d9_entries,
                 id + ": D9 entries != the captured upper triangle");
    } else {
        SW_CHECK(e.d9_entries == 0, id + ": golden D9 without fituc line");
    }
}

void run_case(const FitExpect& e, const Block& b, std::size_t& live_total,
              std::size_t& fact_total, std::size_t& d9_total) {
    TramoFitRequest rq;
    build_fit(e.id, rq);
    qualify(e, b, rq);
    const TramoFitResult r = tramo_fit(rq);
    if (e.refuses[0] != '\0') {
        SW_CHECK(r.status == e.refuses, std::string(e.id) + ": refusal");
        SW_CHECK(r.seam_checks.empty() && r.kernel.events.empty(),
                 std::string(e.id) + ": refused chain empty");
        return;
    }
    SW_CHECK(r.status == "available", std::string(e.id) + ": available");
    SW_CHECK(r.transform_matches_capture,
             std::string(e.id) + ": transform mirror");
    SW_CHECK(r.seams_live == static_cast<std::size_t>(e.live) &&
                 r.seams_fact == static_cast<std::size_t>(e.fact),
             std::string(e.id) + ": tiers");
    SW_CHECK(r.covariance.matches_capture,
             std::string(e.id) + ": D9 matches_capture");
    SW_CHECK(e.d9_entries > 0 || r.covariance.covariance.empty(),
             std::string(e.id) + ": D9 zero-block consistency");
    live_total += r.seams_live;
    fact_total += r.seams_fact;
    d9_total += static_cast<std::size_t>(e.d9_entries);
}

void synthetic_refusals() {
    TramoFitRequest base;
    build_fit("fit_tr3_air", base);

    TramoFitRequest d6 = base;
    d6.spec.seats_decomposition_mode = 1;
    const TramoFitResult r6 = tramo_fit(d6);
    SW_CHECK(r6.status == "refused:d6-decomposition", "synthetic d6");

    TramoFitRequest i10 = base;
    i10.spec.auto_method = tramo_auto_method::bic;
    const TramoFitResult r10 = tramo_fit(i10);
    SW_CHECK(r10.status == "refused:i10-automethod", "synthetic i10");
}

} // namespace

int main() {
    const std::vector<FitExpect>& exp = fit_expectations();
    std::vector<std::string> ids;
    for (const FitExpect& e : exp)
        ids.push_back(e.id);
    const std::vector<Block> blocks = read_blocks(ids);

    std::size_t live_total = 0, fact_total = 0, d9_total = 0;
    for (std::size_t i = 0; i < exp.size(); ++i)
        run_case(exp[i], blocks[i], live_total, fact_total, d9_total);
    synthetic_refusals();

    std::cout << "verify_tramo_fit: " << (failures == 0 ? "PASS" : "FAIL")
              << ", " << exp.size() << " cases, " << checks
              << " checks, tiers live " << live_total << " fact "
              << fact_total << ", D9 entries " << d9_total << std::endl;
    return failures == 0 ? 0 : 1;
}
