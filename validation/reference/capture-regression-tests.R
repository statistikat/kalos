# Pinned-reference capture for item 21-G: the stage-R regression tests and
# mean rules of the pinned jdplus 3.9.0 pipeline - the decoder-built
# RegressionVariablesTest machine (tdTest/meanTest/movingHolidaysTest), the
# MeanController test (checkMu call sites at 1.96 and 1.6) and the
# ModelController rTval measure feeding the final mean rule - through the
# public-API observer RegressionTestsProbe.java. The observer constructs the
# design with the pinned factories (GenericTradingDaysVariable /
# LengthOfPeriod / EasterVariable exactly as X13ModelBuilder builds them),
# estimates through RegSarimaModelling.estimate(eps) and calls the pinned
# machines; no decision rule is re-implemented here. Per-test OBS rows call
# the pinned TRegressionTest/DerivedRegressionTest accept forms for
# observation only and never feed the process() run.
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
  stop('Usage: capture-regression-tests.R [--repro=<captured directory>] --output=<existing empty directory>')
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
probe <- tempfile('kalos-regression-tests-'); dir.create(probe)
jars <- c(list.files(system.file('java', package = 'rjd3jars'), pattern = '[.]jar$',
  full.names = TRUE),
  list.files(system.file('java', package = 'rjd3x13'), pattern = '[.]jar$',
    full.names = TRUE))
classpath <- paste(jars, collapse = .Platform$path.sep)
status <- system2(javac, c('--release', jvm_major, '-d', shQuote(probe),
  '-cp', shQuote(classpath),
  shQuote(file.path(tooling, 'RegressionTestsProbe.java'))))
if (status != 0L) stop('probe compilation failed')
.jaddClassPath(c(jars[grep('rjd3x13', jars)], probe))
if (inherits(try(.jfindClass('RegressionTestsProbe'), silent = TRUE), 'try-error'))
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

# -- deterministic recipes ----------------------------------------------------
wn <- function(n, seed) { set.seed(seed); rnorm(n) }
ar1 <- function(n, seed, a = 0.6) {
  set.seed(seed); e <- rnorm(n); x <- numeric(n)
  for (t in seq_len(n)) x[t] <- e[t] + (if (t >= 2) a * x[t - 1] else 0)
  x
}
ar2 <- function(n, seed, a1, a2) {
  set.seed(seed); e <- rnorm(n); x <- numeric(n)
  for (t in seq_len(n)) {
    x[t] <- e[t] + (if (t >= 2) a1 * x[t - 1] else 0) +
      (if (t >= 3) a2 * x[t - 2] else 0)
  }
  x
}
walk <- function(n, seed, drift = 0) { set.seed(seed); cumsum(rnorm(n, drift, 1)) }
i2 <- function(n, seed) { set.seed(seed); as.numeric(cumsum(cumsum(rnorm(n, 0.05, 1)))) }
trend <- function(n) as.numeric(seq_len(n))
sine7 <- function(n) as.numeric(sin(2 * pi * seq_len(n) / 7))
sine5 <- function(n) as.numeric(sin(2 * pi * seq_len(n) / 5))

user_cols <- function(kind, n) switch(kind,
  none = NULL,
  trend = matrix(trend(n), ncol = 1),
  sine7 = matrix(sine7(n), ncol = 1),
  'trend+sine5' = cbind(trend(n), sine5(n)))

