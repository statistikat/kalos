# Pinned-reference capture for item 21-A (estimation control): the fixed-order
# estimation-CONTROL layer of the pinned jdplus RegSarimaComputer (rjd3 3.9.0
# jars). Each case runs the pinned computer itself through the independently
# authored public-API observer EstimationControlProbe.java: a complete
# fixed-order regression model (caller coefficients, regression columns,
# explicit missing positions) plus the pinned control fields (eps = 1.0E-7,
# feps = 1.0E-4, ml = true, mt = false, cdf = true, fast = true, default
# Levenberg-Marquardt minimizer). The starting point is the only field the
# capture moves, to Zero or Default; Hannan-Rissanen and Multiple belong to
# 21-A.2. The response carries the mean/drift through the regression columns
# exactly like the qualified regression-likelihood contract: a stationary
# level column when no differencing is present, t^(d+D)/((d+D)!*12^D)
# otherwise. No Kalos code is an input and no production output is read; this
# is the oracle side compared by tools/verify_estimation_control.cpp.
# Parameters are stored in the KALOS convention: the pinned raw AR
# coefficients of SarimaModel.parameters() [phi, bphi, theta, btheta] are sign
# flipped (pinned AR polynomial 1 + phi(B) versus Kalos 1 - phi(B); both use
# MA 1 + theta(B)).
args <- commandArgs(trailingOnly = FALSE)
script <- sub('^--file=', '', args[startsWith(args, '--file=')][[1L]])
tooling <- dirname(normalizePath(script)); source(file.path(tooling, 'common.R'))
root <- reference_root()
args <- commandArgs(trailingOnly = TRUE)
if (length(args) != 1L || !startsWith(args, '--output='))
  stop('Usage: capture-estimation-control.R --output=<existing empty directory>')
output <- normalizePath(sub('^--output=', '', args), mustWork = TRUE)
if (!dir.exists(output) || length(list.files(output, all.files = TRUE, no.. = TRUE)))
  stop('Output must be empty')
suppressWarnings(suppressMessages({
  library(rjd3jars); library(rjd3toolkit); library(rJava)
}))
.jinit()
probe <- tempfile('kalos-estimation-control-'); dir.create(probe)
jars <- list.files(system.file('java', package = 'rjd3jars'), pattern = '[.]jar$',
  full.names = TRUE)
classpath <- paste(jars, collapse = .Platform$path.sep)
javac <- Sys.which('javac'); if (!nzchar(javac)) stop('javac is required on PATH')
status <- system2(javac, c('-d', shQuote(probe), '-cp', shQuote(classpath),
  shQuote(file.path(tooling, 'EstimationControlProbe.java'))))
if (status != 0L) stop('probe compilation failed')
.jaddClassPath(probe)
J <- rJava::J

# -- deterministic input recipes ---------------------------------------------------
base_series <- function(n, seed) {
  set.seed(seed)
  as.numeric(50 + cumsum(rnorm(n, 0, .8)) + rnorm(n))
}
# Integrated MA drivers for the unit-root-cancellation fixtures: the
# (near-)cancelling root lives in the data-generating recursion, exactly as a
# near unit root arrives from a real fixed-order estimation.
integrated_ma <- function(n, seed, root, d, D) {
  set.seed(seed)
  m <- n + 60L
  e <- rnorm(m)
  w <- e; if (length(root)) w[2:m] <- w[2:m] + root * e[1:(m - 1L)]
  v <- numeric(m)
  for (t in 2:m) v[t] <- if (D > 0L && t > 12L) v[t - 12L] + w[t] else
    if (D == 0L && d > 0L) v[t - 1L] + w[t] else w[t]
  v[(m - n + 1L):m] - v[m - n]
}
# Stationary AR(1) drivers for the over-differencing fixtures: an AR near +1
# estimated with a differencing makes the pinned unit-root cancellation
# condition fire on the final coefficients.
stationary_ar <- function(n, seed, ar, m = n + 124L) {
  set.seed(seed)
  e <- rnorm(m)
  v <- numeric(m)
  for (t in 2:m) v[t] <- ar * v[t - 1L] + e[t]
  v[(m - n + 1L):m] - v[m - n]
}
level_column <- function(d, D) if (d + D == 0L) 'level' else 'drift'

cases <- list(); inputs <- list()
run_case <- function(id, note, y, ar, ma, bar, mar, d, D, columns, missing, start) {
  n <- length(y)
  cols <- list()
  for (cc in columns) cols[[length(cols) + 1L]] <- switch(cc,
    level = rep(1, n),
    drift = if (d + D == 1L) seq_len(n) / 12^D else
      seq_len(n)^(d + D) / (factorial(d + D) * 12^D),
    ao = {z <- numeric(n); z[max(1L, n %/% 3L)] <- 1; z},
    cal = sin(2 * pi * seq_len(n) / 7) + cos(2 * pi * seq_len(n) / 5))
  yj <- y; if (length(missing)) yj[missing] <- 0
  res <- .jcall('EstimationControlProbe', 'S', 'run', 12L, as.integer(d), as.integer(D),
    .jarray(as.double(yj)),
    .jarray(if (length(cols)) as.double(unlist(cols)) else numeric(0)),
    as.integer(length(cols)), .jarray(as.integer((missing %||% integer(0)) - 1L)),
    .jarray(as.double(ar)), .jarray(as.double(bar)),
    .jarray(as.double(ma)), .jarray(as.double(mar)),
    start, 1e-07, 1e-04)
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
    stringsAsFactors = FALSE)
  inputs[[length(inputs) + 1L]] <<- data.frame(id = id,
    p_ar = length(ar), p_ma = length(ma), p_sar = length(bar), p_sma = length(mar),
    d = as.integer(d), D = as.integer(D), start = start,
    y = paste(sprintf('%.17g', y), collapse = ';'),
    missing = paste(missing %||% integer(0), collapse = ';'),
    columns = paste(vapply(cols, function(v) paste(sprintf('%.17g', v),
      collapse = ';'), character(1)), collapse = '|'),
    stringsAsFactors = FALSE)
  invisible(res)
}
`%||%` <- function(a, b) if (is.null(a) || !length(a)) b else a
y60 <- base_series(60, 2101); y96 <- base_series(96, 2102)
# Likelihood-only guard: the pinned parametersCount == 0 short circuit.
run_case('guard_regular', 'ARIMA(0,1,0), drift column', y60, numeric(0), numeric(0),
  numeric(0), numeric(0), 1L, 0L, 'drift', integer(0), 'Zero')
