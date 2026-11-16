# Pinned-reference capture for item 22c (X-13 deterministic-effect allocation
# staging): the fixed-coefficient preadjust/restore seam of the pinned
# X13Kernel (pinned rjd3 3.9.0 jars). Each case is captured by running the
# pinned kernel itself (X13Kernel.of(spec, ctx).process) on a complete,
# fully specified fixed-coefficient regression model (a zero ARIMA-differencing
# specification still runs the kernel's own ARIMA estimation, but every
# captured table here depends only on the input, the regression columns and
# the given coefficients), and by re-deriving the stored a-tables, `out` and
# the finals through the pinned public pieces (GeneralLinearModel.
# deterministicEffect and backTransform, RegArimaDecomposer.deterministicEffect
# buckets, TsData add/subtract/divide folds, X11Utility.correctSeries) in the
# exact forms transcribed from the kernel bytecode. Every recomposition is
# compared bit-exactly against the kernel's own tables and the capture refuses
# to write on any disagreement, on any removed variable, or on a mode-forcing
# rule violation (non-log models are forced Additive, log models Multiplicative
# unless LogAdditive or PseudoAdditive is requested). The X11 decomposition
# tables (D10..D13, C17) are captured as stored in X13Results. No production
# output and no old corpus is input.
args <- commandArgs(trailingOnly = FALSE)
script <- sub('^--file=', '', args[startsWith(args, '--file=')][[1L]])
tooling <- dirname(normalizePath(script)); source(file.path(tooling, 'common.R'))
root <- reference_root()
args <- commandArgs(trailingOnly = TRUE)
case_filter <- NULL
if (length(args) && startsWith(args[[1L]], '--case=')) {
  case_filter <- sub('^--case=', '', args[[1L]]); args <- args[-1L]
}
if (length(args) != 1L || !startsWith(args, '--output='))
  stop('Usage: capture-x13-allocation.R [--case=<id>] --output=<existing empty directory>')
output <- normalizePath(sub('^--output=', '', args), mustWork = TRUE)
if (!dir.exists(output) || length(list.files(output, all.files = TRUE, no.. = TRUE)))
  stop('Output must be empty')
suppressWarnings(suppressMessages({
  library(rjd3jars); library(rjd3toolkit); library(rjd3x13); library(rJava)
}))
.jinit()
# Compile the independently authored public-API observer against the pinned
# jars. It builds each spec, runs the pinned kernel and recomposes every
# stored table from the pinned pieces; it implements no allocation algorithm.
probe <- tempfile('kalos-x13-allocation-'); dir.create(probe)
jars <- c(list.files(system.file('java', package = 'rjd3jars'), pattern = '[.]jar$', full.names = TRUE),
          list.files(system.file('java', package = 'rjd3x13'), pattern = '[.]jar$', full.names = TRUE))
classpath <- paste(jars, collapse = .Platform$path.sep)
javac <- Sys.which('javac'); if (!nzchar(javac)) stop('javac is required on PATH')
status <- system2(javac, c('-d', shQuote(probe), '-cp', shQuote(classpath),
  shQuote(file.path(tooling, 'X13AllocationProbe.java'))))
if (status != 0L) stop('probe compilation failed')
.jaddClassPath(probe)
fmtv <- function(v) paste(sprintf('%.17g', v), collapse = ';')
base_series <- function(n, f, seed) {
  set.seed(seed)
  as.numeric(200 + 5 * sin(2 * pi * (seq_len(n) %% f) / f) +
    40 * cumsum(rnorm(n, 0, .03)) + rnorm(n))
}
user_columns <- function(n, f, seed) {
  set.seed(seed)
  sa <- numeric(n); sa[max(1, n %/% 3L)] <- 1; sa[max(1, n %/% 3L + 1L)] <- -1
  list(sa = sa, cal = rnorm(n), ser = as.numeric((seq_len(n) - 1L) %% f == 0L),
    tr = seq(0, 1, length.out = n), un = rnorm(n))
}
td_coefficients <- c(0.1, -0.05, 0.02, 0.03, -0.01, 0.04) # TD7 spans 6 columns
# Fixed outlier mix: AO at position 9 into the trend bucket, LS at 40% into
# seasonal, AO at 70% into irregular, AO into Undefined (contributes to no
# a-table: only user variables carry the Undefined bucket).
outlier_mix <- function(n) c(
  paste0('outlier AO|', round(0.09 * n), '|Trend|0.5'),
  paste0('outlier LS|', round(0.4 * n), '|Seasonal|-0.8'),
  paste0('outlier AO|', round(0.7 * n), '|Irregular|-2.1'),
  paste0('outlier AO|', round(0.83 * n), '|Undefined|0.25'))
