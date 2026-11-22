# Pinned-reference capture for item 23-D: the arma selection of the live
# tramo.internal.ArmaModule in the pinned jdplus 3.9.0 stack, observed by
# TramoArmaSelectionProbe.java. Every case echoes the package-private static
# gates the module reads (comespa / maxInic / calcmaxspec), the
# RegArimaUtility.olsResiduals vector that feeds the selector (request data
# for the C++ unit, %.17g tokens), a direct ArmaModelSelector replay with the
# five-row gePreferredModels() panel (orders from the numeric getters, BIC at
# %.17g), and the LIVE ArmaModule.process(RegSarimaModelling) run: result
# enum, post-run specification, and the Observer event log whose
# "selected model" info detail is formatted from the CInfo public getters
# (identity-hash toStrings are never echoed - pinning notes 6.1/6.2). The
# corpus forces HannanRissanen candidate estimation on every pcnt != 0 case
# (the sort() stage always runs HannanRissanen.process per candidate; the
# BIC is FastKalmanFilter.fastProcessing, no ML machine); the force-load
# witness runs spawn separate JVMs with -Xlog:class+load=info and count the
# HannanRissanen / FastKalmanFilter loads against the ML-side negatives
# (RegSarimaComputer, GlsSarimaComputer, HannanRissanenInitializer). Series
# are built in R and handed over as %.17g tokens (21-H precedent); the G2
# recipe below pins the JVM. Pass --repro=<dir> for the byte-identity
# cross-check between independent JVM processes.
#
# Corpus (16 cases): the clean seasonal base and the seasonal walk on the
# airline entry (pcnt = 3 full three-stage run), the acceptWhiteNoise flag,
# the seasonal = false two-stage path with the white-noise return gate, pure
# noise entries (WN-preferred tiers), the n = 64 slice (pcnt = 2, the
# stage-1 panel that sets preferred WITHOUT merging it), the short slices
# (pcnt = 0: airline Unprocessed and the default-model Changed fill), the
# flat (no-differencing) entries (bp = 1 maxspec with bd = 0), the constant
# series (empty panel -> null selection -> Failed), the log-growth channel,
# the random walk, and the quarterly shape (D11 refusal evidence at the unit
# boundary; captured for record).
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
  stop('Usage: capture-tramo-arma-selection.R [--repro=<captured directory>] --output=<existing empty directory>')
output <- normalizePath(sub('^--output=', '', args), mustWork = TRUE)
if (!dir.exists(output) || length(list.files(output, all.files = TRUE, no.. = TRUE)))
  stop('Output must be empty')
suppressWarnings(suppressMessages({
  library(rjd3jars); library(rjd3toolkit); library(rjd3tramoseats); library(rJava)
}))
.jinit()
# The rJava JVM fixes the loadable class-file version; compile the observer
# for exactly that release (JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64).
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
probe <- tempfile('kalos-tramo-arma-'); dir.create(probe)
jars <- c(list.files(system.file('java', package = 'rjd3jars'), pattern = '[.]jar$',
  full.names = TRUE),
  list.files(system.file('java', package = 'rjd3toolkit'), pattern = '[.]jar$',
    full.names = TRUE),
  list.files(system.file('java', package = 'rjd3tramoseats'), pattern = '[.]jar$',
    full.names = TRUE))
classpath <- paste(jars, collapse = .Platform$path.sep)
status <- system2(javac, c('--release', jvm_major, '-d', shQuote(probe),
  '-cp', shQuote(classpath),
  shQuote(file.path(tooling, 'TramoArmaSelectionProbe.java'))))
if (status != 0L) stop('probe compilation failed')
.jaddClassPath(c(jars[grep('tramoseats', jars)], probe))
PROBE_CLASS <- 'jdplus.tramoseats.base.core.tramo.internal.TramoArmaSelectionProbe'
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
    } else if (startsWith(ln, 'req ') || startsWith(ln, 'resvals ') ||
        startsWith(ln, 'in ')) {
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
    result = sub(paste0('^', id, ' '), '', fact('arm_result')),
    spec_after = fact('after'), pcnt = fact('pcnt'),
    direct = fact('direct_sel'),
    nevents = length(grep('^evt ', out)),
    stringsAsFactors = FALSE)
  invisible(out)
}

# Force-load witness: re-run one case in a fresh JVM with the class-load
# journal on, and count the load lines of the estimation-stack classes. The
# decode says candidate fits run HannanRissanen.process and the
# FastKalmanFilter.fastProcessing BIC; the witness proves the HR machine
# class IS loaded and that the ML-side classes (RegSarimaComputer,
# GlsSarimaComputer, the Levenberg problem and the HannanRissanen
# initializer) are NOT.
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
  loaded <- sort(unique(gsub('0x[0-9a-fA-F]+', '0xADDR',
    sub(' source:.*$', '', sub('^.*class,load\\] ', '',
      grep(' source: file:.*[.]jar', log, value = TRUE))))))
  writeLines(loaded, file.path(output, paste0('loadlog-', id, '.txt')))
  greps <- c('jdplus.toolkit.base.core.sarima.estimation.HannanRissanen',
    'jdplus.toolkit.base.core.arima.estimation.FastKalmanFilter',
    'jdplus.toolkit.base.core.arima.AutoCovarianceFunction',
    'jdplus.tramoseats.base.core.tramo.internal.ArmaModelSelector',
    'jdplus.toolkit.base.core.regsarima.RegSarimaComputer',
    'jdplus.toolkit.base.core.regsarima.GlsSarimaComputer',
    'jdplus.toolkit.base.core.regsarima.internal.HannanRissanenInitializer',
    'jdplus.toolkit.base.core.math.functions.levmar.LevenbergMarquardtMinimizer')
  counts <- vapply(greps, function(pat) sum(startsWith(loaded, pat)), integer(1))
  names(counts) <- c('hr', 'fastkf', 'acf', 'selector',
    'regsarimacomputer', 'glssarimacomputer', 'hrinit', 'levmar')
  # The module class comes from the tramoseats jar.
  data.frame(case = id, as.list(counts),
    arma_module = sum(startsWith(loaded,
      'jdplus.tramoseats.base.core.tramo.internal.ArmaModule')),
    check.names = FALSE, row.names = NULL)
}

