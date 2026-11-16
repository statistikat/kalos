# Pinned-reference capture for item 22c M3-finale (restricted fixed-model
# X-13 fit): monthly, fully fixed X13Specs executed end to end through the
# pinned FAST processing entry point jdplus.x13.base.r.X13.process(ts, spec,
# ctx) — exactly what rjd3x13::x13_fast calls — with every regression
# coefficient fixed (the probe refuses any free parameter, so the estimation
# cursor is never read) and zero backcast/forecast horizons (the probe
# asserts a1a/a1b null and the unextended out calendar, i.e. the fixed path
# the fit represents). Every stored table is recomposed bit-exactly from the
# pinned pieces (GeneralLinearModel.deterministicEffect and backTransform,
# RegArimaDecomposer.deterministicEffect buckets, TsData folds,
# X11Utility.correctSeries) and the capture refuses to write on any
# disagreement, variable removal, or mode-forcing violation. Two evidence
# cases run the same fixed specs with nonzero horizons and record only the
# deterministic route facts (a1a/a1b non-null, the extended out calendar,
# the split finals) proving the pinned engine leaves the fixed path there —
# the delivered fit answers the explicit unsupported status instead. One
# case is additionally re-executed through the R x13_fast result transform
# (R ts marshalling, the pinned process call and the proto round trip) and
# compared bit-exactly against the probe tables (x13fast.csv). No production
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
  stop('Usage: capture-x13-fit.R [--case=<id>] --output=<existing empty directory>')
output <- normalizePath(sub('^--output=', '', args), mustWork = TRUE)
if (!dir.exists(output) || length(list.files(output, all.files = TRUE, no.. = TRUE)))
  stop('Output must be empty')
suppressWarnings(suppressMessages({
  library(rjd3jars); library(rjd3toolkit); library(rjd3x13); library(rJava)
}))
.jinit()
# Compile the independently authored public-API observer against the pinned
# jars. It builds each spec, runs the pinned fast entry and recomposes every
# stored table from the pinned pieces; it implements no fit algorithm.
probe <- tempfile('kalos-x13-fit-'); dir.create(probe)
jars <- c(list.files(system.file('java', package = 'rjd3jars'), pattern = '[.]jar$', full.names = TRUE),
          list.files(system.file('java', package = 'rjd3x13'), pattern = '[.]jar$', full.names = TRUE))
