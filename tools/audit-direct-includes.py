#!/usr/bin/env python3
"""Direct-include hygiene audit (REVIEW-1.A).

Every translation unit in the shipped library surface - each public header
under include/ and each source TU under src/ and backends/ - must include
the standard header that DEFINES every std:: entity it names, instead of
reaching it through some other header's transitive includes. libstdc++
currently leaks most of these dependencies; libc++ (Homebrew Clang, the
macOS P1 report of adaption_in_the_implemetation_needed.md) does not, so
every unbacked name here is a latent build break on that toolchain.

The scan is deliberately TYPE-level (named types + the exception/rethrow
functions the review named + the small utility set below): a use of
std::move or std::max is only flagged for the header that DEFINES it when
the entity is in the table, and table entries were chosen so every hit is
a real libc++-drift candidate, not style noise.

Usage: tools/audit-direct-includes.py [--tsv]
Exit code 1 when any file has a missing direct include; 0 when clean.
"""
import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# std:: entity (as written after "std::") -> required direct header.
# First matching word-bounded token wins; compound names (exception_ptr)
# must be checked before their prefixes (exception), which the regex below
# handles by tokenizing the longest identifier after std::.
ENTITY = {}


def _add(header, *names):
    for name in names:
        assert name not in ENTITY or ENTITY[name] == header, name
        ENTITY[name] = header


_add("<exception>", "exception", "exception_ptr", "current_exception",
     "rethrow_exception", "make_exception_ptr", "uncaught_exceptions")
_add("<stdexcept>", "invalid_argument", "runtime_error", "logic_error",
     "out_of_range", "domain_error", "length_error", "overflow_error",
     "underflow_error", "range_error", "system_error")
_add("<string>", "string", "wstring", "u16string", "u32string", "to_string",
     "to_wstring", "stoi", "stol", "stoll", "stoul", "stoull", "stof",
     "stod", "stold")
_add("<string_view>", "string_view", "wstring_view")
_add("<vector>", "vector")
_add("<map>", "map", "multimap")
_add("<unordered_map>", "unordered_map", "unordered_multimap")
_add("<set>", "set", "multiset")
_add("<unordered_set>", "unordered_set", "unordered_multiset")
_add("<optional>", "optional", "nullopt", "nullopt_t")
_add("<variant>", "variant", "monostate", "variant_size", "visit",
     "holds_alternative", "get_if")
_add("<array>", "array")
_add("<tuple>", "tuple", "make_tuple", "tie", "forward_as_tuple", "apply",
     "tuple_size", "tuple_element")
_add("<utility>", "pair", "make_pair", "move", "forward", "swap", "declval",
     "as_const", "exchange", "integer_sequence", "index_sequence",
     "make_index_sequence", "piecewise_construct_t", "in_place_t",
     "in_place_index", "in_place_type")
_add("<memory>", "unique_ptr", "shared_ptr", "weak_ptr", "make_unique",
     "make_shared", "allocate_shared", "enable_shared_from_this",
     "default_delete", "align")
_add("<functional>", "function", "reference_wrapper", "ref", "cref", "hash",
     "less", "greater", "less_equal", "greater_equal", "equal_to",
     "not_equal_to", "plus", "minus", "multiplies", "divides", "negate")
_add("<limits>", "numeric_limits")
_add("<cstddef>", "size_t", "ptrdiff_t", "nullptr_t", "byte", "max_align_t")
_add("<cstdint>", "int8_t", "uint8_t", "int16_t", "uint16_t", "int32_t",
     "uint32_t", "int64_t", "uint64_t", "int_least8_t", "int_least16_t",
     "int_least32_t", "int_least64_t", "uint_least8_t", "uint_least16_t",
     "uint_least32_t", "uint_least64_t", "int_fast8_t", "int_fast16_t",
     "int_fast32_t", "int_fast64_t", "intmax_t", "uintmax_t", "intptr_t",
     "uintptr_t")
_add("<cmath>", "isnan", "isfinite", "isinf", "signbit", "fpclassify",
     "copysign", "hypot", "hypotf", "fma", "sqrt", "sqrtf", "fabs",
     "exp", "expf", "exp2", "expm1", "log", "logf", "log2", "log10",
     "log1p", "pow", "powf", "sin", "cos", "tan", "asin", "acos", "atan",
     "atan2", "sinh", "cosh", "tanh", "floor", "ceil", "round", "trunc",
     "nearbyint", "rint", "fmod", "remainder", "fmin", "fmax", "cbrt",
     "erf", "erfc", "tgamma", "lgamma", "frexp", "ldexp", "modf", "ilogb",
     "scalbn", "nextafter", "nexttoward")