# calendar columns over the probe domain (monthly, from 2000-01)
month_seq <- function(n) seq(as.Date('2000-01-01'), by = 'month', length.out = n)
mdays <- function(n) {
  m <- month_seq(n)
  as.numeric(vapply(format(m, '%Y-%m'), function(k) {
    d <- seq(as.Date(paste0(k, '-01')), by = 'day',
      length.out = 31)
    sum(format(d, '%Y-%m') == k)
  }, numeric(1)))
}
wday_count <- function(n, wday) {
  m <- month_seq(n)
  as.numeric(vapply(format(m, '%Y-%m'), function(k) {
    d <- seq(as.Date(paste0(k, '-01')), by = 'day', length.out = 31)
    d <- d[format(d, '%Y-%m') == k]
    sum(weekdays(d) == wday)
  }, numeric(1)))
}
# anonymous gregorian computus
easter_date <- function(year) {
  a <- year %% 19; b <- year %/% 100; c <- year %% 100
  d <- b %/% 4; e <- b %% 4; f <- (b + 3) %/% 4
  g <- (8 * b + 13) %/% 25; h <- (19 * a + b - d - g + 15) %% 30
  i <- c %/% 4; k <- c %% 4; l <- (32 + 2 * e + 2 * i - h - k) %% 7
  m <- (a + 11 * h + 22 * l) %/% 451
  month <- (h + l - 7 * m + 114) %/% 31
  day <- ((h + l - 7 * m + 114) %% 31) + 1
  as.Date(sprintf('%04d-%02d-%02d', year, month, day))
}
# +/- window easter indicator per month (the probe easter variable shape)
easter_col <- function(n, w) {
  m <- month_seq(n)
  as.numeric(vapply(seq_len(n), function(t) {
    y <- as.integer(format(m[t], '%Y'))
    ed <- easter_date(y)
    lo <- as.Date(sprintf('%04d-01-01', y)); hi <- lo + 365
    days <- seq(lo, hi, by = 'day')
    inmonth <- days[format(days, '%Y-%m') == format(m[t], '%Y-%m')]
    any(abs(as.numeric(inmonth - ed)) <= w)
  }, numeric(1)))
}

inputs <- list(); clrows <- list(); ucov <- list(); vars <- list()
rvts <- list(); obs <- list(); means <- list(); cases <- list()

