#!/usr/bin/env python3
"""Generate tests/jvm_check_roots.h from the JVM golden capture
(validation/reference/check_roots_golden.txt, produced by
validation/reference/CheckRootsGolden.java against the pinned jdplus 3.9.0
jars). Rows are "cr len limitHex c0Hex c1Hex c2Hex c3Hex resHex" (padded
zeros never change the effective degree). Regenerate after re-running the
capture; content is bit-stable for a given jar set."""
import sys


def main(src, dst):
    rows = []
    for line in open(src):
        parts = line.split()
        threw = 1 if parts[0] == "crthrow" else 0
        rows.append((threw, int(parts[1]), parts[2:8]))
    with open(dst, "w") as f:
        f.write("""// JVM golden table for FilterUtility.checkRoots(DoubleSeq, double) over the
// effective degrees <= 2 (plus the negative-limit guard for degree >= 3),
// captured bit-exactly from the pinned jdplus 3.9.0 runtime by
// ../validation/reference/CheckRootsGolden.java (intrinsic-disabled JVM; see
// that file's capture recipe). Do not edit by hand; regenerate with
// tools/gen_check_roots_golden.py.
#ifndef KALOS_TESTS_JVM_CHECK_ROOTS_H
#define KALOS_TESTS_JVM_CHECK_ROOTS_H

#include <cstdint>

namespace golden_roots {

struct Row {
    std::uint8_t threw;
    int len;
    std::uint64_t limit;
    std::uint64_t c[4];
    std::uint64_t res;  // 1.0 true, 0.0 false
};

inline const Row* table(std::size_t& count) {
    static const Row kRows[] = {
""")
        for threw, ln, hexes in rows:
            f.write("        {%d, %d, %sULL, {%sULL, %sULL, %sULL, %sULL}, %sULL},\n"
                    % (threw, ln, hexes[0], hexes[1], hexes[2], hexes[3],
                       hexes[4], hexes[5]))
        f.write("""    };
    count = sizeof(kRows) / sizeof(kRows[0]);
    return kRows;
}

}  // namespace golden_roots

#endif
""")
    print("wrote %s (%d rows)" % (dst, len(rows)))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
