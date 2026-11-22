# Pinned-reference capture for item 23-C.2: the BATTERY STAGE of the round-1
# TRAMO differencing decision of the pinned jdplus 3.9.0 stack, observed by
# TramoBatteryProbe.java - the live driver segment (estimate(ip), the LIVE
# DifferencingModule, the iterate()-style outlier cleanup, ModelEstimator
# (precision, curva, null)), then the SeasonalityController passes, the
# RegularUnderDifferencingTest/SeasonalUnderDifferencingTest(+2) loop with the
# refAuto references and the SeasonalOverDifferencingTest over-side, with the
# per-state stage facts the battery decisions consume: the F-test values and
# the F degrees of freedom (parsed from the StatisticalTest description - the
# raw getter of dstats.F), the ModelStatistics comparator panel (ten raw
# getters), the ConcentratedLikelihood residuals, the linearized series, the
# checkMean pair, the controller-side and residual-ladder SeasonalityTests
# facts with the SpectralPeaks records, and the ObservingEstimator snapshots
# of every candidate model the battery estimates internally. Estimation,
# ML/JointTest algebra and the periodogram stay pinned at the JVM boundary
# (docs/contracts/tramo-differencing-decision-contract.md); the differencing
# DECISIONS and the incomplete-beta F p-values are the new native surface.
#
# Corpus: the nine io = n BAT cases of the item 23-C capture (rebuilt from
# its seeded series recipes and replayed here with the battery observers)
# plus six forcing cases: RegularUnderDifferencingTest
# test1 with a real candidate (f_ru1), the checkMean branch of
# RegularUnderDifferencingTest2 at d = 2 (f_mean), a second
# RegularUnderDifferencingTest2 Changed (f_ru2q, quarterly), the
# SeasonalUnderDifferencingTest bq-branch candidate (f_su2) and the two
# spectral-peak-forcing cases (f_peaks monthly, f_qpeaks quarterly) whose
# periodogram arrays carry the k = 3..6 peak records. The outlier-module-on
# cases of the old corpus (io = y) are out of scope here - the multi-round
# ODM driver belongs to item 23-F.
#
# Force-load witness: every new case (and rw0 from the replay corpus) is
# re-run in a separate JVM with -Xlog:class+load=info:file=... over the probe
# main; the raw normalized load logs and the per-class counts (battery
# classes plus jdplus.sa.base.core.tests.SpectralPeaks, SeasonalityTests,
# ModelStatistics and the dstats.F/SpecialFunctions kernel) land in the
# evidence directory. SeasonalOverDifferencingTest load lines PROVE the
# over-side runs (the old corpus never loaded it).
#
# Series are built in R and handed over as %.17g tokens (inputs bit-exact by
# construction); the G2 intrinsic-disable recipe is the same as the item 21-D
# captures and must be live in the capture JVM (ground-truth guard below).
#
# Usage (the rJava JVM needs the pinned G2 flags and OpenJDK 21):
#   JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64 \
#   _JAVA_OPTIONS="-XX:+UnlockDiagnosticVMOptions -XX:DisableIntrinsic=_dexp \
#       -XX:DisableIntrinsic=_dlog" \
#   Rscript validation/reference/capture-tramo-differencing-battery.R \
#       --out=validation/evidence/native/tramo-differencing-battery
# Add --repro=<dir> for the byte-identity cross-check between independent
# JVM processes (same inputs -> identical fact files).
#!/usr/bin/env Rscript

options(stringsAsFactors = FALSE)
suppressPackageStartupMessages({
  library(rJava)
  library(rjd3jars)
  library(rjd3toolkit)
  library(rjd3tramoseats)
})

args <- commandArgs(trailingOnly = TRUE)
outarg <- grep('^--out=', args, value = TRUE)
output <- if (length(outarg))
  sub('^--out=', '', outarg[[1L]]) else 'validation/evidence/native/tramo-differencing-battery'
