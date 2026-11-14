# Reference tooling is deliberately independent of the package/adapter namespace.
reference_root <- function() {
  args <- commandArgs(trailingOnly = FALSE)
  file_arg <- args[startsWith(args, "--file=")]
  if (length(file_arg)) {
    script <- normalizePath(sub("^--file=", "", file_arg[[1]]), mustWork = TRUE)
    root <- normalizePath(file.path(dirname(script), "../.."), mustWork = TRUE)
  } else root <- normalizePath(".", mustWork = TRUE)
  if (!file.exists(file.path(root, "CMakeLists.txt"))) stop("Run from the repository or via Rscript validation/reference/<script>.R")
  root
}

reference_fixture_dir <- function(root = reference_root()) {
  file.path(root, "validation/fixtures/backend")
}

reference_sha256 <- function(path) {
  if (!requireNamespace("digest", quietly = TRUE)) stop("Reference generation/verification requires the developer dependency 'digest'.")
  vapply(path, function(p) digest::digest(file = reference_current_path(p), algo = "sha256", serialize = FALSE), character(1), USE.NAMES = FALSE)
}

reference_requests <- function(root) {
  dget(file.path(root, "validation/migration/output-requests.R"))
}

# Current report hashes identify current code across both repositories.
reference_r_root <- function(root = reference_root()) {
  path <- Sys.getenv("RKALOS_SOURCE", file.path(dirname(root), "rkalos"))
  path <- normalizePath(path, mustWork = TRUE)
  if (!file.exists(file.path(path, "DESCRIPTION")) ||
      read.dcf(file.path(path, "DESCRIPTION"))[1L, "Package"] != "rkalos") stop("RKALOS_SOURCE must identify rkalos source")
  path
}
reference_current_path <- function(path) {
  root <- reference_root()
  prefix <- paste0(root, "/")
  if (!startsWith(path, prefix)) return(path)
  relative <- substring(path, nchar(prefix) + 1L)
  if (startsWith(relative, "src/core/")) return(file.path(root, sub("^src/core/", "include/kalos/", relative)))
  if (startsWith(relative, "R/") || grepl("^src/(rcpp_|RcppExports)", relative))
    return(file.path(reference_r_root(root), relative))
  path
}
reference_capture_sha256 <- function(path) {
  root <- reference_root()
  relative <- substring(path, nchar(root) + 2L)
  if (startsWith(relative, "validation/reference/")) {
    relative <- sub("^validation/reference/", "tools/reference/", relative)
    path <- file.path(root, "validation/migration/source-snapshot", relative)
  }
  reference_sha256(path)
}

# Preserve the historical fixture source and adapt its single namespace lookup
# in a private lexical environment. No fixture source or hash is rewritten.
reference_source_fixture <- function(file, local = FALSE, ...) {
  destination <- if (isTRUE(local)) parent.frame() else if (is.environment(local)) local else .GlobalEnv
  result <- base::source(file, local = destination, ...)
  if (identical(basename(file), "arma-fit-cases.R")) {
    helper <- get("arma_fit_alternate", envir = destination, inherits = FALSE)
    environment(helper) <- list2env(list(getFromNamespace = function(x, ns)
      utils::getFromNamespace(x, if (identical(ns, "persephone3")) "rkalos" else ns)), parent = environment(helper))
    assign("arma_fit_alternate", helper, envir = destination)
  }
  invisible(result)
}
