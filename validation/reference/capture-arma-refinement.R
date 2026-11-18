# Pinned-reference capture for item 21-F: the ARMA refinement and comparison
# machine of the pinned jdplus 3.9.0 x13 AutoModellingModule (rjd3 jars)
# through the public-API observer ArmaRefinementProbe.java. The observer
# calls the real pinned classes (RegArimaUtility.processor/linearizedData,
# DifferencingModule.process on the decoder-built module, the module's own
# BackFilter(UnitRoots.D(d).times(UnitRoots.D(period, bd))) application,
# ArmaModule.select/getPreferedModels, SarimaOrders.of/airline/equals); no
# decision rule is re-implemented here. This script builds deterministic raw
# series with current specifications and mean flags, runs the full stage per
# case and parses the stage lines into the corpus CSVs. The qualified period
# domain is {4, 12}; every case uses the decoder call-site module state
# (cancel 0.1, the JVM reciprocals of the default ub1/ub2, precision 1e-7,
# maxd/maxbd 2/1; modelsCount 5, max_p/max_q 2, max_bp/max_bq 1, balanced
# false, mixed true, eps 1.0E-7), reported per case through the MODULE line.
# Cases with a failed current estimation (Gls failed on annihilated
# regressors) are recorded in est.csv and excluded from the sweep; pinned
# stage failures (differencing/filter/select) are VALID fallback rows.
# Pass --repro=<dir> to cross-check the written corpus against an earlier
# capture of the same cases from an independent JVM process.
# JVM ground truth (21-D.2 qualification): the estimation machine reaches
# Math.exp / Math.log; capture runs with
#   _JAVA_OPTIONS="-XX:+UnlockDiagnosticVMOptions -XX:DisableIntrinsic=_dexp -XX:DisableIntrinsic=_dlog"
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
  stop('Usage: capture-arma-refinement.R [--repro=<captured directory>] --output=<existing empty directory>')
output <- normalizePath(sub('^--output=', '', args), mustWork = TRUE)
if (!dir.exists(output) || length(list.files(output, all.files = TRUE, no.. = TRUE)))
  stop('Output must be empty')
suppressWarnings(suppressMessages({
  library(rjd3jars); library(rjd3toolkit); library(rJava)
}))
.jinit()
javahome <- Sys.getenv('JAVA_HOME')
javac <- if (nzchar(javahome)) {
  file.path(javahome, 'bin', paste0('javac',
    if (.Platform$OS.type == 'windows') '.exe' else ''))
} else {
  Sys.which('javac')
}
if (!nzchar(javac) || !file.exists(javac))
  stop('javac is required (JAVA_HOME/bin or PATH)')
jvm_major <- as.integer(sub('^([0-9]+).*', '\\1',
  .jcall('java/lang/System', 'S', 'getProperty', 'java.version')))
if (jvm_major != 21L)
  stop('the pinned capture JVM is OpenJDK 21: run with ',
    'JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64 (rJava picked up JVM ',
    .jcall('java/lang/System', 'S', 'getProperty', 'java.version'), ')')
probe <- tempfile('kalos-arma-refinement-'); dir.create(probe)
jars <- c(list.files(system.file('java', package = 'rjd3jars'), pattern = '[.]jar$',
  full.names = TRUE),
  list.files(system.file('java', package = 'rjd3x13'), pattern = '[.]jar$',
    full.names = TRUE))
classpath <- paste(jars, collapse = .Platform$path.sep)
status <- system2(javac, c('--release', jvm_major, '-d', shQuote(probe),
  '-cp', shQuote(classpath),
  shQuote(file.path(tooling, 'ArmaRefinementProbe.java'))))
if (status != 0L) stop('probe compilation failed')
.jaddClassPath(c(jars[grep('rjd3x13', jars)], probe))
if (inherits(try(.jfindClass('ArmaRefinementProbe'), silent = TRUE), 'try-error'))
  stop('observer class not loadable in the rJava JVM')
# Ground-truth guard: the machine reaches Math.exp; the contract pins the
# bytecode (FdLibm) semantics, so the intrinsics must be disabled.
local({
  x <- 0.022970045955171898
  em <- .jcall('java/lang/Math', 'D', 'exp', x)
  es <- .jcall('java/lang/StrictMath', 'D', 'exp', x)
  if (!identical(em, es))
    stop('capture JVM still uses the intrinsic Math.exp: run with ',
      '_JAVA_OPTIONS="-XX:+UnlockDiagnosticVMOptions -XX:DisableIntrinsic=_dexp ',
      '-XX:DisableIntrinsic=_dlog"')
})

