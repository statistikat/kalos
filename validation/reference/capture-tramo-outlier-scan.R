#!/usr/bin/env Rscript
# Byte-identity capture: TRAMO round-0 outlier scan corpus (M5 item 23-B, part 2).
#
# Runs the pinned JVM scan stack per case through the PUBLIC spec-topology
# entry point (tramo.internal.OutlierDetection.create(spec, Ts, null).detect())
# and validates against TramoOutlierScanProbe.class. The spec builder pins the
# whole path: TramoSpec -> OutlierDetectionModule -> ODM -> PeriodicOutlierFactory
# -> FastOutliersDetector -> FastOutlierDetector.
#
# The unit's round-0 decision is scanned per case: the first scan's T-table,
# mad, and the selected cell under the critical value. The probe also records
# the JVM driver's later-round scans (scan events with the round index) as
# evidence for the deferred multi-round driver (D3); the replay test consumes
# only the round-0 facts.
#
# Each case runs in its OWN JVM process (run(String[])). With --repro=<dir>
# the capture is re-run and byte-compared file by file (two-process gate).
#
# Usage: env -u LD_LIBRARY_PATH env JAVA_HOME=<jdk21> \
#   _JAVA_OPTIONS='-XX:+UnlockDiagnosticVMOptions -XX:DisableIntrinsic=_dexp \
#   -XX:DisableIntrinsic=_dlog' \
#   Rscript validation/reference/capture-tramo-outlier-scan.R \
#     --output=validation/evidence/native/tramo-outlier-scan [--repro=<prior>]
#
# Output: scan-facts.csv, scan-events.csv, cases.csv, inputs.csv, metadata.R,
#         SHA256SUMS.csv.

suppressWarnings(suppressMessages(library(jdartifact)))

args <- commandArgs(trailingOnly = TRUE)
out_dir <- sub("^--output=(.*)$", "\\1", grep("^--output=", args, value = TRUE))
repro_dir <- sub("^--repro=(.*)$", "\\1", grep("^--repro=", args, value = TRUE))
if (length(out_dir) != 1) stop("usage: capture-tramo-outlier-scan.R --output=<dir> [--repro=<prior-dir>]")
if (!dir.exists(out_dir)) stop("output directory does not exist: ", out_dir)
if (length(list.files(out_dir)) != 0) stop("output directory is not empty: ", out_dir)

jar_path <- local({
    configured <- Sys.getenv("JDPLUS_JAR", "")
    if (nzchar(configured)) configured else jdartifact::find_jar()
})
stopifnot(nzchar(jar_path), file.exists(jar_path))
jar_path <- normalizePath(jar_path, mustWork = TRUE)
jar_sha <- unname(tools::md5sum(jar_path))
stopifnot(jar_sha == "39f9f28a0c4ab4b5b368557462924f54")

java_bin <- local({
    jh <- Sys.getenv("JAVA_HOME")
    cand <- if (nzchar(jh)) file.path(jh, "bin", "java") else "java"
    stopifnot(nzchar(Sys.which(cand)))
    normalizePath(cand, mustWork = TRUE)
})

probe_root <- file.path("build", "validation-reference")
probe_class <- file.path(probe_root, "TramoOutlierScanProbe.class")
stopifnot(file.exists(probe_class))

# ---------------------------------------------------------------- cases ----
# pvars: pre-registered outlier variables (position, type 0..3); pexcl:
# estimation-domain positions excluded from detection (the JVM tags the
# matching pre-registered variable {excluded=true}; out-of-domain positions
# are tagged with a no-op exclude); select: selection span (span_from,
# span_to) or NULL; ml: full ML on the ARMA parameters (removes the unit-root
# filter correction and changes the scan window).
cases <- list()
mkcase <- function(id, y, freq = 12L, maxout = 12L, maxround = 2L, cv = NA_real_,
                   ao = TRUE, ls = TRUE, so = FALSE, tc = NA_real_, robust = TRUE,
                   select = NULL, pvars = NULL, pexcl = NULL, ml = FALSE) {
    cases[[length(cases) + 1L]] <<- list(id = id, y = y, freq = freq, maxout = maxout,
        maxround = maxround, cv = cv, ao = ao, ls = ls, so = so, tc = tc, robust = robust,
        select = select, pvars = pvars, pexcl = pexcl, ml = ml)
}

