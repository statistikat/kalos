# Pinned-reference capture for item 23-C: the round-1 TRAMO differencing
# decision of the pinned jdplus 3.9.0 stack, observed by
# TramoDifferencingDecisionProbe.java - the LIVE internal DifferencingModule
# (DIFF: module facts through IDifferencingModule$Info and the reflectively
# read private field panel), the driver-segment replay (BAT: live module, the
# iterate() outlier cleanup, ModelEstimator(precision, curva, odm), the
# SeasonalityController passes and the RegularUnder(+2)/SeasonalUnder(+2)
# battery with refAuto references, plus residual-ladder/peak-test/over-side
# stage facts on the sweep cases), and the LIVE TramoKernel end-to-end runs
# (FULL). Raw getter values only (%.17g doubles, enum names, identity-hash
# toStrings never echoed - pinning notes 6.1/6.2). Series are built in R and
# handed over as %.17g tokens, so input bits are intrinsic-independent (21-H
# precedent); the G2 recipe below pins the JVM anyway. Pass --repro=<dir> for
# the byte-identity cross-check between independent JVM processes. The
# force-load witness runs are spawned as separate JVMs with
# -Xlog:class+load=info:file=... over the compiled probe main; the raw load
# logs and the per-class counts land in the evidence directory.
#
# Corpus (25 cases): DIFF on the clean seasonal base, the random walk, the
# seasonal walk, the deterministic-seasonal series, the quarterly shape, the
# no-seasonal module (maxbd=0), the fixed-airline re-entry (initial=n), the
# growth/log case and the short series (comespd gate). BAT replays the driver
# segment for the same shapes (sweep stage facts on the ladder cases), the
# ODM-on estimation seam and the seasonal-walk forced-load case where the
# SeasonalUnder(+2) battery changes the specification and the second
# SeasonalityController pass drives compareReferenceModels /
# SeasonalOverDifferencingTest. FULL runs trfull end to end on the base,
# seasonal-walk and deterministic-seasonal series.
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
  stop('Usage: capture-tramo-differencing.R [--repro=<captured directory>] --output=<existing empty directory>')
output <- normalizePath(sub('^--output=', '', args), mustWork = TRUE)
if (!dir.exists(output) || length(list.files(output, all.files = TRUE, no.. = TRUE)))
  stop('Output must be empty')
suppressWarnings(suppressMessages({
  library(rjd3jars); library(rjd3toolkit); library(rjd3tramoseats); library(rJava)
}))
.jinit()
# The rJava JVM fixes the loadable class-file version; compile the observer
# for exactly that release. rJava selects the JVM through JAVA_HOME (21-H.3
  # precedent: pin JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64).
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
probe <- tempfile('kalos-tramo-differencing-'); dir.create(probe)
jars <- c(list.files(system.file('java', package = 'rjd3jars'), pattern = '[.]jar$',
  full.names = TRUE),
  list.files(system.file('java', package = 'rjd3toolkit'), pattern = '[.]jar$',
    full.names = TRUE),
  list.files(system.file('java', package = 'rjd3tramoseats'), pattern = '[.]jar$',
    full.names = TRUE))
classpath <- paste(jars, collapse = .Platform$path.sep)
status <- system2(javac, c('--release', jvm_major, '-d', shQuote(probe),
  '-cp', shQuote(classpath),
  shQuote(file.path(tooling, 'TramoDifferencingDecisionProbe.java'))))
if (status != 0L) stop('probe compilation failed')
.jaddClassPath(c(jars[grep('tramoseats', jars)], probe))
PROBE_CLASS <- 'jdplus.tramoseats.base.core.tramo.TramoDifferencingDecisionProbe'
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

case <- function(id, note, hdr, y) {
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
  summaries[[length(summaries) + 1L]] <<- data.frame(
    id = id, note = note,
    mode = sub(' .*', '', hdr),
    diff_result = sub(paste0('^', id, ' '), '', fact('diff_result')),
    final_spec = fact('final'), full_spec = fact('full_spec'),
    steps = paste(sub(paste0('^', id, ' '), '', out[startsWith(out, paste0('step ', id, ' '))]),
      collapse = '; '),
    nevents = length(grep('^evt ', out)),
    stringsAsFactors = FALSE)
  invisible(out)
}

# Force-load witness: re-run one case in a fresh JVM with the class-load
# journal on, and count the load lines of the decision-stack classes.
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
  greps <- c('core.tramo.internal.DifferencingModule',
    'core.tramo.RegularUnderDifferencingTest',
    'core.tramo.SeasonalUnderDifferencingTest',
    'core.tramo.RegularUnderDifferencingTest2',
    'core.tramo.SeasonalUnderDifferencingTest2',
    'core.tramo.SeasonalOverDifferencingTest', 'core.tramo.SeasonalityController',
    'core.tramo.ModelEstimator', 'core.tramo.internal.OutliersDetectionModule')
  counts <- vapply(greps, function(pat) {
    suffix <- if (grepl('UnderDifferencingTest$', pat))
      sum(loaded == paste0('jdplus.tramoseats.base.', pat))
    else sum(startsWith(loaded, paste0('jdplus.tramoseats.base.', pat)))
  }, integer(1))
  names(counts) <- c('diffmodule', 'ru1', 'su1', 'ru2', 'su2', 'over', 'sctl',
    'estimator', 'odm')
  data.frame(case = id, as.list(counts), check.names = FALSE, row.names = NULL)
}

