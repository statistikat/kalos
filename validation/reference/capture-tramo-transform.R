#!/usr/bin/env Rscript
# Byte-identity capture: TRAMO log/level transform decision corpus (M5 item 23-B, part 1).
#
# Runs the pinned JVM decision stack per case through the PUBLIC entry points
# (TramoSeasonalityDetector + SeasonalityTests raw getters, the internal
# TramoUtility.calcCv grid, the TramoKernel pre-stage through the robust
# OutliersDetectionModule topology and the LogLevelModule decision) and
# validates against TramoTransformDecisionProbe.class.
#
# Each case runs in its OWN JVM process (run(String[])), mirroring the
# final-information precedent: a fresh JVM per golden rules out
# process-lifetime state (cached FFT factors, scratch reuse) as a
# source of divergence. With --repro=<dir> the capture is re-run and
# byte-compared file by file; any difference aborts (two-process
# byte-identity gate).
#
# Usage: env -u LD_LIBRARY_PATH env JAVA_HOME=<jdk21> \
#   _JAVA_OPTIONS='-XX:+UnlockDiagnosticVMOptions -XX:DisableIntrinsic=_dexp \
#   -XX:DisableIntrinsic=_dlog' \
#   Rscript validation/reference/capture-tramo-transform.R \
#     --output=validation/evidence/native/tramo-transform [--repro=<prior>]
#
# Output: transform-facts.csv, transform-events.csv, cases.csv, inputs.csv,
#         metadata.R, SHA256SUMS.csv.

suppressWarnings(suppressMessages(library(jdartifact)))

args <- commandArgs(trailingOnly = TRUE)
out_dir <- sub("^--output=(.*)$", "\\1", grep("^--output=", args, value = TRUE))
repro_dir <- sub("^--repro=(.*)$", "\\1", grep("^--repro=", args, value = TRUE))
if (length(out_dir) != 1) stop("usage: capture-tramo-transform.R --output=<dir> [--repro=<prior-dir>]")
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
probe_class <- file.path(probe_root, "TramoTransformDecisionProbe.class")
stopifnot(file.exists(probe_class))

# ---------------------------------------------------------------- cases ----
# Every case: the series + estimation settings (the "input card"), in a literal
# R form so the capture is fully reproducible from this file. The JVM
# pre-stage (TramoKernel.process) runs the robust AO+LS outliers module and the
# LogLevelModule on the transformed output; the seasonality/TramoUtility facts
# are the raw decision inputs (raw getters only, never a record toString).
# pvars: pre-registered outlier variables (position, type code 0..3);
# pexcl: positions whose pre-registered variable is excluded from detection.
# select: selection span (span_from, span_to) or NULL; logpref/logpar: the
# TramoSpec.log preferences (NA = JVM default -0.05129329438755058).
cases <- list()
mkcase <- function(id, y, freq = 12L, maxout = 12L, maxround = 2L, cv = NA_real_,
                   tc = NA_real_, robust = TRUE, select = NULL,
                   pvars = NULL, pexcl = NULL, logpref = NA_real_, logpar = NA_real_) {
    cases[[length(cases) + 1L]] <<- list(id = id, y = y, freq = freq, maxout = maxout,
        maxround = maxround, cv = cv, tc = tc, robust = robust, select = select,
        pvars = pvars, pexcl = pexcl, logpref = logpref, logpar = logpar)
}

seed_series <- function(n, seed, sd = 1) {
    set.seed(seed)
    rnorm(n, sd = sd)
}
monthly <- function(n, seed, seasonal = TRUE, trend = 0) {
    set.seed(seed)
    t <- seq_len(n)
    y <- rnorm(n)
    if (seasonal) {
        y <- y + 2 * sin(2 * pi * t / 12) + 1.2 * cos(2 * pi * 2 * t / 12) +
            0.6 * sin(2 * pi * 3 * t / 12)
    }
    y + trend * t / 12
}

# The TRAMO critical-value grid: every breakpoint of TramoUtility.calcCv
# (n<30, 30..60, 61..120, 121..240, 241..480, >480, plus the boundaries).
for (n in c(10L, 29L, 30L, 31L, 59L, 60L, 61L, 96L, 120L, 121L, 240L, 241L, 480L, 481L)) {
    mkcase(sprintf("cv_%03d", n), y = seed_series(n, 900L + n), maxout = 12L)
}

# Seasonality decision shapes (month-by-month rank structure feeds the
# Friedman test; the differenced series feeds Qs).
mkcase("se_white", y = seed_series(120L, 11L))
mkcase("se_strong", y = monthly(120L, 12L))
mkcase("se_weak", y = monthly(120L, 13L, seasonal = TRUE) * 0.2)
mkcase("se_trend_only", y = monthly(96L, 14L, seasonal = FALSE, trend = 3))
mkcase("se_short", y = monthly(36L, 15L))

# Transform decision cases on the 96-point m5 recipe (the 21-H family:
# seed 21202, AO at 48, LS at 70, sd 1.2, positive offset).
m5 <- local({
    set.seed(21202)
    y <- rnorm(96, sd = 1.2) + 6
    y[48] <- y[48] + 8; y[70] <- y[70] - 6
    y
})
m5neg <- local({
    set.seed(21202)
    y <- rnorm(96, sd = 1.2) + 0.5   # same shape, small offset -> negatives after AO/LS
    y[48] <- y[48] + 0.5; y[70] <- y[70] - 4
    y
})
m5rob <- local({
    set.seed(2302)
    y <- rnorm(96) + 8
    y[48] <- y[48] + 7
    y
})