# -- one case run -------------------------------------------------------------
case <- function(id, note, y, period, log0, mean0, td, lop, easter, preadj,
                 kind, dOrd = 0L, pOrd = 1L, qOrd = 0L) {
  n <- length(y)
  u <- user_cols(kind, n)
  flat <- if (is.null(u)) numeric(0) else as.vector(u)
  k <- if (is.null(u)) 0L else ncol(u)
  easterstr <- paste(easter, collapse = ';')
  cfgs <- if (td == 0 && !nzchar(easterstr))
    '1.96,0,0,0;1.0,0,0,0' else
    '1.96,1.96,1.96,1;1.0,1.96,1.96,1;1.96,0,0,0;1.0,0,0,0'
  res <- .jcall('RegressionTestsProbe', 'S', 'run',
    as.integer(period), as.integer(dOrd), as.integer(pOrd), as.integer(qOrd),
    as.logical(log0), as.logical(mean0), as.integer(td), as.logical(lop),
    easterstr, as.logical(preadj), 1.0E-7,
    cfgs, '1.96;1.6', das(y), das(flat), as.integer(k))
  lines <- strsplit(res, '\n', fixed = TRUE)[[1L]]
  grab <- function(tag) {
    lapply(lines[startsWith(lines, paste0(tag, '|'))], function(l) {
      f <- strsplit(l, '|', fixed = TRUE)[[1L]]
      # strsplit drops trailing empty cells (empty sequences, nx = 0)
      while (length(f) < 2L) f <- c(f, '')
      f
    })
  }
  est <- grab('EST')
  if (length(est)) {
    cases[[length(cases) + 1L]] <<- data.frame(id = id, note = note,
      status = 'error',
      error = substr(paste(est[[1L]][-(1:2)], collapse = '|'), 1, 200),
      stringsAsFactors = FALSE)
    return(invisible(NULL))
  }
  cl <- grab('CL')[[1L]]
  coef <- grab('COEF')[[1L]]
  tst <- grab('TSTATS')[[1L]]
  st <- grab('STATE')[[1L]]
  rt <- grab('RTVAL')
  clrows[[length(clrows) + 1L]] <<- data.frame(id = id, nx = cl[[2L]],
    dim = cl[[3L]], ssq = cl[[4L]], coef = coef[[2L]], tstats = tst[[2L]],
    stringsAsFactors = FALSE)
  for (row in grab('UCOV'))
    ucov[[length(ucov) + 1L]] <<- data.frame(id = id, row = row[[2L]],
      values = row[[3L]], stringsAsFactors = FALSE)
  for (v in grab('VAR'))
    vars[[length(vars) + 1L]] <<- data.frame(id = id, idx = v[[2L]],
      name = v[[3L]], kind = v[[4L]], dim = v[[5L]], pos = v[[6L]],
      free = v[[7L]], pre = v[[8L]], stringsAsFactors = FALSE)
  for (o in grab('OBS'))
    obs[[length(obs) + 1L]] <<- data.frame(id = id, cfg = o[[2L]], w = o[[3L]],
      kind = o[[4L]], accept = o[[5L]], derived = o[[6L]], dvalue = o[[7L]],
      dtstat = o[[8L]], stringsAsFactors = FALSE)
  for (r in grab('RVT')) {
    if (length(r) > 3 && r[[4L]] == 'error') {
      rvts[[length(rvts) + 1L]] <<- data.frame(id = id, cfg = r[[2L]],
        scalars = r[[3L]], status = 'error',
        error = substr(paste(r[-(1:3)], collapse = '|'), 1, 200),
        result = '', mean_after = '', preadj_after = '', remaining = '',
        stringsAsFactors = FALSE)
    } else {
      rvts[[length(rvts) + 1L]] <<- data.frame(id = id, cfg = r[[2L]],
        scalars = r[[3L]], status = 'ok', error = '', result = r[[4L]],
        mean_after = r[[5L]], preadj_after = r[[6L]], remaining = r[[7L]],
        stringsAsFactors = FALSE)
    }
  }
  mres <- grab('MEAN'); mstat <- grab('MEANSTAT')
  for (mm in mres) {
    if (length(mm) > 2 && mm[[3L]] == 'error') {
      means[[length(means) + 1L]] <<- data.frame(id = id, cval = mm[[2L]],
        status = 'error', result = '', mean_after = '', stat = '',
        error = substr(paste(mm[-(1:2)], collapse = '|'), 1, 200),
        stringsAsFactors = FALSE)
    } else {
      st0 <- Filter(function(s) s[[2L]] == mm[[2L]], mstat)[[1L]]
      means[[length(means) + 1L]] <<- data.frame(id = id, cval = mm[[2L]],
        status = 'ok', result = mm[[4L]], mean_after = mm[[5L]],
        stat = paste(st0[-(1:2)], collapse = '|'), error = '',
        stringsAsFactors = FALSE)
    }
  }
  cases[[length(cases) + 1L]] <<- data.frame(id = id, note = note,
    status = 'ok', error = '', period = as.integer(period), n = as.integer(n),
    log = if (log0) '1' else '0', mean = if (mean0) '1' else '0',
    td = as.integer(td), lop = if (lop) '1' else '0',
    easter = paste(easter, collapse = ';'), preadj = if (preadj) '1' else '0',
    user_kind = kind, d = as.integer(dOrd), p = as.integer(pOrd),
    q = as.integer(qOrd), mean_state = st[[2L]], log_state = st[[3L]],
    preadj_state = st[[4L]], adjusted = st[[5L]],
    rtval = if (length(rt)) rt[[1L]][[2L]] else '',
    rvr = if (length(rt)) rt[[1L]][[3L]] else '',
    y = j(vapply(y, g, character(1))), stringsAsFactors = FALSE)
  invisible(TRUE)
}

# -- monthly calendar cases ----------------------------------------------------
case('m_allfail', 'wn120 td4+lp+easter15 mean on: every calendar test fails',
  wn(120, 7101), 12, FALSE, TRUE, 1, TRUE, '15', FALSE, 'none')
case('m_tdpass', 'ar1 + 5*friday-count: TD test passes, LoP fails, b-gate blocks',
  { f <- wday_count(120, 'Friday'); as.numeric(ar1(120, 7102) + 5 * scale(f)) },
  12, FALSE, TRUE, 1, TRUE, '15', FALSE, 'none')
