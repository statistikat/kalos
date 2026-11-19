#!/usr/bin/env python3
"""Generate tests/jvm_special_functions.h from the JVM golden capture
(validation/reference/special_functions_golden.txt, produced by
validation/reference/SpecialFunctionsGolden.java against the pinned jdplus
3.9.0 jars). Regenerate after re-running the capture; the file content is
bit-stable for a given jar set."""
import sys

TAGS = {
    "loggamma": 0,
    "igseries": 1,
    "igcf": 2,
    "incgamma": 3,
    "chi2upper": 4,
}

def main(src, dst):
    rows = []
    for line in open(src):
        tag, a, b, v = line.split()
        thr = 0
        if tag.endswith("-throw"):
            tag, thr = tag[:-len("-throw")], 1
        rows.append((TAGS[tag], thr, a, b, v))
    with open(dst, "w") as f:
        f.write("""// JVM golden table for the incomplete-gamma / Chi-square p-value machine,
// captured bit-exactly from the pinned jdplus 3.9.0 runtime by
// ../validation/reference/SpecialFunctionsGolden.java (intrinsic-disabled
// JVM, see the capture recipe in that file's comment); rows are
// {tag, threw, a, b, value} with doubles stored as raw bits
// (validation/reference/special_functions_golden.txt holds the same data in
// text form). Do not edit by hand; regenerate with tools/gen_special_golden_header.py.
#ifndef KALOS_TESTS_JVM_SPECIAL_FUNCTIONS_H
#define KALOS_TESTS_JVM_SPECIAL_FUNCTIONS_H

#include <cstdint>
#include <cstring>

namespace golden {

enum Tag : std::uint8_t {
    kLogGamma = %d,
    kIgSeries = %d,
    kIgContFract = %d,
    kIncGamma = %d,
    kChi2Upper = %d,
};

struct Row {
    std::uint8_t tag;
    std::uint8_t threw;  // JVM threw DStatException; value field unused
    std::uint64_t a;
    std::uint64_t b;
    std::uint64_t v;
};

inline double bits(std::uint64_t u) {
    static_assert(sizeof(double) == sizeof(u), "requires IEEE754 double");
    double d = 0.0;
    std::memcpy(&d, &u, sizeof(d));
    return d;
}

inline const Row* table(std::size_t& count) {
    static const Row kRows[] = {
""" % tuple(TAGS[t] for t in ("loggamma", "igseries", "igcf", "incgamma", "chi2upper")))
        for tag, thr, a, b, v in rows:
            f.write("        {%d, %d, %sULL, %sULL, %sULL},\n" % (tag, thr, a, b, v))
        f.write("""    };
    count = sizeof(kRows) / sizeof(kRows[0]);
    return kRows;
}

}  // namespace golden

#endif
""" % ())
    print("wrote %s (%d rows)" % (dst, len(rows)))

if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
