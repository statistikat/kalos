# Pinned-reference capture for item 21-C: the outlier critical-value chain,
# the ExactSingleOutlierDetector scan unit and the ExactOutliersDetector
# forward-selection driver of the pinned jdplus 3.9.0 stack (rjd3 jars)
# through the public-API observer OutlierScoringProbe.java. The observer runs
# the real pinned classes (no scoring, scan or selection algorithm is
# re-implemented here); this script only builds deterministic inputs, parses
# the printed outputs and writes the corpus. The pinned driver call site is
# the x13 OutliersDetectionModule kernel: processor X13Utility.processor(true,
# 1.0E-7) (ML, Multiple starting point), factories AO/LS/TC/SO in order, cv
# from X13Utility.calcCv(n) (module default) or a caller value, maxRound /
# maxOutliers decoder defaults 50/30. Parameters are stored in the KALOS
# convention (pinned AR signs flipped, [ar sar ma sma]). Pass --repro=<dir> to
# cross-check the written corpus against an earlier capture of the same
# cases from an independent JVM process.
args <- commandArgs(trailingOnly = FALSE)
script <- sub('^--file=', '', args[startsWith(args, '--file=')][[1L]])
tooling <- dirname(normalizePath(script)); source(file.path(tooling, 'common.R'))
root <- reference_root()
args <- commandArgs(trailingOnly = TRUE)
repro <- ''
if (any(startsWith(args, '--repro='))) {
  repro <- normalizePath(sub('^--repro=', '', args[startsWith(args, '--repro=')][[1L]]),
    mustWork = TRUE)
  args <- args[!startsWith(args, '--repro=')]
}
if (length(args) != 1L || !startsWith(args, '--output='))
  stop('Usage: capture-outlier-scoring.R [--repro=<captured directory>] --output=<existing empty directory>')
output <- normalizePath(sub('^--output=', '', args), mustWork = TRUE)
if (!dir.exists(output) || length(list.files(output, all.files = TRUE, no.. = TRUE)))
  stop('Output must be empty')
suppressWarnings(suppressMessages({
  library(rjd3jars); library(rjd3toolkit); library(rJava)
}))
.jinit()
# The rJava JVM fixes the loadable class-file version; compile the observer
# for exactly that release (javac on PATH may be newer).
jvm_major <- as.integer(sub('^([0-9]+).*', '\\1',
  .jcall('java/lang/System', 'S', 'getProperty', 'java.version')))
probe <- tempfile('kalos-outlier-scoring-'); dir.create(probe)
jars <- c(list.files(system.file('java', package = 'rjd3jars'), pattern = '[.]jar$',
  full.names = TRUE),
  list.files(system.file('java', package = 'rjd3x13'), pattern = '[.]jar$',
    full.names = TRUE))
classpath <- paste(jars, collapse = .Platform$path.sep)
javac <- Sys.which('javac'); if (!nzchar(javac)) stop('javac is required on PATH')
status <- system2(javac, c('--release', jvm_major, '-d', shQuote(probe),
  '-cp', shQuote(classpath),
  shQuote(file.path(tooling, 'OutlierScoringProbe.java'))))
if (status != 0L) stop('probe compilation failed')
.jaddClassPath(c(jars[grep('rjd3x13', jars)], probe))
if (inherits(try(.jfindClass('OutlierScoringProbe'), silent = TRUE), 'try-error'))
  stop('observer class not loadable in the rJava JVM')

g <- function(x) sprintf('%.17g', x)
ias <- function(v) .jarray(as.integer(v))
das <- function(v) .jarray(as.double(v))
empty_i <- function() ias(integer(0))
empty_d <- function() das(numeric(0))
j <- function(v) paste(v, collapse = ';')

# The probe speaks the pinned raw SarimaModel.parameters() convention; the
# corpus stores KALOS convention: raw [phi bphi theta btheta] = [-ar -sar ma sma].
raw_blocks <- function(ar, sar, ma, sma)
  list(phi = -as.double(ar), bphi = -as.double(sar),
       theta = as.double(ma), btheta = as.double(sma))

grids <- list(); scans_in <- list(); scans_out <- list()
drivers_in <- list(); drivers_out <- list()

grid_case <- function(kind, input, res) {
  f <- strsplit(res, '|', fixed = TRUE)[[1L]]
  grids[[length(grids) + 1L]] <<- data.frame(kind = kind,
    input = g(input), value = if (f[[1L]] == 'ok') f[[2L]] else '',
    value2 = if (length(f) > 2L) f[[3L]] else '',
    error = if (f[[1L]] == 'ok') '' else substr(res, 7L, 200L),
    stringsAsFactors = FALSE)
}