user_mix <- function(id, cols) c(
  paste('user', paste0('sa|SeasonallyAdjusted|0.3|', fmtv(cols$sa)), sep = ' '),
  paste('user', paste0('cal|CalendarEffect|0.15|', fmtv(cols$cal)), sep = ' '),
  paste('user', paste0('ser|Series|0.2|', fmtv(cols$ser)), sep = ' '),
  paste('user', paste0('tr|Trend|0.45|', fmtv(cols$tr)), sep = ' '),
  paste('user', paste0('un|Undefined|0.12|', fmtv(cols$un)), sep = ' '))
cases <- list(
  list(id = 'q_bare', freq = 4L, n = 96L, log = FALSE, mode = 'Additive',
    expected = 'Additive', full = FALSE),
  list(id = 'q_alloc', freq = 4L, n = 96L, log = FALSE, mode = 'Additive',
    expected = 'Additive', full = TRUE),
  # A Multiplicative request on a non-log model is forced Additive.
  list(id = 'q_nonlog_mult', freq = 4L, n = 96L, log = FALSE, mode = 'Multiplicative',
    expected = 'Additive', full = TRUE),
  # A log model forces the Multiplicative X-11 mode.
  list(id = 'q_log', freq = 4L, n = 96L, log = TRUE, mode = 'Additive',
    expected = 'Multiplicative', full = TRUE),
  list(id = 'q_logla', freq = 4L, n = 96L, log = TRUE, mode = 'LogAdditive',
    expected = 'LogAdditive', full = TRUE),
  list(id = 'q_logpa', freq = 4L, n = 96L, log = TRUE, mode = 'PseudoAdditive',
    expected = 'PseudoAdditive', full = TRUE),
  list(id = 'm_alloc', freq = 12L, n = 120L, log = FALSE, mode = 'Additive',
    expected = 'Additive', full = TRUE))
if (!is.null(case_filter))
  cases <- Filter(function(ca) identical(ca$id, case_filter), cases)
case_rows <- list(); input_rows <- list(); var_rows <- list(); column_rows <- list()
table_rows <- list(); log_rows <- list(); checks_done <- 0L
table_names <- c('a1', 'a6', 'a7', 'a8', 'a8t', 'a8s', 'a8i', 'a9', 'a9u', 'a9cal',
  'a9sa', 'a9ser', 'a1a', 'a1b', 'out', 'd10', 'd11', 'd12', 'd13', 'c17',
  'd11final', 'd12final', 'd13final', 'd16', 'd18', 'e1', 'e2', 'e3', 'e11', 'd11a')