repro <- grep('^--repro=', args, value = TRUE)
repro <- if (length(repro)) sub('^--repro=', '', repro[[1L]]) else NULL
dir.create(output, recursive = TRUE, showWarnings = FALSE)
output <- normalizePath(output)

tooling <- if (file.exists('TramoBatteryProbe.java'))
  '.' else 'validation/reference'
javahome <- Sys.getenv('JAVA_HOME')
javac <- if (nzchar(javahome)) file.path(javahome, 'bin', 'javac') else Sys.which('javac')
if (!nzchar(javac) || !file.exists(javac))
  stop('javac is required (JAVA_HOME/bin or PATH)')
jvm_major <- as.integer(sub('^([0-9]+).*', '\\1',
  .jcall('java/lang/System', 'S', 'getProperty', 'java.version')))
if (jvm_major != 21L)
  stop('the pinned capture JVM is OpenJDK 21: run with ',
    'JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64 (rJava picked up JVM ',
    .jcall('java/lang/System', 'S', 'getProperty', 'java.version'), ')')
probe <- tempfile('kalos-tramo-battery-'); dir.create(probe)
jars <- c(list.files(system.file('java', package = 'rjd3jars'), pattern = '[.]jar$',
  full.names = TRUE),
  list.files(system.file('java', package = 'rjd3toolkit'), pattern = '[.]jar$',
    full.names = TRUE),
  list.files(system.file('java', package = 'rjd3tramoseats'), pattern = '[.]jar$',
    full.names = TRUE))
classpath <- paste(jars, collapse = .Platform$path.sep)
status <- system2(javac, c('--release', jvm_major, '-d', shQuote(probe),
  '-cp', shQuote(classpath),
  shQuote(file.path(tooling, 'TramoBatteryProbe.java'))))
if (status != 0L) stop('probe compilation failed')
status <- system2(javac, c('--release', jvm_major, '-d', shQuote(probe),
  '-cp', shQuote(classpath),
  shQuote(file.path(tooling, 'FcdfGolden.java'))))
if (status != 0L) stop('F-kernel golden compilation failed')
refdir <- if (tooling == '.') 'validation/reference' else tooling
.jaddClassPath(c(jars[grep('tramoseats', jars)], probe))
PROBE_CLASS <- 'jdplus.tramoseats.base.core.tramo.TramoBatteryProbe'
if (inherits(try(.jfindClass(PROBE_CLASS), silent = TRUE), 'try-error'))
  stop('observer class not loadable in the rJava JVM')
# Ground-truth guard (21-D.2): the intrinsic-disable flags must be live.
local({
  x <- 0.022970045955171898  # bits 3f978575b0a90ad5: 1 ulp dexp deviation
  em <- .jcall('java/lang/Math', 'D', 'exp', x)
  es <- .jcall('java/lang/StrictMath', 'D', 'exp', x)
  if (!identical(em, es))
    stop('capture JVM still uses the intrinsic Math.exp: run with ',
      '_JAVA_OPTIONS="-XX:+UnlockDiagnosticVMOptions -XX:DisableIntrinsic=_dexp ',
      '-XX:DisableIntrinsic=_dlog"')
})

g <- function(x) sprintf('%.17g', x)

facts <- list(); events <- list(); inputs <- list(); summaries <- list()
corpus_inputs <- list()

