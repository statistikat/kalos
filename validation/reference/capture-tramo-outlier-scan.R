# Pinned-reference capture for item 23-B (part 2): the round-0 TRAMO outlier
# scan of the pinned jdplus 3.9.0 stack (rjd3tramoseats), observed through the
# LIVE module entry point tramo.internal.OutliersDetectionModule by
# TramoOutlierScanProbe.java - builder + process(modelling, cv) over a
# RegSarimaModelling built from a live ModelDescription, exactly as
# TramoKernel/outlierSelection drives it (both JVM builder topologies: the
# spec mix and the AO+LS robust mix). Raw getter values throughout (%.17g
# doubles, identity-hash toStrings never echoed - pinning notes 6.1/6.2).
# Series are built in R (21-H recipes: seed 21202 base, 2302 growth) and
# handed to the probe as %.17g tokens; the G2 recipe below pins the JVM.
# Pass --repro=<dir> to cross-check a capture against an earlier one from an
# independent JVM process (byte-identity gate, final-information precedent).
#
# Corpus (26 cases): noout/ao/ls/multi shapes on the standard airline with the
# full AO+LS+TC+SO mix (the spec topology of TramoKernel); SO-only and AO-only
# factory mixes (the JVM's PeriodicOutlierFactory picks nothing - a golden);
# tc-rate flips 0.5/0.7/0.9; selection spans (in-domain between, an excluding
# window, and the epoch-anchored between whose empty hit collapses the bounds
# - the fail-closed spannone shape); pre-registered outlier variables (the
# double effect in-domain: X column + excluded cell; the {excluded=true}
# out-of-domain tag), maximum likelihood on, interior missing values, a raised
# critical value, the growth series, a tightened precision, and the short-
# series comatip failures (n=20, n=30 with and without ml: need=max(14,13)+1+
# 4+12=31 > 30 so ml cannot rescue). Monthly only (register D11).
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
  stop('Usage: capture-tramo-outlier-scan.R [--repro=<captured directory>] --output=<existing empty directory>')
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
probe <- tempfile('kalos-tramo-outlier-scan-'); dir.create(probe)
jars <- c(list.files(system.file('java', package = 'rjd3jars'), pattern = '[.]jar$',
  full.names = TRUE),
  list.files(system.file('java', package = 'rjd3toolkit'), pattern = '[.]jar$',
    full.names = TRUE),
  list.files(system.file('java', package = 'rjd3tramoseats'), pattern = '[.]jar$',
    full.names = TRUE))
classpath <- paste(jars, collapse = .Platform$path.sep)
status <- system2(javac, c('--release', jvm_major, '-d', shQuote(probe),
  '-cp', shQuote(classpath),
  shQuote(file.path(tooling, 'TramoOutlierScanProbe.java'))))
if (status != 0L) stop('probe compilation failed')
.jaddClassPath(c(jars[grep('tramoseats', jars)], probe))
if (inherits(try(.jfindClass('TramoOutlierScanProbe'), silent = TRUE), 'try-error'))
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

case <- function(id, note, y, hdr) {
  lines <- c(hdr, paste('values',
    paste(ifelse(is.na(y), 'nan', vapply(y, g, character(1))), collapse = ' ')))
  for (ln in lines)
    inputs[[length(inputs) + 1L]] <<- data.frame(id = id, line = ln, stringsAsFactors = FALSE)
  res <- .jcall('TramoOutlierScanProbe', 'S', 'run', .jarray(lines))
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
  ninfo <- 0L
  for (e in events)
    if (identical(e$id, id) && grepl('odmInfo', e$detail, fixed = TRUE))
      ninfo <- ninfo + 1L
  outliers <- ''
  for (e in events)
    if (identical(e$id, id) && grepl('odmInfo', e$detail, fixed = TRUE))
      outliers <- sub('^.*outliers\\[', '', sub('\\]$', '', e$detail))
  nn <- as.integer(sub('^n ([0-9]+) .*', '\\1', fact('in')))
  summaries[[length(summaries) + 1L]] <<- data.frame(
    id = id, note = note, n = nn,
    status = sub('^scan_status ([A-Za-z]+)$', '\\1', fact('scan_status')),
    need_estimation = sub('^need_estimation ([01])$', '\\1', fact('need_estimation')),
    n_info = ninfo,
    n_outliers = if (nzchar(outliers) && outliers != ']')
      length(strsplit(outliers, '\\], \\[')[[1L]]) else 0L,
    outliers = outliers,
    nevents = length(grep('^evt ', out)),
    stringsAsFactors = FALSE)
  invisible(NULL)
}

# -- corpus --------------------------------------------------------------------
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
gs2 <- gs; gs2[61] <- gs2[61] * 2.5; gs2[80:90] <- gs2[80:90] * 0.6
miss <- b; miss[21] <- NA; miss[46] <- NA
short20 <- b[1:20]
short30 <- b[1:30]

# hdr: id freq mean log d D p q bp bq fac tcrate prec ml maxround maxout
#      span cv interp pre
S <- 'SCAN %s 12 y y 1 1 1 1 0 1 %s %s 1e-07 %s 50 30 %s %s %s %s'
scan <- function(id, note, y, fac = 'yyyy', tcrate = '0.7', ml = 'n', span = 'all',
    cv = 'calc', interp = 'y', pre = '-') {
  case(id, note, y, sprintf(S, id, fac, tcrate, ml, span, cv, interp, pre))
}

