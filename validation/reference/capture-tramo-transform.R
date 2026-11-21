# Pinned-reference capture for item 23-B (part 1): the TRAMO transformation
# pre-stage of the pinned jdplus 3.9.0 stack (rjd3tramoseats), observed through
# the LIVE public entry points by TramoTransformDecisionProbe.java - the
# seasonality machine (TramoSeasonalityDetector.hasSeasonality with the raw
# SeasonalityTests getters), the internal TramoUtility.calcCv critical-value
# grid, the robust AO+LS correction through the LIVE internal
# OutliersDetectionModule (the robust topology TramoKernel.testTransformation
# wires), and the LIVE LogLevelModule decision (builder + process exactly as
# TramoKernel drives it). Raw getter values throughout (%.17g doubles, enum
# constant names, identity-hash toStrings never echoed - pinning notes
# 6.1/6.2). Series are built in R and handed to the probe as %.17g tokens, so
# the input bits are intrinsic-independent (21-H precedent); the G2 recipe
# below pins the JVM anyway. Pass --repro=<dir> to cross-check a capture
# against an earlier one from an independent JVM process (byte-identity gate,
# final-information precedent).
#
# Corpus (29 cases): the calcCv grid at the pinned breakpoints (20/30/50/51/
# 100/144/240/449/450/480); five SEASON shapes (clean seasonal, noisy, growth
# log-flip candidate, random walk, flat); fourteen TRANSFORM cases on the
# TramoKernel pre-stage: m5 (Strong -> levels), growth (Weak -> m011 downgrade
# + log flip), growth with/without the robust AO+LS correction (fixed cv and
# calcCv), AO/noout robust log decisions, the negative-values gate, mean-off,
# fixed seasonal flags, log-preference variants (token 0/0.2/-1.0), and a
# tightened estimation precision.
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
  stop('Usage: capture-tramo-transform.R [--repro=<captured directory>] --output=<existing empty directory>')
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
probe <- tempfile('kalos-tramo-transform-'); dir.create(probe)
jars <- c(list.files(system.file('java', package = 'rjd3jars'), pattern = '[.]jar$',
  full.names = TRUE),
  list.files(system.file('java', package = 'rjd3toolkit'), pattern = '[.]jar$',
    full.names = TRUE),
  list.files(system.file('java', package = 'rjd3tramoseats'), pattern = '[.]jar$',
    full.names = TRUE))
classpath <- paste(jars, collapse = .Platform$path.sep)
status <- system2(javac, c('--release', jvm_major, '-d', shQuote(probe),
  '-cp', shQuote(classpath),
  shQuote(file.path(tooling, 'TramoTransformDecisionProbe.java'))))
if (status != 0L) stop('probe compilation failed')
.jaddClassPath(c(jars[grep('tramoseats', jars)], probe))
if (inherits(try(.jfindClass('TramoTransformDecisionProbe'), silent = TRUE), 'try-error'))
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

# case: one probe call. `hdr` is the header line, `y` the series (NULL for
# CALCCV). The header and the values line go to inputs.csv verbatim; the
# probe's req echo is recorded as an input line too (protocol read-back).
case <- function(id, note, kind, n, y, hdr) {
  lines <- hdr
  if (!is.null(y))
    lines <- c(hdr, paste('values', paste(ifelse(is.na(y), 'nan', vapply(y, g, character(1))),
      collapse = ' ')))
  for (ln in lines)
    inputs[[length(inputs) + 1L]] <<- data.frame(id = id, line = ln, stringsAsFactors = FALSE)
  res <- .jcall('TramoTransformDecisionProbe', 'S', 'run', .jarray(lines))
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
  verdict <- sub('^season_verdict ([^ ]+) .*', '\\1', fact('season_verdict'))
  summaries[[length(summaries) + 1L]] <<- data.frame(
    id = id, note = note, kind = kind, n = n,
    verdict = verdict,
    seasonal = sub('^seasonal ([01])$', '\\1', fact('seasonal')),
    robust_status = fact('robust_status'),
    loglevel_status = fact('loglevel_status'),
    choosing_log = sub('^choosing_log ([01])$', '\\1', fact('choosing_log')),
    log_transform = sub('^log_transform ([01])$', '\\1', fact('log_transform')),
    nevents = length(grep('^evt ', out)),
    stringsAsFactors = FALSE)
  invisible(NULL)
}

# -- corpus --------------------------------------------------------------------
n <- 96L
t <- seq_len(n)
# M5 deterministic seasonal series (identical recipe to the pinned M5Probe).
m5 <- 50 + 0.05 * t + 5 * sin(2 * pi * t / 12) + 3 * cos(4 * pi * t / 12) +
  0.5 * sin(14 * pi * t / 12)
# 21-H standard base (seeded additive-noise seasonal).
set.seed(21202)
b <- 50 + 0.05 * t + 5 * sin(2 * pi * t / 12) + 3 * cos(4 * pi * t / 12) +
  0.5 * sin(14 * pi * t / 12) + rnorm(n, 0, 0.3)
ao <- b; ao[49] <- ao[49] + 15
# growth (log-flip) 21-C recipe.
set.seed(2302)
gs <- exp(cumsum(rnorm(n, .01, .02)) + .2 * sin(2 * pi * t / 12))
gs2 <- gs; gs2[61] <- gs2[61] * 2.5; gs2[80:90] <- gs2[80:90] * 0.6
# non-seasonal random walk: NotObservable candidate.
set.seed(2303)
rw <- 50 + cumsum(rnorm(n, 0, .8)) + rnorm(n)
# negative-values gate series.
set.seed(2304)
neg <- cumsum(rnorm(n, 0, 1))
flat <- rep(7, n)