# -- corpus -------------------------------------------------------------------
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
set.seed(2321)
rn <- rnorm(n)                       # pure white noise
q4 <- 40 + 0.1 * seq_len(60) + 6 * sin(2 * pi * seq_len(60) / 4) +
  rnorm(60, 0, 0.1)
k56 <- b[1:56]                       # pcnt = 0 (below the 62 gate)
k64 <- b[1:64]                       # pcnt = 2 (stage-1 merge skipped)
const <- rep(50, n)                  # degenerate: empty panel / Failed

AIR <- c(1, 1, 0, 1, 0, 1)          # (0,1,1)(0,1,1): d bd p q bp bq
FLAT <- c(0, 0, 0, 0, 0, 0)

ar <- function(id, note, y, freq = 12L, sea = 'y', wn = 'n', log = 'n',
    spec = AIR, mean = 'y') {
  hdr <- sprintf('ARM %s %d %s %s %s %d %d %d %d %d %d %s', id, freq, sea, wn,
    log, spec[1], spec[2], spec[3], spec[4], spec[5], spec[6], mean)
  case(id, note, hdr, y)
}

ar('a_base', 'clean seasonal base on the airline entry: pcnt 3 full run', b)
ar('a_srw', 'seasonal walk on the airline entry: dense panel', srw)
ar('a_wnflag', 'acceptWhiteNoise on the clean base', b, wn = 'y')
ar('a_noseas', 'seasonal = false two-stage path', b, sea = 'n')
ar('a_noise_noseas', 'pure noise, seasonal = false: white-noise return gate', rn,
  sea = 'n')
ar('a_noise_wn', 'pure noise with acceptWhiteNoise', rn, wn = 'y')
ar('a_noise_flat', 'pure noise on the flat entry', rn, spec = FLAT)
ar('a_m2', 'n = 64 slice: pcnt 2, stage-1 panel without merge', k64)
ar('a_short', 'n = 56 slice: pcnt 0 on the airline, Unprocessed', k56)
ar('a_default', 'n = 56 flat entry: default-model Changed fill', k56, spec = FLAT)
ar('a_flat96', 'clean base flat entry: bd = 0 with bp = 1 maxspec', b, spec = FLAT)
ar('a_dsem_flat', 'deterministic seasonal flat entry: strong selection change',
  dsem, spec = FLAT)
ar('a_rw', 'random walk on the airline entry', rw)
ar('a_growth_log', 'growth series through the log channel', gs, log = 'y')
ar('a_const', 'constant series: empty panel, null selection, Failed', const)
ar('a_quarter', 'quarterly shape (D11 refusal evidence, record only)', q4,
  freq = 4L)

witness <- rbind(
  loadwitness('a_srw', 'ARM a_srw 12 y n n 1 1 0 1 0 1 y', srw),
  loadwitness('a_base', 'ARM a_base 12 y n n 1 1 0 1 0 1 y', b))

wcsv <- function(name, df) {
  con <- file(file.path(output, name), 'w')
  write.csv(df, con, row.names = FALSE, na = '')
  close(con)
}
wcsv('cases.csv', do.call(rbind, summaries))
wcsv('inputs.csv', do.call(rbind, inputs))
wcsv('arma-facts.csv', do.call(rbind, facts))
wcsv('arma-events.csv', do.call(rbind, events))
wcsv('load-witness.csv', witness)

writeLines(c(
  paste0('# metadata for the item 23-D tramo-arma-selection capture ',
    '(generated by validation/reference/capture-tramo-arma-selection.R)'),
  paste0('r_version <- "', paste(R.version$major, R.version$minor, sep = '.'), '"'),
  paste0('jvm_version <- "', .jcall('java/lang/System', 'S', 'getProperty', 'java.version'), '"'),
  paste0('jvm_vendor <- "', .jcall('java/lang/System', 'S', 'getProperty', 'java.vendor'), '"'),
  paste0('java_options <- "', Sys.getenv('_JAVA_OPTIONS'), '"'),
  paste0('capture_time <- "', format(Sys.time(), tz = 'UTC'), '"'),
  paste0('capture_host <- "', Sys.info()[['nodename']], '"'),
  paste0('capture_user <- "', Sys.info()[['user']], '"'),
  paste0('probe_sha256 <- "', reference_sha256(file.path(root,
    'validation/reference/TramoArmaSelectionProbe.java')), '"'),
  paste0('script_sha256 <- "', reference_sha256(file.path(root,
    'validation/reference/capture-tramo-arma-selection.R')), '"'),
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
