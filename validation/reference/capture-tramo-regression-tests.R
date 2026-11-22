# Pinned-reference capture for item 23-E: the NO-CALENDAR branch of the tramo
# regression-test layer of the pinned jdplus 3.9.0 stack, observed by
# TramoRegressionTestsProbe.java. Every case runs in its own JVM process
# (per-case isolation: the estimation-heavy cases share nothing, and the host
# has a history of killing long-lived JVMs) with the G2 recipe below
# (-XX:DisableIntrinsic=_dexp/_dlog, so Math.log/Math.exp and Math.sqrt go
# through the fdlibm/StrictMath route the freeze rules pin). The corpus covers
# ModelStatistics.of (consumed rows plus the record-only block), the
# FastRegressionTest and DefaultRegressionTest machines over the mean/no-mean
# and no-calendar shapes (their likelihood hand-overs are echoed as request
# data for the C++ replay, including BOTH arms of cl.tstats - accept() calls
# tstats(nobs, nobs >= 0), so the TRAMO sites read the addmu arm), the
# TramoKernel.pass2 refStats gate (kernel state is reflective plumbing only,
# the gate itself runs from the pinned bytecode) and the readAmiOptions
# derivation. Calendar-branch cases (auto-identified calendar variables for
# Fast, non-null calendar builder fields for Default) are SEALED WITNESSES for
# the 21-I.3 seam: the live JVM runs its T kernels, the kalos unit refuses
# them; the load-witness rows below are the class-load evidence for the seam.
# Pass --repro=<dir> for the byte-identity cross-check between independent
# process runs.
#
# Corpus (~50 cases): MS the ModelStatistics surfaces (realistic real fits,
# crafted likelihoods: zero ssq, small-n m = n/2, the df <= 0 throw, the
# seasonal catch, the quarterly D11 mirror); FR the Fast machine (mean test
# reject/accept with the exact |t| boundary and one ulp above it from the
# machine's own arm, testMean off, no-mean, outliers/user inert, non-auto
# calendar inert, the auto td / td+lp / easter seam witnesses); DR the Default
# machine (Unprocessed, mean accept/reject with the boundary, the test-model
# hand-over, joinTest-without-calendar, the calendar-field seam witnesses);
# PG the pass2 ladder (refAuto null, skip nr > nb, the three revert arms, the
# shapeA/shapeB reverts and their near misses, NaN reference rows, the pcr
# evolution, the reduceVa/remark arm, the last-solution rewrite); AM the
# AmiOptions derivation over TR0..TRfull.
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
  stop('Usage: capture-tramo-regression-tests.R [--repro=<captured directory>] --output=<existing empty directory>')
output <- normalizePath(sub('^--output=', '', args), mustWork = TRUE)
if (!dir.exists(output) || length(list.files(output, all.files = TRUE, no.. = TRUE)))
  stop('Output must be empty')
