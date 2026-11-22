# Pinned-reference capture for item 24-C: the SEATS Wiener-Kolmogorow
# extraction machine of the pinned jdplus 3.9.0 stack (rjd3tramoseats),
# observed through the public-API observer SeatsWkExtractionProbe.java: the
# live SeatsKernel(SeatsToolkit.of(spec)).process(SeatsModelSpec, log) run
# end to end - validation, decomposition (the 24-B seam facts reflowed as
# context), estimateComponents (WienerKolmogorovEstimator.decompose + Burman
# initial surface) and the DefaultBiasCorrector final surface. Component rows
# are read from SeatsResults.getInitialComponents()/getFinalComponents()
# through the fixed (ComponentType x ComponentInformation) enumeration as RAW
# TsData (%.17g, never a display toString; pinning notes 6.1/6.2/7). The
# request language and series generators are the 24-A/24-B ones plus src E
# (planted spikes); refusal surfaces (KalmanSmoother, quarterly, var 0, the
# D6 approximation continuations) are captured as data: the 24-C unit refuses
# them and the golden frame around them is asserted.
# Pass --repro=<dir> to cross-check a capture against an earlier one from an
# independent JVM process (byte-identity gate, final-information precedent).
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
  stop('Usage: capture-seats-wk-extraction.R [--repro=<captured directory>] --output=<existing empty directory>')
output <- normalizePath(sub('^--output=', '', args), mustWork = TRUE)
if (!dir.exists(output) || length(list.files(output, all.files = TRUE, no.. = TRUE)))
  stop('Output must be empty')
suppressWarnings(suppressMessages({
  library(rjd3jars); library(rjd3toolkit); library(rjd3tramoseats); library(rJava)
}))
.jinit()
# The rJava JVM fixes the loadable class-file version; compile the observer
# for exactly that release (21-H.3 precedent: pin
# JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64).
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
probe <- tempfile('kalos-seats-wk-extraction-'); dir.create(probe)
jars <- c(list.files(system.file('java', package = 'rjd3jars'), pattern = '[.]jar$',
  full.names = TRUE),
  list.files(system.file('java', package = 'rjd3toolkit'), pattern = '[.]jar$',
    full.names = TRUE),
  list.files(system.file('java', package = 'rjd3tramoseats'), pattern = '[.]jar$',
    full.names = TRUE))
classpath <- paste(jars, collapse = .Platform$path.sep)
status <- system2(javac, c('--release', jvm_major, '-d', shQuote(probe),
  '-cp', shQuote(classpath),
  shQuote(file.path(tooling, 'SeatsWkExtractionProbe.java'))))