classpath <- paste(jars, collapse = .Platform$path.sep)
javac <- Sys.which('javac'); if (!nzchar(javac)) stop('javac is required on PATH')
status <- system2(javac, c('-d', shQuote(probe), '-cp', shQuote(classpath),
  shQuote(file.path(tooling, 'X13FitProbe.java'))))
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
# The monthly fixed-specification corpus. `model`: full (TD7 + Easter + the
# four-outlier mix + the five-bucket user mix = 11 variables), users (the
# five user variables), td (TD7 only), empty. filters is the per-period
# fixed SeasonalFilterOption request code. route: fixed = zero-horizon
# tables captured; arima = nonzero-horizon route evidence only.
cases <- list(
  list(id = 'f_add_full', n = 120L, log = FALSE, mode = 'Additive', expected = 'Additive',
    model = 'full', filters = rep(9L, 12L), henderson = 13L, lower = 1.5, upper = 2.5,
    bc = 0L, fc = 0L, route = 'fixed'),
  list(id = 'f_add_s3x1', n = 96L, log = FALSE, mode = 'Additive', expected = 'Additive',
    model = 'users', filters = rep(1L, 12L), henderson = 5L, lower = 2., upper = 3.,
    bc = 0L, fc = 0L, route = 'fixed'),
  list(id = 'f_add_s3x15', n = 120L, log = FALSE, mode = 'Additive', expected = 'Additive',
    model = 'full', filters = rep(15L, 12L), henderson = 13L, lower = 1.5, upper = 2.5,
    bc = 0L, fc = 0L, route = 'fixed'),
  list(id = 'f_add_stable', n = 96L, log = FALSE, mode = 'Additive', expected = 'Additive',
    model = 'td', filters = rep(0L, 12L), henderson = 5L, lower = 1.5, upper = 2.5,
    bc = 0L, fc = 0L, route = 'fixed'),
  # A Multiplicative request on a non-log model is forced Additive.
  list(id = 'f_mult_forced', n = 96L, log = FALSE, mode = 'Multiplicative', expected = 'Additive',
    model = 'full', filters = rep(9L, 12L), henderson = 5L, lower = 1.5, upper = 2.5,
    bc = 0L, fc = 0L, route = 'fixed'),
  # A log model forces Multiplicative; LogAdditive and PseudoAdditive stay.
  list(id = 'f_log', n = 120L, log = TRUE, mode = 'Additive', expected = 'Multiplicative',
    model = 'full', filters = rep(9L, 12L), henderson = 13L, lower = 1.5, upper = 2.5,
    bc = 0L, fc = 0L, route = 'fixed'),
  list(id = 'f_logla', n = 96L, log = TRUE, mode = 'LogAdditive', expected = 'LogAdditive',
    model = 'full', filters = rep(9L, 12L), henderson = 5L, lower = 1.5, upper = 2.5,
    bc = 0L, fc = 0L, route = 'fixed'),
  list(id = 'f_logpa', n = 120L, log = TRUE, mode = 'PseudoAdditive', expected = 'PseudoAdditive',
    model = 'full', filters = rep(9L, 12L), henderson = 13L, lower = 1.5, upper = 2.5,
    bc = 0L, fc = 0L, route = 'fixed'),
  list(id = 'f_empty', n = 96L, log = FALSE, mode = 'Additive', expected = 'Additive',
    model = 'empty', filters = rep(9L, 12L), henderson = 5L, lower = 1.5, upper = 2.5,
    bc = 0L, fc = 0L, route = 'fixed'),
  # Horizon evidence: the same fixed spec with the pinned nonzero horizons
  # leaves the fixed path (ExactArimaForecasts, a1a/a1b, extended out).
  list(id = 'f_h6', n = 120L, log = FALSE, mode = 'Additive', expected = 'Additive',
    model = 'full', filters = rep(9L, 12L), henderson = 13L, lower = 1.5, upper = 2.5,
    bc = 0L, fc = 6L, route = 'arima'),
  list(id = 'f_b3h3', n = 120L, log = FALSE, mode = 'Additive', expected = 'Additive',
    model = 'full', filters = rep(9L, 12L), henderson = 13L, lower = 1.5, upper = 2.5,
    bc = 3L, fc = 3L, route = 'arima'))
if (!is.null(case_filter))
  cases <- Filter(function(ca) identical(ca$id, case_filter), cases)
model_vars <- function(m) c(full = 11L, users = 5L, td = 1L, empty = 0L)[[m]]
case_rows <- list(); input_rows <- list(); var_rows <- list(); column_rows <- list()
table_rows <- list(); horizon_rows <- list(); x13fast_rows <- list(); checks_done <- 0L
table_names <- c('a1', 'a6', 'a7', 'a8', 'a8t', 'a8s', 'a8i', 'a9', 'a9u', 'a9cal',
  'a9sa', 'a9ser', 'out', 'd10', 'd11', 'd12', 'd13', 'c17',
  'd11final', 'd12final', 'd13final', 'd16', 'd18', 'e1', 'e2', 'e3', 'e11')
