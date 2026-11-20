# Pinned-reference capture for item 21-H.2: the decoder-driven automatic
# X-13 pipeline of the pinned jdplus 3.9.0 jars (rjd3 + rjd3x13), observed
# through the public-API observer X13AutomaticProbe.java. The observer runs
# the REAL pipeline - X13Kernel.of(spec, ctx).process(ts, log) is the exact
# entry point rjd3x13::x13 drives, and the spec is the decoder-shaped
# automatic spec: RegArimaSpec.usingAutoModel(true) makes X13SpecDecoder
# wire the x13.regarima AutoModellingModule(DifferencingModule, ArmaModule)
# and OutliersDetectionModule with the decoder call-site scalars (cancel,
# the JVM reciprocals 1/ub1, 1/ub2 and precision 1e-7 for differencing;
# balanced/mixed/1e-7 for arma; maxRound=maxIter, tcrate=0.7 for outliers;
# the ArmaModule keeps its builder defaults maxP/maxQ=2, maxBp/maxBq=1,
# modelsCount=5, wn=false). Every spec scalar is a pinned default of the
# decoder path (AutoModelSpec DEF_CANCEL .1, DEF_UB1 1.0309278350515465,
# DEF_UB2 1.1363636363636365, DEF_MIXED true, DEF_BALANCED false; OutlierSpec
# DEF_TCRATE .7, DEF_NMAX 30; TransformSpec DEF_AICDIFF -2) and the probe
# echoes the held values per case ("req" line). The ProcessingLog observer
# implements the interface only - no decision rule is re-implemented here;
# the event stream IS the JVM's own decision trace, and the stored tables
# are the kernel's own outputs (%.17g). Pass --repro=<dir> to cross-check
# the written corpus against an earlier capture of the same cases from an
# independent JVM process (byte-identity gate).
#
# Corpus (13 monthly cases, n=96, start 1990:1, horizons {0,0}): plain
# seasonal baseline, AO/LS/TC/SO detections (SO through a SO-only type set),
# a multi-outlier round, an all-reject round
# series, a detection-off twin, d=1 vs d=2 vs random-walk vs seasonal-only
# shapes, mean kept and rejected, and the Multiplicative/LogAdditive/
# PseudoAdditive X-11 forcings on a log case (Additive through the
# Undefined-mode cases). Series are complete and finite.
#
# JVM ground truth (21-D.2 precedent): the estimation machine reaches
# Math.exp / Math.log, which the pinned build intrinsifies to build- and
# CPU-specific stubs at hot call sites (1 ulp off the bytecode's own FdLibm
# semantics). The contract fixes the bytecode semantics: capture runs with
#   _JAVA_OPTIONS="-XX:+UnlockDiagnosticVMOptions -XX:DisableIntrinsic=_dexp -XX:DisableIntrinsic=_dlog"
# so Math.exp/log equal StrictMath.exp/log == FdLibm kernels, the transcribed
# Kalos machines. The guard below proves the flags took effect.
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
  stop('Usage: capture-x13-automatic.R [--repro=<captured directory>] --output=<existing empty directory>')
output <- normalizePath(sub('^--output=', '', args), mustWork = TRUE)
if (!dir.exists(output) || length(list.files(output, all.files = TRUE, no.. = TRUE)))
  stop('Output must be empty')
suppressWarnings(suppressMessages({
  library(rjd3jars); library(rjd3toolkit); library(rJava)
}))
.jinit()
# The rJava JVM fixes the loadable class-file version; compile the observer
# for exactly that release (javac on PATH may be newer). rJava selects the
# JVM through JAVA_HOME, so the capture pins it there; without it rJava may
# pick up a system default that is not the pinned build.
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
probe <- tempfile('kalos-x13-automatic-'); dir.create(probe)
jars <- c(list.files(system.file('java', package = 'rjd3jars'), pattern = '[.]jar$',
  full.names = TRUE),
  list.files(system.file('java', package = 'rjd3x13'), pattern = '[.]jar$',
    full.names = TRUE))
classpath <- paste(jars, collapse = .Platform$path.sep)
status <- system2(javac, c('--release', jvm_major, '-d', shQuote(probe),
  '-cp', shQuote(classpath),
  shQuote(file.path(tooling, 'X13AutomaticProbe.java'))))