suppressWarnings(suppressMessages({
  library(rjd3jars); library(rjd3toolkit); library(rjd3tramoseats); library(rJava)
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
probe <- tempfile('kalos-tramo-rt-'); dir.create(probe)
jars <- c(list.files(system.file('java', package = 'rjd3jars'), pattern = '[.]jar$',
  full.names = TRUE),
  list.files(system.file('java', package = 'rjd3toolkit'), pattern = '[.]jar$',
    full.names = TRUE),
  list.files(system.file('java', package = 'rjd3tramoseats'), pattern = '[.]jar$',
    full.names = TRUE))
classpath <- paste(c(jars, probe), collapse = .Platform$path.sep)
status <- system2(javac, c('--release', jvm_major, '-d', shQuote(probe),
  '-cp', shQuote(classpath),
  shQuote(file.path(tooling, 'TramoRegressionTestsProbe.java'))))
if (status != 0L) stop('probe compilation failed')
PROBE_CLASS <- 'jdplus.tramoseats.base.core.tramo.TramoRegressionTestsProbe'
.jaddClassPath(c(jars[grep('tramoseats', jars)], probe))
if (inherits(try(.jfindClass(PROBE_CLASS), silent = TRUE), 'try-error'))
  stop('observer class not loadable in the rJava JVM')
g <- function(x) sprintf('%.17g', x)

# One JVM process per case (never more than one live at a time), with the
# intrinsic-disable recipe pinned in _JAVA_OPTIONS as a second line of
# defence. extra carries the -Xlog:class+load flag of the witness runs.
javabin <- if (nzchar(javahome)) file.path(javahome, 'bin', 'java') else Sys.which('java')
jvmrun <- function(casefile, cp, extra = character(0)) {
  st <- system2(javabin, c('-XX:+UnlockDiagnosticVMOptions',
    '-XX:DisableIntrinsic=_dexp', '-XX:DisableIntrinsic=_dlog', extra,
    '-cp', shQuote(cp), shQuote(PROBE_CLASS), shQuote(casefile)),
    stdout = TRUE, stderr = TRUE)
  rc <- attr(st, 'status')
  list(result = st, status = if (is.null(rc)) 0L else rc)
}

# One JVM process per case (never more than one live at a time).

# Ground-truth guard (21-D.2): the intrinsic-disable flags must be live in the
# JVM that runs the cases.
local({
  x <- 0.022970045955171898  # bits 3f978575b0a90ad5: 1 ulp dexp deviation
  jvmflags <- c('-XX:+UnlockDiagnosticVMOptions', '-XX:DisableIntrinsic=_dexp',
    '-XX:DisableIntrinsic=_dlog')
  cf <- tempfile('guard-'); on.exit(unlink(cf))
  writeLines(c('AM guard TRfull'), cf)
  out <- jvmrun(cf, classpath, extra = jvmflags)
  if (!any(startsWith(out$result, 'am guard ')))
    stop('probe guard run failed: ', paste(utils::tail(out$result, 3L), collapse = ' | '))
  em <- .jcall('java/lang/Math', 'D', 'exp', x)
  es <- .jcall('java/lang/StrictMath', 'D', 'exp', x)
  if (!identical(em, es))
    stop('capture JVM still uses the intrinsic Math.exp: run with ',
      '_JAVA_OPTIONS="-XX:+UnlockDiagnosticVMOptions -XX:DisableIntrinsic=_dexp ',
      '-XX:DisableIntrinsic=_dlog"')
})

facts <- list(); events <- list(); inputs <- list(); summaries <- list()

case <- function(id, note, hdr, y, extra = character(0)) {
  lines <- c(hdr, paste('values',
    paste(ifelse(is.na(y), 'nan', vapply(y, g, character(1))), collapse = ' ')))
  for (ln in lines)
    inputs[[length(inputs) + 1L]] <<- data.frame(id = id, line = ln, stringsAsFactors = FALSE)
  cf <- tempfile('rtcase-'); on.exit(unlink(cf))
  writeLines(lines, cf)
  out <- jvmrun(cf, classpath, extra = extra)
  if (out$status != 0L)
    stop('case JVM failed for ', id, ': ',
      paste(utils::tail(out$result, 4L), collapse = ' | '))
  out <- out$result
  out <- out[nzchar(out) & !grepl('^(Picked up|NOTE:|WARNING:|^OpenJDK)', out)]
  if (!identical(out[[1L]], paste('case', id))) {
    if (any(startsWith(out, 'fatal ')))
      stop('probe failed on ', id, ': ', paste(out[startsWith(out, 'fatal ')],
        collapse = ' | '))
    stop('observer echoed a different case id for ', id, ': ',
      paste(utils::head(out, 2L), collapse = ' | '))
  }
  for (ln in out[-1L]) {
    if (startsWith(ln, 'evt ')) {
      m <- regmatches(ln, regexec('^evt ([0-9]+) ([a-z]+) ([0-9]+) (.*)$', ln))[[1L]]
      if (length(m) != 5L) stop('unparsable event line: ', ln)
      events[[length(events) + 1L]] <<- data.frame(id = id, seq = m[[2L]],
        kind = m[[3L]], depth = m[[4L]], msg = m[[5L]], stringsAsFactors = FALSE)
    } else if (startsWith(ln, 'req ')) {
      inputs[[length(inputs) + 1L]] <<- data.frame(id = id, line = ln,
        stringsAsFactors = FALSE)
    } else {
      k <- regmatches(ln, regexec('^([^ ]+) (.*)$', ln))[[1L]]
      if (length(k) != 3L) stop('unparsable fact line: ', ln)
      facts[[length(facts) + 1L]] <<- data.frame(id = id, key = k[[2L]],
        value = k[[3L]], stringsAsFactors = FALSE)
    }
  }
  fact <- function(key) {
    v <- out[startsWith(out, paste0(key, ' '))]
    if (!length(v)) '' else sub(paste0('^', key, ' '), '', v[[1L]])
  }
  factlast <- function(key) {
    v <- out[startsWith(out, paste0(key, ' '))]
    if (!length(v)) '' else sub(paste0('^', key, ' '), '', v[[length(v)]])
  }
  grab <- function(key, pat) {
    v <- out[startsWith(out, key)]
    if (!length(v)) '' else {
      m <- regmatches(v[[length(v)]], regexec(pat, v[[length(v)]]))[[1L]]
      if (length(m) >= 2L) m[[2L]] else ''
    }
  }
  summaries[[length(summaries) + 1L]] <<- data.frame(
    id = id, note = note, kind = sub(' .*', '', hdr),
    built = 'NA', threw = '',
    decision = if (nzchar(grab(paste0('fr ', id, ' run a '), 'result ([A-Za-z]+)'))) {
      grab(paste0('fr ', id, ' run a '), 'result ([A-Za-z]+)')
    } else if (nzchar(grab(paste0('dr ', id, ' run a '), 'result ([A-Za-z]+)'))) {
      grab(paste0('dr ', id, ' run a '), 'result ([A-Za-z]+)')
    } else grab(paste0('pg ', id, ' ret '), '^pg [^ ]+ ret ([01])'),
    spec_after = local({
      v <- c(out[startsWith(out, paste0('fr', ' ', id, ' '))],
             out[startsWith(out, paste0('dr', ' ', id, ' '))],
             out[startsWith(out, paste0('pgm ', id, ' '))])
      if (!length(v)) '' else {
        m <- regmatches(v[[1L]], regexec('spec ([^ ]+)', v[[1L]]))[[1L]]
        if (length(m) == 2L) m[[2L]] else ''
      }
    }),
    lbp = local({
      v <- out[startsWith(out, paste0('msr ', id, ' '))]
      if (!length(v)) '' else {
        m <- regmatches(v[[1L]], regexec('lbp ([^ ]+)', v[[1L]]))[[1L]]
        if (length(m) == 2L) m[[2L]] else ''
      }
    }),
    nevents = length(grep('^evt ', out)),
    stringsAsFactors = FALSE)
  invisible(out)
}

# Class-load witness: one fresh JVM per case with the load journal on; the
# counts prove which machines the no-calendar cases really run and that the
# 21-I.3-owned controller classes stay unloaded there (and DO load on the
# calendar seam case, which is why the unit refuses those requests).
loadwitness <- function(id, lines) {
  cf <- tempfile('loadcase-'); on.exit(unlink(cf))
  writeLines(lines, cf)
  cl <- tempfile('classload-'); on.exit(unlink(cl), add = TRUE)
  st <- system2(javabin, c('-XX:+UnlockDiagnosticVMOptions',
    '-XX:DisableIntrinsic=_dexp', '-XX:DisableIntrinsic=_dlog',
    shQuote(paste0('-Xlog:class+load=info:file=', cl)),
    '-cp', shQuote(classpath), shQuote(PROBE_CLASS), shQuote(cf)),
    stdout = TRUE, stderr = TRUE)
  rc <- attr(st, 'status')
  if (!is.null(rc) && rc != 0L)
    stop('load-witness JVM failed for ', id, ': ',
      paste(utils::tail(st, 4L), collapse = ' | '))
  log <- readLines(cl, warn = FALSE)
  loaded <- sort(unique(gsub('0x[0-9a-fA-F]+', '0xADDR',
    sub(' source:.*$', '', sub('^.*class,load\\] ', '',
      grep(' source: file:.*[.]jar', log, value = TRUE))))))
  writeLines(loaded, file.path(output, paste0('loadlog-', id, '.txt')))
  greps <- c(
    'jdplus.tramoseats.base.core.tramo.FastRegressionTest',
    'jdplus.tramoseats.base.core.tramo.DefaultRegressionTest',
    'jdplus.tramoseats.base.core.tramo.ModelStatistics',
    'jdplus.toolkit.base.core.stats.tests.LjungBox',
    'jdplus.toolkit.base.core.stats.likelihood.ConcentratedLikelihood',
    'jdplus.tramoseats.base.core.tramo.TramoKernel',
    'jdplus.tramoseats.base.core.tramo.TramoModelBuilder',
    'jdplus.tramoseats.base.core.tramo.internal.TradingDaysController',
    'jdplus.tramoseats.base.core.tramo.internal.AutomaticWaldTest',
    'jdplus.tramoseats.base.core.tramo.internal.AutomaticFRegressionTest',
    'jdplus.toolkit.base.core.stats.tests.WaldTest',
    'jdplus.toolkit.base.core.stats.tests.FRegressionTest')
  counts <- vapply(greps, function(pat) sum(startsWith(loaded, pat)), integer(1))
  names(counts) <- c('fastregressiontest', 'defaultregressiontest',
    'modelstatistics', 'ljungbox', 'concentratedlikelihood', 'tramokernel',
    'tramomodelbuilder', 'tradingdayscontroller', 'automaticwaldtest',
    'automaticfregressiontest', 'waldtest', 'fregressiontest')
  data.frame(case = id, as.list(counts), check.names = FALSE, row.names = NULL)
}

# -- series --------------------------------------------------------------------
n <- 120L
t <- seq_len(n)
set.seed(2351)
cln <- 100 + 8 * sin(2 * pi * ((t - 1) %% 12) / 12) +
  4 * cos(4 * pi * ((t - 1) %% 12) / 12) +
  0.5 * sin(2 * pi * ((t - 1) %% 7) / 7) + rnorm(n, 0, 0.6)
set.seed(2352)
nz1 <- rnorm(n)                     # ladder lbp ~ 0.14 (rejects)
set.seed(2353)
nz2 <- rnorm(n)                     # ladder lbp ~ 0.30
set.seed(2355)
nz3 <- rnorm(n)                     # ladder lbp ~ 0.92 (near the band edge)
set.seed(2357)
walk <- 50 + cumsum(rnorm(n, 0.05, 1)) +
  0.3 * sin(2 * pi * ((t - 1) %% 12) / 12)
set.seed(2359)
q4 <- 40 + 0.2 * seq_len(60) + 6 * sin(2 * pi * (seq_len(60) - 1) / 4) +
  rnorm(60, 0, 0.5)
set.seed(2361)
short12 <- 50 + rnorm(20, 0, 2) + 3 * sin(2 * pi * (seq_len(20) - 1) / 12)
zer <- rep(0, n)

AIR <- '1 1 0 1 0 1'      # d bd p q bp bq (0,1,1)(0,1,1)
HARD <- '1 1 1 1 1 1'
RW <- '1 0 1 1 0 0'

# -- MS: ModelStatistics.of ------------------------------------------------------
ms <- function(id, note, y, freq = 12L, spec = AIR, mean = 'n', log = 'n',
    nout = 0L, ip = '1e-8', mode = 'real', ndata = 0L, nmissing = 0L,
    ssq = '0', b = NULL, cov = NULL, ami = 'n') {
  hdr <- paste('MS', id, freq, log, mean, spec, nout, ip, mode)
  if (mode == 'craft')
    hdr <- paste(hdr, ndata, nmissing, ssq, ami)
  else if (nzchar(ami))
    hdr <- paste(hdr, '0', '0', '0', ami)
  lines <- c(hdr)
  if (!is.null(b)) lines <- c(lines, paste('b', paste(vapply(b, g, character(1)),
    collapse = ' ')))
  if (!is.null(cov)) lines <- c(lines, paste('cov',
    paste(vapply(cov, g, character(1)), collapse = ' ')))
  lines <- c(lines, paste('values',
    paste(ifelse(is.na(y), 'nan', vapply(y, g, character(1))), collapse = ' ')))
  for (ln in lines)
    inputs[[length(inputs) + 1L]] <<- data.frame(id = id, line = ln,
      stringsAsFactors = FALSE)
  cf <- tempfile('rtcase-'); on.exit(unlink(cf))
  writeLines(lines, cf)
  out <- jvmrun(cf, classpath)
  if (out$status != 0L)
    stop('case JVM failed for ', id, ': ', paste(utils::tail(out$result, 4L),
      collapse = ' | '))
  res <- out$result
  res <- res[nzchar(res) & !grepl('^(Picked up|NOTE:|WARNING:|^OpenJDK)', res)]
  if (!identical(res[[1L]], paste('case', id)))
    stop('observer echoed a different case id for ', id, ': ',
      paste(utils::head(res, 2L), collapse = ' | '))
  for (ln in res[-1L]) {
    if (startsWith(ln, 'evt ')) {
      m <- regmatches(ln, regexec('^evt ([0-9]+) ([a-z]+) ([0-9]+) (.*)$', ln))[[1L]]
      events[[length(events) + 1L]] <<- data.frame(id = id, seq = m[[2L]],
        kind = m[[3L]], depth = m[[4L]], msg = m[[5L]], stringsAsFactors = FALSE)
    } else if (startsWith(ln, 'req ')) {
      inputs[[length(inputs) + 1L]] <<- data.frame(id = id, line = ln,
        stringsAsFactors = FALSE)
    } else {
      k <- regmatches(ln, regexec('^([^ ]+) (.*)$', ln))[[1L]]
      facts[[length(facts) + 1L]] <<- data.frame(id = id, key = k[[2L]],
        value = k[[3L]], stringsAsFactors = FALSE)
    }
  }
  line <- res[startsWith(res, paste0('ms ', id, ' built '))][[1L]]
  summaries[[length(summaries) + 1L]] <<- data.frame(id = id, note = note,
    kind = 'MS', built = sub('^.* built ([01]) .*$', '\\1', line),
    threw = sub('^.* threw (.*)$', '\\1', line), decision = '', spec_after = '',
    lbp = local({
      v <- res[startsWith(res, paste0('msr ', id, ' '))]
      if (!length(v)) '' else sub('.* lbp ([^ ]+) .*$', '\\1', v[[1L]])
    }), nevents = length(grep('^evt ', res)), stringsAsFactors = FALSE)
  invisible(res)
}

ms('ms_airline', 'real airline fit on the clean seasonal base', cln)
ms('ms_airline_mean', 'real airline fit with the mean regressor', cln, mean = 'y')
ms('ms_outliers', 'real fit carrying two ami-identified outliers', cln,
  nout = 2L, ami = 'y')
ms('ms_walk', 'real airline fit on the seasonal walk', walk)
ms('ms_craft_flat', 'crafted likelihood, no regressors, unit ssq', cln,
  mode = 'craft', ndata = n, ssq = '1')
ms('ms_craft_zerossq', 'crafted likelihood with ssq = 0 (sigma2 = 0, bic = -Inf)',
  cln, mode = 'craft', ndata = n, ssq = '0')
ms('ms_craft_nancoef', 'crafted likelihood with a NaN coefficient and covariance',
  cln, mode = 'craft', ndata = n, ssq = '1',
  b = c(0.5, NaN), cov = c(1, NaN))
ms('ms_short', 'n = 20 slice: m = n/2 with the seasonal block', short12)
ms('ms_quarter', 'quarterly shape (D11 refusal evidence, record only)', q4,
  freq = 4L)
ms('ms_craft_dfneg', 'crafted likelihood with k = m (df = 0): the Chi2 throw ' ,
  cln, spec = '0 0 12 12 0 0', mode = 'craft', ndata = n, ssq = '1')

# -- FR / DR / PG / AM through the generic case() --------------------------------
fr <- function(id, note, y, spec = AIR, mean = 'y', log = 'n', ip = '1e-8',
    tmean = '1.96', testmean = 'y', vars = '-', exact = 'n') {
  case(id, note, sprintf('FR %s 12 %s %s %s %s %s', id, log, mean, spec, ip,
    paste(tmean, testmean, vars, exact, paste = ' ')), y)
}

fr('fr_reject', 'mean test rejects the mean (|t| 0.52 < 1.96) on the clean base',
  cln)
fr('fr_boundary', 'meanThreshold = |t| of the machine addmu arm: >= accepts',
  cln, exact = 'y')
fr('fr_nomeantest', 'testMean off: the mean branch never runs', cln,
  testmean = 'n')
fr('fr_nomean', 'no mean regressor: the mean test is inert', cln, mean = 'n')
fr('fr_inert_regs', 'outliers and a user regressor are inert in Fast', cln,
  vars = 'ao@2,ls@5,tc@8,u1')
fr('fr_nonauto_cal', 'plain (non-ami) td/lp/easter are inert in Fast', cln,
  vars = 'tdn,lpn,ean')
fr('fr_seam_td', 'SEAM: ami td (the JVM T kernel runs, the unit refuses)', cln,
  vars = 'tda')
fr('fr_seam_td_lp', 'SEAM: ami td plus ami lp (the lp fallback runs)', cln,
  vars = 'tda,lpa')
fr('fr_seam_easter', 'SEAM: ami easter (mhTest path)', cln, vars = 'eaa')
fr('fr_walk_reject', 'mean test on the seasonal walk (different t bits)', walk)
fr('fr_boundary_walk', 'boundary on the walk (exact |t| plus one ulp)', walk,
  exact = 'y')
fr('fr_mean_reject_noise', 'mean test on white noise with a wide threshold',
  nz1, tmean = '0.1')

dr <- function(id, note, y, spec = AIR, mean = 'y', ip = '1e-8',
    testmean = 'y', tmean = '1.96', prec = '1e-5', adjust = 'n', join = 'n',
    td = 'n', lp = 'n', easter = 'n', exact = 'n') {
  case(id, note, paste('DR', id, 12, 'n', mean, spec, ip, testmean, tmean,
    prec, adjust, join, td, lp, easter, exact), y)
}

dr('dr_unprocessed', 'no calendar field and no mean test: Unprocessed', cln,
  testmean = 'n')
dr('dr_reject', 'mean test rejects (test-model estimation echoed)', cln,
  exact = 'y')
dr('dr_nomean', 'no mean regressor and no calendar field: Unprocessed', cln,
  mean = 'n')
dr('dr_join', 'joinTest with no calendar field (F kernel never built)', cln,
  join = 'y')
dr('dr_adjust', 'adjust with no calendar field: the adjust arm cannot run', cln,
  adjust = 'y')
dr('dr_seam_td', 'SEAM: calendar td field (the JVM builds and tests it)', cln,
  td = 't')
dr('dr_seam_lp', 'SEAM: calendar leap-year field', cln, lp = 'l')
dr('dr_seam_easter', 'SEAM: calendar easter field', cln, easter = 'e')
dr('dr_walk', 'mean test on the seasonal walk', walk)
dr('dr_noise_boundary', 'boundary on white noise (test-model |t| exactly)',
  nz2, exact = 'y')

pg <- function(id, note, y, spec = 'TRfull', pass = 0L, round = 0L,
    pcr = '0.975', changed = 'y', seasonal = 'y', curva = '3.5', cur = AIR,
    cmean = 'n', curout = 0L, refmode = 'none', ref = AIR, refout = 0L,
    reflbp = '0.5', refse = '1', phi = '-', bphi = '-', ami = 'man') {
  hdr <- paste('PG', id, spec, pass, round, pcr, changed, seasonal, curva, cur,
    cmean, curout, refmode, ref, refout, reflbp, refse, phi, bphi, ami)
  case(id, note, hdr, y)
}

pg('pg_null_ref', 'refAuto null: the model becomes the reference', cln,
  pass = 0L)
pg('pg_skip', 'skip rule: nr > nb keeps the reference and evolves pcr', nz2,
  pass = 2L, round = 1L, pcr = '0.99', cur = HARD, curout = 1L,
  refmode = 'craft', ref = AIR, refout = 3L, reflbp = '0.9', refse = '5',
  ami = 'ami')
pg('pg_revert_A', 'revert arm A: both LB p-values weak and the reference se wins',
  nz1, pass = 1L, refmode = 'craft', reflbp = '0.95', refse = '0.01')
pg('pg_revert_pass1', 'revert arm at pass 1: current LB fine, reference weak',
  cln, pass = 1L, refmode = 'craft', reflbp = '0.3', refse = '0.01')
pg('pg_revert_se', 'revert arm at pass 2 (the se branch after the LB band)',
  cln, pass = 2L, pcr = '0.99', refmode = 'craft', reflbp = '0.3',
  refse = '0.01')
pg('pg_shapeA', 'shapeA revert: d = 0 bd = 1 p = 1 phi(1) <= -0.82 bq = 1', cln,
  pass = 2L, pcr = '0.99', cur = '0 1 1 1 0 1', refmode = 'craft',
  reflbp = '0.02', refse = '99', phi = '-0.9')
pg('pg_shapeA_miss', 'shapeA near miss: phi(1) = -0.8 keeps the current model',
  cln, pass = 2L, pcr = '0.99', cur = '0 1 1 1 0 1', refmode = 'craft',
  reflbp = '0.02', refse = '99', phi = '-0.8')
pg('pg_shapeB', 'shapeB revert: d = 1 bd = 0 p = 0 q = 1 bp = 1 bphi(1) <= -0.65',
  walk, pass = 2L, pcr = '0.99', cur = '1 0 0 1 1 1', refmode = 'craft',
  reflbp = '0.02', refse = '99', bphi = '-0.7')
pg('pg_shapeB_miss', 'shapeB near miss: bphi(1) = -0.6 keeps', walk,
  pass = 2L, pcr = '0.99', cur = '1 0 0 1 1 1', refmode = 'craft',
  reflbp = '0.02', refse = '99', bphi = '-0.6')
pg('pg_nan_ref', 'crafted NaN reference rows: the dcmpg/dcmpl ordering', cln,
  pass = 1L, refmode = 'craft', reflbp = 'nan', refse = 'nan')
pg('pg_real_ref', 'both sides live (real reference statistics)', walk,
  pass = 2L, pcr = '0.99', refmode = 'real', ref = RW, cmean = 'y')
pg('pg_remark', 'pass 1 continue: reduceVa(curva) and the critical-value remark',
  cln, pass = 1L, pcr = '0.5', curout = 1L, refmode = 'craft',
  reflbp = '0.01', refse = '99', ami = 'ami')
pg('pg_last_solution', 'pass 4 after the gate: the last-chance rewrite', cln,
  spec = 'TRfull', pass = 3L, round = 2L, pcr = '0.5', cur = HARD,
  refmode = 'craft', reflbp = '0.01', refse = '99')
pg('pg_keep_low_lbp', 'no revert with the weak-LB current model (branch D)',
  nz2, pass = 2L, pcr = '0.99', refmode = 'craft', reflbp = '0.99',
  refse = '99')
pg('pg_revert_C', 'revert arm C at pass 2: reference se wins with lbp above 0.95',
  cln, pass = 2L, pcr = '0.99', refmode = 'craft', reflbp = '0.7',
  refse = '0.01')
pg('pg_revert_A_short', 'revert arm A with the short (n = 20) model', short12,
  pass = 1L, cur = RW, ref = RW, refmode = 'craft', reflbp = '0.95',
  refse = '0.01')

for (nm in c('TR0', 'TR1', 'TR2', 'TR3', 'TR4', 'TR5', 'TRfull'))
  case(paste0('am_', nm), paste0('readAmiOptions for ', nm),
    paste('AM', paste0('am_', nm), nm), numeric(0))

witness <- rbind(
  loadwitness('fr_reject', c('FR fr_reject 12 n y 1 1 0 1 0 1 1e-8 1.96 y - n',
    paste('values', paste(vapply(cln, g, character(1)), collapse = ' ')))),
  loadwitness('dr_reject', c('DR dr_reject 12 n y 1 1 0 1 0 1 1e-8 y 1.96 1e-5 n n n n n n',
    paste('values', paste(vapply(cln, g, character(1)), collapse = ' ')))),
  loadwitness('pg_remark', c(paste('PG pg_remark TRfull 1 0 0.5 y y 3.5', AIR,
    'n 1 craft', RW, 0, '0.01', '99', '-', '-', 'ami'),
    paste('values', paste(vapply(cln, g, character(1)), collapse = ' ')))),
  loadwitness('fr_seam_td', c('FR fr_seam_td 12 n y 1 1 0 1 0 1 1e-8 1.96 y tda n',
    paste('values', paste(vapply(cln, g, character(1)), collapse = ' ')))))

wcsv <- function(name, df) {
  con <- file(file.path(output, name), 'w')
  write.csv(df, con, row.names = FALSE, na = '')
  close(con)
}
wcsv('cases.csv', do.call(rbind, summaries))
wcsv('inputs.csv', do.call(rbind, inputs))
wcsv('rt-facts.csv', do.call(rbind, facts))
wcsv('rt-events.csv', do.call(rbind, events))
wcsv('load-witness.csv', witness)

writeLines(c(
  paste0('# metadata for the item 23-E tramo-regression-tests capture ',
    '(generated by validation/reference/capture-tramo-regression-tests.R)'),
  paste0('r_version <- "', paste(R.version$major, R.version$minor, sep = '.'), '"'),
  paste0('jvm_version <- "', .jcall('java/lang/System', 'S', 'getProperty', 'java.version'), '"'),
  paste0('jvm_vendor <- "', .jcall('java/lang/System', 'S', 'getProperty', 'java.vendor'), '"'),
  paste0('java_options <- "', Sys.getenv('_JAVA_OPTIONS'), '"'),
  paste0('capture_time <- "', format(Sys.time(), tz = 'UTC'), '"'),
  paste0('capture_host <- "', Sys.info()[['nodename']], '"'),
  paste0('capture_user <- "', Sys.info()[['user']], '"'),
  paste0('probe_sha256 <- "', reference_sha256(file.path(root,
    'validation/reference/TramoRegressionTestsProbe.java')), '"'),
  paste0('script_sha256 <- "', reference_sha256(file.path(root,
    'validation/reference/capture-tramo-regression-tests.R')), '"'),
  'jar_sha256 <- c(',
  paste0('  "', vapply(jars, function(p) paste0(basename(p), '=',
    reference_sha256(p)), character(1)), '"',
    collapse = ',\n  '),
  ')'
), file.path(output, 'metadata.R'))

files <- setdiff(list.files(output), 'SHA256SUMS.csv')
if (nzchar(repro)) {
  # Byte-identity gate over the data files; metadata.R legitimately differs in
  # its capture_time field only (final-information precedent).
  data_files <- setdiff(files, 'metadata.R')
  prior <- setdiff(intersect(list.files(repro), data_files),
    c('metadata.R', 'SHA256SUMS.csv'))
  if (!identical(sort(data_files), sort(prior)))
    stop('repro capture has a different file set: ',
      paste(setxor(data_files, prior), collapse = ', '))
  for (f in data_files) {
    a <- readBin(file.path(output, f), 'raw', file.size(file.path(output, f)))
    b <- readBin(file.path(repro, f), 'raw', file.size(file.path(repro, f)))
    if (!identical(a, b)) stop('repro capture differs byte-wise: ', f)
  }
  message('repro check: ', length(data_files), ' data files byte-identical')
}
write.csv(data.frame(file = files,
  sha256 = vapply(files, function(f)
    digest::digest(file = file.path(output, f), algo = 'sha256'), character(1))),
  file.path(output, 'SHA256SUMS.csv'), row.names = FALSE, quote = FALSE)
message('wrote ', length(files), ' files to ', output)