# -- corpus -------------------------------------------------------------------
n <- 96L
t <- seq_len(n)
# 21-H standard base (seeded additive-noise seasonal).
set.seed(21202)
b <- 50 + 0.05 * t + 5 * sin(2 * pi * t / 12) + 3 * cos(4 * pi * t / 12) +
  0.5 * sin(14 * pi * t / 12) + rnorm(n, 0, 0.3)
ao <- b; ao[49] <- ao[49] + 15
ls <- b; ls[61:96] <- ls[61:96] + 15
multi <- b; multi[31] <- multi[31] + 14; multi[71:96] <- multi[71:96] + 10
k <- seq_len(24); multi[49:72] <- multi[49:72] + 18 * 0.9^(k - 1)
# growth (log-flip) 21-C recipe.
set.seed(2302)
gs <- exp(cumsum(rnorm(n, .01, .02)) + .2 * sin(2 * pi * t / 12))
# random walk with drift (unit roots at both thresholds).
set.seed(2307)
rw <- 100 + cumsum(rnorm(n, 0.1, 1))
# seasonal walk: unit root plus strong deterministic seasonality (23-B
# short-series shapes).
set.seed(2311)
ts2 <- seq_len(120)
srw <- cumsum(rnorm(120, 0.02, 1)) + 8 * sin(2 * pi * ts2 / 12) +
  3 * cos(4 * pi * ts2 / 12) + rnorm(120, 0, 0.4)
# deterministic seasonal: stationary around a strong seasonal pattern.
set.seed(2313)
dsem <- 50 + 0.05 * t + 8 * sin(2 * pi * t / 12) + 4 * cos(4 * pi * t / 12) +
  rnorm(n, 0, 0.05)
# quarterly deterministic seasonal.
q4 <- 40 + 0.1 * seq_len(60) + 6 * sin(2 * pi * seq_len(60) / 4) +
  rnorm(60, 0, 0.1)
short50 <- b[1:50]

DS <- 'DIFF %s 12 %s n %s %s %s %s %s %s %s 0.05 0.97 0.91 1e-07 %s'
BS <- 'BAT %s %s %s %s %s %s %s %s %s %s %s %s %s 0.05 0.97 0.91 1e-07 1e-07 %s %s y y n y 0.7 %s %s %s'

dif <- function(id, note, y, freq = 12L, sea = 'y', log = 'n', spec = c(0, 0, 0, 0, 0, 0),
    mean = 'y', init = 'y') {
  hdr <- sprintf('DIFF %s %d %s %s %d %d %d %d %d %d %s 0.05 0.97 0.91 1e-07 %s',
    id, freq, sea, log, spec[1], spec[2], spec[3], spec[4], spec[5], spec[6],
    mean, init)
  case(id, note, hdr, y)
}
bt <- function(id, note, y, freq = 12L, sea = 'y', ost = '0', log = 'n',
    spec = c(0, 0, 0, 0, 0, 0), mean = 'y', init = 'y', io = 'n', ml = 'n',
    cv = 'calc', sweep = 'n') {
  hdr <- sprintf('BAT %s %d %s %s %s %d %d %d %d %d %d %s 0.05 0.97 0.91 1e-07 1e-07 %s %s y y n y 0.7 %s %s %s',
    id, freq, sea, ost, log, spec[1], spec[2], spec[3], spec[4], spec[5],
    spec[6], mean, init, io, ml, cv, sweep)
  case(id, note, hdr, y)
}

dif('d_air', 'clean seasonal base from flat spec: module ladder facts', b)
dif('d_rw', 'random walk: regular unit roots at both thresholds', rw)
dif('d_srw', 'seasonal walk: regular and seasonal root searches', srw)
dif('d_sem', 'deterministic seasonal: seasonal roots below the bound', dsem)
dif('d_quarter', 'quarterly deterministic seasonal (freq 4)', q4, freq = 4L)
dif('d_noseas', 'module without seasonal differencing (maxbd = 0)', b, sea = 'n')
dif('d_fixed', 'fixed airline re-entry (initial = n, estimated path)', b,
  spec = c(2, 1, 1, 1, 0, 1), init = 'n')
dif('d_growth_log', 'growth series with the log transformation', gs, log = 'y')
dif('d_short', 'short series: comespd gate and airline fallback', short50)
dif('d_qtr_noseas', 'quarterly module without seasonal differencing', q4,
  freq = 4L, sea = 'n')

bt('b0', 'clean base, flat spec: module + Unchanged battery chain', b, sweep = 'y')
bt('srw0', 'seasonal walk: ladder/peak stage facts, over-side code', srw,
  sweep = 'y', ost = '2')