if (status != 0L) stop('probe compilation failed')
.jaddClassPath(c(jars[grep('rjd3x13', jars)], probe))
if (inherits(try(.jfindClass('X13AutomaticProbe'), silent = TRUE), 'try-error'))
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
j <- function(v) paste(v, collapse = ';')

events <- list(); facts <- list(); inputs <- list(); summaries <- list()

# lines: the exact observer input block (id, start, values, then flags).
case <- function(id, note, y, ...) {
  lines <- c(paste('id', id), 'start 1990:1', paste('values', j(vapply(y, g, character(1)))),
    ...)
  for (ln in lines)
    inputs[[length(inputs) + 1L]] <<- data.frame(id = id, line = ln,
      stringsAsFactors = FALSE)
  res <- .jcall('X13AutomaticProbe', 'S', 'run', .jarray(lines))
  out <- strsplit(res, '\n', fixed = TRUE)[[1L]]
  out <- out[nzchar(out)]
  if (!identical(out[[1L]], paste('case', id)))
    stop('observer echoed a different case id for ', id)
  if (length(out) < 2L || !startsWith(out[[2L]], 'req '))
    stop('missing req echo for ', id)
  status <- out[startsWith(out, 'status ')]
  for (ln in out) {
    if (startsWith(ln, 'evt ')) {
      m <- regmatches(ln, regexec(
        '^evt ([0-9]+) ([a-z]+) ([0-9]+) (.*) det (.*)$', ln))[[1L]]
      if (length(m) != 6L) stop('unparsable event line: ', ln)
      events[[length(events) + 1L]] <<- data.frame(id = id, seq = m[[2L]],
        kind = m[[3L]], depth = m[[4L]], msg = m[[5L]], detail = m[[6L]],
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
  orders <- strsplit(fact('orders'), ' ', fixed = TRUE)[[1L]]
  nvars <- sum(vapply(out, function(l) startsWith(l, 'var '), logical(1)))
  mode_requested <- sub('.* mode ([A-Za-z]+).*', '\\1',
    out[startsWith(out, 'req ')][[1L]])
  summaries[[length(summaries) + 1L]] <<- data.frame(
    id = id, note = note, n = length(y),
    status = sub('^status ', '', status[[1L]]),
    log = fact('log'), mean = fact('mean'),
    p = orders[1L], d = orders[2L], q = orders[3L],
    bp = orders[4L], bd = orders[5L], bq = orders[6L],
    mode_requested = mode_requested,
    mode_effective = fact('mode_effective'),
    nvars = nvars, nevents = length(grep('^evt ', out)),
    stringsAsFactors = FALSE)
  if (!identical(sub('^status ', '', status[[1L]]), 'ok'))
    stop('case ', id, ' did not produce a valid result')
  invisible(NULL)
}

# -- corpus (deterministic under the seeds below) -------------------------------
n <- 96L
base <- function() {
  set.seed(21202)
  t <- seq_len(n)
  50 + 0.05 * t + 5 * sin(2 * pi * t / 12) + 3 * cos(4 * pi * t / 12) +
    0.5 * sin(14 * pi * t / 12) + rnorm(n, 0, 0.3)
}
b <- base()
shock <- function(mag, at) { v <- b; v[at] <- v[at] + mag; v }

case('quick', 'plain seasonal baseline, no outlier detection', b,
  'outliers off')
# A 'plain' variant (mild AO at 20 plus a decaying TC from 70) was dropped
# from the corpus at qualification: its round-0 ODM sits on a
# TC@69-versus-AO@63 selection knife edge (ramp/shift equivalence), and the
# JVM-versus-native ordering of that cell flips with the row-1/row-3
# landing drift the contract's G2 seam keeps at basin tiers. The mixed
# multi-round breadth stays covered by 'multi'; the clean data with
# detection by 'noout'. Removal recorded in the pipeline contract.
case('ao', 'AO +15 at 1994-01: single irregular outlier, log chosen',
  shock(15, 49), 'outliers on')
v <- b; v[61:96] <- v[61:96] + 15
case('ls', 'level shift +15 from 1995-01: regeffect Trend', v, 'outliers on')
v <- b
v[31] <- v[31] + 14
v[71:96] <- v[71:96] + 10
k <- seq_len(24); v[49:72] <- v[49:72] + 18 * 0.9^(k - 1)
case('multi', 'AO + TC ramp + late level shift: four-variable round', v,
  'outliers on')
case('noout', 'clean baseline with detection on: every round rejects',
  b, 'outliers on')
case('so', 'AO-shaped shock with the SO-only type set: regeffect Seasonal',
  shock(15, 49), 'outliers on', 'otypes SO')
case('ao_nodet', 'AO +15 at 1994-01 with detection off: model-only path',
  shock(15, 49), 'outliers off')
set.seed(777)
v <- 100 + cumsum(cumsum(rnorm(n, 0, .3))) + 1.5 * sin(2 * pi * seq_len(n) / 12)
case('d2', 'integrated-of-integrated level: d=2 selection boundary', abs(v),
  'outliers off')
# The rw series continues from the post-d2 RNG state (one 96-draw rnorm
# was consumed by the d2 line above in the pinned generator).
set.seed(777); invisible(rnorm(n))
v <- 100 + cumsum(rnorm(n, .2, 1))
case('rw', 'random walk with drift, log chosen, mean kept (tmean 2.685)', v,
  'outliers off')
set.seed(555)
e <- rnorm(n, 0, 2); for (i in 2:n) e[i] <- 0.5 * e[i - 1] + rnorm(1, 0, 1.5)
v <- 100 + e + 25 * sin(2 * pi * seq_len(n) / 12) + 10 * cos(4 * pi * seq_len(n) / 12)
case('seas', 'strong seasonality, no trend: seasonal-only (0,0)(0,1)', v,
  'outliers off')
case('mode_mult', 'AO case with the Multiplicative forcing', shock(15, 49),
  'outliers off', 'mode Multiplicative')
case('mode_logadd', 'AO case with the LogAdditive forcing', shock(15, 49),
  'outliers off', 'mode LogAdditive')
case('mode_pseudo', 'AO case with the PseudoAdditive forcing', shock(15, 49),
  'outliers off', 'mode PseudoAdditive')

# -- corpus files ---------------------------------------------------------------
wcsv <- function(name, df) {
  con <- file(file.path(output, name), open = 'wt')
  write.table(df, con, sep = ',', qmethod = 'double', row.names = FALSE,
    col.names = TRUE, na = '')
  close(con)
}
wcsv('cases.csv', do.call(rbind, summaries))
wcsv('inputs.csv', do.call(rbind, inputs))
wcsv('oracle-events.csv', do.call(rbind, events))
wcsv('oracle-facts.csv', do.call(rbind, facts))

writeLines(c(
  paste0('# metadata for the item 21-H.2 automatic-pipeline capture ',
    '(generated by validation/reference/capture-x13-automatic.R)'),
  paste0('r_version <- "', paste(R.version$major, R.version$minor, sep = '.'), '"'),
  paste0('jvm_version <- "', .jcall('java/lang/System', 'S', 'getProperty', 'java.version'), '"'),
  paste0('jvm_vendor <- "', .jcall('java/lang/System', 'S', 'getProperty', 'java.vendor'), '"'),
  paste0('java_options <- "', Sys.getenv('_JAVA_OPTIONS'), '"'),
  paste0('capture_time <- "', format(Sys.time(), tz = 'UTC'), '"'),
  paste0('capture_host <- "', Sys.info()[['nodename']], '"'),
  paste0('capture_user <- "', Sys.info()[['user']], '"'),
  paste0('probe_sha256 <- "', reference_sha256(file.path(root,
    'validation/reference/X13AutomaticProbe.java')), '"'),
  paste0('script_sha256 <- "', reference_sha256(file.path(root,
    'validation/reference/capture-x13-automatic.R')), '"'),
  'jar_sha256 <- c(',
  paste0('  "', vapply(jars, function(p) paste0(basename(p), '=',
    reference_sha256(p)), character(1)), '"',
    collapse = ',\n  '),
  ')'
), file.path(output, 'metadata.R'))

files <- setdiff(list.files(output), 'SHA256SUMS.csv')
if (nzchar(repro)) {
  # Byte-identity gate over the data files; metadata.R legitimately differs
  # in its captured_at field only (final-information precedent).
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
