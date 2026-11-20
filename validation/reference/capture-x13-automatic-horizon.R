# Pinned-reference capture for item 21-H.4: the same decoder-driven automatic
# X-13 pipeline as capture-x13-automatic.R (item 21-H.2), observed through
# X13AutomaticHorizonProbe.java with NON-ZERO X-11 spec horizons, so that the
# initialStep forecast route (the ExactArimaForecasts stack over the
# preadjusted series, the extended out-calendar and the split finals) lands
# in the stored tables. The observer builds the identical decoder-shaped
# automatic spec; only X11Spec.backcastHorizon/forecastHorizon differ per
# case. The four cases replay the zero-horizon oracle series (quick, ao,
# multi, so - same seeded generators as capture-x13-automatic.R) with
# horizons {0,6}, {3,0}, {3,6} and {0,6} months; the horizon values and the
# JVM SarimaModel surface feeding the route are emitted as facts.
#
# JVM ground truth (21-D.2 precedent): capture runs with
#   _JAVA_OPTIONS="-XX:+UnlockDiagnosticVMOptions -XX:DisableIntrinsic=_dexp -XX:DisableIntrinsic=_dlog"
# so Math.exp/log equal StrictMath.exp/log == FdLibm kernels. The guard
# below proves the flags took effect. Pass --repro=<dir> for the byte-identity
# gate against an earlier capture of the same cases from an independent JVM
# process (all four captures passed: 2026-11-20).
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
  stop('Usage: capture-x13-automatic-horizon.R [--repro=<captured directory>] --output=<existing empty directory>')
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
probe <- tempfile('kalos-x13-automatic-horizon-'); dir.create(probe)
jars <- c(list.files(system.file('java', package = 'rjd3jars'), pattern = '[.]jar$',
  full.names = TRUE),
  list.files(system.file('java', package = 'rjd3x13'), pattern = '[.]jar$',
    full.names = TRUE))
classpath <- paste(jars, collapse = .Platform$path.sep)
status <- system2(javac, c('--release', jvm_major, '-d', shQuote(probe),
  '-cp', shQuote(classpath),
  shQuote(file.path(tooling, 'X13AutomaticHorizonProbe.java'))))
if (status != 0L) stop('probe compilation failed')
.jaddClassPath(c(jars[grep('rjd3x13', jars)], probe))
if (inherits(try(.jfindClass('X13AutomaticHorizonProbe'), silent = TRUE), 'try-error'))
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

facts <- list(); inputs <- list(); summaries <- list()

# lines: the exact observer input block (id, start, values, flags, bc/fc).
case <- function(id, note, y, bc, fc, ...) {
  lines <- c(paste('id', id), 'start 1990:1', paste('values', j(vapply(y, g, character(1)))),
    ..., paste('bc', bc), paste('fc', fc))
  for (ln in lines)
    inputs[[length(inputs) + 1L]] <<- data.frame(id = id, line = ln,
      stringsAsFactors = FALSE)
  res <- .jcall('X13AutomaticHorizonProbe', 'S', 'run', .jarray(lines))
  out <- strsplit(res, '\n', fixed = TRUE)[[1L]]
  out <- out[nzchar(out)]
  if (!identical(out[[1L]], paste('case', id)))
    stop('observer echoed a different case id for ', id)
  if (length(out) < 2L || !startsWith(out[[2L]], 'req '))
    stop('missing req echo for ', id)
  status <- out[startsWith(out, 'status ')]
  for (ln in out) {
    k <- regmatches(ln, regexec('^([^ ]+) (.*)$', ln))[[1L]]
    if (length(k) != 3L) stop('unparsable fact line: ', ln)
    facts[[length(facts) + 1L]] <<- data.frame(id = id, key = k[[2L]],
      value = k[[3L]], stringsAsFactors = FALSE)
  }
  fact <- function(key) {
    v <- out[startsWith(out, paste0(key, ' '))]
    if (!length(v)) '' else sub(paste0('^', key, ' '), '', v[[1L]])
  }
  orders <- strsplit(fact('orders'), ' ', fixed = TRUE)[[1L]]
  summaries[[length(summaries) + 1L]] <<- data.frame(
    id = id, note = note, n = length(y), bc = bc, fc = fc,
    status = sub('^status ', '', status[[1L]]),
    log = fact('log'), mean = fact('mean'),
    p = orders[1L], d = orders[2L], q = orders[3L],
    bp = orders[4L], bd = orders[5L], bq = orders[6L],
    stringsAsFactors = FALSE)
  if (!identical(sub('^status ', '', status[[1L]]), 'ok'))
    stop('case ', id, ' did not produce a valid result')
  invisible(NULL)
}

# -- corpus: the zero-horizon oracle series (capture-x13-automatic.R
# generators, byte-identical) under non-zero X-11 horizons -------------------
n <- 96L
base <- function() {
  set.seed(21202)
  t <- seq_len(n)
  50 + 0.05 * t + 5 * sin(2 * pi * t / 12) + 3 * cos(4 * pi * t / 12) +
    0.5 * sin(14 * pi * t / 12) + rnorm(n, 0, 0.3)
}
b <- base()
shock <- function(mag, at) { v <- b; v[at] <- v[at] + mag; v }

case('quick_h6', 'plain seasonal baseline with a 6-month forecast', b, 0, 6,
  'outliers off')
case('ao_b3', 'AO +15 at 1994-01 with a 3-month backcast (mean kept)',
  shock(15, 49), 3, 0, 'outliers on')
v <- b
v[31] <- v[31] + 14
v[71:96] <- v[71:96] + 10
k <- seq_len(24); v[49:72] <- v[49:72] + 18 * 0.9^(k - 1)
case('multi_b3h6', 'AO + TC ramp + late level shift, 3-month backcast and 6-month forecast',
  v, 3, 6, 'outliers on')
case('so_h6', 'AO-shaped shock with the SO-only type set, 6-month forecast',
  shock(15, 49), 0, 6, 'outliers on', 'otypes SO')

# -- corpus files ---------------------------------------------------------------
wcsv <- function(name, df) {
  con <- file(file.path(output, name), open = 'wt')
  write.table(df, con, sep = ',', qmethod = 'double', row.names = FALSE,
    col.names = TRUE, na = '')
  close(con)
}
wcsv('cases.csv', do.call(rbind, summaries))
wcsv('inputs.csv', do.call(rbind, inputs))
wcsv('oracle-facts.csv', do.call(rbind, facts))

writeLines(c(
  paste0('# metadata for the item 21-H.4 automatic-pipeline horizon capture ',
    '(generated by validation/reference/capture-x13-automatic-horizon.R)'),
  paste0('r_version <- "', paste(R.version$major, R.version$minor, sep = '.'), '"'),
  paste0('jvm_version <- "', .jcall('java/lang/System', 'S', 'getProperty', 'java.version'), '"'),
  paste0('jvm_vendor <- "', .jcall('java/lang/System', 'S', 'getProperty', 'java.vendor'), '"'),
  paste0('java_options <- "', Sys.getenv('_JAVA_OPTIONS'), '"'),
  paste0('capture_time <- "', format(Sys.time(), tz = 'UTC'), '"'),
  paste0('capture_host <- "', Sys.info()[['nodename']], '"'),
  paste0('capture_user <- "', Sys.info()[['user']], '"'),
  paste0('probe_sha256 <- "', reference_sha256(file.path(root,
    'validation/reference/X13AutomaticHorizonProbe.java')), '"'),
  paste0('script_sha256 <- "', reference_sha256(file.path(root,
    'validation/reference/capture-x13-automatic-horizon.R')), '"'),
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