case('m_passall', 'ar1 + strong friday + leap + easter + offset: all tests pass',
  { f <- wday_count(120, 'Friday'); md <- mdays(120); e <- easter_col(120, 15)
    as.numeric(ar1(120, 7103) + 6 * scale(f) + 5 * scale(md) + 4 * e + 20) },
  12, FALSE, TRUE, 1, TRUE, '15', FALSE, 'none')
case('m_derived', 'ar1 + 2.5*days-in-month: individual TD t below cut, joint above',
  { md <- mdays(120); as.numeric(ar1(120, 7104) + 2.5 * scale(md)) },
  12, FALSE, TRUE, 1, TRUE, '15', FALSE, 'none')
case('m_adj', 'log exp(ar) preadj LeapYear td4+easter: adjusted reset branch',
  as.numeric(exp(0.3 * ar1(120, 7105))), 12, TRUE, TRUE, 1, FALSE, '15',
  TRUE, 'none')
case('m_mhonly', 'wn96 easter15 only mean off: moving-holiday immediate removal',
  wn(96, 7106), 12, FALSE, FALSE, 0, FALSE, '15', FALSE, 'none')
case('m_td2users', 'wn120 td2 (dim1) + easter15 + trend user mean on',
  { set.seed(7107); as.numeric(rnorm(120)) }, 12, FALSE, TRUE, 2, FALSE,
  '15', FALSE, 'trend')
case('m_easter2', 'wn120 + two easter windows (15, 8) mean on: two mh vars',
  wn(120, 7108), 12, FALSE, TRUE, 0, FALSE, c('15', '8'), FALSE, 'none')
# -- mean-rule cases -----------------------------------------------------------
case('m_meanton', 'trend120 mean on: mean significant, Unchanged path',
  { set.seed(7109); as.numeric(0.5 * seq_len(120) + rnorm(120)) },
  12, FALSE, TRUE, 0, FALSE, '', FALSE, 'none')
case('m_meanadd', 'wn96 + 50 mean off: MeanController adds the mean',
  as.numeric(wn(96, 7110) + 50), 12, FALSE, FALSE, 0, FALSE, '', FALSE, 'none')
case('m_meanoff_signif', 'wn120 + 3 sine7 mean off: added-mean t below cut',
  { s <- sine7(120); as.numeric(ar1(120, 7111) + 3 * s - mean(3 * s)) },
  12, FALSE, FALSE, 0, FALSE, '', FALSE, 'sine7')
case('m_log_meanoff', 'exp(0.25 ar) log mean off: mean-added statistics row',
  as.numeric(exp(0.25 * ar1(120, 7112))), 12, TRUE, FALSE, 0, FALSE, '',
  FALSE, 'none')
# -- plain and quarterly cases -------------------------------------------------
case('m_walk96_sine', 'random walk n=96 sine7 mean on d=1',
  { s <- sine7(96); as.numeric(walk(96, 7113, 0.05) + 2 * s) },
  12, FALSE, TRUE, 0, FALSE, '', FALSE, 'sine7', dOrd = 1L)
case('m_i2_cal', 'doubly integrated n=144 td4+lp+easter15 mean off d=1',
  { md <- mdays(144); e <- easter_col(144, 15)
    as.numeric(i2(144, 7114) + 0.02 * md + 0.01 * e) },
  12, FALSE, FALSE, 1, TRUE, '15', FALSE, 'none', dOrd = 1L)
case('m_wn60_two', 'wn60 trend+sine5 users mean off',
  { set.seed(7115); as.numeric(rnorm(60)) }, 12, FALSE, FALSE, 0, FALSE, '',
  FALSE, 'trend+sine5', pOrd = 1L, qOrd = 1L)
case('q_wn48', 'quarterly white noise n=48 mean on',
  wn(48, 7201), 4, FALSE, TRUE, 0, FALSE, '', FALSE, 'none', pOrd = 0L)
case('q_ar48', 'quarterly ar2 n=48 mean off sine5',
  { s <- sine5(48); as.numeric(ar2(48, 7202, 0.6, -0.25) + s) },
  4, FALSE, FALSE, 0, FALSE, '', FALSE, 'sine5', dOrd = 1L)
