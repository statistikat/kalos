# Pinned-reference capture for item 24-A: SEATS model validation and
# decomposition of the pinned jdplus 3.9.0 stack (rjd3tramoseats), observed
# through the public-API observer TramoSeatsModelProbe.java. The observer
# drives the LIVE pipeline exactly as SeatsKernel does - SeatsToolkit.of(spec),
# the toolkit-owned DefaultModelValidator at the SeatsKernel.validate seam,
# DefaultModelDecomposer + the raw selector chain (the kalos 24-A unit scope),
# and the full SeatsKernel.process - reporting every fact through getters
# only: raw doubles (%.17g, never a display toString), enum constant names,
# ordered ProcessingLog events with kind/depth/message/details (pinning notes
# 6.1/6.2/7). "r" facts are the raw selector-chain decomposition (before
# setVarianceMax) - the golden-assert scope of the unit; "d" facts are the
# toolkit decomposer result and "k" facts the kernel surface, whose component
# variances are 24-B-preview evidence (variance allocation is 24-B) and are
# not golden-asserted. The series is built inside the probe with StrictMath
# only (input bits intrinsic-independent) and the G2 recipe below pins the JVM
# anyway. Failure statuses (SeatsException "Invalid decomposition" under
# mode None, the DefaultModelApproximator MatrixException crash, the Noisy
# NullPointerException) are captured as data: they pin the live boundary the
# unit envelope must respect. Pass --repro=<dir> to cross-check a capture
# against an earlier one from an independent JVM process (byte-identity gate,
# final-information precedent).
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
  stop('Usage: capture-tramo-seats-model.R [--repro=<captured directory>] --output=<existing empty directory>')
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
probe <- tempfile('kalos-tramo-seats-model-'); dir.create(probe)
jars <- c(list.files(system.file('java', package = 'rjd3jars'), pattern = '[.]jar$',
  full.names = TRUE),
  list.files(system.file('java', package = 'rjd3toolkit'), pattern = '[.]jar$',
    full.names = TRUE),
  list.files(system.file('java', package = 'rjd3tramoseats'), pattern = '[.]jar$',
    full.names = TRUE))
classpath <- paste(jars, collapse = .Platform$path.sep)
status <- system2(javac, c('--release', jvm_major, '-d', shQuote(probe),
  '-cp', shQuote(classpath),
  shQuote(file.path(tooling, 'TramoSeatsModelProbe.java'))))
if (status != 0L) stop('probe compilation failed')
.jaddClassPath(c(jars[grep('tramoseats', jars)], probe))
if (inherits(try(.jfindClass('TramoSeatsModelProbe'), silent = TRUE), 'try-error'))
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
  res <- .jcall('TramoSeatsModelProbe', 'S', 'run', .jarray(lines))
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
  summaries[[length(summaries) + 1L]] <<- data.frame(
    id = id, note = note, freq = freq, start = start, n = n,
    status = st,
    xl = knobs[1L], mode = knobs[6L],
    vchanged = sub('^v.changed ', '',
      out[startsWith(out, 'v.changed ')][[1L]]),
    vorders = fact('v.orders'),
    ddecompose = fact('d.decompose'), rdecompose = fact('r.decompose'),
    rcount = fact('r.count'),
    kcut = fact('k.cutOff'), kchanged = fact('k.modelChanged'),
    kevts = length(grep('^kevt ', out)),
    stringsAsFactors = FALSE)
  invisible(NULL)
}

# -- corpus -------------------------------------------------------------------
case('v_airline', 'classical airline frozen Fixed (0.5/0.4); template echoed (theta/btheta 0.0 Undefined); Legacy approximates (not decomposable)',
  'specname airline', 'var 0.5')
case('v_tpl_zero', 'airline orders with zero MA parameters: simplifyModel trims both to (0,1,0)(0,1,0), cutOff',
  'spec 0 1 1 0 1 1', 'theta 0', 'btheta 0', 'var 1.0', 'mode None')
case('v_in_xl', 'MA inside the XL ball (|theta| < 0.95): validator no-op',
  'spec 0 1 1 0 1 1', 'theta -0.9', 'btheta 0.8', 'var 1.0')
case('v_cut_neg', 'negative MA beyond XL: -0.96/-0.98 clamped to -0.95/-0.95 (c0 var 6.0055801870305150e-4 cross-checks the m5 fixture)',
  'spec 0 1 1 0 1 1', 'theta -0.96', 'btheta -0.98', 'var 1.0', 'mode None')
