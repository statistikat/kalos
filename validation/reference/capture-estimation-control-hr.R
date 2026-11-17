# Pinned-reference capture for item 21-A.2: the HannanRissanen- and
# Multiple-start branches of the pinned jdplus RegSarimaComputer (rjd3 3.9.0
# jars), through the same public-API observer EstimationControlProbe.java used
# by the 21-A.1 capture (no estimation algorithm here; no Kalos code is an
# input). Cases carry complete responses and fully free designs (the Kalos
# Hannan-Rissanen initializer is qualified only there), and eps/feps vary:
# the builder defaults 1e-7/1e-4 plus the pinned PROCESSOR singleton at
# eps = 1e-9. The starting point decides the control flow: HannanRissanen
# seeds the eps pass with the initializer; Multiple adds the two
# internal-precision (feps) passes with the pinned selection. Regression
# columns carry the mean/drift exactly as in the 21-A.1 capture.
# Parameters are stored in the KALOS convention (pinned AR signs flipped).
args <- commandArgs(trailingOnly = FALSE)
script <- sub('^--file=', '', args[startsWith(args, '--file=')][[1L]])
tooling <- dirname(normalizePath(script)); source(file.path(tooling, 'common.R'))
root <- reference_root()
args <- commandArgs(trailingOnly = TRUE)
if (length(args) != 1L || !startsWith(args, '--output='))
  stop('Usage: capture-estimation-control-hr.R --output=<existing empty directory>')
output <- normalizePath(sub('^--output=', '', args), mustWork = TRUE)
if (!dir.exists(output) || length(list.files(output, all.files = TRUE, no.. = TRUE)))
  stop('Output must be empty')
suppressWarnings(suppressMessages({
  library(rjd3jars); library(rjd3toolkit); library(rJava)
}))
.jinit()
probe <- tempfile('kalos-estimation-control-hr-'); dir.create(probe)
jars <- list.files(system.file('java', package = 'rjd3jars'), pattern = '[.]jar$',
  full.names = TRUE)
classpath <- paste(jars, collapse = .Platform$path.sep)
javac <- Sys.which('javac'); if (!nzchar(javac)) stop('javac is required on PATH')
status <- system2(javac, c('-d', shQuote(probe), '-cp', shQuote(classpath),
  shQuote(file.path(tooling, 'EstimationControlProbe.java'))))
if (status != 0L) stop('probe compilation failed')
.jaddClassPath(probe)
J <- rJava::J

# -- deterministic input recipes (identical to the 21-A.1 capture) ----------
base_series <- function(n, seed) {
  set.seed(seed)
  as.numeric(50 + cumsum(rnorm(n, 0, .8)) + rnorm(n))
}
stationary_ar <- function(n, seed, ar, m = n + 124L) {
  set.seed(seed)
  e <- rnorm(m)
  v <- numeric(m)
  for (t in 2:m) v[t] <- ar * v[t - 1L] + e[t]
  v[(m - n + 1L):m] - v[m - n]
}
level_column <- function(d, D) if (d + D == 0L) 'level' else 'drift'

cases <- list(); inputs <- list()
run_case <- function(id, note, y, ar, ma, bar, mar, d, D, columns, start,
                     eps = 1e-07, feps = 1e-04) {
  n <- length(y)
  cols <- list()
  for (cc in columns) cols[[length(cols) + 1L]] <- switch(cc,
    level = rep(1, n),
    drift = if (d + D == 1L) seq_len(n) / 12^D else
      seq_len(n)^(d + D) / (factorial(d + D) * 12^D),
    ao = {z <- numeric(n); z[max(1L, n %/% 3L)] <- 1; z},
    cal = sin(2 * pi * seq_len(n) / 7) + cos(2 * pi * seq_len(n) / 5))
  res <- .jcall('EstimationControlProbe', 'S', 'run', 12L, as.integer(d), as.integer(D),
    .jarray(as.double(y)),
    .jarray(if (length(cols)) as.double(unlist(cols)) else numeric(0)),
    as.integer(length(cols)), .jarray(integer(0)),
    .jarray(as.double(ar)), .jarray(as.double(bar)),
    .jarray(as.double(ma)), .jarray(as.double(mar)),
    start, eps, feps)
  fields <- strsplit(res, '|', fixed = TRUE)[[1L]]
  if (!identical(fields[[1L]], 'ok'))
    stop(sprintf('case %s failed: %s', id, res))
  np <- length(c(ar, bar)); nq <- length(c(ma, mar))
  stored <- if (nzchar(fields[[8L]])) as.numeric(strsplit(fields[[8L]], ';')[[1L]])
    else numeric(0)
  stopifnot(length(stored) == np + nq)
  kalos <- c(-stored[seq_len(np)], stored[np + seq_len(nq)])
  cases[[length(cases) + 1L]] <<- data.frame(id = id, note = note %||% '',
    start = start, orders = paste(length(c(ar, bar)), length(c(ma, mar)), d, D,
      sep = '/'),
    params = if (length(kalos)) paste(sprintf('%.17g', kalos), collapse = ';') else '',
    loglik = sprintf('%.17g', as.numeric(fields[[9L]])),
    eps = sprintf('%.17g', eps), feps = sprintf('%.17g', feps),
    stringsAsFactors = FALSE)
  inputs[[length(inputs) + 1L]] <<- data.frame(id = id,
    p_ar = length(ar), p_ma = length(ma), p_sar = length(bar), p_sma = length(mar),
    d = as.integer(d), D = as.integer(D), start = start,
    y = paste(sprintf('%.17g', y), collapse = ';'),
    missing = '',
    columns = paste(vapply(cols, function(v) paste(sprintf('%.17g', v),
      collapse = ';'), character(1)), collapse = '|'),
    stringsAsFactors = FALSE)
  invisible(res)
}
`%||%` <- function(a, b) if (is.null(a) || !length(a)) b else a
y60 <- base_series(60, 2101); y96 <- base_series(96, 2102)