g <- function(x) sprintf('%.17g', x)
das <- function(v) .jarray(as.double(v))
j <- function(v) paste(v, collapse = ';')

inputs <- list(); ests <- list(); lins <- list(); zs <- list()
cases <- list(); panels <- list()
seen_scalars <- NULL

# -- deterministic recipes (same generator family as the 21-D/21-E corpora) --
wn <- function(n, seed) { set.seed(seed); rnorm(n) }
ar2 <- function(n, seed, a1, a2) {
  set.seed(seed); e <- rnorm(n, 0, 1); x <- numeric(n)
  for (t in seq_len(n)) {
    x[t] <- e[t] + (if (t >= 2) a1 * x[t - 1] else 0) +
      (if (t >= 3) a2 * x[t - 2] else 0)
  }
  x
}
walk <- function(n, seed, drift = 0) { set.seed(seed); cumsum(rnorm(n, drift, 1)) }
swn <- function(n, seed, lag = 12) {
  set.seed(seed); e <- rnorm(n, 0, 1)
  x <- e; if (n > lag) for (t in (lag + 1):n) x[t] <- x[t - lag] + e[t]
  x
}
i2 <- function(n, seed) { set.seed(seed); as.numeric(cumsum(cumsum(rnorm(n, 0.05, 1)))) }
trend <- function(n) as.numeric(seq_len(n))
sine7 <- function(n) as.numeric(sin(2 * pi * seq_len(n) / 7))
sine12 <- function(n) as.numeric(sin(2 * pi * seq_len(n) / 12))
sine5 <- function(n) as.numeric(sin(2 * pi * seq_len(n) / 5))

user_cols <- function(kind, n) switch(kind,
  none = NULL,
  trend = matrix(trend(n), ncol = 1),
  sine12 = matrix(sine12(n), ncol = 1),
  sine7 = matrix(sine7(n), ncol = 1),
  'trend+sine7' = cbind(trend(n), sine7(n)),
  'trend+sine5' = cbind(trend(n), sine5(n)))

