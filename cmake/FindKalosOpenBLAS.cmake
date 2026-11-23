# Kalos OpenBLAS provider finder.
#
# Precedence rule: an explicit provider root (cache entry -DOpenBLAS_ROOT=<dir>
# or the OpenBLAS_ROOT environment variable) is EXCLUSIVE. When it is set, the
# header and the library are searched only under that root, so a configuration
# can never mix pinned provider headers with a system library found through
# pkg-config (for example a system OpenBLAS without LAPACKE exports). If the
# explicit root lacks either component, this module fails loudly instead of
# silently falling back to system hints. Without an explicit root, pkg-config
# and system hints are used as before.
#
# REVIEW-1.C (2026-11-23, external review "Validate OpenBLAS selection on
# reconfigure and downstream install") adds two guarantees:
#
# 1. Selection-change invalidation. The effective root is recorded in the
#    cache (KALOS_OPENBLAS_SELECTION). When it changes inside an existing
#    build directory, the previously discovered KalosOpenBLAS_INCLUDE_DIR /
#    KalosOpenBLAS_LIBRARY and the cached ABI probe are dropped, so a root
#    change can never be silently served by the old provider's cached paths
#    and green probe result. Pre-seeded path cache entries (the documented
#    downstream pattern of passing KalosOpenBLAS_INCLUDE_DIR/_LIBRARY
#    directly) are additionally validated against an active explicit root:
#    a path outside the root is dropped and rediscovered under it, exactly
#    as the precedence rule demands.
# 2. The LP64 + CBLAS + LAPACKE link probe lives HERE, on the shared finder
#    path, so in-tree builds and installed consumers (kalosConfig runs this
#    module through find_dependency) get the same check. The probe compiles
#    the lapack_int/int LP64 static_assert and forces real references to
#    cblas_dgemm, LAPACKE_dpotrf_work and openblas_get_config into the link
#    (a volatile function-pointer table - the references survive
#    optimization and nothing is executed). A provider whose shared library
#    has the headers but not the LAPACKE/CBLAS exports therefore fails the
#    configure that selects it, instead of the link of some later dependent
#    target. The probe result is cached, keyed by the include/library pair
#    (KALOS_OPENBLAS_ABI_PROBE_SIGNATURE), and re-runs whenever either path
#    changes; KalosOpenBLAS_ABI_OK is a REQUIRED_VARS entry of
#    find_package_handle_standard_args, so a failed probe answers with
#    KalosOpenBLAS_FOUND=FALSE and the required-package diagnostic.

# ---- effective root (documented precedence: cache entry, then environment) --
set(KalosOpenBLAS_ROOT "${OpenBLAS_ROOT}")
if(NOT KalosOpenBLAS_ROOT AND DEFINED ENV{OpenBLAS_ROOT})
  set(KalosOpenBLAS_ROOT "$ENV{OpenBLAS_ROOT}")
endif()

# ---- selection-change invalidation (REVIEW-1.C) -----------------------------
if(NOT "${KalosOpenBLAS_ROOT}" STREQUAL "${KALOS_OPENBLAS_SELECTION}")
  unset(KalosOpenBLAS_INCLUDE_DIR CACHE)
  unset(KalosOpenBLAS_LIBRARY CACHE)
  unset(KalosOpenBLAS_ABI_OK CACHE)
  unset(KalosOpenBLAS_ABI_PROBE_SIGNATURE CACHE)
  set(KALOS_OPENBLAS_SELECTION "${KalosOpenBLAS_ROOT}" CACHE INTERNAL
    "KalosOpenBLAS provider selection (explicit root or '' for system)" FORCE)
endif()

# True when ${path} equals ${root} or sits under it (literal prefix compare,
# no regex characters from user-supplied paths are interpreted).
function(_kalos_openblas_path_under path root out_var)
  file(TO_CMAKE_PATH "${path}" _d)
  file(TO_CMAKE_PATH "${root}" _r)
  string(LENGTH "${_r}" _rl)
  string(SUBSTRING "${_d}" 0 ${_rl} _pre)
  set(_under FALSE)
  if(_d STREQUAL _r)
    set(_under TRUE)
  elseif(_pre STREQUAL _r)
    string(SUBSTRING "${_d}" ${_rl} 1 _next)
    if(_next STREQUAL "/")
      set(_under TRUE)
    endif()
  endif()
  set(${out_var} "${_under}" PARENT_SCOPE)
endfunction()