case('v_cut_pos', 'positive MA beyond XL: 0.97 clamped to 0.95',
  'spec 0 1 1 0 1 1', 'theta 0.97', 'btheta 0.5', 'var 1.0')
case('v_cut_bq2', 'bq=2 inside XL: both btheta kept (canSimplify |last| >= 1e-6)',
  'spec 0 1 0 0 1 2', 'btheta 0.6 0.45', 'var 1.0', 'mode None')
case('v_stab_q2', 'degenerate q=2 (root 1): stabilizeMA lands (-1.8818425868650992, 0.9024999999999995)',
  'spec 0 1 2 0 1 1', 'theta -1.9 0.92', 'btheta 0.4', 'var 1.0')
case('v_simplify', 'tiny parameters trimmed by simplify (p 1->0, q 2->1, bp 2->1) with cutOff',
  'spec 1 1 2 2 1 1', 'phi 2e-7', 'theta 0.5 5e-7', 'bphi 0.3 -1e-9',
  'btheta -0.5', 'var 1.0', 'mode None')
case('v_fixma_pos', 'fixMa deg-1 quirk: +0.99995 lands -1.0; kernel then hits the DefaultModelApproximator MatrixException (record-only)',
  'xl 1.0', 'spec 0 1 1 0 1 1', 'theta 0.99995', 'btheta 0.5', 'var 1.0')
case('v_fixma_neg', 'fixMa deg-1 quirk: -0.99995 also lands -1.0; MatrixException recorded',
  'xl 1.0', 'spec 0 1 1 0 1 1', 'theta -0.99995', 'btheta 0.5', 'var 1.0')
case('v_fixma_btheta', 'fixMa on btheta: 0.9995 lands +1.0 (mirror-root branch)',
  'xl 1.0', 'spec 0 1 1 0 1 1', 'theta 0.5', 'btheta 0.9995', 'var 1.0')
case('v_fixma_below', 'theta just below the 1-eps threshold: validator untouched, Legacy approximates downstream',
  'xl 1.0', 'spec 0 1 1 0 1 1', 'theta 0.99985', 'btheta 0.5', 'var 1.0')
case('v_fixma_q2proj', 'q=2 with root |r| < 1-eps: unit-circle projection without constant renormalization',
  'xl 1.0', 'spec 0 1 2 0 1 1', 'theta -1.25020002500125 0.2500500125015629',
  'btheta 0.5', 'var 1.0')
case('d_trend_ma', 'trend-only MA (no seasonal differencing): seasonal selector with bd+bp=0 (k=seasBoundaryAtPi)',
  'spec 0 1 1 0 0 0', 'theta 0.5', 'var 1.0', 'mode None')
case('d_wn', 'pure white noise: RootDecomposer checkSpecialCases path, zero signal sfd',
  'spec 0 0 0 0 0 0', 'var 2.0', 'mode None')
case('d_seasonal', 'no trend differencing, seasonal (0,0,1)(0,1,1): signal/noise split without D1',
  'spec 0 0 1 0 1 1', 'theta 0.2', 'btheta -0.6', 'var 0.3', 'mode None')
case('d_ar1_slow', 'AR root |z| >= bound accepted into trend-cycle (TrendCycleSelector accept, exact comparison)',
  'spec 1 1 0 0 1 0', 'phi 0.7', 'var 1.0', 'mode None')
case('d_ar1_rej', 'same model with trendBoundary 0.8: root rejected -> separate component',
  'spec 1 1 0 0 1 0', 'phi 0.7', 'rmod 0.8', 'var 1.0', 'mode None')
case('d_ar2', 'quadratic stationary AR (closed-form roots path in the selector)',
  'spec 2 1 0 0 1 0', 'phi 0.5 -0.06', 'var 1.0', 'mode None')
case('d_d2', 'double unit root D1^2: exact unit-root split degree 2 trend polynomial',
  'spec 0 2 1 0 1 1', 'theta -0.95', 'btheta 0.9', 'var 1.0')
case('d_stab_land', 'the stabilizeMA landing point as input: decomposition of the (1-B)(1+1.88B)(1-0.9B^12)-shaped model',
  'spec 0 1 1 0 1 1', 'theta -1.8818425868650992', 'btheta 0.9024999999999995',
  'var 1.0')
