#!/usr/bin/env python3
"""Generate tests/jvm_arma_diagnostics.h from the JVM golden capture
(validation/reference/arma_diagnostics_golden.txt, produced by
validation/reference/LjungBoxGolden.java against the pinned jdplus 3.9.0
jars). Rows are "tag i0 i1 i2 vHex"; "series" rows carry a variable-length
hex payload. Regenerate after re-running the capture; content is bit-stable
for a given jar set."""
import sys

TAGS = {"acf": 0, "lbStat": 1, "lbP": 2, "lbLen": 3, "rtval": 4}


def main(src, dst):
    series_rows = []
    rows = []
    for line in open(src):
        parts = line.split()
        tag = parts[0]
        if tag == "series":
            series_rows.append((int(parts[1]), int(parts[2]), parts[3:]))
            continue
        threw = 0
        if tag.endswith("-throw"):
            tag, threw = tag[:-len("-throw")], 1
        if tag == "lb":
            # "lb case k nhp which hex" - which selects statistic vs p-value.
            enum = 1 if parts[4] == "0" else 2
            ints, val = parts[1:4], parts[5]
        else:
            enum = TAGS[tag]
            ints, val = parts[1:4], parts[4]
        rows.append((enum, threw, int(ints[0]), int(ints[1]),
                     int(ints[2]), val))
    series_rows.sort()
    with open(dst, "w") as f:
        f.write("""// JVM golden table for the automatic-model diagnostics machine
// (autoCorrelationFunction, the LjungBox/Chi2 chain, calcLbLength and
// calcResStat), captured bit-exactly from the pinned jdplus 3.9.0 runtime by
// ../validation/reference/LjungBoxGolden.java (intrinsic-disabled JVM; the
// input series are exact recurrences replayed through the table below - see
// that file's capture recipe). Do not edit by hand; regenerate with
// tools/gen_arma_diagnostics_golden.py.
#ifndef KALOS_TESTS_JVM_ARMA_DIAGNOSTICS_H
#define KALOS_TESTS_JVM_ARMA_DIAGNOSTICS_H

#include <cstdint>
#include <cstring>
#include <vector>

namespace golden_diag {

enum Tag : std::uint8_t {
    kAcf = 0,
    kLbStat = 1,
    kLbP = 2,
    kLbLen = 3,
    kRtval = 4,
};

inline double bits(std::uint64_t u) {
    static_assert(sizeof(double) == sizeof(u), "requires IEEE754 double");
    double d = 0.0;
    std::memcpy(&d, &u, sizeof(d));
    return d;
}

struct Row {
    std::uint8_t tag;
    std::uint8_t threw;  // JVM threw (LjungBox build / Chi2 df)
    int i0;              // case id (acf, lb, rtval); freq for lbLen
    int i1;              // acf: mean flag; lb: k; lbLen: neff; rtval: 0
    int i2;              // acf: lag; lb: nhp; lbLen/rtval: 0
    std::uint64_t v;     // raw result bits (statistic for kLbStat, p for kLbP)
};

struct Series {
    int id;
    std::size_t n;
    const std::uint64_t* values;
};

inline const Row* table(std::size_t& count) {
    static const Row kRows[] = {
""")
        for tag, threw, i0, i1, i2, v in rows:
            f.write("        {%d, %d, %d, %d, %d, %sULL},\n"
                    % (tag, threw, i0, i1, i2, v))
        f.write("""    };
    count = sizeof(kRows) / sizeof(kRows[0]);
    return kRows;
}

inline const Series* series_table(std::size_t& count) {
""")
        for idx, (sid, n, vals) in enumerate(series_rows):
            f.write("    static const std::uint64_t kSeries%d[] = {%s};\n"
                    % (sid, ", ".join(v + "ULL" for v in vals)))
        f.write("    static const Series kSeries[] = {\n")
        for sid, n, vals in series_rows:
            f.write("        {%d, %d, kSeries%d},\n" % (sid, n, sid))
        f.write("""    };
    count = sizeof(kSeries) / sizeof(kSeries[0]);
    return kSeries;
}

}  // namespace golden_diag

#endif
""")
    print("wrote %s (%d rows, %d series)" % (dst, len(rows), len(series_rows)))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