# -- critical-value grids ---------------------------------------------------------
for (x in c(0, 0.1, 0.5, 0.66291, 1, 1.6449, 2, 2.5, 3, 4, 5,
            5.656854249492381, 6, 8, 10, 12, 26, 27, 40,
            -0.1, -0.5, -0.66291, -1, -2, -3, -4, -5, -5.656854249492381,
            -6, -8, -10, -12, -26, -27, 40, NaN, Inf, -Inf))
  grid_case('cdf', x, .jcall('OutlierScoringProbe', 'S', 'cdf', as.double(x)))
for (p in c(1e-15, 1e-12, 1e-9, 1e-6, 0.01, 0.025, 0.05, 0.1, 0.25, 0.4, 0.5,
            0.6, 0.75, 0.9, 0.95, 0.975, 0.99, 0.999,
            1 - 1e-6, 1 - 1e-9, 1 - 1e-12, 1 - 1e-15,
            0, 1, -0.1, 1.1, 5e-16, NaN))
  grid_case('quantile', p, .jcall('OutlierScoringProbe', 'S', 'quantile', as.double(p)))
for (n in c(1, 2, 3, 5, 10, 24, 36, 50, 96, 99, 100, 101, 120, 156, 240, 360,
            512, 1000))
  grid_case('cv', n, .jcall('OutlierScoringProbe', 'S', 'calccv', as.integer(n)))

# -- input recipes ---------------------------------------------------------------
rw_monthly <- function(n, seed) {
  set.seed(seed); as.numeric(50 + cumsum(rnorm(n, 0, .8)) + rnorm(n))
}
growth_seasonal <- function(n, seed) {
  set.seed(seed); t <- seq_len(n)
  as.numeric(exp(cumsum(rnorm(n, .01, .02)) + .2 * sin(2 * pi * t / 12)))
}
rw_quarterly <- function(n, seed) {
  set.seed(seed); as.numeric(100 + cumsum(rnorm(n, .3, 1.5)) + rnorm(n))
}

scan_case <- function(id, note, y, period, d, D, blocks, types, tcrate,
    lbound, ubound, excl = list()) {
  n <- length(y)
  res <- .jcall('OutlierScoringProbe', 'S', 'scan', as.integer(period),
    as.integer(d), as.integer(D), das(y), empty_d(), 0L,
    das(blocks$phi), das(blocks$bphi), das(blocks$theta), das(blocks$btheta),
    ias(types), as.double(tcrate), as.integer(lbound), as.integer(ubound),
    ias(vapply(excl, function(e) e[[1L]], numeric(1))),
    ias(vapply(excl, function(e) e[[2L]], numeric(1))))
  f <- strsplit(res, '|', fixed = TRUE)[[1L]]
  if (!identical(f[[1L]], 'ok')) {
    # Pinned escape recorded as an error row (e.g. the all-NaN T table makes
    # getMaxTStat index T(-1, -1): the pinned throw the Kalos sentinel stop
    # documents as its deviation).
    scans_out[[length(scans_out) + 1L]] <<- data.frame(id = id, ok = 'error',
      max_t = '', max_type = '', max_pos = '', stringsAsFactors = FALSE)
    scans_tbl[[length(scans_tbl) + 1L]] <<- data.frame(id = id,
      t_table = '', coef_table = '', allowed = '', stringsAsFactors = FALSE)
    scans_in[[length(scans_in) + 1L]] <<- data.frame(id = id, note = note,
      period = as.integer(period), d = as.integer(d), D = as.integer(D),
      n = as.integer(n), y = j(vapply(y, g, character(1))),
      ar = j(vapply(blocks$phi * -1, g, character(1))),
      sar = j(vapply(blocks$bphi * -1, g, character(1))),
      ma = j(vapply(blocks$theta, g, character(1))),
      sma = j(vapply(blocks$btheta, g, character(1))),
      types = j(types), tcrate = g(tcrate),
      lbound = as.integer(lbound), ubound = as.integer(ubound),
      excl_pos = j(vapply(excl, function(e) e[[1L]], numeric(1))),
      excl_type = j(vapply(excl, function(e) e[[2L]], numeric(1))),
      stringsAsFactors = FALSE)
    return(invisible(data.frame(ok = FALSE)))
  }
  nf <- length(types)
  stopifnot(length(f) == 8L)
  tv <- as.numeric(strsplit(f[[6L]], ';')[[1L]])
  cv <- as.numeric(strsplit(f[[7L]], ';')[[1L]])
  av <- as.numeric(strsplit(f[[8L]], ';')[[1L]])
  stopifnot(length(tv) == nf * n, length(cv) == nf * n, length(av) == nf * n)
  scans_out[[length(scans_out) + 1L]] <<- data.frame(id = id, ok = f[[2L]],
    max_t = f[[3L]], max_type = f[[4L]], max_pos = f[[5L]],
    stringsAsFactors = FALSE)
  # store tables separately (long strings)
  scans_tbl[[length(scans_tbl) + 1L]] <<- data.frame(id = id,
    t_table = j(tv), coef_table = j(cv), allowed = j(av),
    stringsAsFactors = FALSE)
  scans_in[[length(scans_in) + 1L]] <<- data.frame(id = id, note = note,
    period = as.integer(period), d = as.integer(d), D = as.integer(D),
    n = as.integer(n), y = j(vapply(y, g, character(1))),
    ar = j(vapply(blocks$phi * -1, g, character(1))),
    sar = j(vapply(blocks$bphi * -1, g, character(1))),
    ma = j(vapply(blocks$theta, g, character(1))),
    sma = j(vapply(blocks$btheta, g, character(1))),
    types = j(types), tcrate = g(tcrate),
    lbound = as.integer(lbound), ubound = as.integer(ubound),
    excl_pos = j(vapply(excl, function(e) e[[1L]], numeric(1))),
    excl_type = j(vapply(excl, function(e) e[[2L]], numeric(1))),
    stringsAsFactors = FALSE)
  invisible(data.frame(max_t = as.numeric(f[[3L]]),
    max_type = as.integer(f[[4L]]), max_pos = as.integer(f[[5L]]),
    ok = f[[2L]] == '1'))
}
scans_tbl <- list()