captured_tables <- list()
for (ca in cases) {
  values <- base_series(ca$n, 12L, seed = 20261116L)
  cols <- user_columns(ca$n, 12L, seed = 20261117L)
  if (ca$log) stopifnot(all(values > 0))
  lines <- c(sprintf('id %s', ca$id), 'freq 12', 'start 2000:1',
    paste('values', fmtv(values)), sprintf('log %s', tolower(as.character(ca$log))),
    sprintf('mode %s', ca$mode),
    sprintf('filters %s', paste(ca$filters, collapse = ',')),
    sprintf('henderson %d', ca$henderson),
    sprintf('sigma %.17g;%.17g', ca$lower, ca$upper),
    sprintf('horizons %d;%d', ca$bc, ca$fc),
    sprintf('expectedvars %d', model_vars(ca$model)),
    'mean unused', 'arima 1;1',
    if (ca$model %in% c('full', 'td')) paste('td fixed:', paste(td_coefficients, collapse = ',')) else 'td off',
    if (ca$model == 'full') 'easter 0.35' else 'easter off', 'easterdur 2',
    if (ca$model == 'full') outlier_mix(ca$n) else character(),
    if (ca$model %in% c('full', 'users')) user_mix(ca$id, cols) else character())
  raw <- .jcall('X13FitProbe', 'Ljava/lang/String;', 'run', .jarray(lines))
  parsed <- strsplit(raw, '\n', fixed = TRUE)[[1L]]
  keys <- sub(' .*$', '', parsed)
  if ('error' %in% keys) stop('probe error for ', ca$id, ':\n', paste(parsed, collapse = '\n'))
  field <- function(k) { v <- parsed[keys == k]; if (!length(v)) NULL else sub('^[^ ]+ ', '', v[[1L]]) }
  if (!identical(field('status'), 'ok')) stop('kernel refused case ', ca$id)
  effective <- field('mode_effective')
  if (!identical(effective, ca$expected))
    stop('mode forcing violated for ', ca$id, ': ', effective, ' != ', ca$expected)
  varlines <- parsed[keys == 'var']
  if (length(varlines) != model_vars(ca$model))
    stop('variable count ', length(varlines), ' != ', model_vars(ca$model), ' for ', ca$id)
  for (vl in varlines) {
    # var idx name class effect flags coefs columns
    parts <- strsplit(vl, ' ', fixed = TRUE)[[1L]]
    flags <- as.integer(sub('^.*=', '', strsplit(parts[[6]], ',', fixed = TRUE)[[1L]]))
    coefs <- strsplit(parts[[7]], ',', fixed = TRUE)[[1L]]
    kind <- sub(':.*$', '', coefs); value <- as.numeric(sub('^[a-z]+:', '', coefs))
    colvals <- as.numeric(strsplit(parts[[8]], ';', fixed = TRUE)[[1L]])
    stopifnot(all(kind == 'fixed'), length(colvals) == length(coefs) * ca$n)
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
  input_rows[[length(input_rows) + 1L]] <- data.frame(case = ca$id,
    row = seq_along(values) - 1L, value = values, stringsAsFactors = FALSE)
  if (identical(ca$route, 'arima')) {
    ev <- strsplit(field('evidence'), ' ', fixed = TRUE)[[1L]]
    stopifnot(!is.null(ev))
    num <- as.integer(ev[c(2L, 4L, 6L, 8L, 10L, 12L)])
    names(num) <- c('a1a_len', 'a1b_len', 'out_len', 'd11a_len', 'd11b_len', 'd11final_len')
    stopifnot(num[['a1a_len']] == (if (ca$fc > 0) ca$fc else -1L),
      num[['a1b_len']] == (if (ca$bc > 0) ca$bc else -1L),
      num[['out_len']] == ca$n + ca$bc + ca$fc,
      num[['d11a_len']] == (if (ca$fc > 0) ca$fc else -1L),
      num[['d11final_len']] == ca$n)
    horizon_rows[[length(horizon_rows) + 1L]] <- data.frame(case = ca$id, as.list(num),
      backcast = ca$bc, forecast = ca$fc, n = ca$n, stringsAsFactors = FALSE)
    case_rows[[length(case_rows) + 1L]] <- data.frame(
      case = ca$id, frequency = 12L, first_period = 1L, n = ca$n,
      log = ca$log, mode_requested = ca$mode, mode_effective = effective,
      filters = paste(ca$filters, collapse = ','), henderson = ca$henderson,
      lower = ca$lower, upper = ca$upper, backcast = ca$bc, forecast = ca$fc,
      route = ca$route, x13fast = FALSE, checks = 0L, stringsAsFactors = FALSE)
    next
  }
  checklines <- parsed[keys == 'check']
  notok <- checklines[!grepl('^check [^ ]+ ok$', checklines)]
  if (length(checklines) != 22L || length(notok))
    stop('recomposition disagreement in ', ca$id, ':\n', paste(notok, collapse = '\n'))
  checks_done <- checks_done + length(checklines)
  tables <- list()
  for (tn in table_names) {
    v <- field(tn)
    if (is.null(v) || !nzchar(v) || identical(v, 'null'))
      stop('table ', tn, ' missing for ', ca$id)
    value <- as.numeric(strsplit(v, ';', fixed = TRUE)[[1L]])
    tables[[tn]] <- value
    table_rows[[length(table_rows) + 1L]] <- data.frame(case = ca$id, table = tn,
      row = seq_along(value) - 1L, value = value, stringsAsFactors = FALSE)
  }
  stopifnot(length(tables$out) == ca$n, length(tables$d10) == ca$n,
    length(tables$a1) == ca$n, length(tables$c17) == ca$n, length(tables$e1) == ca$n)
  captured_tables[[ca$id]] <- tables
  case_rows[[length(case_rows) + 1L]] <- data.frame(
    case = ca$id, frequency = 12L, first_period = 1L, n = ca$n,
    log = ca$log, mode_requested = ca$mode, mode_effective = effective,
    filters = paste(ca$filters, collapse = ','), henderson = ca$henderson,
    lower = ca$lower, upper = ca$upper, backcast = ca$bc, forecast = ca$fc,
    route = ca$route, x13fast = FALSE, checks = length(checklines), stringsAsFactors = FALSE)
}
# Cross-driver guard: re-run the empty fixed spec through the x13_fast
# result path — the R ts marshalling, the pinned jdplus.x13.base.r.X13.process
# call and the proto result transform, exactly as rjd3x13::x13_fast does —
# and compare the finals and the decomposition tables bit-exactly against
# the probe capture.
fast_case <- Filter(function(ca) identical(ca$id, 'f_empty'), cases)
if (length(fast_case) == 1L) {
  ca <- fast_case[[1L]]
  values <- base_series(ca$n, 12L, seed = 20261116L)
  lines <- c(sprintf('id %s', ca$id), 'freq 12', 'start 2000:1',
    paste('values', fmtv(values)), 'log false', sprintf('mode %s', ca$mode),
    sprintf('filters %s', paste(ca$filters, collapse = ',')),
    sprintf('henderson %d', ca$henderson),
    sprintf('sigma %.17g;%.17g', ca$lower, ca$upper),
    'horizons 0;0', 'expectedvars 0', 'mean unused', 'arima 1;1', 'td off', 'easter off')
  jspec <- .jcall('X13FitProbe', 'Ljdplus/x13/base/api/x13/X13Spec;', 'spec', .jarray(lines))
  jts <- rjd3toolkit::.r2jd_tsdata(ts(values, start = c(2000, 1), frequency = 12))
  jrslt <- .jcall('jdplus/x13/base/r/X13', 'Ljdplus/x13/base/core/x13/X13Results;',
    'process', jts, jspec,
    .jnull('jdplus/toolkit/base/api/timeseries/regression/ModellingContext'))
  stopifnot(!is.jnull(jrslt), .jcall(jrslt, 'Z', 'isValid'))
  res <- rjd3x13:::.x13_rslts(jrslt)
  tables <- captured_tables[['f_empty']]
  compared <- 0L
  for (k in names(res$final)) {
    if (!k %in% names(tables)) next
    value <- as.numeric(stats::window(res$final[[k]]))
    if (!identical(value, tables[[k]]))
      stop('x13_fast finals table ', k, ' differs from the probe capture')
    x13fast_rows[[length(x13fast_rows) + 1L]] <- data.frame(case = 'f_empty', table = k,
      row = seq_along(value) - 1L, value = value, stringsAsFactors = FALSE)
    compared <- compared + 1L
  }
  for (k in c('d10', 'd11', 'd12', 'd13')) {
    value <- as.numeric(stats::window(res$decomposition[[k]]))
    if (!identical(value, tables[[k]]))
      stop('x13_fast decomposition table ', k, ' differs from the probe capture')
    x13fast_rows[[length(x13fast_rows) + 1L]] <- data.frame(case = 'f_empty', table = k,
      row = seq_along(value) - 1L, value = value, stringsAsFactors = FALSE)
    compared <- compared + 1L
  }
  value <- as.numeric(stats::window(res$preadjust$a1))
  stopifnot(identical(value, tables$a1))
  x13fast_rows[[length(x13fast_rows) + 1L]] <- data.frame(case = 'f_empty', table = 'a1',
    row = seq_along(value) - 1L, value = value, stringsAsFactors = FALSE)
  compared <- compared + 1L
  if (compared != 14L) stop('x13_fast guard compared ', compared, ' tables != 14')
  # The case row records the guard.
  idx <- which(vapply(case_rows, function(r) identical(r$case, 'f_empty'), logical(1)))
  case_rows[[idx]]$x13fast <- TRUE
}
fixtures_fmt <- function(d) {
  data.frame(lapply(d, function(col) if (is.numeric(col)) sprintf('%.17g', col) else col),
    stringsAsFactors = FALSE, check.names = FALSE)
}
case_df <- do.call(rbind, case_rows)
input_df <- do.call(rbind, input_rows)
var_df <- do.call(rbind, var_rows)
column_df <- do.call(rbind, column_rows)
table_df <- do.call(rbind, table_rows)
horizon_df <- if (length(horizon_rows)) do.call(rbind, horizon_rows) else
  data.frame(case = character(), a1a_len = integer(), a1b_len = integer(),
    out_len = integer(), d11a_len = integer(), d11b_len = integer(),
    d11final_len = integer(), backcast = integer(), forecast = integer(),
    n = integer(), stringsAsFactors = FALSE)
x13fast_df <- if (length(x13fast_rows)) do.call(rbind, x13fast_rows) else
  data.frame(case = character(), table = character(), row = integer(),
    value = numeric(), stringsAsFactors = FALSE)
write.csv(fixtures_fmt(case_df), file.path(output, 'cases.csv'), row.names = FALSE, quote = TRUE)
write.csv(fixtures_fmt(input_df), file.path(output, 'inputs.csv'), row.names = FALSE, quote = TRUE)
write.csv(fixtures_fmt(var_df), file.path(output, 'vars.csv'), row.names = FALSE, quote = TRUE)
write.csv(fixtures_fmt(column_df), file.path(output, 'columns.csv'), row.names = FALSE, quote = TRUE)
write.csv(fixtures_fmt(table_df), file.path(output, 'tables.csv'), row.names = FALSE, quote = TRUE)
write.csv(fixtures_fmt(horizon_df), file.path(output, 'horizons.csv'), row.names = FALSE, quote = TRUE)
write.csv(fixtures_fmt(x13fast_df), file.path(output, 'x13fast.csv'), row.names = FALSE, quote = TRUE)
# Structural guard: every CSV must carry the header plus exactly the frame
# rows, one row per line (guards against matrix-coerced single-line
# serialisation).
for (spec in list(list('cases.csv', case_df), list('inputs.csv', input_df),
                  list('vars.csv', var_df), list('columns.csv', column_df),
                  list('tables.csv', table_df), list('horizons.csv', horizon_df),
                  list('x13fast.csv', x13fast_df))) {
  lines <- readLines(file.path(output, spec[[1]]))
  header <- gsub('^"|"$', '', strsplit(lines[1L], '","', fixed = TRUE)[[1L]])
  stopifnot(length(lines) == nrow(spec[[2]]) + 1L, identical(header, names(spec[[2]])))
}
java <- list(version = .jcall('java/lang/System', 'Ljava/lang/String;', 'getProperty', 'java.version'),
  vendor = .jcall('java/lang/System', 'Ljava/lang/String;', 'getProperty', 'java.vendor'),
  runtime_version = .jcall('java/lang/System', 'Ljava/lang/String;', 'getProperty', 'java.runtime.version'),
  vm_name = .jcall('java/lang/System', 'Ljava/lang/String;', 'getProperty', 'java.vm.name'),
  architecture = .jcall('java/lang/System', 'Ljava/lang/String;', 'getProperty', 'os.arch'))
files <- c(file.path(tooling, c('capture-x13-fit.R', 'X13FitProbe.java')))
metadata <- list(schema_version = 1L,
  captured_utc = format(Sys.time(), tz = 'UTC', usetz = TRUE),
  source_sha256 = setNames(reference_sha256(files), basename(files)),
  java = java, R = R.version.string, cases = nrow(case_df),
  public_scope = paste(
    'Pinned restricted fixed-model X-13 fit, monthly, executed end to end through the pinned fast',
    'entry point jdplus.x13.base.r.X13.process(TsData, X13Spec, ModellingContext) — the method',
    'rjd3x13::x13_fast calls (X13Kernel.of(spec, ctx).process(ts.cleanExtremities(), null)) — on',
    'fully specified fixed-coefficient X13Specs (TD7 with fixed coefficients, fixed Easter,',
    'AO/LS outliers and user variables with fixed coefficients spanning every regeffect bucket,',
    'OutlierSpec disabled, usingAutoModel false, LengthOfPeriodType.None): the probe refuses any',
    'free coefficient, so the estimation cursor is never read. Zero-horizon cases assert a1a/a1b',
    'null, the unextended out calendar and no split finals (the fixed path), capture the stored',
    'a-tables, out, the X11Results D10..D13/C17 and the nine finals tables, and recompose all 22',
    'bit-exactly from the pinned deterministicEffect/backTransform/TsData-fold/correctSeries',
    'pieces; nonzero-horizon evidence cases (f_h6, f_b3h3) record only the deterministic route',
    'facts (a1a/a1b non-null with the requested lengths, the extended out calendar, the d11a/d11b',
    'split finals) proving the pinned engine leaves the fixed path through ExactArimaForecasts for',
    'any nonzero horizon (the delivered fit answers the explicit unsupported status). The f_empty',
    'case is re-executed through the R x13_fast result path (R ts marshalling, the pinned process',
    'call, the proto round trip) and compared bit-exactly (x13fast.csv, 14 tables). The X-11',
    'specifications vary the fixed seasonal requests (S3X1, Stable, S3X9, S3X15), the fixed',
    'Henderson lengths (5, 13) and the sigma cuttings (1.5/2.5, 2/3); the modes cover the four',
    'qualified decompositions after the updateSpec forcing (Additive, Multiplicative via the log',
    'model, LogAdditive, PseudoAdditive) including the forced cases. Coefficient estimation,',
    'the ARIMA extension route itself, missing-value interpolation, LengthOfPeriod transforms,',
    'benchmarking and diagnostics are not claimed.',
    'Schema: cases.csv one row per case (filters the comma-separated per-period fixed request',
    'codes, route fixed/arima, x13fast the R cross-driver guard flag); horizons.csv the',
    'nonzero-horizon route evidence; x13fast.csv the f_empty tables as the R x13_fast result',
    'path returned them; vars.csv/columns.csv/tables.csv as in the allocation corpus.'))
writeLines(sub('[[:blank:]]+$', '', capture.output(dput(metadata))), file.path(output, 'metadata.R'))
captured <- file.path(output, c('cases.csv', 'inputs.csv', 'vars.csv', 'columns.csv',
  'tables.csv', 'horizons.csv', 'x13fast.csv', 'metadata.R'))
write.csv(data.frame(file = basename(captured), sha256 = reference_sha256(captured)),
  file.path(output, 'SHA256SUMS.csv'), row.names = FALSE)
cat('captured', nrow(case_df), 'cases,', checks_done, 'checks,',
  nrow(x13fast_df) / 96, 'x13_fast tables\n')
