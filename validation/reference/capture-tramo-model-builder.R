# Pinned-reference capture for item 23-A: the TRAMO model builder of the
# pinned jdplus 3.9.0 stack (rjd3tramoseats), observed through the public-API
# observer TramoModelBuilderProbe.java. The observer drives the LIVE builder
# exactly as TramoKernel does - new TramoModelBuilder(spec, ctx).build(ts,
# log) (package-private class, public constructor; reached reflectively, no
# instrumentation) - and reports the built ModelDescription through getters
# only: the start rule (airline iff autoModel and the series is NOT annual,
# else spec.getArima().withPeriod(freq)), the log/LoP flags, the mean flag,
# the interpolation plan (AverageInterpolator), the sorted registered
# variable list with the regeffect attribute vocabulary, and the builder's
# own ProcessingLog trace. Raw getter values throughout (%.17g doubles, enum
# constant names, identity-hash toStrings never echoed - pinning notes 6.1/
# 6.2). Every case echoes its "req" block read back from the constructed
# TramoSpec; the series is built inside the probe with StrictMath only, so
# the input bits are intrinsic-independent, and the G2 recipe below pins the
# JVM anyway. Pass --repro=<dir> to cross-check a capture against an earlier
# one from an independent JVM process (byte-identity gate, final-information
# precedent).
#
# Corpus (22 cases): the seven templates TR0..TRfull on the fixed monthly
# n=96 series from 1990:1; a non-autoModel fixed-order (1,1,0)(1,1,1) with
# fixed coefficients; forced log; mean none (autoModel off AND on) and fixed
# mean coefficient; leading/interior/trailing missing patterns; manual
# AO/LS/TC/SO registration with unknown codes and a preset regeffect
# attribute; an unresolvable user variable (warning event); ramp +
# intervention; a fixed calendar (TD2 + LeapYear + Easter(Standard, no
# test), LoP adjust LeapYear); a non-January monthly start; ANNUAL and
# QUARTERLY shapes (record cases: the kalos unit refuses non-monthly per
# register D11). Calendar variables never register through the seven
# templates - the test/automatic gates skip them (decode finding, contract).
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
  stop('Usage: capture-tramo-model-builder.R [--repro=<captured directory>] --output=<existing empty directory>')
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
probe <- tempfile('kalos-tramo-model-builder-'); dir.create(probe)
jars <- c(list.files(system.file('java', package = 'rjd3jars'), pattern = '[.]jar$',
  full.names = TRUE),
  list.files(system.file('java', package = 'rjd3toolkit'), pattern = '[.]jar$',
    full.names = TRUE),
  list.files(system.file('java', package = 'rjd3tramoseats'), pattern = '[.]jar$',
    full.names = TRUE))
classpath <- paste(jars, collapse = .Platform$path.sep)
status <- system2(javac, c('--release', jvm_major, '-d', shQuote(probe),
  '-cp', shQuote(classpath),
  shQuote(file.path(tooling, 'TramoModelBuilderProbe.java'))))
if (status != 0L) stop('probe compilation failed')
.jaddClassPath(c(jars[grep('tramoseats', jars)], probe))
if (inherits(try(.jfindClass('TramoModelBuilderProbe'), silent = TRUE), 'try-error'))
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

facts <- list(); events <- list(); inputs <- list(); summaries <- list()