case <- function(id, note, hdr, y) {
  corpus_inputs[[id]] <<- list(hdr = hdr, y = y)
  lines <- c(hdr, paste('values',
    paste(ifelse(is.na(y), 'nan', vapply(y, g, character(1))), collapse = ' ')))
  for (ln in lines)
    inputs[[length(inputs) + 1L]] <<- data.frame(id = id, line = ln, stringsAsFactors = FALSE)
  res <- .jcall(PROBE_CLASS, 'S', 'run', .jarray(lines))
  out <- strsplit(res, '\n', fixed = TRUE)[[1L]]
  out <- out[nzchar(out)]
  if (!identical(out[[1L]], paste('case', id))) {
    if (length(out) > 1L && startsWith(out[[2L]], 'fatal '))
      stop('probe failed on ', id, ': ', out[[2L]])
    stop('observer echoed a different case id for ', id)
  }
  for (ln in out[-1L]) {
    if (startsWith(ln, 'evt ')) {
      m <- regmatches(ln, regexec(
        '^evt ([0-9]+) ([a-z]+) ([0-9]+) (.*) det (.*)$', ln))[[1L]]
      if (length(m) != 6L) stop('unparsable event line: ', ln)
      events[[length(events) + 1L]] <<- data.frame(id = id, seq = m[[2L]],
        kind = m[[3L]], depth = m[[4L]], msg = m[[5L]], detail = m[[6L]],
        stringsAsFactors = FALSE)
    } else if (startsWith(ln, 'req ') || startsWith(ln, 'in ')) {
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
  summaries[[length(summaries) + 1L]] <<- data.frame(
    id = id, note = note,
    diff_result = sub(paste0('^', id, ' '), '', fact('diff_result')),
    changed = any(grepl('^step .* Changed', out)),
    ftests = sum(grepl('^ftest ', out) & !grepl(' null$', out)),
    cands = length(grep('^cand ', out)),
    nevents = length(grep('^evt ', out)),
    stringsAsFactors = FALSE)
  invisible(out)
}

# Force-load witness: re-run one case in a fresh JVM with the class-load
# journal on, and count the load lines of the battery-stack classes.
loadwitness <- function(id, hdr, y) {
  cf <- tempfile('loadcase-'); on.exit(unlink(cf))
  writeLines(c(hdr, paste('values',
    paste(vapply(y, g, character(1)), collapse = ' '))), cf)
  cl <- tempfile('classload-'); on.exit(unlink(cl), add = TRUE)
  javabin <- if (nzchar(javahome)) file.path(javahome, 'bin', 'java') else Sys.which('java')
  st <- system2(javabin, c('-XX:+UnlockDiagnosticVMOptions',
    '-XX:DisableIntrinsic=_dexp', '-XX:DisableIntrinsic=_dlog',
    shQuote(paste0('-Xlog:class+load=info:file=', cl)),
    '-cp', shQuote(paste(classpath, probe, sep = .Platform$path.sep)),
    shQuote(PROBE_CLASS), shQuote(cf)), stdout = TRUE, stderr = TRUE)
  rc <- attr(st, 'status')
  if (!is.null(rc) && rc != 0L)
    stop('load-witness JVM failed for ', id, ': ',
      paste(utils::tail(st, 4L), collapse = ' | '))
  log <- readLines(cl, warn = FALSE)
  # Normalize to the jar-loaded application class set: class names only
  # (uptime/CDS prefixes dropped, per-JVM class-space addresses in anonymous
  # class names canonicalized), unique and sorted.
  loaded <- sort(unique(gsub('0x[0-9a-fA-F]+', '0xADDR',
    sub(' source:.*$', '', sub('^.*class,load\\] ', '',
      grep(' source: file:.*[.]jar', log, value = TRUE))))))
  writeLines(loaded, file.path(output, paste0('loadlog-', id, '.txt')))
  greps <- c('jdplus.tramoseats.base.core.tramo.SeasonalityController',
    'jdplus.tramoseats.base.core.tramo.RegularUnderDifferencingTest',
    'jdplus.tramoseats.base.core.tramo.SeasonalUnderDifferencingTest',
    'jdplus.tramoseats.base.core.tramo.RegularUnderDifferencingTest2',
    'jdplus.tramoseats.base.core.tramo.SeasonalUnderDifferencingTest2',
    'jdplus.tramoseats.base.core.tramo.SeasonalOverDifferencingTest',
    'jdplus.tramoseats.base.core.tramo.ModelEstimator',
    'jdplus.tramoseats.base.core.tramo.ModelStatistics',
    'jdplus.tramoseats.base.core.tramo.ModelComparator',
    'jdplus.sa.base.core.tests.SpectralPeaks',
    'jdplus.sa.base.core.tests.SeasonalityTests',
    'jdplus.toolkit.base.core.dstats.F',
    'jdplus.toolkit.base.core.dstats.SpecialFunctions')
  counts <- vapply(greps, function(pat) {
    suffix <- if (grepl('UnderDifferencingTest$', pat))
      sum(loaded == pat)     # exact: ...Test must not count ...Test2
    else sum(startsWith(loaded, pat))
  }, integer(1))
  data.frame(id = id, t(counts), row.names = NULL, check.names = FALSE,
    stringsAsFactors = FALSE)
}

write_lines <- function(name, lines)
  writeLines(lines, file.path(output, name))
write_df <- function(name, df)
  write.csv(df, file.path(output, name), row.names = FALSE, quote = TRUE)

facts_df <- function() {
  if (!length(facts))
    return(data.frame(id = character(), key = character(), value = character()))
  do.call(rbind, facts)
}
all_lines <- function()
  c(facts_df()$id, facts_df()$key, facts_df()$value,
    if (length(events)) do.call(rbind, events)$msg else character())

# --- corpus -----------------------------------------------------------------
# Replay layer: the io = n BAT cases of the item 23-C corpus, rebuilt from
# the exact seeded recipes of capture-tramo-differencing.R (same R RNG on the
# capture host -> bit-identical %.17g tokens) and replayed through the
# battery probe. The three io = y cases (b_io, ao_io, multi_io) stay with the
# 23-F multi-round ODM driver and are not replayed here.
n <- 96L
t <- seq_len(n)
set.seed(21202)
b <- 50 + 0.05 * t + 5 * sin(2 * pi * t / 12) + 3 * cos(4 * pi * t / 12) +
  0.5 * sin(14 * pi * t / 12) + rnorm(n, 0, 0.3)
set.seed(2302)
gs <- exp(cumsum(rnorm(n, .01, .02)) + .2 * sin(2 * pi * t / 12))
set.seed(2307)
rw <- 100 + cumsum(rnorm(n, 0.1, 1))
set.seed(2311)
ts2 <- seq_len(120)
srw <- cumsum(rnorm(120, 0.02, 1)) + 8 * sin(2 * pi * ts2 / 12) +
  3 * cos(4 * pi * ts2 / 12) + rnorm(120, 0, 0.4)
set.seed(2313)
dsem <- 50 + 0.05 * t + 8 * sin(2 * pi * t / 12) + 4 * cos(4 * pi * t / 12) +
  rnorm(n, 0, 0.05)
q4 <- 40 + 0.1 * seq_len(60) + 6 * sin(2 * pi * seq_len(60) / 4) +
  rnorm(60, 0, 0.1)
short50 <- b[1:50]

replayed <- 0L
bt <- function(id, note, y, freq = 12L, sea = 'y', ost = '0', log = 'n',
    spec = c(0, 0, 0, 0, 0, 0), mean = 'y', init = 'y') {
  hdr <- sprintf('BAT %s %d %s %s %s %d %d %d %d %d %d %s 0.05 0.97 0.91 1e-07 1e-07 %s n y y n y 0.7 n calc y',
    id, freq, sea, ost, log, spec[1], spec[2], spec[3], spec[4], spec[5],
    spec[6], mean, init)
  case(id, note, hdr, y)
  replayed <<- replayed + 1L
  invisible(NULL)
}
bt('b0', 'clean base, flat spec (23-C replay)', b)
bt('srw0', 'seasonal walk (23-C replay)', srw, ost = '2')
bt('dsem0', 'deterministic seasonal (23-C replay)', dsem)
bt('air_in', 'fixed airline entry (23-C replay)', b,
  spec = c(2, 1, 1, 1, 0, 1), init = 'n')
bt('rw0', 'random walk, RU2 fire (23-C replay)', rw)
bt('q4_0', 'quarterly driver segment (23-C replay)', q4, freq = 4L)
bt('growth_log', 'growth series with log (23-C replay)', gs, log = 'y')
bt('short_b', 'short base (23-C replay)', short50)
bt('sem_noseas', 'deterministic seasonal, seasonal = false (23-C replay)',
  dsem, sea = 'n')

# Forcing cases (all io = n, no calendar variables).
mk <- function(freq, n, f) {
  t <- seq_len(n)
  switch(as.character(freq),
    '12' = f(t),
    '4' = f(t))
}
set.seed(4242)
# f_ru1: monthly (0,1,0) with a strong annual cycle and a near-unit AR root:
# RegularUnderDifferencingTest test1 runs (d = 1, bd = 0) and its d = 2
# candidate is estimated through the observing estimator.
ru1_y <- mk(12, 96, function(t) 8 * sin(2 * pi * t / 12) + 4 * cos(2 * pi * t / 6)
  + arima.sim(list(ar = 0.7), 96) * 0.5)
case('f_ru1', 'ru1 test1 with a real d=2 candidate',
  'BAT f_ru1 12 n 0 n 1 0 1 0 0 0 y 0.1 4 4 1e-10 1e-8 n n 0 0 0 0 0.7 n calc 0', ru1_y)

# f_mean: d = 2 with a non-zero mean component -> the
# RegularUnderDifferencingTest2 checkMean branch (candidate without the mean).
mean_y <- mk(12, 96, function(t) 20 + 0.6 * t + 5 * sin(2 * pi * t / 12)
  + rnorm(96, sd = 0.7))
case('f_mean', 'ru2 checkMean branch at d=2',
  'BAT f_mean 12 n 0 n 2 0 0 0 0 0 n 0.1 4 4 1e-10 1e-8 n n 0 0 0 0 0.7 n calc 0', mean_y)

# f_su2: monthly (0,0,0) with bq = 1 -> the SeasonalUnderDifferencingTest
# bq-branch candidate (bq = 0); its post-step Changed also forces the
# SeasonalOverDifferencingTest over-side.
su2_y <- mk(12, 96, function(t) 7 * sin(2 * pi * t / 12) + 3 * sin(4 * pi * t / 12)
  + rnorm(96, sd = 1.1))
case('f_su2', 'su2 bq-branch candidate',
  'BAT f_su2 12 n 0 n 0 0 0 0 0 1 y 0.1 4 4 1e-10 1e-8 n n 0 0 0 0 0.7 n calc 0', su2_y)

# f_ru2q: quarterly RegularUnderDifferencingTest2 Changed (second ru2
# Changed witness beyond rw0).
set.seed(777)
ru2q_y <- mk(4, 60, function(t) arima.sim(list(ar = c(1.2, -0.55)), 60)
  + 6 * sin(2 * pi * t / 4))
case('f_ru2q', 'second ru2 Changed (quarterly)',
  'BAT f_ru2q 4 y 0 n 0 0 1 0 0 0 y 0.1 4 4 1e-10 1e-8 n n 0 0 0 0 0.7 n calc 0', ru2q_y)

# f_peaks: monthly d = 1 with a sharp annual spike -> the controller-side and
# ladder periodograms fill the k = 3..6 peak records (array length 6, k = 6
# sets the period-6 record -> hasSeasonalPeaks true).
peaks_y <- mk(12, 120, function(t) 10 * sin(2 * pi * t / 12) + rnorm(120, sd = 0.05))
case('f_peaks', 'monthly spectral peaks (k=6 record)',
  'BAT f_peaks 12 y 0 n 1 0 0 0 0 0 y 0.1 4 4 1e-10 1e-8 n n 0 0 0 0 0.7 n calc 0', peaks_y)

# f_qpeaks: quarterly n = 40 (differenced length 38 >= 36) with a pure annual
# cycle -> the period-4 test fires on both tests -> the ALL record and the
# quarterly hasSeasonalPeaks branch.
qpeaks_y <- mk(4, 40, function(t) 5 * sin(2 * pi * t / 4) + rnorm(40, sd = 0.02))
case('f_qpeaks', 'quarterly spectral peaks (ALL record)',
  'BAT f_qpeaks 4 y 0 n 0 0 0 0 0 0 y 0.1 4 4 1e-10 1e-8 n n 0 0 0 0 0.7 n calc 0', qpeaks_y)

# Force-load witnesses: every new case plus rw0 (replay).
witness <- list()
for (cid in c('f_ru1', 'f_mean', 'f_su2', 'f_ru2q', 'f_peaks', 'f_qpeaks', 'rw0')) {
  witness[[length(witness) + 1L]] <-
    do.call(loadwitness, c(list(id = cid), corpus_inputs[[cid]]))
}

# --- F-kernel golden (corpus = every (value, k1, k2) triple the live FTest
# reached in the cases above). Two separate JVM processes must produce
# byte-identical goldens (the rJava capture JVM cannot host this - it is the
# same process as the battery observer, and the golden contract is
# cross-process byte identity).
triples <- facts_df()
tf <- triples[triples$key == 'ftest' & !grepl('null$', triples$value) &
  !grepl('unavailable', triples$value), ]
trip <- vapply(seq_len(nrow(tf)), function(i) {
  # value = "<id>:<tag> v V p P k1 K1 k2 K2"
  f <- strsplit(tf$value[[i]], ' ')[[1L]]
  paste0(tf$id[[i]], ' ', f[[3]], ' ', f[[7]], ' ', f[[9]])
}, character(1))
writeLines(trip, file.path(output, 'ftest-triples.txt'))
javabin <- if (nzchar(javahome)) file.path(javahome, 'bin', 'java') else Sys.which('java')
for (i in c('1', '2')) {
  st <- system2(javabin, c('-XX:+UnlockDiagnosticVMOptions',
    '-XX:DisableIntrinsic=_dexp', '-XX:DisableIntrinsic=_dlog',
    '-cp', shQuote(paste(classpath, probe, sep = .Platform$path.sep)),
    'jdplus.toolkit.base.core.dstats.FcdfGolden',
    shQuote(file.path(output, paste0('fcdf-capture', i, '.txt'))),
    shQuote(file.path(output, 'ftest-triples.txt'))),
    stdout = TRUE, stderr = TRUE)
  rc <- attr(st, 'status')
  if (!is.null(rc) && rc != 0L)
    stop('F-kernel golden run ', i, ' failed: ', paste(utils::tail(st, 3L), collapse = ' | '))
}
c1 <- readLines(file.path(output, 'fcdf-capture1.txt'), warn = FALSE)
c2 <- readLines(file.path(output, 'fcdf-capture2.txt'), warn = FALSE)
fcdf_identical <- identical(c1, c2)
writeLines(c(paste('fcdf_capture1_equals_capture2', fcdf_identical),
  paste('fcdf_rows', length(c1)),
  paste('fcdf_corpus_rows', sum(startsWith(c1, 'corpus ')))),
  file.path(output, 'fcdf-bytes.txt'))
if (!fcdf_identical)
  stop('the two F-kernel golden captures differ - nondeterminism claim needs a re-check')
writeLines(c1, file.path(refdir, 'fcdf_golden.txt'))

# --- outputs ----------------------------------------------------------------
cases <- data.frame(id = vapply(summaries, function(s) s$id, character(1)),
  note = vapply(summaries, function(s) s$note, character(1)),
  diff_result = vapply(summaries, function(s) s$diff_result, character(1)),
  changed = vapply(summaries, function(s) s$changed, logical(1)),
  ftests = vapply(summaries, function(s) s$ftests, integer(1)),
  cands = vapply(summaries, function(s) s$cands, integer(1)),
  nevents = vapply(summaries, function(s) s$nevents, integer(1)),
  stringsAsFactors = FALSE)
write_df('cases.csv', cases)
write_df('inputs.csv', do.call(rbind, inputs))
write_df('diff-facts.csv', facts_df())
if (length(events))
  write_df('battery-events.csv', do.call(rbind, events)) else
  write_df('battery-events.csv', data.frame(id = character(), seq = integer(),
    kind = character(), depth = integer(), msg = character(), detail = character()))
write_df('loadwitness.csv', if (length(witness))
  do.call(rbind, witness) else data.frame())
write_df('summaries.csv', cases)

stopifnot(all(grepl('^[A-Za-z0-9_:.]+$', facts_df()$key)))
stopifnot(all(grepl('[^ ]', all_lines())))
write_lines('fact-grammar.txt', sort(unique(c(facts_df()$key))))

wit <- if (length(witness)) do.call(rbind, witness) else data.frame()
write_lines('metadata.R', c(
  '# Generated by capture-tramo-differencing-battery.R - item 23-C.2 pinned',
  '# battery-stage capture. The facts are raw getters of the live',
  '# SeasonalityController/battery/SeasonalOverDifferencingTest/ModelEstimator/',
  '# ModelStatistics/SpectralPeaks/dstats.F stack (OpenJDK 21.0.9, G2 exp/log',
  '# intrinsics disabled). Replay corpus = the 23-C BAT cases with io = n;',
  '# forcing cases f_ru1/f_mean/f_su2/f_ru2q/f_peaks/f_qpeaks. The',
  '# SeasonalOverDifferencingTest and SpectralPeaks load lines in the',
  '# loadlog-*.txt witnesses PROVE those classes execute (the 23-C corpus',
  '# never loaded them). Estimation, ML/JointTest algebra and the periodogram',
  '# stay pinned; the F p-values are the native incBeta kernel',
  '# (docs/contracts/tramo-differencing-decision-contract.md).',
  paste0('n_cases <- ', nrow(cases)),
  paste0('replayed <- ', replayed),
  paste0('changed_cases <- ', sum(cases$changed)),
  paste0('n_facts <- ', nrow(facts_df())),
  paste0('n_events <- ', if (length(events)) nrow(do.call(rbind, events)) else 0L),
  paste0('n_cands <- ', sum(cases$cands)),
  paste0('sodt_loads <- ', if (nrow(wit)) sum(wit[['jdplus.tramoseats.base.core.tramo.SeasonalOverDifferencingTest']]) else 0L),
  paste0('spectralpeaks_loads <- ', if (nrow(wit)) sum(wit[['jdplus.sa.base.core.tests.SpectralPeaks']]) else 0L),
  paste0('capture_jvm <- "', .jcall('java/lang/System', 'S', 'getProperty', 'java.version'), '"'),
  'jars <- c(',
  paste0('  "', vapply(jars, function(p) paste0(basename(p), '=',
    digest::digest(file = p, algo = 'sha256')), character(1)), '"',
    collapse = ',\n  '),
  ')'))

sums <- setdiff(list.files(output), 'SHA256SUMS.csv')
writeLines(c('file,sha256', vapply(sums, function(f)
    paste0(f, ',', digest::digest(file = file.path(output, f), algo = 'sha256')),
    character(1)),
  file.path(output, 'SHA256SUMS.csv'))
cat('captured', nrow(cases), 'cases,', nrow(facts_df()), 'facts ->', output, '\n')

if (!is.null(repro)) {
  # Byte-identity gate over the data files; metadata.R legitimately differs
  # in nothing here (no timestamp) but stays excluded like the 23-C script.
  data_files <- setdiff(sums, c('metadata.R', 'SHA256SUMS.csv'))
  prior <- setdiff(intersect(list.files(repro), data_files),
    c('metadata.R', 'SHA256SUMS.csv'))
  for (f in prior) {
    if (!identical(readLines(file.path(output, f), warn = FALSE),
      readLines(file.path(repro, f), warn = FALSE)))
      stop('repro mismatch in ', f)
  }
  message('repro check: ', length(prior), ' data files byte-identical')
}