# HannanRissanen and Multiple starts on the 21-A.1 base series, builder
# defaults eps = 1e-7 / feps = 1e-4.
run_case('multiple_r11', 'regular ARMA(1,1), Multiple', y60, .2, .3, numeric(0),
  numeric(0), 1L, 0L, 'drift', 'Multiple')
run_case('hrs_r11', 'regular ARMA(1,1), HannanRissanen', y60, .2, .3, numeric(0),
  numeric(0), 1L, 0L, 'drift', 'HannanRissanen')
# Pinned PROCESSOR singleton: eps = 1e-9 with the Multiple start.
run_case('multiple_processor', 'PROCESSOR precision 1e-9', y60, .2, .3, numeric(0),
  numeric(0), 1L, 0L, 'drift', 'Multiple', 1e-09, 1e-04)
# Full seasonal orders (p, bp, q, bq all nonzero) with both starts.
run_case('multiple_full', 'seasonal ARIMA(1,1,1)(1,1,1)12', y96, .2, .3, .1, .1,
  1L, 1L, 'drift', 'Multiple')
run_case('hrs_full', 'same orders, HannanRissanen', y96, .2, .3, .1, .1, 1L, 1L,
  'drift', 'HannanRissanen')
# Order-shape coverage: the pinned Multiple branch skips the theta(1) restart
# when the regular AR or MA block is empty, and the HR pipeline skips the
# Levinson prefilter for pure-AR orders and the final correction without a
# regular AR part.
run_case('multiple_ar_only', 'p > 0, q = 0', y60, .3, numeric(0), numeric(0),
  numeric(0), 1L, 0L, 'drift', 'Multiple')
run_case('multiple_ma_only', 'p = 0, q > 0', y60, numeric(0), .4, numeric(0),
  numeric(0), 1L, 0L, 'drift', 'Multiple')
run_case('hrs_seasonal_only', 'bp/bq only, no regular ARMA', y96, numeric(0),
  numeric(0), .1, .1, 1L, 1L, 'drift', 'HannanRissanen')
# Regression columns beyond the drift exercise the initializer OLS step.
run_case('multiple_regressors', 'AO plus calendar column', y96, .2, .3, .1, .1,
  1L, 1L, c('drift', 'ao', 'cal'), 'Multiple')
# Note: the rms < 1e-9 variance-guard branch is deliberately not a corpus
# case. The guard fires only when the differenced design explains the
# response, which makes the ML variance zero and the likelihood unbounded;
# the pinned optimizer's final state there is process-dependent (measured:
# the same input in two pinned JVM processes landed at [-0, -0.9]/1869.854
# and at [-9.7e-10, -3.6e-9]/1841.836). The branch is qualified at the unit
# level instead (tests/test_hannan_rissanen.cpp, Source::ZeroVariance).
# Over-differenced near-unit-root data: the HR start lands near the pinned
# unit-root-cancellation gate on a HannanRissanen start.
y_urp <- stationary_ar(96, 1018, .93)
run_case('hrs_near_unit', 'over-differenced near +1 root', y_urp, .4, -.4,
  numeric(0), numeric(0), 1L, 0L, 'drift', 'HannanRissanen')

cases <- do.call(rbind, cases); inputs <- do.call(rbind, inputs)

# -- corpus ------------------------------------------------------------------------
write.csv(cases, file.path(output, 'cases.csv'), row.names = FALSE)
write.csv(inputs, file.path(output, 'inputs.csv'), row.names = FALSE)
writeLines(c(
  '# Capture provenance for the item 21-A.2 estimation-control HR corpus.',
  '# Oracle: pinned jdplus RegSarimaComputer.process through the public-API',
  '# observer EstimationControlProbe.java (no estimation algorithm here).',
  sprintf('# R %s', getRversion()),
  sprintf('# rjd3jars %s', as.character(packageVersion('rjd3jars'))),
  '# Pinned control: ml=true mt=false cdf=true fast=true, default',
  '# Levenberg-Marquardt minimizer builder; startingPoint HannanRissanen/',
  '# Multiple; eps/feps per case (1e-7/1e-4 builder defaults, 1e-9 for the',
  '# pinned PROCESSOR case). Complete series and fully free designs only.',
  '# Regression columns carry the mean/drift (regression-likelihood contract).',
  '# Parameters are KALOS convention: stored [ar buar ma buma]',
  '# with AR signs flipped from the pinned SarimaModel.parameters().',
  '# cases.csv orders column is "free_ar/free_ma/d/D".',
  sprintf('# capture-estimation-control-hr.R sha256 %s', reference_sha256(script)),
  sprintf('# EstimationControlProbe.java sha256 %s',
    reference_sha256(file.path(tooling, 'EstimationControlProbe.java'))),
  sprintf('captured_at <- "%s"', format(Sys.time(), tz = 'UTC'))),
  file.path(output, 'metadata.R'))
files <- sort(setdiff(list.files(output, full.names = TRUE),
  file.path(output, 'SHA256SUMS.csv')))
write.csv(data.frame(file = basename(files),
  sha256 = reference_sha256(files)),
  file.path(output, 'SHA256SUMS.csv'), row.names = FALSE)
cat('cases:', nrow(cases), '\n')