for (nn in c(20L, 30L, 50L, 51L, 100L, 144L, 240L, 449L, 450L, 480L))
  case(paste0('cv', nn), paste('calcCv grid point n =', nn), 'calccv', nn, NULL,
    sprintf('CALCCV cv%d %d', nn, nn))

case('season_m5', 'clean deterministic seasonal: Qs + Friedman both fire, Strong',
  'season', 96L, m5, 'SEASON season_m5 12')
case('season_noout', 'seeded noisy seasonal: verdict on the noisy shape',
  'season', 96L, b, 'SEASON season_noout 12')
case('season_gs', 'growth series: Weak verdict (Moderate missing) - downgrade path',
  'season', 96L, gs, 'SEASON season_gs 12')
case('season_rw', 'random walk: NotObservable verdict',
  'season', 96L, rw, 'SEASON season_rw 12')
case('season_flat', 'flat series: degenerate differencing, verdict pinned',
  'season', 96L, flat, 'SEASON season_flat 12')

# TRANSFORM hdr: id freq mean detect specseas logpref prec robust cv
case('tr_m5', 'auto detect: Strong verdict keeps the seasonal airline, levels win',
  'transform', 96L, m5, 'TRANSFORM tr_m5 12 y y y def 0.00001 n 5')
case('tr_gs', 'auto detect: Weak verdict -> m011 downgrade, log flip wins',
  'transform', 96L, gs, 'TRANSFORM tr_gs 12 y y y def 0.00001 n 5')
case('tr_gsrob', 'growth + robust AO+LS correction (fixed cv 5): corrected scan, log',
  'transform', 96L, gs2, 'TRANSFORM tr_gsrob 12 y y y def 0.00001 y 5')
case('tr_gsrobc', 'growth + robust correction at calcCv(96)',
  'transform', 96L, gs2, 'TRANSFORM tr_gsrobc 12 y y y def 0.00001 y calc')
case('tr_ao', 'AO series + robust correction: corrected model, log wins',
  'transform', 96L, ao, 'TRANSFORM tr_ao 12 y y y def 0.00001 y 5')
case('tr_noout', 'no-outlier series + robust correction: scan stays empty, log wins',
  'transform', 96L, b, 'TRANSFORM tr_noout 12 y y y def 0.00001 y 5')
case('tr_neg', 'negative values: LogLevelModule gate keeps levels (Unchanged)',
  'transform', 96L, neg, 'TRANSFORM tr_neg 12 y y y def 0.00001 n 5')
case('tr_m0', 'mean off: concentration without the mean column',
  'transform', 96L, gs, 'TRANSFORM tr_m0 12 n y y def 0.00001 n 5')
case('tr_fixseas', 'no detection, spec seasonal kept: airline path, log decision',
  'transform', 96L, gs, 'TRANSFORM tr_fixseas 12 y n y def 0.00001 n 5')
case('tr_fixnoseas', 'no detection, spec seasonal off: m011-style path',
  'transform', 96L, gs, 'TRANSFORM tr_fixnoseas 12 y n n def 0.00001 n 5')
case('tr_pref0', 'log preference 0: flip threshold vanishes',
  'transform', 96L, gs, 'TRANSFORM tr_pref0 12 y y y 0.0 0.00001 n 5')
case('tr_prefp', 'log preference 0.2: penalises the log side',
  'transform', 96L, gs, 'TRANSFORM tr_prefp 12 y y y 0.2 0.00001 n 5')
case('tr_prefm', 'log preference -1.0: favours the log side',
  'transform', 96L, gs, 'TRANSFORM tr_prefm 12 y y y -1.0 0.00001 n 5')
case('tr_prec8', 'estimation precision 1e-8: tighter LM landing',
  'transform', 96L, gs, 'TRANSFORM tr_prec8 12 y y y def 0.00000001 n 5')

# -- corpus files ---------------------------------------------------------------
wcsv <- function(name, df) {
  con <- file(file.path(output, name), open = 'wt')
  write.table(df, con, sep = ',', qmethod = 'double', row.names = FALSE,
    col.names = TRUE, na = '')
  close(con)
}
wcsv('cases.csv', do.call(rbind, summaries))
wcsv('inputs.csv', do.call(rbind, inputs))
wcsv('transform-facts.csv', do.call(rbind, facts))
wcsv('transform-events.csv', do.call(rbind, events))

writeLines(c(
  paste0('# metadata for the item 23-B tramo-transform capture ',
    '(generated by validation/reference/capture-tramo-transform.R)'),
  paste0('r_version <- "', paste(R.version$major, R.version$minor, sep = '.'), '"'),
  paste0('jvm_version <- "', .jcall('java/lang/System', 'S', 'getProperty', 'java.version'), '"'),
  paste0('jvm_vendor <- "', .jcall('java/lang/System', 'S', 'getProperty', 'java.vendor'), '"'),
  paste0('java_options <- "', Sys.getenv('_JAVA_OPTIONS'), '"'),
  paste0('capture_time <- "', format(Sys.time(), tz = 'UTC'), '"'),
  paste0('capture_host <- "', Sys.info()[['nodename']], '"'),
  paste0('capture_user <- "', Sys.info()[['user']], '"'),
  paste0('probe_sha256 <- "', reference_sha256(file.path(root,
    'validation/reference/TramoTransformDecisionProbe.java')), '"'),
  paste0('script_sha256 <- "', reference_sha256(file.path(root,
    'validation/reference/capture-tramo-transform.R')), '"'),
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