# -- one stage run -----------------------------------------------------------
# spec = c(p, q, bp, bq, d, bd) of the current SarimaOrders.
case <- function(id, note, y, period, mean0, kind, spec) {
  n <- length(y)
  u <- user_cols(kind, n)
  flat <- if (is.null(u)) numeric(0) else as.vector(u) # column-major
  k <- if (is.null(u)) 0L else ncol(u)
  userstr <- if (k == 0L) '' else
    j(apply(u, 2, function(c) j(vapply(c, g, character(1)))))
  res <- .jcall('ArmaRefinementProbe', 'S', 'run',
    as.integer(period), as.integer(spec[1]), as.integer(spec[2]),
    as.integer(spec[3]), as.integer(spec[4]), as.integer(spec[5]),
    as.integer(spec[6]), as.logical(mean0), das(y), das(flat), as.integer(k))
  lines <- strsplit(res, '\n', fixed = TRUE)[[1L]]
  stage <- function(tag) {
    hit <- lines[startsWith(lines, paste0(tag, '|'))]
    if (length(hit) == 0L) character(0) else strsplit(hit[[1L]], '|', fixed = TRUE)[[1L]]
  }
  est <- stage('EST'); lin <- stage('LIN'); dif <- stage('DIF')
  zl <- stage('Z'); sel <- stage('SEL'); res0 <- stage('RESULT')
  mod <- stage('MODULE')
  if (!is.null(seen_scalars) && length(mod)) {
    if (!identical(mod, seen_scalars)) stop('module scalars differ across cases')
  }
  if (is.null(seen_scalars) && length(mod)) seen_scalars <<- mod
  if (!identical(est[[1L]], 'EST') || !identical(est[[2L]], 'ok')) {
    ests[[length(ests) + 1L]] <<- data.frame(id = id, status = 'error',
      coef = '', params = '', error = if (length(est) >= 3)
        substr(paste(est[-(1:2)], collapse = '|'), 1, 200) else '',
      stringsAsFactors = FALSE)
    return(invisible(NULL))
  }
  # the JVM prints empty parameter blocks without the trailing field
  if (length(est) < 4L) est <- c(est, character(4L - length(est)))
  ests[[length(ests) + 1L]] <<- data.frame(id = id, status = 'ok',
    coef = est[[3L]], params = est[[4L]], error = '', stringsAsFactors = FALSE)
  stopifnot(identical(lin[[2L]], 'ok'))
  lins[[length(lins) + 1L]] <<- data.frame(id = id, n = lin[[3L]], lin = lin[[4L]],
    stringsAsFactors = FALSE)
  difok <- length(dif) >= 6 && identical(dif[[2L]], 'ok')
  if (!difok) {
    zs[[length(zs) + 1L]] <<- data.frame(id = id, kind = 'none', z = '',
      stringsAsFactors = FALSE)
  } else if (identical(zl[[2L]], 'identity')) {
    zs[[length(zs) + 1L]] <<- data.frame(id = id, kind = 'identity', z = '',
      stringsAsFactors = FALSE)
  } else if (identical(zl[[2L]], 'ok')) {
    zs[[length(zs) + 1L]] <<- data.frame(id = id, kind = 'ok',
      z = if (length(zl) >= 4) zl[[4]] else '', stringsAsFactors = FALSE)
  } else {
    zs[[length(zs) + 1L]] <<- data.frame(id = id, kind = 'error', z = '',
      stringsAsFactors = FALSE)
  }
  selok <- length(sel) >= 6 && identical(sel[[2L]], 'ok')
  stopifnot(length(res0) == 12L)
  cases[[length(cases) + 1L]] <<- data.frame(id = id, dif = dif[[2L]],
    d = if (difok) dif[[3L]] else '', bd = if (difok) dif[[4L]] else '',
    mu = if (difok) dif[[5L]] else '', tmean = if (difok) dif[[6L]] else '',
    sel = if (selok) 'ok' else 'error',
    sel_p = if (selok) sel[[3L]] else '', sel_q = if (selok) sel[[4L]] else '',
    sel_bp = if (selok) sel[[5L]] else '', sel_bq = if (selok) sel[[6L]] else '',
    final_p = res0[[3L]], final_q = res0[[4L]], final_bp = res0[[5L]],
    final_bq = res0[[6L]], final_d = res0[[7L]], final_bd = res0[[8L]],
    mean = res0[[9L]], spec_changed = res0[[10L]], mean_changed = res0[[11L]],
    changed = res0[[12L]], fallback = if (identical(res0[[2L]], 'normal')) 0L else 1L,
    stringsAsFactors = FALSE)
  if (selok) {
    pal <- lines[startsWith(lines, 'PANEL|')]
    stopifnot(length(pal) == 5L)
    for (p in seq_along(pal)) {
      f <- strsplit(pal[[p]], '|', fixed = TRUE)[[1L]]
      # trailing empty parameter blocks collapse; pad to 7 fields
      if (length(f) < 7L) f <- c(f, character(7L - length(f)))
      panels[[length(panels) + 1L]] <<- data.frame(id = id, slot = p - 1L,
        p = f[[2L]], q = f[[3L]], bp = f[[4L]], bq = f[[5L]], bic = f[[6L]],
        params = f[[7L]], stringsAsFactors = FALSE)
    }
  }
  inputs[[length(inputs) + 1L]] <<- data.frame(id = id, note = note,
    period = as.integer(period), mean = as.character(mean0), user_kind = kind,
    cur_p = as.integer(spec[1]), cur_q = as.integer(spec[2]),
    cur_bp = as.integer(spec[3]), cur_bq = as.integer(spec[4]),
    cur_d = as.integer(spec[5]), cur_bd = as.integer(spec[6]),
    n = as.integer(n), y = j(vapply(y, g, character(1))), user = userstr,
    stringsAsFactors = FALSE)
  invisible(TRUE)
}

# -- monthly cases -----------------------------------------------------------
case('m_wn96_a', 'white noise n=96: unchanged-air path, spec none',
  wn(96, 4101), 12, TRUE, 'none', c(0, 0, 0, 0, 0, 0))
case('m_wn96_b', 'white noise n=96: spec already (0,0,0,1)',
  wn(96, 4101), 12, TRUE, 'none', c(0, 0, 0, 1, 0, 0))
case('m_wn96_m0', 'white noise n=96 mean off: setMean branch',
  wn(96, 4101), 12, FALSE, 'none', c(0, 0, 0, 1, 0, 0))
case('m_ar96', 'AR(2) a=(0.5,-0.25) n=96: overfit spec (2,2,1,1|1,1)',
  ar2(96, 4102, 0.5, -0.25), 12, TRUE, 'none', c(2, 2, 1, 1, 1, 1))