driver_case <- function(id, note, y, period, d, D, p, q, bp, bq, types, tcrate,
    cv, maxround, maxoutliers, lbound, ubound) {
  n <- length(y)
  res <- .jcall('OutlierScoringProbe', 'S', 'driver', as.integer(period),
    as.integer(d), as.integer(D), das(y), empty_d(), 0L,
    das(rep(0, p)), das(rep(0, bp)), das(rep(0, q)), das(rep(0, bq)),
    ias(types), as.double(tcrate), as.double(cv), as.integer(maxround),
    as.integer(maxoutliers), as.integer(lbound), as.integer(ubound),
    empty_i(), empty_i())
  f <- strsplit(res, '|', fixed = TRUE)[[1L]]
  if (!identical(f[[1L]], 'ok')) stop(sprintf('driver case %s failed: %s', id, res))
  stopifnot(length(f) == 7L)
  drivers_out[[length(drivers_out) + 1L]] <<- data.frame(id = id, note = note,
    changed = f[[2L]], n_outliers = f[[3L]], outliers = f[[4L]],
    tstats = f[[5L]], xcount = f[[6L]], final_params = f[[7L]],
    stringsAsFactors = FALSE)
  drivers_in[[length(drivers_in) + 1L]] <<- data.frame(id = id,
    period = as.integer(period), d = as.integer(d), D = as.integer(D),
    p_ar = as.integer(p), p_ma = as.integer(q), p_sar = as.integer(bp),
    p_sma = as.integer(bq),
    n = as.integer(n), y = j(vapply(y, g, character(1))),
    types = j(types), tcrate = g(tcrate), cv = g(cv),
    maxround = as.integer(maxround), maxoutliers = as.integer(maxoutliers),
    lbound = as.integer(lbound), ubound = as.integer(ubound),
    stringsAsFactors = FALSE)
  invisible(f)
}

# -- scan cases (deterministic linear algebra) -------------------------------------
b_sd1 <- raw_blocks(0.55, numeric(0), 0.25, numeric(0))   # (2,1,1)(0,0,0)12
b_sd2 <- raw_blocks(0.6, 0.0, -0.3, 0.35)                 # (1,1,1)(0,1,1)12
b_qtr <- raw_blocks(0.4, 0.0, numeric(0), 0.3)            # (1,1,0)(0,1,1)4

y_spikes <- rw_monthly(144, 2301); y_spikes[40] <- y_spikes[40] + 25
y_spikes[90] <- y_spikes[90] - 20
w1 <- scan_case('scan_ao', 'AO only, two spikes, (2,1,1)(0,0,0)12',
  y_spikes, 12, 1, 0, b_sd1, 0, 0.7, 0, 144)