case('d_airline_var0', 'frozen airline with innovationVariance 0: variance passthrough pin',
  'specname airline', 'var 0.0')
case('d_log', 'log-transform flag with frozen airline: model path unaffected by log (series-free validator scope)',
  'specname airline', 'var 0.0', 'log 1')
case('d_srcC', 'series without seasonality (airline model fixed): seasonality flag + decomposition structure',
  'specname airline', 'var 0.5', 'src C')
case('d_mean_off', 'meanCorrection off with frozen airline',
  'specname airline', 'var 0.5', 'mean 0')
case('d_fail_wn_unit', 'non-invertible WN theta=1.0: XL cut to 0.95 then decomposable',
  'spec 0 0 1 0 0 0', 'theta 1.0', 'var 1.0', 'mode None')
case('d_fail_wn_unit_legacy', 'same under Legacy mode',
  'spec 0 0 1 0 0 0', 'theta 1.0', 'var 1.0', 'mode Legacy')
case('d_fail_airline_nod', 'theta=1 btheta=-1 without differencing: cuts to +-0.95, seasonal differencing split',
  'spec 0 0 1 0 1 1', 'theta 1.0', 'btheta -1.0', 'var 1.0', 'mode None')
case('d_fail_airline_nod_legacy', 'same under Legacy mode',
  'spec 0 0 1 0 1 1', 'theta 1.0', 'btheta -1.0', 'var 1.0', 'mode Legacy')
case('d_nondec_none', 'canonical (0,1,1)(0,1,1) -0.9/0.8 is NOT decomposable (noise sfd -0.72): SeatsException Invalid decomposition + warning non decomposable + error canonical decomposition failed',
  'spec 0 1 1 0 1 1', 'theta -0.9', 'btheta 0.8', 'var 1.0', 'mode None')
case('d_nondec_legacy', 'same model under Legacy: silent approximation rewrite (modelChanged 1, remark model replaced by an approximation, app parameters visible in k.ucar model)',
  'spec 0 1 1 0 1 1', 'theta -0.9', 'btheta 0.8', 'var 1.0', 'mode Legacy')
case('d_nondec_noisy', 'same model under Noisy: live NPE (values is marked non-null) after the noisy-model remark - envelope edge, record-only',
  'spec 0 1 1 0 1 1', 'theta -0.9', 'btheta 0.8', 'var 1.0', 'mode Noisy')
case('d_cut_then_decomp', 'cut then decompose interaction on the B series (n=120): -1.2/-0.98 clamped, c0 var 6.0055801870305150e-4',
  'spec 0 1 1 0 1 1', 'theta -1.2', 'btheta -0.98', 'var 1.0', 'mode None',
  'src B', 'n 120')
case('q_airline', 'quarterly period-4 airline -0.9/0.7: S(4,1) seasonal unit roots, non-decomposable under None',
  'freq 4', 'period 4', 'spec 0 1 1 0 1 1', 'theta -0.9', 'btheta 0.7',
  'var 1.0', 'mode None')
case('q_d2', 'quarterly d=2 with 0.95/0.9: double unit root + seasonal, non-decomposable under None, raw chain ArimaException on getMa (record)',
  'freq 4', 'period 4', 'spec 0 2 1 0 1 1', 'theta -0.95', 'btheta 0.9',
  'var 1.0', 'mode None')

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
  paste0('# metadata for the item 24-A tramo-seats-model capture ',
    '(generated by validation/reference/capture-tramo-seats-model.R)'),
  paste0('r_version <- "', paste(R.version$major, R.version$minor, sep = '.'), '"'),
  paste0('jvm_version <- "', .jcall('java/lang/System', 'S', 'getProperty', 'java.version'), '"'),
  paste0('jvm_vendor <- "', .jcall('java/lang/System', 'S', 'getProperty', 'java.vendor'), '"'),
  paste0('java_options <- "', Sys.getenv('_JAVA_OPTIONS'), '"'),
  paste0('capture_time <- "', format(Sys.time(), tz = 'UTC'), '"'),
  paste0('capture_host <- "', Sys.info()[['nodename']], '"'),
  paste0('capture_user <- "', Sys.info()[['user']], '"'),
  paste0('probe_sha256 <- "', reference_sha256(file.path(root,
    'validation/reference/TramoSeatsModelProbe.java')), '"'),
  paste0('script_sha256 <- "', reference_sha256(file.path(root,
    'validation/reference/capture-tramo-seats-model.R')), '"'),
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