case('q_trend60', 'quarterly trend n=60 mean on',
  { set.seed(7203); as.numeric(0.2 * seq_len(60) + rnorm(60)) },
  4, FALSE, TRUE, 0, FALSE, '', FALSE, 'none', pOrd = 1L)

# -- write the corpus ----------------------------------------------------------
write_df <- function(rows, name) {
  if (length(rows) == 0) return(invisible(NULL))
  df <- do.call(rbind, rows)
  write.csv(df, file.path(output, name), row.names = FALSE, quote = TRUE)
}
write_df(cases, 'cases.csv')
write_df(clrows, 'cl.csv')
write_df(ucov, 'ucov.csv')
write_df(vars, 'vars.csv')
write_df(rvts, 'rvt.csv')
write_df(obs, 'obs.csv')
write_df(means, 'mean.csv')

n_ok <- sum(vapply(cases, function(r) r$status == 'ok', logical(1)))
n_err <- sum(vapply(cases, function(r) r$status == 'error', logical(1)))
meta <- c(
  '# Capture provenance for the item 21-G regression-tests + mean-rules corpus.',
  '# Oracle: the pinned jdplus 3.9.0 stage-R machines through the public-API',
  '# observer RegressionTestsProbe.java (no decision rule there): the design',
  '# built with the pinned factories (GenericTradingDaysVariable over',
  '# GenericTradingDays.contrasts(TD4/TD2), LengthOfPeriod(LeapYear),',
  '# EasterVariable(duration, PreComputed, endPosition -1), UserVariable),',
  '# the estimation through RegSarimaModelling.estimate(1e-7) =',
  '# processor(true, 1e-7), the concentrated-likelihood statistics through',
  '# coefficients/unscaledCovariance/ssq/dim/tstats(0,false), the',
  '# RegressionVariablesTest built by its public builder exactly as',
  '# X13SpecDecoder.readRegression builds it (tdTest(1.96, derived) iff the',
  '# TD spec test is active, movingHolidaysTest(1.96), meanTest 1.96 initial',
  '# / 1.0 final; ftd stays 0: the decoder never calls tdJointTest),',
  '# MeanController(cval).test for cval in {1.96 (round-0 checkMu), 1.6',
  '# (redoEstimation checkMu)} with the mean-added estimation duplicated',
  '# through the same processor(true, 1.0E-5) call for the MEANSTAT rows',
  '# only, and ModelController(0.95).accept/getRTval for the rtval0 the',
  '# final mean rule consumes. OBS rows call the pinned TRegressionTest/',
  '# DerivedRegressionTest accept forms for observation only and never feed',
  '# the process() run (which always executes on a fresh description copy).',
  sprintf('# %d sweep cases, %d estimation-error rows (recorded, excluded).',
    n_ok, n_err),
  '# The qualified period domain is {4, 12}; the mean-added MEANSTAT rows',
  '# carry (coef0, unscaledCov00, ssq, dim, tstat(0,0,false)).',
  sprintf('# rJava JVM %s (observer compiled with javac --release 21)',
    .jcall('java/lang/System', 'S', 'getProperty', 'java.version')))
if (nzchar(repro)) {
  prior <- list.files(repro, pattern = '[.]csv$', full.names = TRUE)
  fresh <- list.files(output, pattern = '[.]csv$', full.names = TRUE)
  prior <- prior[basename(prior) != 'SHA256SUMS.csv']
  fresh <- fresh[basename(fresh) != 'SHA256SUMS.csv']
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
    '# reproducibility of every captured decision and statistic.')
}
writeLines(meta, file.path(output, 'metadata.R'))

sums <- data.frame(file = character(0), sha256 = character(0))
files <- sort(setdiff(list.files(output), 'SHA256SUMS.csv'))
sums <- data.frame(file = files,
  sha256 = reference_sha256(file.path(output, files)),
  stringsAsFactors = FALSE)
write.csv(sums, file.path(output, 'SHA256SUMS.csv'), row.names = FALSE)
cat('captured', n_ok, 'cases,', n_err, 'error rows ->', output, '\n')