y_seas <- growth_seasonal(132, 2302)
y_seas[60] <- y_seas[60] + 2.2; y_seas[96] <- y_seas[96] - 1.8
w2 <- scan_case('scan_all4', 'AO LS TC SO, seasonal growth, (1,1,1)(0,1,1)12',
  y_seas, 12, 1, 1, b_sd2, c(0, 1, 2, 3), 0.7, 0, 132)
scan_case('scan_excl', 'scan_all4 with the winner and one neighbour excluded',
  y_seas, 12, 1, 1, b_sd2, c(0, 1, 2, 3), 0.7, 0, 132,
  excl = list(list(w2$max_pos, w2$max_type),
              list(w2$max_pos + 2L, w2$max_type)))
scan_case('scan_bounds', 'AO LS with setBounds(20, 70) zeros outside',
  y_spikes, 12, 1, 0, b_sd1, c(0, 1), 0.7, 20, 70)
y_quarter <- rw_quarterly(96, 2303); y_quarter[50] <- y_quarter[50] + 14
scan_case('scan_quarterly', 'quarterly AO TC SO, (1,1,0)(0,1,1)4, TC rate pow(0.7,3)',
  y_quarter, 4, 1, 1, b_qtr, c(0, 2, 3), 0.7, 0, 96)
scan_case('scan_sentinel', 'constant response: Mad zero, all-NaN T, searchMax sentinel',
  rep(50, 120), 12, 1, 1, b_sd2, c(0, 1, 2, 3), 0.7, 0, 120)

# -- driver cases (full pinned estimation) -------------------------------------------
cv144 <- as.numeric(strsplit(.jcall('OutlierScoringProbe', 'S', 'calccv', 144L),
  '|', fixed = TRUE)[[1L]][[2L]])
cv132 <- as.numeric(strsplit(.jcall('OutlierScoringProbe', 'S', 'calccv', 132L),
  '|', fixed = TRUE)[[1L]][[2L]])
cv96 <- as.numeric(strsplit(.jcall('OutlierScoringProbe', 'S', 'calccv', 96L),
  '|', fixed = TRUE)[[1L]][[2L]])

y_d1 <- rw_monthly(144, 2311); y_d1[40] <- y_d1[40] + 25; y_d1[95] <- y_d1[95] - 20
driver_case('drv_ao2', 'two AO spikes, AO only, calcCv(144), module limits',
  y_d1, 12, 1, 1, 1, 1, 0, 1, 0, 0.7, cv144, 50, 30, 0, 144)
y_d2 <- rw_monthly(144, 2312); y_d2[70:144] <- y_d2[70:144] + 12; y_d2[20] <- y_d2[20] + 18
driver_case('drv_ls', 'late level shift plus an AO, AO+LS, calcCv(144), maxOutliers 6',
  y_d2, 12, 1, 1, 1, 1, 0, 1, c(0, 1), 0.7, cv144, 50, 6, 0, 144)
y_d3 <- growth_seasonal(132, 2313); y_d3[44] <- y_d3[44] * 1.9
y_d3[77:88] <- y_d3[77:88] * 0.75
driver_case('drv_all4', 'growth+seasonal with a spike and a quarter-long break, four factories',
  y_d3, 12, 1, 1, 1, 1, 0, 1, c(0, 1, 2, 3), 0.7, cv132, 50, 30, 0, 132)
y_d4 <- rw_monthly(132, 2314)
driver_case('drv_clean', 'no outliers injected: selection must stop below cv',
  y_d4, 12, 1, 1, 1, 1, 0, 1, c(0, 1, 2, 3), 0.7, cv132, 50, 30, 0, 132)
y_d5 <- rw_monthly(144, 2315)
y_d5[30] <- y_d5[30] + 22; y_d5[70] <- y_d5[70] - 22; y_d5[110] <- y_d5[110] + 22
driver_case('drv_maxout3', 'three spikes with maxOutliers 3 (stop at the limit)',
  y_d5, 12, 1, 1, 1, 1, 0, 1, 0, 0.7, cv144, 50, 3, 0, 144)
driver_case('drv_cv4', 'same spikes with cv 4.0 and AO+TC factories',
  y_d5, 12, 1, 1, 1, 1, 0, 1, c(0, 2), 0.7, 4.0, 50, 30, 0, 144)
y_d6 <- rw_quarterly(96, 2316); y_d6[42] <- y_d6[42] + 12
driver_case('drv_quarterly', 'quarterly spike, AO TC SO, calcCv(96)',
  y_d6, 4, 1, 1, 1, 0, 0, 1, c(0, 2, 3), 0.7, cv96, 50, 30, 0, 96)