seed_series <- function(n, seed, sd = 1) {
    set.seed(seed)
    rnorm(n, sd = sd)
}
monthly <- function(n, seed) {
    set.seed(seed)
    t <- seq_len(n)
    rnorm(n) + 2 * sin(2 * pi * t / 12) + 1.2 * cos(2 * pi * 2 * t / 12) +
        0.6 * sin(2 * pi * 3 * t / 12)
}
m5 <- local({
    set.seed(21202)
    y <- rnorm(96, sd = 1.2) + 6
    y[48] <- y[48] + 8; y[70] <- y[70] - 6
    y
})
m5so <- local({
    set.seed(21202)
    y <- rnorm(96, sd = 1.2) + 6
    y[61:96] <- y[61:96] + 4
    y
})
gs <- local({
    set.seed(2302)
    y <- rnorm(96) + 8
    y[48] <- y[48] + 7
    y
})

mkcase("sc_m5", y = m5)
mkcase("sc_gs", y = gs)
mkcase("sc_ao", y = m5, ls = FALSE)
mkcase("sc_ls", y = m5, ao = FALSE)
mkcase("sc_so", y = m5so, ao = FALSE, ls = FALSE, so = TRUE)
mkcase("sc_alsotc", y = m5, so = TRUE, tc = 0.7)
mkcase("sc_nomax", y = m5, maxout = 0L)
mkcase("sc_multi", y = m5, maxout = 5L)
mkcase("sc_rounds", y = m5, maxout = 5L, maxround = 3L)
mkcase("sc_rounds0", y = m5, maxout = 2L, maxround = 0L)
mkcase("sc_cv5", y = m5, cv = 5)
mkcase("sc_cv2", y = m5, cv = 2)
mkcase("sc_noao", y = seed_series(96L, 99L, sd = 1), maxout = 0L)     # nothing to find
mkcase("sc_clean", y = seed_series(120L, 98L), maxout = 2L)           # gate-fail path
mkcase("sc_pre48", y = m5, pvars = list(c(48, 0)))
mkcase("sc_pre48x", y = m5, pvars = list(c(48, 0)), pexcl = 48L)      # excluded cell
mkcase("sc_prels70x", y = m5, pvars = list(c(70, 1)), pexcl = 70L)
mkcase("sc_preboth_x48", y = m5, pvars = list(c(48, 0), c(70, 1)), pexcl = 48L)
mkcase("sc_pexout", y = m5, pvars = list(c(48, 0)), pexcl = 108L)     # out-of-domain tag
mkcase("sc_span", y = m5, select = c(60L, 96L))
mkcase("sc_spannone", y = m5, select = c(1L, 10L))                    # out-of-domain span
mkcase("sc_ml", y = m5, ml = TRUE)
mkcase("sc_missing", y = { z <- m5; z[30] <- NA; z[52] <- NA; z })
mkcase("sc_shortfail", y = m5[1:20], maxout = 12L)
mkcase("sc_s30", y = m5[1:30], maxout = 12L)
mkcase("sc_robustoff", y = m5, robust = FALSE)

# ----------------------------------------------------------------- run ----
literal <- function(x) {
    if (is.null(x)) return("NULL")
    if (length(x) == 1 && is.logical(x)) return(if (x) "TRUE" else "FALSE")
    if (is.na(x)) return("NA")
    if (is.character(x)) return(sprintf('"%s"', x))
    format(x, digits = 17)
}
r_vector <- function(v) {
    if (length(v) == 0) return("numeric(0)")
    paste0("c(", paste(vapply(v, literal, ""), collapse = ", "), ")")
}
r_pair_list <- function(ps) {
    if (is.null(ps) || length(ps) == 0) return("NULL")
    paste0("list(", paste(vapply(ps, function(p)
        paste0("c(", literal(p[1]), ", ", literal(p[2]), ")"), ""), collapse = ", "), ")")
}

