#!/usr/bin/env python3
"""Generate tests/jvm_fcdf.h from the JVM F-kernel golden capture
(validation/reference/fcdf_golden.txt, produced by
validation/reference/FcdfGolden.java against the pinned jdplus 3.9.0 jars;
the corpus triples come from the item 23-C.2 battery capture, see
validation/reference/capture-tramo-differencing-battery.R). Regenerate after
re-running the capture; the file content is bit-stable for a given jar set.
Grid rows are {tag, threw, a, b, v, out} with doubles stored as raw bits;
corpus rows carry the (value, k1, k2) triple and the FProbability result of
the FTest triple recorded by the battery capture."""
import sys

TAGS = {
    "cf": 0,
    "beta": 1,
    "fprob": 2,
    "corpus": 3,
}


def main(src, dst):
    rows = []
    corpus_index = 0
    for line in open(src):
        parts = line.split()
        if not parts:
            continue
        tag = parts[0]
        thr = 0
        if tag.endswith("-throw"):
            tag, thr = tag[:-len("-throw")], 1
        if tag not in TAGS:
            raise SystemExit("unknown golden tag: " + tag)
        if tag == "corpus":
            # corpus <id> <v> <k1> <k2> <out|throw>
            if parts[5] == "throw":
                thr = 1
                out = "0x0"
            else:
                out = parts[5]
            rows.append((TAGS[tag], thr, corpus_index, parts[2], parts[3], parts[4], out))
            corpus_index += 1
            continue
        if len(parts) != 5:
            raise SystemExit("unparsable golden row: " + line)
        rows.append((TAGS[tag], thr, 0, parts[1], parts[2], parts[3], parts[4]))
    with open(dst, "w") as f:
        f.write("""// JVM golden table for the noncentral-free F kernel (SpecialFunctions.
// incBetaCf / incBeta / FProbability), captured bit-exactly from the pinned
// jdplus 3.9.0 runtime by ../validation/reference/FcdfGolden.java
// (intrinsic-disabled JVM; double-captured byte-identical, see
// ../validation/reference/capture-tramo-differencing-battery.R). Rows are
// {tag, threw, index, a, b, v, out} with doubles stored as raw bits;
// index numbers the corpus rows (the (value, k1, k2) triples reached by the
// item 23-C.2 differencing-battery F-tests, in capture order -
// validation/reference/fcdf_golden.txt holds the ids in text form). Do not
// edit by hand.
#ifndef KALOS_TESTS_JVM_FCDF_H
#define KALOS_TESTS_JVM_FCDF_H

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace kalos {
namespace tests {
namespace golden_fcdf {

enum Tag : std::uint8_t {
    kIncBetaCf = %d,
    kIncBeta = %d,
    kFProbability = %d,
    kCorpus = %d,
};

struct Row {
    std::uint8_t tag;
    std::uint8_t threw;  // JVM threw DStatException; out field unused
    std::uint16_t index;  // corpus row ordinal (0 for grid rows)
    std::uint64_t a;
    std::uint64_t b;
    std::uint64_t v;
    std::uint64_t out;
};

inline double bits(std::uint64_t u) {
    static_assert(sizeof(double) == sizeof(u), "requires IEEE754 double");
    double d = 0.0;
    std::memcpy(&d, &u, sizeof(d));
    return d;
}

inline const Row* table(std::size_t& count) {
    static const Row kRows[] = {
""" % tuple(TAGS[t] for t in ("cf", "beta", "fprob", "corpus")))
        for tag, thr, idx, a, b, v, o in rows:
            f.write("        {%d, %d, %d, %sULL, %sULL, %sULL, %sULL},\n"
                    % (tag, thr, idx, a, b, v, o))
        f.write("""    };
    count = sizeof(kRows) / sizeof(kRows[0]);
    return kRows;
}

}  // namespace golden_fcdf
}  // namespace tests
}  // namespace kalos

#endif
""" % ())
    print("wrote %s (%d rows, %d corpus)" % (dst, len(rows), corpus_index))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