set(KalosOpenBLAS_PROVIDER_SOURCE "system")
if(KalosOpenBLAS_ROOT)
  set(KalosOpenBLAS_PROVIDER_SOURCE "explicit-root")
  # An active explicit root owns the selection: drop pre-seeded or stale
  # cached paths that do not live under it (the exclusivity of the
  # precedence rule applies to cache entries as well as to discovery).
  foreach(_cached KalosOpenBLAS_INCLUDE_DIR KalosOpenBLAS_LIBRARY)
    if(DEFINED ${_cached} AND NOT "${${_cached}}" STREQUAL "")
      _kalos_openblas_path_under("${${_cached}}" "${KalosOpenBLAS_ROOT}" _under)
      if(NOT _under)
        message(STATUS
          "KalosOpenBLAS: cached ${_cached}='${${_cached}}' is outside OpenBLAS_ROOT='${KalosOpenBLAS_ROOT}'; dropped and rediscovered under the root.")
        unset(${_cached} CACHE)
      endif()
    endif()
  endforeach()
  find_path(KalosOpenBLAS_INCLUDE_DIR lapacke.h
    HINTS "${KalosOpenBLAS_ROOT}" NO_DEFAULT_PATH
    PATH_SUFFIXES include include/openblas openblas)
  find_library(KalosOpenBLAS_LIBRARY NAMES openblas
    HINTS "${KalosOpenBLAS_ROOT}" NO_DEFAULT_PATH
    PATH_SUFFIXES lib lib64)
  if(NOT KalosOpenBLAS_INCLUDE_DIR OR NOT KalosOpenBLAS_LIBRARY)
    message(WARNING
      "OpenBLAS_ROOT='${KalosOpenBLAS_ROOT}' does not contain both lapacke.h (under include/) and libopenblas (under lib/); system provider hints are deliberately NOT used as a fallback. Unset OpenBLAS_ROOT to use pkg-config/system discovery.")
  endif()
else()
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(PC_OPENBLAS QUIET openblas)
  endif()
  find_path(KalosOpenBLAS_INCLUDE_DIR lapacke.h
    HINTS ${PC_OPENBLAS_INCLUDE_DIRS}
    PATH_SUFFIXES include include/openblas openblas)
  find_library(KalosOpenBLAS_LIBRARY NAMES openblas
    HINTS ${PC_OPENBLAS_LIBRARY_DIRS}
    PATH_SUFFIXES lib lib64)
endif()

# ---- shared LP64 + CBLAS + LAPACKE link probe (REVIEW-1.C) ------------------
# Cached per (headers, library) pair; invalidated when either path changes.
if(KalosOpenBLAS_INCLUDE_DIR AND KalosOpenBLAS_LIBRARY)
  set(_kalos_openblas_abi_sig "${KalosOpenBLAS_INCLUDE_DIR};${KalosOpenBLAS_LIBRARY}")
  if(NOT _kalos_openblas_abi_sig STREQUAL "${KALOS_OPENBLAS_ABI_PROBE_SIGNATURE}")
    unset(KalosOpenBLAS_ABI_OK CACHE)
    set(KALOS_OPENBLAS_ABI_PROBE_SIGNATURE "${_kalos_openblas_abi_sig}" CACHE INTERNAL
      "KalosOpenBLAS ABI probe inputs (include dir;library)" FORCE)
  endif()
  include(CheckCXXSourceCompiles)
  set(CMAKE_REQUIRED_INCLUDES "${KalosOpenBLAS_INCLUDE_DIR}")
  set(CMAKE_REQUIRED_LIBRARIES "${KalosOpenBLAS_LIBRARY}")
  check_cxx_source_compiles("#include <cblas.h>
#include <lapacke.h>
#include <type_traits>
static_assert(sizeof(int)==4 && std::is_same<lapack_int,int>::value, \"LP64 required\");
int main() {
  // Volatile table: the references survive optimization, nothing executes.
  volatile void* entries[] = {(void*)&cblas_dgemm,
                              (void*)&LAPACKE_dpotrf_work,
                              (void*)&openblas_get_config};
  return entries[0] && entries[1] && entries[2] ? 0 : 1;
}" KalosOpenBLAS_ABI_OK)
  unset(CMAKE_REQUIRED_LIBRARIES)
  unset(CMAKE_REQUIRED_INCLUDES)
  if(NOT KalosOpenBLAS_ABI_OK)
    message(WARNING
      "Kalos requires OpenBLAS with CBLAS, LAPACKE and a 32-bit LP64 integer ABI: the provider at '${KalosOpenBLAS_LIBRARY}' with headers at '${KalosOpenBLAS_INCLUDE_DIR}' failed the shared LP64/CBLAS/LAPACKE link probe (missing CBLAS or LAPACKE entry points, an ILP64/wrong integer ABI, or headers from a different provider than the library). Select a complete LP64 provider, for example -DOpenBLAS_ROOT=<root>.")
  endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(KalosOpenBLAS REQUIRED_VARS
  KalosOpenBLAS_INCLUDE_DIR KalosOpenBLAS_LIBRARY KalosOpenBLAS_ABI_OK)
if(KalosOpenBLAS_FOUND AND NOT TARGET Kalos::OpenBLAS)
  add_library(Kalos::OpenBLAS UNKNOWN IMPORTED)
  set_target_properties(Kalos::OpenBLAS PROPERTIES
    IMPORTED_LOCATION "${KalosOpenBLAS_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${KalosOpenBLAS_INCLUDE_DIR}")
endif()
