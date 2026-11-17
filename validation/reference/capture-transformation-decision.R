# Pinned-reference capture for item 21-B: the stage-T log/level
# transformation decision of the pinned jdplus sa-core LogLevelModule
# (rjd3 3.9.0 jars) through the public-API observer
# TransformationDecisionProbe.java. The observer runs the real module over a
# real ModelDescription/RegSarimaModelling (no decision rule or estimation
# algorithm is re-implemented here); this script only builds deterministic
# inputs, parses the printed module outputs and writes the corpus.
# The module call site is decoder-pinned: estimationPrecision 1.0E-7,
# preadjust None, outliersCorrection false (no preadjust variant in this
# slice); the AICC log correction is the TransformSpec default -2.0. The
# pinned decision processor is DemetraUtility.processor(true, 1e-7):
# maximum likelihood with the Multiple starting point. Parameters are
# stored in the KALOS convention (pinned AR signs flipped, [ar sar ma sma]).
args <- commandArgs(trailingOnly = FALSE)
script <- sub('^--file=', '', args[startsWith(args, '--file=')][[1L]])
tooling <- dirname(normalizePath(script)); source(file.path(tooling, 'common.R'))
root <- reference_root()
args <- commandArgs(trailingOnly = TRUE)
if (length(args) != 1L || !startsWith(args, '--output='))
  stop('Usage: capture-transformation-decision.R --output=<existing empty directory>')
output <- normalizePath(sub('^--output=', '', args), mustWork = TRUE)
if (!dir.exists(output) || length(list.files(output, all.files = TRUE, no.. = TRUE)))
  stop('Output must be empty')
suppressWarnings(suppressMessages({
  library(rjd3jars); library(rjd3toolkit); library(rJava)
}))
.jinit()
probe <- tempfile('kalos-transformation-decision-'); dir.create(probe)
jars <- c(list.files(system.file('java', package = 'rjd3jars'), pattern = '[.]jar$',
  full.names = TRUE),
  list.files(system.file('java', package = 'rjd3x13'), pattern = '[.]jar$',
    full.names = TRUE))
classpath <- paste(jars, collapse = .Platform$path.sep)
javac <- Sys.which('javac'); if (!nzchar(javac)) stop('javac is required on PATH')
status <- system2(javac, c('-d', shQuote(probe), '-cp', shQuote(classpath),
  shQuote(file.path(tooling, 'TransformationDecisionProbe.java'))))
if (status != 0L) stop('probe compilation failed')
.jaddClassPath(probe)

# -- deterministic input recipes ------------------------------------------------
base_series <- function(n, seed) {
  set.seed(seed)
  as.numeric(50 + cumsum(rnorm(n, 0, .8)) + rnorm(n))
}
growth_series <- function(n, seed) {
  set.seed(seed)
  as.numeric(exp(cumsum(rnorm(n, .02, .03))))
}
seasonal_growth <- function(n, seed) {
  set.seed(seed)
  t <- seq_len(n)
  as.numeric(exp(cumsum(rnorm(n, .01, .02)) + .2 * sin(2 * pi * t / 12)))
}
reflected <- function(n, seed) {
  set.seed(seed)
  as.numeric(abs(cumsum(rnorm(n, 0, 1))) + .5)
}
cal_column <- function(n) sin(2 * pi * seq_len(n) / 7) + cos(2 * pi * seq_len(n) / 5)