bt('dsem0', 'deterministic seasonal: battery spec change forces the over-side',
  dsem, sweep = 'y')
bt('air_in', 'fixed airline entry, no differencing module iteration', b,
  spec = c(2, 1, 1, 1, 0, 1), init = 'n')
bt('rw0', 'random walk: d ladder with the RU2 under-differencing fire', rw,
  sweep = 'y')
bt('b_io', 'clean base with the ODM estimation seam live', b, io = 'y')
bt('q4_0', 'quarterly driver segment (D11 refusal evidence)', q4, freq = 4L,
  sweep = 'y')
bt('growth_log', 'growth series with log: module and battery facts', gs,
  log = 'y')
bt('short_b', 'short base: come-spd gate through the driver segment', short50,
  sweep = 'y')
bt('sem_noseas', 'deterministic seasonal with seasonal = false', dsem,
  sea = 'n', ost = '0')
bt('ao_io', 'AO series through the ODM-on estimation seam', ao, io = 'y')
bt('multi_io', 'AO+LS+TC series through the ODM-on seam', multi, io = 'y')

case('f_full_b', 'trfull end to end on the clean base',
  sprintf('FULL f_full_b trfull %d', 12L), b)
case('f_full_srw', 'trfull end to end on the seasonal walk',
  sprintf('FULL f_full_srw trfull %d', 12L), srw)
case('f_full_dsem', 'trfull end to end on the deterministic seasonal',
  sprintf('FULL f_full_dsem trfull %d', 12L), dsem)

# Force-load witness runs. The live SeasonalityController keeps its reference
# model as the live model object itself (computeReferenceModel bci 0/182), so
# compareReferenceModels' spec-difference guard never fires inside the kernel
# - the FULL logs show no SeasonalOverDifferencingTest load line, confirming
# the roadmap fact that the class is NOT loaded on the probe path. The corpus
# forces the class load through the observer's direct instantiation of the
# package-private test (the sweep stage-facts), so witness runs use sweep = y
# and the load line is attributed to that corpus case; the over-code fact
# pins what the live test returns on the post-estimation model.
witness <- rbind(
  loadwitness('b0',
    'BAT b0 12 y 0 n 0 0 0 0 0 0 y 0.05 0.97 0.91 1e-07 1e-07 y n y y n y 0.7 n calc y',
    b),
  loadwitness('srw0',
    'BAT srw0 12 y 2 n 0 0 0 0 0 0 y 0.05 0.97 0.91 1e-07 1e-07 y n y y n y 0.7 n calc y',
    srw),
  loadwitness('rw0',
    'BAT rw0 12 y 0 n 0 0 0 0 0 0 y 0.05 0.97 0.91 1e-07 1e-07 y n y y n y 0.7 n calc y',
    rw),
  loadwitness('f_full_dsem', 'FULL f_full_dsem trfull 12', dsem))

wcsv <- function(name, df) {
  con <- file(file.path(output, name), 'w')
  write.csv(df, con, row.names = FALSE, na = '')
  close(con)
}
wcsv('cases.csv', do.call(rbind, summaries))
wcsv('inputs.csv', do.call(rbind, inputs))
wcsv('diff-facts.csv', do.call(rbind, facts))
wcsv('diff-events.csv', do.call(rbind, events))
wcsv('load-witness.csv', witness)

writeLines(c(
  paste0('# metadata for the item 23-C tramo-differencing capture ',
    '(generated by validation/reference/capture-tramo-differencing.R)'),
  paste0('r_version <- "', paste(R.version$major, R.version$minor, sep = '.'), '"'),
  paste0('jvm_version <- "', .jcall('java/lang/System', 'S', 'getProperty', 'java.version'), '"'),
  paste0('jvm_vendor <- "', .jcall('java/lang/System', 'S', 'getProperty', 'java.vendor'), '"'),
  paste0('java_options <- "', Sys.getenv('_JAVA_OPTIONS'), '"'),
  paste0('capture_time <- "', format(Sys.time(), tz = 'UTC'), '"'),
  paste0('capture_host <- "', Sys.info()[['nodename']], '"'),
  paste0('capture_user <- "', Sys.info()[['user']], '"'),
  paste0('probe_sha256 <- "', reference_sha256(file.path(root,
    'validation/reference/TramoDifferencingDecisionProbe.java')), '"'),
  paste0('script_sha256 <- "', reference_sha256(file.path(root,
    'validation/reference/capture-tramo-differencing.R')), '"'),
  'jar_sha256 <- c(',
  paste0('  "', vapply(jars, function(p) paste0(basename(p), '=',
    reference_sha256(p)), character(1)), '"',
    collapse = ',\n  '),
  ')'
), file.path(output, 'metadata.R'))

files <- setdiff(list.files(output), 'SHA256SUMS.csv')
if (nzchar(repro)) {
  # Byte-identity gate over the data files; metadata.R legitimately differs
  # in its capture_time field only (final-information precedent).
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