driver_case('drv_span', 'spikes with a pinned scan window [12, 132), AO+LS',
  y_d1, 12, 1, 1, 1, 1, 0, 1, c(0, 1), 0.7, cv144, 50, 30, 12, 132)

grids <- do.call(rbind, grids)
scans_in <- do.call(rbind, scans_in); scans_out <- do.call(rbind, scans_out)
scans_tbl <- do.call(rbind, scans_tbl)
scans_out$t_table <- scans_tbl$t_table; scans_out$coef_table <- scans_tbl$coef_table
scans_out$allowed <- scans_tbl$allowed
drivers_in <- do.call(rbind, drivers_in); drivers_out <- do.call(rbind, drivers_out)

write.csv(grids, file.path(output, 'grids.csv'), row.names = FALSE)
write.csv(scans_in, file.path(output, 'scan-inputs.csv'), row.names = FALSE)
write.csv(scans_out, file.path(output, 'scan-cases.csv'), row.names = FALSE)
write.csv(drivers_in, file.path(output, 'driver-inputs.csv'), row.names = FALSE)
write.csv(drivers_out, file.path(output, 'driver-cases.csv'), row.names = FALSE)

repro_note <- '# double capture: run this script twice into separate directories and
# compare all csv files; identical bytes prove JVM-process reproducibility.'
if (nzchar(repro)) {
  same <- all(vapply(c('grids.csv', 'scan-inputs.csv', 'scan-cases.csv',
    'driver-inputs.csv', 'driver-cases.csv'), function(f)
    identical(readLines(file.path(output, f)),
              readLines(file.path(repro, f))), logical(1)))
  if (!same) stop('reproduction capture differs from ', repro)
  repro_note <- sprintf(
    '# double capture: byte-identical csv files against the independent JVM
# capture in %s (captured_at differs only in metadata.R).', repro)
}
writeLines(c(
  '# Capture provenance for the item 21-C outlier-scoring corpus.',
  '# Oracle: pinned jdplus 3.9.0 classes through the public-API observer',
  '# OutlierScoringProbe.java (no scoring, scan or selection algorithm there).',
  '# Grids: Utility.intProbability(x, Lower), Normal.getProbabilityInverse(p,',
  '# Lower) and X13Utility.calcCv(n) next to advancedComputer(0.05).',
  '# Scan: ExactSingleOutlierDetector(Mad(false), Ansley default, MLJ-ML',
  '# residuals), factories AO/LS(zero-ended)/TC(rate(freq,tcrate))/SO(zero-',
  '# ended), prepare(n), setBounds(l, r), exclusions, then process(regarima);',
  '# T/coefficient/allowed tables printed column-major.',
  '# Driver: ExactOutliersDetector.builder with X13Utility.processor(true,',
  '# 1.0E-7), caller cv/maxRound/maxOutliers (module decoder defaults 50/30),',
  '# process(regarima, null) - the fully free map of the qualified 21-A',
  '# convention. changed, kept (position, factory) pairs, the final tstats',
  '# decision vector (reflective read, no public getter), the final design',
  '# column count and final SarimaModel.parameters() are printed.',
  '# Parameters are KALOS convention: stored [ar sar ma sma] with the AR signs',
  '# flipped from the pinned raw parameters; driver cases start from zero',
  '# coefficients at the orders pinned in driver-inputs.csv (p_ar/...).',
  repro_note,
  sprintf('# R %s', getRversion()),
  sprintf('# rjd3jars %s', as.character(packageVersion('rjd3jars'))),
  sprintf('# rJava JVM %s (observer compiled with javac --release %d)',
    .jcall('java/lang/System', 'S', 'getProperty', 'java.version'), jvm_major),
  sprintf('# capture-outlier-scoring.R sha256 %s', reference_sha256(script)),
  sprintf('# OutlierScoringProbe.java sha256 %s',
    reference_sha256(file.path(tooling, 'OutlierScoringProbe.java'))),
  sprintf('captured_at <- "%s"', format(Sys.time(), tz = 'UTC'))),
  file.path(output, 'metadata.R'))
files <- sort(setdiff(list.files(output, full.names = TRUE),
  file.path(output, 'SHA256SUMS.csv')))
write.csv(data.frame(file = basename(files),
  sha256 = reference_sha256(files)),
  file.path(output, 'SHA256SUMS.csv'), row.names = FALSE)
cat('grids:', nrow(grids), 'scans:', nrow(scans_in), 'drivers:', nrow(drivers_in), '\n')