case('m_walk96', 'random walk n=96: d decision from (0,1,0,1|1,0)',
  walk(96, 4103, 0.1), 12, FALSE, 'none', c(0, 1, 0, 1, 1, 0))
case('m_walk96_trend', 'random walk n=96 with fitted trend mean on',
  walk(96, 4103, 0), 12, TRUE, 'trend', c(0, 0, 0, 0, 0, 0))
case('m_walk96_trend_d1', 'random walk n=96 trend fit d=1 spec, mean off',
  walk(96, 4103, 0), 12, FALSE, 'trend', c(1, 1, 0, 1, 1, 0))
case('m_swn144_sine', 'seasonal walk n=144 sine12 fit d=1: seasonal decision',
  swn(144, 4104), 12, FALSE, 'sine12', c(0, 0, 0, 0, 1, 0))
case('m_swn144_trend', 'seasonal walk n=144 trend mean on from (1,1,0,1|0,0)',
  { set.seed(4105); e <- rnorm(144); z <- e
    for (t in 13:144) z[t] <- z[t - 12] + e[t]; as.numeric(z + 0.3 * seq_len(144)) },
  12, TRUE, 'trend', c(1, 1, 0, 1, 0, 0))
case('m_i2_d2a', 'doubly integrated n=144: pressure toward d=2 (1-B^2 filter)',
  i2(144, 4162), 12, FALSE, 'none', c(0, 1, 0, 1, 1, 1))
case('m_i2_d2b', 'doubly integrated n=144: current spec already d=2 bd=1',
  i2(144, 4162), 12, FALSE, 'none', c(2, 2, 1, 1, 2, 1))
case('m_i2_s7', 'doubly integrated n=120 sine7 fit: regressor survives',
  i2(120, 4163), 12, FALSE, 'sine7', c(0, 0, 0, 0, 0, 0))
case('m_trend120', 'trend + noise n=120 mean on trend fit: stationary decision',
  { set.seed(4106); as.numeric(0.5 * seq_len(120) + rnorm(120)) },
  12, TRUE, 'trend', c(0, 0, 0, 0, 0, 0))
case('m_wn60_two', 'white noise n=60 trend+sine7 two regressors',
  wn(60, 4107), 12, FALSE, 'trend+sine7', c(1, 0, 0, 0, 0, 0))
case('m_sw20', 'seasonal walk n=20: pinned differencing NAE -> airline fallback',
  swn(20, 4108), 12, FALSE, 'none', c(0, 0, 0, 0, 0, 0))
case('m_sw16', 'seasonal walk n=16: fallback path at filter/select stage',
  swn(16, 4109), 12, TRUE, 'none', c(0, 0, 0, 0, 0, 0))
case('m_double_rw', 'regular + seasonal random walk n=132 from (1,0,1,0|0,0)',
  { set.seed(4110); e <- rnorm(132); z <- cumsum(e)
    w <- z; for (t in 13:132) w[t] <- w[t - 12] + z[t] - z[t - 12] + e[t]
    as.numeric(w) },
  12, FALSE, 'none', c(1, 0, 1, 0, 0, 0))

# -- quarterly cases ---------------------------------------------------------
case('q_wn48', 'quarterly white noise n=48 mean on: decision + select',
  wn(48, 4201), 4, TRUE, 'none', c(0, 0, 0, 0, 0, 0))
case('q_sw48', 'quarterly seasonal walk n=48 from (0,1,0,0|1,0)',
  swn(48, 4202, lag = 4), 4, FALSE, 'none', c(0, 1, 0, 0, 1, 0))
case('q_i2_60', 'quarterly doubly integrated n=60: d=2 filter pressure, period 4',
  i2(60, 4203), 4, FALSE, 'none', c(0, 1, 0, 1, 1, 1))
case('q_trend48_s5', 'quarterly trend + sine5 n=48 two regressors mean off',
  { set.seed(4204); as.numeric(seq_len(48) * 0.2 + sine5(48) + rnorm(48, 0, .5)) },
  4, FALSE, 'trend+sine5', c(0, 0, 0, 0, 0, 0))
case('q_ar48', 'quarterly AR(2) n=48 from (2,2,1,1|0,0)',
  ar2(48, 4205, 0.6, -0.25), 4, TRUE, 'none', c(2, 2, 1, 1, 0, 0))
case('q_sw32_sine', 'quarterly seasonal walk n=32 sine5 fit mean on',
  swn(32, 4206, lag = 4), 4, TRUE, 'sine7', c(0, 0, 0, 1, 0, 0))