for (ca in cases) {
  values <- base_series(ca$n, ca$freq, seed = 20261116L)
  cols <- user_columns(ca$n, ca$freq, seed = 20261117L)
  lines <- c(sprintf('id %s', ca$id), sprintf('freq %d', ca$freq), 'start 2000:1',
    paste('values', fmtv(values)), sprintf('log %s', tolower(as.character(ca$log))),
    sprintf('mode %s', ca$mode), sprintf('filters %s', paste(rep(9L, ca$freq), collapse = ',')),
    'henderson 5', 'sigma 1.5;2.5', 'mean unused', 'arima 1;1',
    if (ca$full) paste('td fixed:', paste(td_coefficients, collapse = ',')) else 'td off',
    if (ca$full) 'easter 0.35' else 'easter off', 'easterdur 2',
    if (ca$full) outlier_mix(ca$n) else character(),
    if (ca$full) user_mix(ca$id, cols) else character())
  raw <- .jcall('X13AllocationProbe', 'Ljava/lang/String;', 'run', .jarray(lines))
  parsed <- strsplit(raw, '\n', fixed = TRUE)[[1L]]
  keys <- sub(' .*$', '', parsed)
  if ('error' %in% keys) stop('probe error for ', ca$id, ':\n', paste(parsed, collapse = '\n'))
  field <- function(k) { v <- parsed[keys == k]; if (!length(v)) NULL else sub('^[^ ]+ ', '', v[[1L]]) }
  if (!identical(field('status'), 'ok')) stop('kernel refused case ', ca$id)
  logs <- parsed[keys == 'log']
  if (any(grepl('removed from the model', logs)))
    stop('a variable was removed from the model in ', ca$id)
  effective <- field('mode_effective')
  if (!identical(effective, ca$expected))
    stop('mode forcing violated for ', ca$id, ': ', effective, ' != ', ca$expected)
  checklines <- parsed[keys == 'check']
  notok <- checklines[!grepl('^check [^ ]+ ok$', checklines)]
  if (length(checklines) != 22L || length(notok))
    stop('recomposition disagreement in ', ca$id, ':\n', paste(notok, collapse = '\n'))
  checks_done <- checks_done + length(checklines)
  # Removed-variable guard also through the model variable count.
  varlines <- parsed[keys == 'var']
  expected_vars <- if (ca$full) 11L else 0L
  if (length(varlines) != expected_vars)
    stop('variable count ', length(varlines), ' != ', expected_vars, ' for ', ca$id)
  for (vl in varlines) {
    # var idx name class effect flags coefs columns
    parts <- strsplit(vl, ' ', fixed = TRUE)[[1L]]
    flags <- as.integer(sub('^.*=', '', strsplit(parts[[6]], ',', fixed = TRUE)[[1L]]))
    coefs <- strsplit(parts[[7]], ',', fixed = TRUE)[[1L]]
    kind <- sub(':.*$', '', coefs); value <- as.numeric(sub('^[a-z]+:', '', coefs))
    colvals <- as.numeric(strsplit(parts[[8]], ';', fixed = TRUE)[[1L]])
    stopifnot(length(colvals) == length(coefs) * ca$n)
    mat <- matrix(colvals, nrow = length(coefs), byrow = TRUE)
    for (j in seq_along(coefs))
      var_rows[[length(var_rows) + 1L]] <- data.frame(
        case = ca$id, index = as.integer(parts[[2]]), name = parts[[3]],
        class = parts[[4]], effect = parts[[5]],
        outlier = flags[[1]], days = flags[[2]],
        holiday = flags[[3]], user = flags[[4]],
        coefficient = value[[j]], fixed = kind[[j]] == 'fixed',
        stringsAsFactors = FALSE)
    for (j in seq_len(nrow(mat)))
      column_rows[[length(column_rows) + 1L]] <- data.frame(
        case = ca$id, index = as.integer(parts[[2]]), coef = j - 1L,
        row = seq_len(ca$n) - 1L, value = mat[j, ], stringsAsFactors = FALSE)
  }
  tables <- list()
  for (tn in table_names) {
    v <- field(tn)
    if (is.null(v)) stop('table ', tn, ' missing for ', ca$id)
    if (!nzchar(v) || identical(v, 'null')) next
    value <- as.numeric(strsplit(v, ';', fixed = TRUE)[[1L]])
    tables[[tn]] <- value
    table_rows[[length(table_rows) + 1L]] <- data.frame(case = ca$id, table = tn,
      row = seq_along(value) - 1L, value = value, stringsAsFactors = FALSE)
  }
  # The out table spans the (unextended) calendar and must equal the
  # interpolation minus the allocation; the X11 tables share its domain.
  stopifnot(length(tables$out) == ca$n, length(tables$d10) == ca$n,
    length(tables$a1) == ca$n, length(tables$c17) == ca$n, length(tables$e1) == ca$n)
  for (i in seq_along(logs))
    log_rows[[length(log_rows) + 1L]] <- data.frame(case = ca$id,
      index = i - 1L, message = sub('^log ', '', logs[[i]]), stringsAsFactors = FALSE)
  input_rows[[length(input_rows) + 1L]] <- data.frame(case = ca$id,
    row = seq_along(values) - 1L, value = values, stringsAsFactors = FALSE)
  case_rows[[length(case_rows) + 1L]] <- data.frame(
    case = ca$id, frequency = ca$freq, first_period = 1L, n = ca$n,
    log = ca$log, mode_requested = ca$mode, mode_effective = effective,
    full = ca$full, checks = length(checklines), stringsAsFactors = FALSE)
}
fixtures_fmt <- function(d) {
  data.frame(lapply(d, function(col) if (is.numeric(col)) sprintf('%.17g', col) else col),
    stringsAsFactors = FALSE, check.names = FALSE)
}
write.csv(fixtures_fmt(do.call(rbind, case_rows)), file.path(output, 'cases.csv'),
  row.names = FALSE, quote = TRUE)
write.csv(fixtures_fmt(do.call(rbind, input_rows)), file.path(output, 'inputs.csv'),
  row.names = FALSE, quote = TRUE)
write.csv(fixtures_fmt(do.call(rbind, var_rows)), file.path(output, 'vars.csv'),
  row.names = FALSE, quote = TRUE)
write.csv(fixtures_fmt(do.call(rbind, column_rows)), file.path(output, 'columns.csv'),
  row.names = FALSE, quote = TRUE)