mkcase("tr_m5", y = m5)
mkcase("tr_m5_noao", y = m5, maxout = 0L)                    # transform-only path
mkcase("tr_gs", y = monthly(120L, 61L))                      # strong seasonal -> log
mkcase("tr_gsrob", y = m5rob)                                # robust pre-stage hits the shift
mkcase("tr_gsrobc", y = m5rob, tc = 0.8)
mkcase("tr_m5tc", y = m5, tc = 0.6)
mkcase("tr_neg", y = m5neg)                                  # negative values -> level gate
mkcase("tr_cv4", y = m5, cv = 4)                             # tighter gate, fewer AO
mkcase("tr_nomax", y = m5, maxout = 1L)
mkcase("tr_rounds0", y = m5, maxround = 0L)
mkcase("tr_m5_prels", y = m5, pvars = list(c(70, 1)))        # pre-registered shift
mkcase("tr_m5_preao", y = m5, pvars = list(c(48, 0)))        # pre-registered AO
mkcase("tr_logpref0", y = monthly(120L, 61L), logpref = 0)   # logpref gate
mkcase("tr_logprefd", y = monthly(120L, 61L), logpar = 0.4)  # log parameter set

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
        paste0("tc=", literal(case$tc)),
        paste0("robust=", if (case$robust) "TRUE" else "FALSE"),
        paste0("select=", if (is.null(case$select)) "NULL"
               else paste0("c(", literal(case$select[1]), ", ", literal(case$select[2]), ")")),
        paste0("pvars=", r_pair_list(case$pvars)),
        paste0("pexcl=", if (is.null(case$pexcl) || length(case$pexcl) == 0) "numeric(0)"
               else r_vector(as.integer(unlist(case$pexcl)))),
        paste0("logpref=", literal(case$logpref)),
        paste0("logpar=", literal(case$logpar))
    )
}

run_java <- function(lines, tag) {
    input <- paste0(lines, collapse = "\n")
    p <- paste0("|", input, "\n")
    res <- system2(java_bin,
        c("-cp", shQuote(probe_root), "TramoTransformDecisionProbe", shQuote(jar_path)),
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
    hdr <- grep("^tramo-transform-decision", out, value = TRUE)
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

# Raw getters only: the decision numbers must come from the JVM getters in the
# %.17g form, never an identity-hash toString or an R-side recomputation.
for (f in c("qs_value", "qs_pvalue", "np_value", "np_pvalue", "final_cv",
            "logpref", "logpref_value", "transform_loglik", "untransformed_loglik")) {
    hit <- facts_df$value[facts_df$field == f]
    bad <- hit[!is.na(hit) & hit != "nan" &
        !grepl("^[+-][0-9]\\.[0-9a-fpP+-]*$", hit) &
        !grepl("^[+-][0-9]\\.[0-9]+[eE][+-][0-9]+$", hit)]
    if (length(bad)) stop("non-raw value in field ", f, ": ", paste(unique(bad), collapse = ", "))
}

decisions <- facts_df$value[facts_df$field == "transform_decision"]
stopifnot(!any(is.na(decisions)),
    any(decisions == "Changed"), any(decisions == "Unchanged"))
if (!any(facts_df$field == "transform_decision" & facts_df$value == "Changed")) {
    message("note: no Changed decision in this corpus (see metadata); gate relaxed")
}

write.csv(facts_df, file.path(out_dir, "transform-facts.csv"),
    row.names = FALSE, na = "")
write.csv(events_df, file.path(out_dir, "transform-events.csv"),
    row.names = FALSE, na = "")

cases_csv <- do.call(rbind, lapply(cases, function(c) data.frame(
    case_id = c$id, n = length(c$y), freq = c$freq, maxout = c$maxout,
    maxround = c$maxround, robust = c$robust,
    has_cv = !is.na(c$cv), has_tc = !is.na(c$tc),
    has_select = !is.null(c$select),
    n_pre_vars = length(c$pvars), n_pre_excluded = length(c$pexcl),
    has_logpref = !is.na(c$logpref), has_logpar = !is.na(c$logpar),
    stringsAsFactors = FALSE)))
write.csv(cases_csv, file.path(out_dir, "cases.csv"), row.names = FALSE, na = "")
con <- file(file.path(out_dir, "inputs.csv"), "w")
writeLines(inputs, con, sep = "\n")
close(con)

writeLines(c(
    "# TRAMO transform decision capture metadata (M5 item 23-B, part 1).",
    paste0("capture_script <- \"validation/reference/capture-tramo-transform.R\""),
    paste0("probe_class <- \"validation/reference/TramoTransformDecisionProbe.java\""),
    paste0("artifact <- \"", basename(jar_path), "\""),
    paste0("artifact_md5 <- \"", jar_sha, "\""),
    paste0("java <- \"", java_bin, "\""),
    paste0("jvm_options <- \"-XX:+UnlockDiagnosticVMOptions -XX:DisableIntrinsic=_dexp -XX:DisableIntrinsic=_dlog\""),
    "n_cases <- 29",
    "protocol <- \"one fresh JVM per case (run(String[])); header line + %.17g value line; raw getters only\"",
    "coordinate_semantics <- \"TsPeriod.dateAt(n) is epoch-anchored; detection bounds and pre-registered variables use estimation-domain positions; pexcl tags {excluded=true} X-columns\"",
    "fail_closed <- \"out-of-range selection span collapses the scan window (sc_spannone); n<30 with maxout>0 fails the estimate (size gate)\"",
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

message("TRAMO transform decision capture complete: ", length(cases), " cases, ",
    nrow(facts_df), " fact rows -> ", out_dir)