encode_case <- function(case) {
    c(
        paste0("id=", case$id),
        paste0("y=", r_vector(case$y)),
        paste0("freq=", case$freq),
        paste0("maxout=", case$maxout),
        paste0("maxround=", case$maxround),
        paste0("cv=", literal(case$cv)),
        paste0("ao=", if (case$ao) "TRUE" else "FALSE"),
        paste0("ls=", if (case$ls) "TRUE" else "FALSE"),
        paste0("so=", if (case$so) "TRUE" else "FALSE"),
        paste0("tc=", literal(case$tc)),
        paste0("robust=", if (case$robust) "TRUE" else "FALSE"),
        paste0("select=", if (is.null(case$select)) "NULL"
               else paste0("c(", literal(case$select[1]), ", ", literal(case$select[2]), ")")),
        paste0("pvars=", r_pair_list(case$pvars)),
        paste0("pexcl=", if (is.null(case$pexcl) || length(case$pexcl) == 0) "numeric(0)"
               else r_vector(as.integer(unlist(case$pexcl)))),
        paste0("ml=", if (case$ml) "TRUE" else "FALSE")
    )
}

run_java <- function(lines, tag) {
    input <- paste0(lines, collapse = "\n")
    p <- paste0("|", input, "\n")
    res <- system2(java_bin,
        c("-cp", shQuote(probe_root), "TramoOutlierScanProbe", shQuote(jar_path)),
        stdin = p, stdout = TRUE, stderr = TRUE, wait = TRUE)
    status <- attr(res, "status")
    if (!is.null(status) && status != 0) {
        stop("probe failed for ", tag, " (status ", status, "):\n  ",
             paste(head(res, 40), collapse = "\n  "))
    }
    res
}

facts <- list()
events <- list()
inputs <- character(0)
for (case in cases) {
    lines <- encode_case(case)
    inputs <- c(inputs, lines, "")
    out <- run_java(lines, case$id)
    hdr <- grep("^tramo-outlier-scan", out, value = TRUE)
    if (length(hdr) != 1) stop("missing header for ", case$id)
    fields <- strsplit(hdr, "\\|")[[1]]
    fact_row <- data.frame(
        case_id = case$id,
        field = vapply(fields[-1], function(f) strsplit(f, "=")[[1]][1], ""),
        value = vapply(fields[-1], function(f) sub("^[^=]*=", "", f), ""),
        stringsAsFactors = FALSE)
    ev <- grep("^evt ", out, value = TRUE)
    ev <- ev[startsWith(ev, paste0("evt ", case$id, "|"))]
    for (e in ev) {
        parts <- strsplit(sub("^evt ", "", e), "\\|")[[1]]
        events[[length(events) + 1L]] <- data.frame(
            case_id = case$id, method = parts[2], line = as.integer(parts[3]),
            group = parts[4], key = parts[5], value = parts[6],
            stringsAsFactors = FALSE)
    }
    facts[[length(facts) + 1L]] <- fact_row
}
facts_df <- do.call(rbind, facts)
events_df <- if (length(events)) do.call(rbind, events) else
    data.frame(case_id = character(), method = character(), line = integer(),
        group = character(), key = character(), value = character())

# Raw getters only for the decision numbers.
for (f in c("scan_max_t", "scan_mad")) {
    hit <- facts_df$value[facts_df$field == f]
    bad <- hit[!is.na(hit) & hit != "nan" &
        !grepl("^[+-][0-9]\\.[0-9a-fpP+-]*$", hit) &
        !grepl("^[+-][0-9]\\.[0-9]+[eE][+-][0-9]+$", hit)]
    if (length(bad)) stop("non-raw value in field ", f, ": ", paste(unique(bad), collapse = ", "))
}

decisions <- facts_df$value[facts_df$field == "decision"]
stopifnot(!any(is.na(decisions)))
if (!any(decisions == "NotDone")) {
    stop("decision-diversity gate: no NotDone cases in the corpus")
}
if (!any(decisions == "Done")) {
    stop("decision-diversity gate: no Done cases in the corpus")
}

write.csv(facts_df, file.path(out_dir, "scan-facts.csv"), row.names = FALSE, na = "")
write.csv(events_df, file.path(out_dir, "scan-events.csv"), row.names = FALSE, na = "")