write.csv(fixtures_fmt(do.call(rbind, table_rows)), file.path(output, 'tables.csv'),
  row.names = FALSE, quote = TRUE)
write.csv(fixtures_fmt(do.call(rbind, log_rows)), file.path(output, 'logs.csv'),
  row.names = FALSE, quote = TRUE)
java <- list(version = .jcall('java/lang/System', 'Ljava/lang/String;', 'getProperty', 'java.version'),
  vendor = .jcall('java/lang/System', 'Ljava/lang/String;', 'getProperty', 'java.vendor'),
  runtime_version = .jcall('java/lang/System', 'Ljava/lang/String;', 'getProperty', 'java.runtime.version'),
  vm_name = .jcall('java/lang/System', 'Ljava/lang/String;', 'getProperty', 'java.vm.name'),
  architecture = .jcall('java/lang/System', 'Ljava/lang/String;', 'getProperty', 'os.arch'))
files <- c(file.path(tooling, c('capture-x13-allocation.R', 'X13AllocationProbe.java')))
metadata <- list(schema_version = 1L,
  captured_utc = format(Sys.time(), tz = 'UTC', usetz = TRUE),
  source_sha256 = setNames(reference_sha256(files), basename(files)),
  java = java, R = R.version.string, cases = nrow(do.call(rbind, case_rows)),
  public_scope = paste(
    'Pinned X13Kernel deterministic-effect allocation staging only:',
    'X13Kernel.of(spec, ModellingContext.getActiveContext()).process(TsData, DefaultProcessingLog)',
    'on fully specified fixed-coefficient regression models (TD7 with fixed coefficients, fixed Easter,',
    'AO/LS outliers with fixed coefficients spanning the Trend/Seasonal/Irregular/Undefined regeffect buckets,',
    'user variables with fixed coefficients spanning the Trend/Seasonal/Irregular/SeasonallyAdjusted/',
    'Series/CalendarEffect/Undefined buckets) with zero backcast/forecast horizons and',
    'LengthOfPeriodType.None, so the kernel never touches ARIMA extension forecasts or missing-value',
    'interpolation; a1 = interpolatedSeries(false), the a6/a7 predicate buckets, the a8/a8t/a8s/a8i outlier+',
    'user ComponentType buckets and the a9/a9cal/a9sa/a9ser/a9u user buckets as stored by initialStep',
    '(backTransform-ed: exp under a log model), out = the concatenated calendar minus (additive) or divided',
    '(log) by backTransform(sum-of-buckets), the updateSpec mode forcing (non-log -> Additive, log ->',
    'Multiplicative unless LogAdditive or PseudoAdditive is requested), the X11 D10..D13/C17 as stored in',
    'X13Results for that out series, and the finals restore (d11final/d12final/d13final/d16/d18/e1/e2/e3/e11)',
    'with the pinned op (divide only for Multiplicative/PseudoAdditive), invOp (multiply for',
    'Multiplicative/LogAdditive/PseudoAdditive via isMultiplicative() || PseudoAdditive, add only for',
    'Additive) and mean (0 only for Additive) truth tables and the X11Utility.correctSeries c17 rule.',
    'Every stored table is recomposed bit-exactly from the pinned GeneralLinearModel.deterministicEffect,',
    'RegArimaDecomposer.deterministicEffect, backTransform, TsData folds and correctSeries pieces',
    '(22 checks per case) and the capture refuses to write on any disagreement. Coefficient estimation,',
    'ARIMA backcast/forecast extension, missing-value interpolation, LengthOfPeriod transforms,',
    'benchmarking, diagnostics, cross-validation and the period-specific corrector are not claimed.',
    'Schema: cases.csv one row per case; vars.csv one row per regression coefficient of the model',
    'variables in pinned model-description order (multi-column variables repeat their identity rows),',
    'with the pinned class flags (outlier/days/holiday/user), the EXACT regeffect bucket and the',
    'coefficient as given; columns.csv (case, index, coef, row, value) the pinned regression columns;',
    'tables.csv (case, table, row, value) every stored table (a1a/a1b absent under zero horizons).'))
writeLines(sub('[[:blank:]]+$', '', capture.output(dput(metadata))), file.path(output, 'metadata.R'))
captured <- file.path(output, c('cases.csv', 'inputs.csv', 'vars.csv', 'columns.csv',
  'tables.csv', 'logs.csv', 'metadata.R'))
write.csv(data.frame(file = basename(captured), sha256 = reference_sha256(captured)),
  file.path(output, 'SHA256SUMS.csv'), row.names = FALSE)
cat('captured', length(case_rows), 'cases,', checks_done, 'checks\n')