cases <- list(); inputs <- list()
run_case <- function(id, note, y, x, p, q, bp, bq, d, D, correction = -2) {
  x <- x[ lengths(x) > 0L ]
  res <- .jcall('TransformationDecisionProbe', 'S', 'run', 12L, as.integer(d),
    as.integer(D), .jarray(as.double(y)),
    .jarray(as.double(unlist(x))), as.integer(length(x)),
    as.integer(p), as.integer(q), as.integer(bp), as.integer(bq), correction)
  f <- strsplit(res, '|', fixed = TRUE)[[1L]]
  if (!identical(f[[1L]], 'ok'))
    stop(sprintf('case %s failed: %s', id, res))
  # strsplit drops the trailing empty design field when xcolumns is 0.
  stopifnot(length(f) == 18L || (length(f) == 17L && f[[17L]] == '0'))
  np <- as.integer(p) + as.integer(bp); nq <- as.integer(q) + as.integer(bq)
  branch_fields <- function(base) { # 6 fields: params llf obs eff npar aicc
    raw <- f[base[1L]]
    if (identical(raw, 'null'))
      return(list(null = TRUE, params = '', llf = '', obs = '', eff = '',
        npar = '', aicc = ''))
    v <- as.numeric(strsplit(raw, ';')[[1L]])
    stopifnot(identical(length(v), np + nq))
    kalos <- c(-v[seq_len(np)], v[np + seq_len(nq)])
    list(null = FALSE,
      params = paste(sprintf('%.17g', kalos), collapse = ';'),
      llf = f[base[2L]], obs = f[base[3L]], eff = f[base[4L]],
      npar = f[base[5L]], aicc = f[base[6L]])
  }
  lvl <- branch_fields(c(5L, 6L, 7L, 8L, 9L, 10L))
  lg <- branch_fields(c(11L, 12L, 13L, 14L, 15L, 16L))
  xcols <- as.integer(f[[17L]])
  xmat <- if (xcols > 0L) {
    v <- as.numeric(strsplit(f[[18L]], ';')[[1L]])
    stopifnot(length(v) == xcols * length(y))
    v
  } else {
    numeric(0)
  }
  cases[[length(cases) + 1L]] <<- data.frame(id = id, note = note,
    decision = if (f[[2L]] == '1') 'log' else 'level',
    shortcircuit = as.integer(lvl$null && lg$null),
    aicc_level = f[[3L]], aicc_log = f[[4L]],
    level_estimated = as.integer(!lvl$null), level_params = lvl$params,
    level_loglik = lvl$llf, level_obs = lvl$obs, level_eff = lvl$eff,
    level_npar = lvl$npar, level_aicc = lvl$aicc,
    log_estimated = as.integer(!lg$null), log_params = lg$params,
    log_loglik = lg$llf, log_obs = lg$obs, log_eff = lg$eff,
    log_npar = lg$npar, log_aicc = lg$aicc,
    stringsAsFactors = FALSE)
  inputs[[length(inputs) + 1L]] <<- data.frame(id = id,
    period = 12L, p_ar = as.integer(p), p_ma = as.integer(q),
    p_sar = as.integer(bp), p_sma = as.integer(bq),
    d = as.integer(d), D = as.integer(D),
    correction = sprintf('%.17g', correction),
    y = paste(sprintf('%.17g', y), collapse = ';'),
    xcolumns = xcols,
    x = paste(sprintf('%.17g', xmat), collapse = ';'),
    stringsAsFactors = FALSE)
  invisible(res)
}

y_level <- base_series(120, 2201)
y_growth <- growth_series(120, 2202)
y_seasgrowth <- seasonal_growth(120, 2203)
y_reflect <- reflected(120, 2204)
y_season <- base_series(120, 2201)
one <- rep(1, 120L); cal <- cal_column(120)

# Log chosen with a decisive gap (multiplicative growth).
run_case('growth_log', 'multiplicative growth, log decisive', y_growth,
  list(), 1, 1, 0, 0, 1, 0)
# Log chosen on a near tie: levels need a gap of at least 2.0 to win
# (measured gap +0.15: the log transform of a base-500 RW is near-linear).
run_case('bigbase_tie', 'base-500 RW, near-tie gap favors logs',
  base_series(120, 2205) + 450, list(), 1, 1, 0, 0, 1, 0)
# Levels chosen with a gap above 2.
run_case('levelish_level', 'level RW, AICC gap above 2', y_level,
  list(), 1, 1, 0, 0, 1, 0)
# Levels chosen decisively (reflected walk: log hurts the fit).
run_case('reflected_level', 'reflected walk, levels decisive', y_reflect,
  list(), 1, 1, 0, 0, 1, 0)