# -- write the corpus ---------------------------------------------------------
write_df <- function(rows, name) {
  df <- do.call(rbind, rows)
  write.csv(df, file.path(output, name), row.names = FALSE, quote = TRUE)
}
write_df(inputs, 'inputs.csv')
write_df(ests, 'estimation.csv')
write_df(lins, 'lin.csv')
write_df(zs, 'z.csv')
write_df(cases, 'cases.csv')
write_df(panels, 'panel.csv')

n_ok <- sum(vapply(cases, function(r) r$fallback == 0L, logical(1)))
n_fb <- sum(vapply(cases, function(r) r$fallback == 1L, logical(1)))
n_est_err <- sum(vapply(ests, function(r) r$status == 'error', logical(1)))
meta <- c(
  '# Capture provenance for the item 21-F arma-refinement corpus.',
  '# Oracle: the pinned jdplus 3.9.0 x13 AutoModellingModule stage machine',
  '# through the public-API observer ArmaRefinementProbe.java (no decision',
  '# rule there): the current estimation through RegArimaUtility.processor',
  '# (true, 1e-7).process, the linearized data through RegArimaUtility',
  '# .linearizedData, the differencing decision through the decoder-built',
  '# DifferencingModule.process (public; tmean through getTMean), the module',
  '# own BackFilter(UnitRoots.D(d).times(UnitRoots.D(period, bd))) filter',
  '# application, ArmaModule.select plus getPreferedModels (the package-',
  '# private Builder reached by reflection, the RegArmaBic arima field read',
  '# for observation only), and SarimaOrders.of/airline/equals for the final',
  '# comparison. Every stage failure reproduces the module RuntimeException',
  '# catch as a fallback row (RESULT|fallback: SarimaOrders',
  '# .airline(period), mean off). Module scalars are decoder call-site',
  '# values, verified identical across cases through the MODULE line:',
  sprintf('#   maxd %s maxbd %s ub1 %s ub2 %s cancel %s precision %s',
    seen_scalars[2], seen_scalars[3], seen_scalars[4], seen_scalars[5],
    seen_scalars[6], seen_scalars[7]),
  sprintf('#   nmod %s maxP %s maxQ %s maxBp %s maxBq %s balanced %s mixed %s wn %s eps %s',
    seen_scalars[8], seen_scalars[9], seen_scalars[10], seen_scalars[11],
    seen_scalars[12], seen_scalars[13], seen_scalars[14], seen_scalars[15],
    seen_scalars[16]),
  '# estimation.csv rows with status=error (Gls failed on annihilated',
  '# regressor designs) are excluded from the sweep: the capture never built',
  '# a case on them; the JVM error text is kept for the record.',
  sprintf('# %d sweep cases (%d normal, %d airline fallback), %d estimation-error rows.',
    length(cases), n_ok, n_fb, n_est_err),
  '# The qualified period domain is {4, 12}; failures are recorded rows.',
  sprintf('# rJava JVM %s (observer compiled with javac --release 21)',
    .jcall('java/lang/System', 'S', 'getProperty', 'java.version')))
if (nzchar(repro)) {
  prior <- list.files(repro, pattern = '[.]csv$', full.names = TRUE)
  fresh <- list.files(output, pattern = '[.]csv$', full.names = TRUE)
  same <- length(prior) == length(fresh) && all(vapply(basename(fresh), function(f) {
    p <- file.path(repro, f)
    file.exists(p) && identical(readBin(p, 'raw', 1e8),
      readBin(file.path(output, f), 'raw', 1e8))
  }, logical(1), USE.NAMES = FALSE))
  meta <- c(meta,
    '# double capture: this run was cross-checked against an independent JVM',
    sprintf('# capture of the same cases (--repro): all %d csv files byte-identical: %s',
      length(fresh), if (same) 'yes' else 'NO'))
  if (!same) stop('repro capture differs; not reproducible across JVM processes')
} else {
  meta <- c(meta, '# double capture: run this script twice into separate directories and',
    '# compare all csv files (--repro); identical bytes prove JVM-process',
    '# reproducibility of every captured decision, landing and panel.')
}
writeLines(meta, file.path(output, 'metadata.R'))
cat('captured', length(inputs), 'inputs,', n_ok, 'normal,', n_fb, 'fallback,',
  n_est_err, 'estimation-error rows ->', output, '\n')