if (status != 0L) stop('probe compilation failed')
.jaddClassPath(c(jars[grep('tramoseats', jars)], probe))
if (inherits(try(.jfindClass('SeatsWkExtractionProbe'), silent = TRUE), 'try-error'))
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
  res <- .jcall('SeatsWkExtractionProbe', 'S', 'run', .jarray(lines))
  out <- strsplit(res, '\n', fixed = TRUE)[[1L]]
  out <- out[nzchar(out)]
  if (!identical(out[[1L]], paste('case', id)))
    stop('observer echoed a different case id for ', id)
  for (ln in out) {
    if (startsWith(ln, 'evt ') || startsWith(ln, 'kevt ')) {
      m <- regmatches(ln, regexec(
        '^(k?evt) ([0-9]+) ([a-z]+) ([0-9]+) (.*) det (.*)$', ln))[[1L]]
      if (length(m) != 7L) stop('unparsable event line: ', ln)
      events[[length(events) + 1L]] <<- data.frame(id = id, stream = m[[2L]],
        seq = m[[3L]], kind = m[[4L]], depth = m[[5L]], msg = m[[6L]],
        detail = m[[7L]], stringsAsFactors = FALSE)
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
  fact <- function(prefix) {
    v <- out[startsWith(out, paste0(prefix, ' '))]
    if (!length(v)) '' else sub(paste0('^', prefix, ' '), '', v[[1L]])
  }
  st <- fact('status')
  if (!nzchar(st)) stop('case ', id, ' produced no status line')
  n <- as.integer(gsub('^n ([0-9]+)$', '\\1',
    lines[startsWith(lines, 'n ')][1L]))
  if (is.na(n)) n <- 96L
  freq <- as.integer(gsub('^freq ([0-9]+)$', '\\1',
    lines[startsWith(lines, 'freq ')][1L]))
  if (length(freq) == 0L || is.na(freq[1L])) freq <- 12L
  startline <- lines[startsWith(lines, 'start ')]
  start <- if (length(startline)) sub('^start ', '', startline[[1L]]) else '1990 1'
  knobs <- strsplit(fact('req knobs'), ' ', fixed = TRUE)[[1L]]
  ucarline <- fact('d count')
  ucar <- if (nzchar(ucarline)) strsplit(ucarline, ' ', fixed = TRUE)[[1L]] else c('', '')
  cmtline <- fact('cmp count')
  cmt <- if (nzchar(cmtline)) strsplit(cmtline, ' ', fixed = TRUE)[[1L]] else c('', '')
  summaries[[length(summaries) + 1L]] <<- data.frame(
    id = id, note = note, freq = freq, start = start, n = n,
    status = st,
    xl = knobs[1L], mode = knobs[6L],
    fc = knobs[7L], bc = knobs[8L], method = knobs[9L], bias = knobs[10L],
    vchanged = sub('^v.changed ', '',
      out[startsWith(out, 'v.changed ')][[1L]]),
    ddecompose = fact('d.decompose'),
    dcount = ucar[1L], dvalid = ucar[2L],
    cmpcount = cmt[1L], cmpvalid = cmt[2L],
    inits = length(grep('^init ', out)),
    finals = length(grep('^final ', out)),
    kevts = length(grep('^kevt ', out)),
    stringsAsFactors = FALSE)
  invisible(NULL)
}

# -- corpus -------------------------------------------------------------------
# D6/D1/D11 refusal-context rows (the unit refuses; the JVM facts around the
# refusal are asserted as context):
case('x_airline', 'classical airline frozen Fixed (0.5/0.4); Legacy approximates - D6 refusal context row',
  'specname airline', 'var 0.5')
case('x_stab_none', 'stabilized q=2 model under None: SeatsException Invalid decomposition status row',
  'spec 0 1 2 0 1 1', 'theta -1.9 0.92', 'btheta 0.4', 'var 1.0', 'mode None')
case('x_crash_fixma', 'fixMa deg-1 quirk lands theta -1.0: approximator MatrixException crash - D6 refusal context row',
  'xl 1.0', 'spec 0 1 1 0 1 1', 'theta 0.99995', 'btheta 0.5', 'var 1.0')
case('x_quarterly', 'quarterly period-4 airline -0.5/-0.3: decomposable extraction in the JVM - D11 monthly-first refusal row',
  'freq 4', 'period 4', 'spec 0 1 1 0 1 1', 'theta -0.5', 'btheta -0.3',
  'var 1.0', 'mode None')
case('x_var0', 'white noise with innovationVariance 0: ConcentratedLikelihood (Kalman) variance path - var 0 refusal row',
  'spec 0 0 0 0 0 0', 'var 0.0', 'mode None')
case('x_kalman', 'canonical decomposable airline under KalmanSmoother - D1 refusal row (JVM KalmanEstimator facts as context)',
  'spec 0 1 1 0 1 1', 'theta -0.6', 'btheta -0.3', 'var 1.0', 'mode None',
  'method KalmanSmoother')
case('x_kalman_wn', 'white noise under KalmanSmoother - D1 refusal row',
  'spec 0 0 0 0 0 0', 'var 2.0', 'mode None', 'method KalmanSmoother')

# Extraction rows: decomposable monthly models under mode None (the unit's
# full-work surface), mirroring the 24-B decomposable corpus:
case('x_tpl_zero', 'airline orders with zero MA parameters (simplified to (0,1,0)(0,1,0)): extraction on the degenerate split',
  'spec 0 1 1 0 1 1', 'theta 0', 'btheta 0', 'var 1.0', 'mode None')
case('x_cut_neg', 'MA beyond XL clamped (-0.95/-0.95): extraction after the validator cut',
  'spec 0 1 1 0 1 1', 'theta -0.96', 'btheta -0.98', 'var 1.0', 'mode None')
case('x_cut_bq2', 'bq=2 seasonal MA (loop-bound quirk drops it in the split): extraction',
  'spec 0 1 0 0 1 2', 'btheta 0.6 0.45', 'var 1.0', 'mode None')
case('x_simplify', 'tiny parameters trimmed by simplify (cutOff): extraction on the trimmed model',
  'spec 1 1 2 2 1 1', 'phi 2e-7', 'theta 0.5 5e-7', 'bphi 0.3 -1e-9',
  'btheta -0.5', 'var 1.0', 'mode None')
case('x_trend_ma', 'trend-only MA (no seasonal differencing): extraction without a seasonal component split',
  'spec 0 1 1 0 0 0', 'theta 0.5', 'var 1.0', 'mode None')
case('x_wn', 'pure white noise: zero-signal split, degenerate WK filters',
  'spec 0 0 0 0 0 0', 'var 2.0', 'mode None')
case('x_seasonal', 'pure seasonal (0,0,1)(0,1,1): extraction without trend differencing',
  'spec 0 0 1 0 1 1', 'theta 0.2', 'btheta -0.6', 'var 0.3', 'mode None')
case('x_ar1_slow', 'AR(1) 0.7 trend root accepted at the trend boundary: extraction',
  'spec 1 1 0 0 1 0', 'phi 0.7', 'var 1.0', 'mode None')
case('x_ar1_rej', 'same model with trendBoundary 0.8 (root rejected -> extra component): extraction on the split',
  'spec 1 1 0 0 1 0', 'phi 0.7', 'rmod 0.8', 'var 1.0', 'mode None')
case('x_ar2', 'quadratic stationary AR trend: extraction',
  'spec 2 1 0 0 1 0', 'phi 0.5 -0.06', 'var 1.0', 'mode None')
case('x_ar3', 'cubic stationary AR: JVM status EcoException Negative variance (extraction abort row)',
  'spec 3 1 0 0 1 0', 'phi 0.9 -0.35 0.05', 'var 1.0', 'mode None')
case('x_stat_ar2', 'stationary quadratic AR, no differencing: extraction',
  'spec 2 0 0 0 0 0', 'phi 0.5 -0.06', 'var 1.0', 'mode None')
case('x_stat_ar3', 'stationary cubic AR: JVM status EcoException Negative variance (extraction abort row)',
  'spec 3 0 0 0 0 0', 'phi 0.9 -0.35 0.05', 'var 1.0', 'mode None')
case('x_airline_neg', 'canonical decomposable negative airline (0,1,1)(0,1,1) -0.6/-0.3: full WK extraction',
  'spec 0 1 1 0 1 1', 'theta -0.6', 'btheta -0.3', 'var 1.0', 'mode None')
case('x_fail_wn_unit', 'non-invertible WN theta=1.0 cut to 0.95: extraction on the cut model',
  'spec 0 0 1 0 0 0', 'theta 1.0', 'var 1.0', 'mode None')
case('x_fail_airline_nod', 'theta=1 btheta=-1 without differencing (cut to +-0.95): extraction',
  'spec 0 0 1 0 1 1', 'theta 1', 'btheta -1', 'var 1.0', 'mode None')
case('x_cut_then_decomp', 'cut then decompose on the B series (n=120): -1.2/-0.98 clamped, extraction',
  'src B', 'n 120', 'spec 0 1 1 0 1 1', 'theta -1.2', 'btheta -0.98',
  'var 1.0', 'mode None')
case('x_smod_lo', 'airline -0.6/-0.3 with seasBoundary 0.2: extraction on the knob-altered split',
  'spec 0 1 1 0 1 1', 'theta -0.6', 'btheta -0.3', 'smod 0.2', 'var 1.0',
  'mode None')

# WK-knob rows on the canonical negative airline (forecast/backcast modes,
# transformation and method flags, series shapes):
case('x_fc0', 'forecastCount 0: no forecast rows, Burman horizon changes',
  'spec 0 1 1 0 1 1', 'theta -0.6', 'btheta -0.3', 'var 1.0', 'mode None',
  'forecastcount 0')
case('x_fc1', 'forecastCount -1 (period forecasts): extraction',
  'spec 0 1 1 0 1 1', 'theta -0.6', 'btheta -0.3', 'var 1.0', 'mode None',
  'forecastcount -1')
case('x_fc3', 'forecastCount -3 (3-period horizon): extraction',
  'spec 0 1 1 0 1 1', 'theta -0.6', 'btheta -0.3', 'var 1.0', 'mode None',
  'forecastcount -3')
case('x_bc1', 'backcastCount 1 (12 backcasts): the backcast surface opens',
  'spec 0 1 1 0 1 1', 'theta -0.6', 'btheta -0.3', 'var 1.0', 'mode None',
  'backcastcount 1')
case('x_bc2_fc1', 'backcastCount 2 with forecastCount -1: both tails open',
  'spec 0 1 1 0 1 1', 'theta -0.6', 'btheta -0.3', 'var 1.0', 'mode None',
  'backcastcount 2', 'forecastcount -1')
case('x_mean_off', 'meanCorrection off: Burman useMean path changes',
  'spec 0 1 1 0 1 1', 'theta -0.6', 'btheta -0.3', 'var 1.0', 'mode None',
  'mean 0')
case('x_log', 'log transform on the canonical split: DefaultBiasCorrector.correctLogs surface',
  'spec 0 1 1 0 1 1', 'theta -0.6', 'btheta -0.3', 'var 1.0', 'mode None',
  'log 1')
case('x_bias_none', 'biasCorrection None on the canonical split: fillLevels surface without the log renormalization',
  'spec 0 1 1 0 1 1', 'theta -0.6', 'btheta -0.3', 'var 1.0', 'mode None',
  'bias None')
case('x_srcE', 'src E (planted spikes on A): endpoint behaviour through Burman',
  'src E', 'spec 0 1 1 0 1 1', 'theta -0.6', 'btheta -0.3', 'var 1.0',
  'mode None')
case('x_srcC', 'src C (no seasonality in the data, airline-shaped model): extraction',
  'src C', 'spec 0 1 1 0 1 1', 'theta -0.6', 'btheta -0.3', 'var 1.0',
  'mode None')
case('x_n72', 'n=60 with forecastCount -4 (horizon above the period): extraction',
  'n 60', 'spec 0 1 1 0 1 1', 'theta -0.6', 'btheta -0.3', 'var 1.0',
  'mode None', 'forecastcount -4')

# -- corpus files --------------------------------------------------------------
wcsv <- function(name, df) {
  con <- file(file.path(output, name), open = 'wt')
  write.table(df, con, sep = ',', qmethod = 'double', row.names = FALSE,
    col.names = TRUE, na = '')
  close(con)
}
wcsv('cases.csv', do.call(rbind, summaries))
wcsv('inputs.csv', do.call(rbind, inputs))
wcsv('model-facts.csv', do.call(rbind, facts))
wcsv('model-events.csv', do.call(rbind, events))

writeLines(c(
  paste0('# metadata for the item 24-C seats-wk-extraction capture ',
    '(generated by validation/reference/capture-seats-wk-extraction.R)'),
  paste0('r_version <- "', paste(R.version$major, R.version$minor, sep = '.'), '"'),
  paste0('jvm_version <- "', .jcall('java/lang/System', 'S', 'getProperty', 'java.version'), '"'),
  paste0('jvm_vendor <- "', .jcall('java/lang/System', 'S', 'getProperty', 'java.vendor'), '"'),
  paste0('java_options <- "', Sys.getenv('_JAVA_OPTIONS'), '"'),
  paste0('capture_time <- "', format(Sys.time(), tz = 'UTC'), '"'),
  paste0('capture_host <- "', Sys.info()[['nodename']], '"'),
  paste0('capture_user <- "', Sys.info()[['user']], '"'),
  paste0('probe_sha256 <- "', reference_sha256(file.path(root,
    'validation/reference/SeatsWkExtractionProbe.java')), '"'),
  paste0('script_sha256 <- "', reference_sha256(file.path(root,
    'validation/reference/capture-seats-wk-extraction.R')), '"'),
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