# Levels chosen just past the boundary (measured gap +2.5).
run_case('seasonal_dd', 'ARIMA(1,1,1)(1,1,1)12, boundary gap +2.5', y_season,
  list(), 1, 1, 1, 1, 1, 1)
# Log chosen for the seasonal multiplicative series.
run_case('seasgrowth_log', 'seasonal growth, log', y_seasgrowth,
  list(), 1, 1, 1, 1, 1, 1)
# User-variable columns flow through both branches unchanged and count in k.
run_case('const_column', 'all-ones user column, no differencing', y_level,
  list(one), 1, 1, 0, 0, 0, 0)
run_case('cal_column', 'calendar user column, no differencing', y_level,
  list(cal), 1, 1, 0, 0, 0, 0)
run_case('cal_column_d1', 'calendar user column, d = 1', y_level,
  list(cal), 1, 1, 0, 0, 1, 0)
# Pinned short-circuit: a strictly non-positive value selects levels
# without any estimation (the dcmpg scan lets NaN pass, but NaN then takes
# the pinned missing-data path, which is outside the qualified Kalos scope,
# so no NaN corpus case here).
zeroed <- y_level; zeroed[7L] <- 0
run_case('zero_shortcircuit', 'one zero value, levels unrun', zeroed,
  list(), 1, 1, 0, 0, 1, 0)
negated <- y_level; negated[7L] <- -1
run_case('negative_shortcircuit', 'one negative value, levels unrun', negated,
  list(), 1, 1, 0, 0, 1, 0)

cases <- do.call(rbind, cases); inputs <- do.call(rbind, inputs)

# -- corpus ---------------------------------------------------------------------
write.csv(cases, file.path(output, 'cases.csv'), row.names = FALSE)
write.csv(inputs, file.path(output, 'inputs.csv'), row.names = FALSE)
writeLines(c(
  '# Capture provenance for the item 21-B transformation-decision corpus.',
  '# Oracle: pinned jdplus sa-core LogLevelModule.process through the public-',
  '# API observer TransformationDecisionProbe.java (no decision rule or',
  '# estimation algorithm there). ModelDescription: mean off, monthly TsData,',
  '# no estimation domain, user-variable design columns.',
  '# Module call site pinned by X13SpecDecoder: estimationPrecision 1.0E-7,',
  '# preadjust None, outliersCorrection false; AICC correction -2.0',
  '# (TransformSpec DEF_AICDIFF). Decision processor: DemetraUtility.',
  '# processor(true, 1e-7) = ML, Multiple starting point, feps 1e-4 default.',
  '# Pinned rule: log iff aiccLevel > aiccLog + correction (dcmpl; NaN picks',
  '# levels); short-circuit to levels on any value <= 0 (NaN passes the scan).',
  '# Parameters are KALOS convention: stored [ar sar ma sma]',
  '# with AR signs flipped from the pinned SarimaModel.parameters().',
  '# inputs.csv x holds the regarima() design matrix actually consumed',
  '# (post-check), column-major; xcolumns is its column count.',
  sprintf('# R %s', getRversion()),
  sprintf('# rjd3jars %s', as.character(packageVersion('rjd3jars'))),
  sprintf('# capture-transformation-decision.R sha256 %s', reference_sha256(script)),
  sprintf('# TransformationDecisionProbe.java sha256 %s',
    reference_sha256(file.path(tooling, 'TransformationDecisionProbe.java'))),
  sprintf('captured_at <- "%s"', format(Sys.time(), tz = 'UTC'))),
  file.path(output, 'metadata.R'))
files <- sort(setdiff(list.files(output, full.names = TRUE),
  file.path(output, 'SHA256SUMS.csv')))
write.csv(data.frame(file = basename(files),
  sha256 = reference_sha256(files)),
  file.path(output, 'SHA256SUMS.csv'), row.names = FALSE)
cat('cases:', nrow(cases), '\n')