run_case('guard_seasonal', 'ARIMA(0,0,0)(0,1,0)12', y96, numeric(0), numeric(0),
  numeric(0), numeric(0), 0L, 1L, 'drift', integer(0), 'Zero')
y50 <- base_series(50, 2103)
run_case('guard_level', 'stationary ARIMA(0,0,0), level column', y50, numeric(0),
  numeric(0), numeric(0), numeric(0), 0L, 0L, 'level', integer(0), 'Zero')
# Zero and Default starts across representative fixed orders.
run_case('zero_r11', '', y60, .2, .3, numeric(0), numeric(0), 1L, 0L,
  'drift', integer(0), 'Zero')
run_case('default_r11', '', y60, .2, .3, numeric(0), numeric(0), 1L, 0L,
  'drift', integer(0), 'Default')
run_case('zero_full', '', y96, .2, .3, .1, .1, 1L, 1L, 'drift', integer(0), 'Zero')
run_case('default_full', '', y96, .2, .3, .1, .1, 1L, 1L, 'drift', integer(0), 'Default')
run_case('zero_regressors', 'AO plus calendar-like column', y96, numeric(0), .4,
  numeric(0), .2, 1L, 1L, c('drift', 'ao', 'cal'), integer(0), 'Zero')
run_case('default_regressors', 'AO column', y96, .3, numeric(0), numeric(0), .2,
  1L, 1L, c('drift', 'ao'), integer(0), 'Default')
run_case('zero_missing', 'two internal holes', y60, .3, .4, numeric(0), numeric(0),
  1L, 0L, 'drift', c(13L, 34L), 'Zero')
run_case('default_missing', '', y60, .3, .4, numeric(0), numeric(0), 1L, 0L,
  'drift', c(13L, 34L), 'Default')
# Unit-root-cancellation fixtures: an over-differenced near-unit-root AR whose
# final coefficients approach the pinned cancellation gate (whether the pass
# fires is decided at the boundary and left to the pinned outcome), a far
# control, and a near -1 seasonal control, with both starting points.
y_urp <- stationary_ar(96, 1018, .93)
run_case('urp_near_unit_zero', 'over-differenced near +1 root', y_urp, .4, -.4,
  numeric(0), numeric(0), 1L, 0L, 'drift', integer(0), 'Zero')
run_case('urp_near_unit_default', 'same data, Default start', y_urp, .4, -.4,
  numeric(0), numeric(0), 1L, 0L, 'drift', integer(0), 'Default')
y_urp2 <- stationary_ar(96, 1033, .93)
run_case('urp_near_unit_second', 'second near-unit-root seed', y_urp2, .3, -.5,
  numeric(0), numeric(0), 1L, 0L, 'drift', integer(0), 'Default')
y_far <- integrated_ma(96, 2151, numeric(0), 1L, 0L)
run_case('urp_gate_miss', 'far roots stay', y_far, .3, .2, numeric(0), numeric(0),
  1L, 0L, 'drift', integer(0), 'Zero')
y_urm <- integrated_ma(96, 2152, .98, 0L, 1L)
run_case('urm_candidate', 'near -1 root with bd=1', y_urm, -.3, .3, numeric(0),
  numeric(0), 0L, 1L, 'drift', integer(0), 'Zero')
run_case('urm_candidate_default', 'same data, Default start', y_urm, -.3, .3,
  numeric(0), numeric(0), 0L, 1L, 'drift', integer(0), 'Default')
cases <- do.call(rbind, cases); inputs <- do.call(rbind, inputs)

# -- corpus ------------------------------------------------------------------------
write.csv(cases, file.path(output, 'cases.csv'), row.names = FALSE)
write.csv(inputs, file.path(output, 'inputs.csv'), row.names = FALSE)
writeLines(c(
  '# Capture provenance for the item 21-A estimation-control corpus.',
  '# Oracle: pinned jdplus RegSarimaComputer.process through the public-API',
  '# observer EstimationControlProbe.java (no estimation algorithm here).',
  sprintf('# R %s', getRversion()),
  sprintf('# rjd3jars %s', as.character(packageVersion('rjd3jars'))),
  '# Pinned control: eps=1e-7 feps=1e-4 ml=true mt=false cdf=true fast=true,',
  '# default Levenberg-Marquardt minimizer builder; startingPoint Zero/Default.',
  '# Regression columns carry the mean/drift (regression-likelihood contract);',
  '# missing responses are zero-filled for the probe and indexed in inputs.csv',
  '# (one-based). Parameters are KALOS convention: stored [ar buar ma buma]',
  '# with AR signs flipped from the pinned SarimaModel.parameters().',
  '# cases.csv orders column is "free_ar/free_ma/d/D".',
  sprintf('# capture-estimation-control.R sha256 %s', reference_sha256(script)),
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