scan('sc_noout', 'clean series: scan finds nothing (Unchanged)', b)
scan('sc_ao', 'single AO at 48: AO wins the round-0 scan', ao)
scan('sc_ls', 'level shift at 60: LS with the zero-ended factory', ls)
scan('sc_multi', 'AO+LS+TC mixture: four-outlier set with the add/verify loop', multi)
scan('sc_so', 'SO-only mix on the AO series: the JVM SO factory picks nothing', ao,
  fac = 'nnny')
scan('sc_aoonly', 'AO-only mix: same winner through the single-factory table', ao,
  fac = 'ynnn')
scan('sc_tc05', 'tc rate 0.5: decay too fast, TC loses to LS at 48', multi, tcrate = '0.5')
scan('sc_tc09', 'tc rate 0.9: TC keeps its win at 48', multi, tcrate = '0.9')
scan('sc_span', 'selection span idx:12-84 (between, in-domain): AO inside the window', ao,
  span = 'idx:12-84')
scan('sc_spanout', 'selection span idx:60-90: AO outside the window -> nothing', ao,
  span = 'idx:60-90')
scan('sc_excl', 'pre-registered AO at 30: X column + excluded cell, set reshapes', multi,
  pre = 'AO:30')
scan('sc_ml', 'maximum likelihood on: ML estimateModel path through the rounds', ao,
  ml = 'y')
scan('sc_missing', 'interior missing values: est-relative exclusions', miss)
scan('sc_cv4', 'critical value 4.0 instead of calcCv: marginal candidates stay out', multi,
  cv = '4.0')
scan('sc_maxout2', 'maxOutliers at the JVM default through calcCv', multi)
scan('sc_gs2', 'growth series with spike + shelf: log-scale scan', gs2)
case('sc_prec8', 'estimation precision 1e-8: tighter landing, same decisions', ao,
  'SCAN sc_prec8 12 y y 1 1 1 1 0 1 yyyy 0.7 0.00000001 n 50 30 all calc y -')
case('sc_shortfail', 'n=20: comatip gate fails the module', short20,
  'SCAN sc_shortfail 12 y y 1 1 1 1 0 1 yyyy 0.7 1e-07 n 50 30 all calc y -')
case('sc_s30', 'n=30: need = max(14,13)+1+4+12 = 31 > 30 -> Failed', short30,
  'SCAN sc_s30 12 y y 1 1 1 1 0 1 yyyy 0.7 1e-07 n 50 30 all calc y -')
case('sc_s30ml', 'n=30 with maximum likelihood: ml cannot rescue the size gate', short30,
  'SCAN sc_s30ml 12 y y 1 1 1 1 0 1 yyyy 0.7 1e-07 y 50 30 all calc y -')
scan('sc_spanfull', 'between over the whole domain == all (inclusivity pin)', ao,
  span = 'idx:0-95')
scan('sc_spannone', 'epoch-anchored between hits nothing: bounds collapse, nothing found', ao,
  span = 'idxe:0-95')
scan('sc_pexao48', 'pre-registered AO exactly at 48: cell suppressed, nothing else fires', ao,
  pre = 'AO:48')
scan('sc_pexao47', 'pre-registered AO at 47 (adjacent control): AO at 48 still wins', ao,
  pre = 'AO:47')
scan('sc_pextc48', 'pre-registered TC at 48: model + cell change, scan lands on the misfit',
  multi, pre = 'TC:48')
scan('sc_pexctl', 'pre-registered AO at 2 (far control): identical set, order pinned', multi,
  pre = 'AO:2')

# -- corpus files ---------------------------------------------------------------
wcsv <- function(name, df) {
  con <- file(file.path(output, name), open = 'wt')
  write.table(df, con, sep = ',', qmethod = 'double', row.names = FALSE,
    col.names = TRUE, na = '')
  close(con)
}
wcsv('cases.csv', do.call(rbind, summaries))
wcsv('inputs.csv', do.call(rbind, inputs))
wcsv('scan-facts.csv', do.call(rbind, facts))
wcsv('scan-events.csv', do.call(rbind, events))

writeLines(c(
  paste0('# metadata for the item 23-B tramo-outlier-scan capture ',
    '(generated by validation/reference/capture-tramo-outlier-scan.R)'),
  paste0('r_version <- "', paste(R.version$major, R.version$minor, sep = '.'), '"'),
  paste0('jvm_version <- "', .jcall('java/lang/System', 'S', 'getProperty', 'java.version'), '"'),
  paste0('jvm_vendor <- "', .jcall('java/lang/System', 'S', 'getProperty', 'java.vendor'), '"'),
  paste0('java_options <- "', Sys.getenv('_JAVA_OPTIONS'), '"'),
  paste0('capture_time <- "', format(Sys.time(), tz = 'UTC'), '"'),
  paste0('capture_host <- "', Sys.info()[['nodename']], '"'),
  paste0('capture_user <- "', Sys.info()[['user']], '"'),
  paste0('probe_sha256 <- "', reference_sha256(file.path(root,
    'validation/reference/TramoOutlierScanProbe.java')), '"'),
  paste0('script_sha256 <- "', reference_sha256(file.path(root,
    'validation/reference/capture-tramo-outlier-scan.R')), '"'),
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