case <- function(id, note, ...) {
  lines <- c(paste('id', id), ...)
  res <- .jcall('TramoModelBuilderProbe', 'S', 'run', .jarray(lines))
  out <- strsplit(res, '\n', fixed = TRUE)[[1L]]
  out <- out[nzchar(out)]
  if (!identical(out[[1L]], paste('case', id)))
    stop('observer echoed a different case id for ', id)
  for (ln in out) {
    if (startsWith(ln, 'evt ')) {
      m <- regmatches(ln, regexec(
        '^evt ([0-9]+) ([a-z]+) ([0-9]+) (.*) det (.*)$', ln))[[1L]]
      if (length(m) != 6L) stop('unparsable event line: ', ln)
      events[[length(events) + 1L]] <<- data.frame(id = id, seq = m[[2L]],
        kind = m[[3L]], depth = m[[4L]], msg = m[[5L]], detail = m[[6L]],
        stringsAsFactors = FALSE)
    } else if (startsWith(ln, 'src ') || startsWith(ln, 'req ') || startsWith(ln, 'values ')) {
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
  status <- sub('^status ', '', out[startsWith(out, 'status ')][[1L]])
  n <- as.integer(gsub('^n ([0-9]+)$', '\\1',
    lines[startsWith(lines, 'n ')][1L]))
  if (is.na(n)) n <- 96L
  freq <- as.integer(gsub('^freq ([0-9]+)$', '\\1',
    lines[startsWith(lines, 'freq ')][1L]))
  if (length(freq) == 0L || is.na(freq[1L])) freq <- 12L
  startline <- lines[startsWith(lines, 'start ')]
  start <- if (length(startline)) sub('^start ', '', startline[[1L]]) else '1990 1'
  arima <- fact('arima')
  orders <- regmatches(arima, regexec(
    '^p([0-9]+)d([0-9]+)q([0-9]+)P([0-9]+)D([0-9]+)Q([0-9]+) per([0-9]+)',
    arima))[[1L]]
  summaries[[length(summaries) + 1L]] <<- data.frame(
    id = id, note = note, freq = freq, start = start, n = n,
    status = status,
    auto = sub('^req auto ', '', out[startsWith(out, 'req auto ')][[1L]]),
    log = sub('^log (y|n) .*', '\\1', out[startsWith(out, 'log ')][[1L]]),
    mean = sub('.* mean (y|n)$', '\\1', out[startsWith(out, 'log ')][[1L]]),
    p = orders[2L], d = orders[3L], q = orders[4L],
    bp = orders[5L], bd = orders[6L], bq = orders[7L], period = orders[8L],
    nvar = sum(vapply(out, function(l) startsWith(l, 'var '), logical(1))),
    nevents = length(grep('^evt ', out)),
    stringsAsFactors = FALSE)
  if (!identical(status, 'ok')) stop('case ', id, ' did not build cleanly')
  invisible(NULL)
}

# -- corpus -------------------------------------------------------------------
case('tr0', 'template TR0: fixed airline start (autoModel off), levels, mean free',
  'template TR0')
case('tr1', 'template TR1: outliers on, autoModel off, undefined airline withPeriod(12)',
  'template TR1')
case('tr2', 'template TR2: calendar TD2+LeapYear+Easter(test) - all builder-gated, no variables',
  'template TR2')
case('tr3', 'template TR3 (rsa3 default): autoModel start airline + mean true',
  'template TR3')
case('tr4', 'template TR4: TR3 + TD2 test-calendar - gated, same builder facts as TR3',
  'template TR4')
case('tr5', 'template TR5: TR3 + TD7 test-calendar - gated, same builder facts as TR3',
  'template TR5')
case('trfull', 'template TRfull: autoMethod FTEST + IncludeEaster(test) - all gated (TR3-identical)',
  'template TRfull')
case('fixed_order', 'autoModel off, defined (1,1,0)(1,1,1) with fixed coefficients: setArimaSpec(withPeriod) branch',
  'template TR0', 'automodel off', 'arima 1 1 0 1 1 1 F:-0.5 F:0.3 F:0.2 F:0.4')
case('log_forced', 'forced Log transform on TR1: setLogTransformation(true)',
  'template TR1', 'function Log')
case('mean_off', 'MeanSpec.none() with autoModel OFF: final mean stays false',
  'template TR1', 'mean none')
case('mean_auto_off', 'MeanSpec.none() with autoModel ON: initializeMean(false) then initializeArima overwrites mean true',
  'template TR3', 'mean none')
case('mean_fixed', 'fixed mean coefficient: const TrendConstant(d,bd,est-start) + Fixed coef, mean false',
  'template TR1', 'mean fixed:55.0')
case('missing_lead', 'leading NaN run: forward-filled with the first finite value, missing list pinned',
  'template TR1', 'nan 0 3')
case('missing_interior', 'interior NaN runs: (left+right)/2 fills, missing positions pinned',
  'template TR1', 'nan 30 2', 'nan 60 1')
case('missing_trail', 'trailing NaN run: backward-filled with the last finite value',
  'template TR1', 'nan 94 2')
case('outliers_manual', 'manual AO/LS/TC/SO (+XX, +lowercase ao skipped, +preset regeffect=Trend kept): factories, regeffect vocabulary, position sort',
  'template TR1', 'outlier AO 1994:1', 'outlier LS 1995:1', 'outlier TC 1996:1',
  'outlier SO 1993:7', 'outlier XX 1990:3', 'outlier ao 1990:4',
  'outlier AO 1992:1 regeffect=Trend')
case('user_absent', 'user variable without context: "<name>: not found (removed from the model)" warning, no registration',
  'template TR1', 'user u1')
case('ramp_interv', 'ramp + intervention variables register as-is (no regeffect attribute), user-category sort',
  'template TR1', 'ramp 1992:1 1994:6', 'intervention 1993:1 1993:3')
case('calendar_fixed', 'fixed TD2 + LeapYear LoP + Easter(Standard, no test) + transform adjust=LeapYear: td/lp/easter register with regeffect=CalendarEffect',
  'template TR1', 'td TD2 LeapYear None', 'easter Standard 0 4 0', 'adjust LeapYear')
case('feb_start', 'non-January monthly start (1990:2): domain/estimation-domain shift, est-start reference of the fixed-mean rule unaffected',
  'template TR3', 'start 1990 2')
case('annual', 'ANNUAL frequency on TR3 (record): setAirline(false) -> m011(1), mean true; unit refuses per D11',
  'template TR3', 'freq 1', 'n 24')
case('quarterly', 'QUARTERLY frequency on TR3 (record): airline withPeriod(4), Feb start; unit refuses per D11',
  'template TR3', 'freq 4', 'start 1990 2', 'n 40')

# -- corpus files --------------------------------------------------------------
wcsv <- function(name, df) {
  con <- file(file.path(output, name), open = 'wt')
  write.table(df, con, sep = ',', qmethod = 'double', row.names = FALSE,
    col.names = TRUE, na = '')
  close(con)
}
wcsv('cases.csv', do.call(rbind, summaries))
wcsv('inputs.csv', do.call(rbind, inputs))
wcsv('builder-facts.csv', do.call(rbind, facts))
wcsv('builder-events.csv', do.call(rbind, events))

writeLines(c(
  paste0('# metadata for the item 23-A tramo-model-builder capture ',
    '(generated by validation/reference/capture-tramo-model-builder.R)'),
  paste0('r_version <- "', paste(R.version$major, R.version$minor, sep = '.'), '"'),
  paste0('jvm_version <- "', .jcall('java/lang/System', 'S', 'getProperty', 'java.version'), '"'),
  paste0('jvm_vendor <- "', .jcall('java/lang/System', 'S', 'getProperty', 'java.vendor'), '"'),
  paste0('java_options <- "', Sys.getenv('_JAVA_OPTIONS'), '"'),
  paste0('capture_time <- "', format(Sys.time(), tz = 'UTC'), '"'),
  paste0('capture_host <- "', Sys.info()[['nodename']], '"'),
  paste0('capture_user <- "', Sys.info()[['user']], '"'),
  paste0('probe_sha256 <- "', reference_sha256(file.path(root,
    'validation/reference/TramoModelBuilderProbe.java')), '"'),
  paste0('script_sha256 <- "', reference_sha256(file.path(root,
    'validation/reference/capture-tramo-model-builder.R')), '"'),
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