_add("<cstring>", "memcpy", "memset", "memcmp", "memmove", "memchr",
     "strlen", "strcmp", "strncmp", "strcpy", "strncpy", "strchr",
     "strstr", "strerror", "strrchr", "strcmp")
_add("<cstdio>", "printf", "fprintf", "sprintf", "snprintf", "sscanf",
     "fscanf", "fopen", "fclose", "fputs", "fgets", "fflush", "perror",
     "vsnprintf", "rename", "tmpfile")
_add("<algorithm>", "sort", "stable_sort", "partial_sort", "nth_element",
     "find", "find_if", "find_if_not", "copy", "copy_if", "copy_n",
     "transform", "count", "count_if", "for_each", "fill", "fill_n",
     "generate", "generate_n", "lower_bound", "upper_bound",
     "equal_range", "binary_search", "equal", "mismatch", "reverse",
     "rotate", "remove", "remove_if", "unique", "none_of", "any_of",
     "all_of", "search", "set_union", "set_intersection", "set_difference",
     "includes", "max_element", "min_element", "clamp", "lexicographical_"
     "compare", "is_sorted", "adjacent_find", "replace", "replace_if",
     "iter_swap", "max", "min", "inplace_merge", "partial_sort_copy",
     "is_permutation")
_add("<numeric>", "accumulate", "iota", "partial_sum", "inner_product",
     "adjacent_difference", "reduce", "transform_reduce", "exclusive_scan",
     "inclusive_scan", "gcd", "lcm")
_add("<iostream>", "cout", "cerr", "cin", "clog", "endl")
_add("<sstream>", "ostringstream", "istringstream", "stringstream",
     "wostringstream")
_add("<fstream>", "ifstream", "ofstream", "fstream")
_add("<chrono>", "chrono")
_add("<type_traits>", "is_same", "enable_if", "conditional", "decay",
     "remove_reference", "add_const", "remove_cv", "is_integral",
     "is_floating_point", "is_arithmetic", "is_base_of", "is_convertible",
     "underlying_type", "aligned_storage", "common_type")

STD_RE = re.compile(r"\bstd::([A-Za-z_][A-Za-z0-9_]*)")
INCLUDE_RE = re.compile(r'^\s*#\s*include\s*[<"]([^">]+)[">]', re.M)
BLOCK_COMMENT_RE = re.compile(r"/\*.*?\*/", re.S)
LINE_COMMENT_RE = re.compile(r"//[^\n]*")


def strip_comments(text):
    return LINE_COMMENT_RE.sub("", BLOCK_COMMENT_RE.sub(" ", text))


def audit_file(path):
    text = strip_comments(path.read_text())
    includes = set(INCLUDE_RE.findall(path.read_text()))
    missing = {}  # header -> sorted entities
    for name in STD_RE.findall(text):
        header = ENTITY.get(name)
        bare = header[1:-1] if header else None
        if header is None or bare in includes:
            continue
        missing.setdefault(header, set()).add(name)
    return [(header, sorted(entities)) for header, entities in
            sorted(missing.items())]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tsv", action="store_true",
                    help="emit file<TAB>header<TAB>entities lines")
    args = ap.parse_args()

    roots = [ROOT / "include", ROOT / "src", ROOT / "backends"]
    files = sorted(p for r in roots for p in r.rglob("*")
                   if p.is_file() and p.suffix in {".hpp", ".cpp"})
    hits = 0
    total_missing = 0
    for path in files:
        missing = audit_file(path)
        if not missing:
            continue
        hits += 1
        total_missing += len(missing)
        rel = path.relative_to(ROOT)
        for header, entities in missing:
            if args.tsv:
                print(f"{rel}\t{header}\t{','.join(entities)}")
            else:
                print(f"{rel}: {header} for {', '.join('std::' + e for e in entities)}")
    print(f"scanned {len(files)} files; {hits} files with "
          f"{total_missing} missing direct includes", file=sys.stderr)
    return 1 if hits else 0


if __name__ == "__main__":
    sys.exit(main())