cases_csv <- do.call(rbind, lapply(cases, function(c) data.frame(
    case_id = c$id, n = length(c$y), freq = c$freq, maxout = c$maxout,
    maxround = c$maxround, ao = c$ao, ls = c$ls, so = c$so,
    robust = c$robust, ml = c$ml,
    has_cv = !is.na(c$cv), has_tc = !is.na(c$tc),
    has_select = !is.null(c$select),
    n_pre_vars = length(c$pvars), n_pre_excluded = length(c$pexcl),
    stringsAsFactors = FALSE)))
write.csv(cases_csv, file.path(out_dir, "cases.csv"), row.names = FALSE, na = "")
con <- file(file.path(out_dir, "inputs.csv"), "w")
writeLines(inputs, con, sep = "\n")
close(con)

scan_t <- sum(events_df$group == "T" & events_df$line == 1078 &
    grepl("^T[0-9]+$", events_df$key))
writeLines(c(
    "# TRAMO round-0 outlier scan capture metadata (M5 item 23-B, part 2).",
    paste0("capture_script <- \"validation/reference/capture-tramo-outlier-scan.R\""),
    paste0("probe_class <- \"validation/reference/TramoOutlierScanProbe.java\""),
    paste0("artifact <- \"", basename(jar_path), "\""),
    paste0("artifact_md5 <- \"", jar_sha, "\""),
    paste0("java <- \"", java_bin, "\""),
    paste0("jvm_options <- \"-XX:+UnlockDiagnosticVMOptions -XX:DisableIntrinsic=_dexp -XX:DisableIntrinsic=_dlog\""),
    "n_cases <- 26",
    "topology <- \"spec builder: TramoSpec -> tramo.internal.OutlierDetection -> internal OutliersDetectionModule -> FastOutliersDetector (both topologies: this is the spec path; the userapi robust topology is captured by the transform corpus)\"",
    "unit_scope <- \"round-0 only: scan 0 T-table (line 1078), mad, selection under the critical value; later-round scan events are recorded as D3 evidence, not replayed\"",
    "coordinate_semantics <- \"TsPeriod.dateAt(n) is epoch-anchored; selection spans and pexcl use estimation-domain positions (est = full span minus the 12-month first lag for this family); out-of-domain pexcl is tagged {excluded=true} with a no-op exclude; an out-of-domain selection span collapses the bounds and finds nothing (sc_spannone)\"",
    paste0("round0_cells <- ", scan_t),
    paste0("decision_counts <- c(",
        paste(sprintf('"%s" = %d', names(table(decisions)), as.integer(table(decisions))),
        collapse = ", "), ")")
), con = file(file.path(out_dir, "metadata.R")), sep = "\n")

files <- sort(list.files(out_dir))
sums <- tools::md5sum(file.path(out_dir, files))
write.csv(data.frame(file = files, md5 = unname(sums)),
    file.path(out_dir, "SHA256SUMS.csv"), row.names = FALSE)

# ------------------------------------------------------- repro gate ----
if (length(repro_dir) == 1) {
    files_a <- sort(list.files(repro_dir))
    files_b <- sort(list.files(out_dir))
    if (!identical(files_a, files_b)) {
        stop("repro file set differs:\n  only in prior: ",
            paste(setdiff(files_a, files_b), collapse = ", "), "\n  only in rerun: ",
            paste(setdiff(files_b, files_a), collapse = ", "))
    }
    bad <- character(0)
    for (f in files_b) {
        a <- readBin(file.path(repro_dir, f), "raw", n = file.size(file.path(repro_dir, f)))
        b <- readBin(file.path(out_dir, f), "raw", n = file.size(file.path(out_dir, f)))
        if (!identical(a, b)) bad <- c(bad, f)
    }
    if (length(bad)) {
        stop("cross-process byte-identity FAILED for: ", paste(bad, collapse = ", "))
    }
    message("repro gate: ", length(files_b), " files byte-identical across JVM processes")
}

message("TRAMO outlier scan capture complete: ", length(cases), " cases, ",
    nrow(facts_df), " fact rows -> ", out_dir)
