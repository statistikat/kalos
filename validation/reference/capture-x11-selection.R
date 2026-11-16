# Pinned-reference capture for item 22b.4 S4 (additive), item 22b.5 M3a
# (multiplicative) and item 22b.5 M3b-i (log-additive): the selection flow of
# the pinned X11Kernel (pinned rjd3
# 3.9.0 jars). Each case is captured by running the kernel itself
# (X11Kernel.process on complete fixed-mode inputs with per-phase
# SeasonalFilterOption requests and a fixed or automatic Henderson length), so
# every automatic Henderson choice, I/C ratio, MSR filter decision, iteration
# count and seasFilter fill is the pinned kernel's own. Captured evidence: the
# post-selection B/C tables (B7, B13, B20, C7, C13, C20), all sixteen D tables,
# per-stage calcICR/selectFilter values re-derived through the pinned
# AutomaticHenderson class on the kernel's own context, a standalone pinned
# MsrFilterSelection.doMSR call on the kernel D9_g_bis, and a pinned-component
# attempt walk (FilteredMeanEndPoints + MsrTable.of) reproducing every doMSR
# iteration. The capture refuses to write on any disagreement. No production
# output and no old corpus is input.
args <- commandArgs(trailingOnly = FALSE)
script <- sub('^--file=', '', args[startsWith(args, '--file=')][[1L]])
tooling <- dirname(normalizePath(script)); source(file.path(tooling, 'common.R'))
root <- reference_root()
args <- commandArgs(trailingOnly = TRUE)
mode_name <- 'Additive'
if (length(args) >= 1L && startsWith(args[[1L]], '--mode=')) {
  mode_name <- sub('^--mode=', '', args[[1L]]); args <- args[-1L]
}
if (length(args) != 1L || !startsWith(args, '--output='))
  stop('Usage: capture-x11-selection.R [--mode=Additive|Multiplicative|LogAdditive] --output=<existing empty directory>')
stopifnot(mode_name %in% c('Additive', 'Multiplicative', 'LogAdditive'))
# The pinned truth table: isMultiplicative() is (mode == Multiplicative ||
# mode == PseudoAdditive), so LogAdditive keeps the additive ctx.remove, the
# 0-valued mode default, the strict MsrTable relative flag == FALSE and no
# makePositivity guards; the kernel itself applies the DoubleSeq.log() input
# transform, so the capture feeds raw values and every captured step table
# is log-domain (inputs.csv keeps the raw values).
mult <- mode_name == 'Multiplicative'
logadd <- mode_name == 'LogAdditive'
mode_label <- switch(mode_name, Additive = 'additive',
  Multiplicative = 'multiplicative', LogAdditive = 'log-additive')
dflt <- if (mult) 1 else 0 # the pinned mode default (DummyFilter/correction factor)
rem <- function(a, b) if (mult) a / b else a - b # pinned ctx.remove
output <- normalizePath(sub('^--output=', '', args), mustWork = TRUE)
if (!dir.exists(output) || length(list.files(output, all.files = TRUE, no.. = TRUE)))
  stop('Output must be empty')
suppressWarnings(suppressMessages({
  library(rjd3jars); library(rjd3toolkit); library(rjd3x13); library(rJava)
}))
.jinit()
mode_ref <- J('jdplus.sa.base.api.DecompositionMode')$valueOf(mode_name)
option <- 'jdplus/x13/base/api/x11/SeasonalFilterOption'
enames <- c('0' = 'Stable', '1' = 'S3X1', '3' = 'S3X3', '5' = 'S3X5', '9' = 'S3X9',
  '15' = 'S3X15', '-1' = 'X11Default', '-2' = 'Msr')
code_of <- function(arr) vapply(arr, function(e) {
  s <- .jcall(e, 'Ljava/lang/String;', 'toString')
  switch(s, Stable = 0L, S3X1 = 1L, S3X3 = 3L, S3X5 = 5L, S3X9 = 9L, S3X15 = 15L,
    X11Default = -1L, Msr = -2L)}, integer(1))
en <- function(spec) .jarray(lapply(spec, function(x) .jfield(option,
  paste0('L', option, ';'), enames[as.character(x)])), contents.class = option)
resolved_final <- function(codes) ifelse(codes %in% c(-1L, -2L), 5L, codes)
resolved_initial <- function(codes) ifelse(codes %in% c(-1L, -2L), 3L, codes)
# Pinned AutomaticHenderson.selectFilter and the b7Step inline branch, applied
# through the pinned class itself; the b7Step inline (icr >= 1 -> period + 1)
# is a pinned X11BStep behaviour, decoded from bytecode, and is cross-checked
# here against the pinned selectFilter only to document the divergence.
select_filter <- function(icr, f) .jcall('jdplus/x13/base/core/x11/filter/AutomaticHenderson',
  'I', 'selectFilter', icr, as.integer(f))
band_of <- function(msr) if (is.na(msr)) 's3x9' else if (msr < 2.5) 's3x3' else
  if (msr < 3.5) 'lower_gap' else if (msr < 5.5) 's3x5' else if (msr < 6.5) 'upper_gap' else 's3x9'
fmend_pilot <- function(w, f) {
  # Pinned MsrFilterSelection.calcComponents: a seven-term symmetric filter
  # with FilteredMeanEndPoints endpoints over each annual phase; the
  # irregular is the mode-aware ctx.remove of the seasonal (x - seas
  # additive, x / seas multiplicative).
  filt <- .jcall('jdplus/x13/base/core/x11/filter/X11FilterFactory',
    'Ljdplus/toolkit/base/core/math/linearfilters/SymmetricFilter;', 'makeSymmetricFilter', 7L)
  fmend <- .jnew('jdplus/x13/base/core/x11/filter/endpoints/FilteredMeanEndPoints', filt)
  out <- rep(0, length(w))
  src <- .jcall('jdplus/toolkit/base/core/data/DataBlock', 'Ljdplus/toolkit/base/core/data/DataBlock;',
    'of', .jcast(J('jdplus.toolkit.base.api.data.Doubles')$of(.jarray(as.double(w))),
      'jdplus/toolkit/base/api/data/DoubleSeq'))
  dst <- .jcall('jdplus/toolkit/base/core/data/DataBlock', 'Ljdplus/toolkit/base/core/data/DataBlock;',
    'of', .jarray(as.double(out)))
  for (p in seq_len(f) - 1L) {
    srcp <- .jcall(src, 'Ljdplus/toolkit/base/core/data/DataBlock;', 'extract', as.integer(p), -1L, as.integer(f))
    dstp <- .jcall(dst, 'Ljdplus/toolkit/base/core/data/DataBlock;', 'extract', as.integer(p), -1L, as.integer(f))
    .jcall(fmend, 'V', 'process',
      .jcast(srcp, 'jdplus/toolkit/base/api/data/DoubleSeq'), dstp)
  }
  seas <- .jcall(dst, '[D', 'toArray')
  seas <- seas[seq_len(length(w))]
  list(seasonal = seas, irregular = rem(as.double(w), seas))
}
msr_table_global <- function(seas, irr, f, first0) {
  # The pinned MsrTable.of relative-delta flag is the strict mode ==
  # Multiplicative test (MsrFilterSelection passes exactly this).
  tab <- .jcall('jdplus/x13/base/api/x11/MsrTable', 'Ljdplus/x13/base/api/x11/MsrTable;',
    'of', .jcast(J('jdplus.toolkit.base.api.data.Doubles')$of(.jarray(seas)),
      'jdplus/toolkit/base/api/data/DoubleSeq'),
    .jcast(J('jdplus.toolkit.base.api.data.Doubles')$of(.jarray(irr)),
      'jdplus/toolkit/base/api/data/DoubleSeq'),
    as.integer(f), as.integer(first0), mult)
  .jcall(tab, 'D', 'getGlobalMsr')
}
tables <- list(); cases <- list(); inputs <- list(); selections <- list(); msrrows <- list()
grab <- function(obj, nm) {
  s <- .jcall(obj, 'Ljdplus/toolkit/base/api/data/DoubleSeq;', paste0('get', nm))
  v <- s$toArray()
  data.frame(table = nm, row = seq_along(v) - 1L, value = v, stringsAsFactors = FALSE)
}
capture <- function(id, values, f, first_period, requests, trend, back = 0L, fc = 0L) {
  if (mult || logadd) stopifnot(all(is.finite(values) & values > 0)) # the pinned gate
  ts <- .jcall('jdplus/toolkit/base/r/timeseries/TsUtility',
    'Ljdplus/toolkit/base/api/timeseries/TsData;', 'of',
    as.integer(f), 2000L, as.integer(first_period), as.double(values))
  sb <- J('jdplus.x13.base.api.x11.X11Spec')$builder()
  sb <- .jcall(sb, 'Ljdplus/x13/base/api/x11/X11Spec$Builder;', 'mode', mode_ref)
  sb <- .jcall(sb, 'Ljdplus/x13/base/api/x11/X11Spec$Builder;', 'filters', en(requests))
  sb <- .jcall(sb, 'Ljdplus/x13/base/api/x11/X11Spec$Builder;', 'hendersonFilterLength', as.integer(trend))
  sb <- .jcall(sb, 'Ljdplus/x13/base/api/x11/X11Spec$Builder;', 'backcastHorizon', as.integer(back))
  sb <- .jcall(sb, 'Ljdplus/x13/base/api/x11/X11Spec$Builder;', 'forecastHorizon', as.integer(fc))
  spec <- .jcast(.jcall(sb, 'Ljdplus/toolkit/base/api/util/Validatable;', 'build'),
    'jdplus/x13/base/api/x11/X11Spec')
  log <- J('jdplus.toolkit.base.api.processing.ProcessingLog')$dummy()
  k <- .jnew('jdplus.x13.base.core.x11.X11Kernel')
  r <- .jcall(k, 'Ljdplus/x13/base/core/x11/X11Results;', 'process', ts, spec,
    .jcast(log, 'jdplus/toolkit/base/api/processing/ProcessingLog'))
  if (is.null(r) || !is.null(.jgetEx(TRUE))) stop('kernel failed for case ', id)
  b <- k$getBstep(); cstep <- k$getCstep(); d <- k$getDstep(); ctx <- k$getContext()
  ctx_initial <- code_of(.jcall(ctx, '[Ljdplus/x13/base/api/x11/SeasonalFilterOption;',
    'getInitialSeasonalFilter'))
  ctx_final <- code_of(.jcall(ctx, '[Ljdplus/x13/base/api/x11/SeasonalFilterOption;',
    'getFinalSeasonalFilter'))
  stopifnot(identical(ctx_initial, resolved_initial(requests)),
    identical(ctx_final, resolved_final(requests)))
  seas <- code_of(.jcall(d, '[Ljdplus/x13/base/api/x11/SeasonalFilterOption;', 'getSeasFilter'))
  # Per-stage pinned I/C ratios on the kernel's own context.
  seq_of <- function(obj, nm) .jcall(obj, 'Ljdplus/toolkit/base/api/data/DoubleSeq;', paste0('get', nm))
  icr <- function(obj, nm) .jcall('jdplus/x13/base/core/x11/filter/AutomaticHenderson', 'D',
    'calcICR', ctx, seq_of(obj, nm))
  ics <- c(b6 = icr(b, 'B6'), c6 = icr(cstep, 'C6'), d6 = icr(d, 'D6'), d11bis = icr(d, 'D11bis'))
  stopifnot(identical(ics[['d11bis']], d$getICRatio()))
  sels <- vapply(ics, function(v) select_filter(v, f), integer(1))
  b7_len <- if (trend != 0L) trend else if (!is.na(ics[['b6']]) && ics[['b6']] >= 1) f + 1L else sels[['b6']]
  d7len <- d$getD7HendersonFilterLength(); finallen <- d$getFinalHendersonFilterLength()
  expected_d7 <- if (trend != 0L) trend else sels[['d6']]
  expected_final <- if (trend != 0L) trend else sels[['d11bis']]
  stopifnot(d7len == expected_d7, finallen == expected_final)
  # Standalone pinned doMSR over the kernel D9_g_bis and the pinned attempt walk.
  gbf <- .jcall(d, 'Ljdplus/toolkit/base/api/data/DoubleSeq;', 'getD9_g_bis')$toArray()
  mfs <- .jnew('jdplus/x13/base/core/x11/filter/MsrFilterSelection')
  f2 <- .jcall(mfs, 'Ljdplus/x13/base/api/x11/SeasonalFilterOption;', 'doMSR',
    .jcall(d, 'Ljdplus/toolkit/base/api/data/DoubleSeq;', 'getD9_g_bis'), ctx)
  stand <- list(filter = code_of(.jarray(list(f2))), iter = mfs$getIterCount(),
    default = mfs$isUsingDefault(), msr = mfs$getGlobalMsr())
  first0 <- (first_period - 1L + back) %% f # getPosition(backcast): original phase
  w <- if (back > 0L) gbf[-seq_len(back)] else gbf
  if (fc > 0L) w <- w[seq_len(length(w) - fc)]
  rem <- (length(w) + first0) %% f
  w <- if (rem > 0) w[-((length(w) - rem + 1):length(w))] else w
  attempts <- list(); off <- 0L; msr <- 0; filter <- NA_integer_; iter <- 0L; def <- TRUE
  while (length(w) - off >= 5L * f) {
    iter <- iter + 1L
    cur <- w[seq_len(length(w) - off)] # pinned drop(0, f) removes trailing years
    part <- fmend_pilot(cur, f)
    msr <- msr_table_global(part$seasonal, part$irregular, f, first0)
    band <- band_of(msr)
    attempts[[length(attempts) + 1L]] <- data.frame(attempt = iter - 1L,
      count = length(w) - off, years_removed = off %/% f, global_msr = msr,
      band = band, stringsAsFactors = FALSE)
    filter <- switch(band, s3x3 = 3L, s3x5 = 5L, s3x9 = 9L, NA_integer_)
    if (!is.na(filter)) { def <- FALSE; break }
    off <- off + f
  }
  if (is.na(filter)) { filter <- 5L; def <- TRUE }
  stopifnot(identical(as.integer(filter), as.integer(stand$filter)), iter == stand$iter,
    def == stand$default, identical(msr, stand$msr) || (is.na(msr) && is.na(stand$msr)))
  engaged <- all(requests == -2L)
  if (engaged) {
    stopifnot(identical(code_of(.jarray(list(d$getD9filter()))), as.integer(filter)),
      d$getD9msriter() == iter, identical(seas, rep(as.integer(filter), f)))
  } else {
    stopifnot(is.null(d$getD9filter()), d$getD9msriter() == 0L,
      identical(seas, resolved_final(requests)))
  }
  stopifnot(!d$isD9default())
  # Case-level records.
  inputs <<- rbind(inputs, data.frame(case = id, row = seq_along(values) - 1L,
    value = as.double(values), stringsAsFactors = FALSE))
  row <- data.frame(case = id, frequency = f, first_period = first_period,
    n = length(values), trend = trend, backcast = back, forecast = fc,
    requests = paste(requests, collapse = ','), d7len = d7len, finallen = finallen,
    icratio = d$getICRatio(), seas = paste(seas, collapse = ','),
    engaged = engaged, d9filter = if (is.null(d$getD9filter())) NA_integer_ else code_of(.jarray(list(d$getD9filter()))),
    d9msriter = d$getD9msriter(), d9default = d$isD9default(),
    b20nz = sum(seq_of(b, 'B20')$toArray() != dflt), stringsAsFactors = FALSE)
  if (mult || logadd) row$mode <- mode_name # mode-carrying schemas
  cases <<- rbind(cases, row)                        # keep the additive schema byte-exact
  stages <- data.frame(stage = c('b7', 'c7', 'd7', 'final'),
    input = c('b6', 'c6', 'd6', 'd11bis'), ic_ratio = as.numeric(ics),
    select_filter = as.integer(sels), stringsAsFactors = FALSE)
  stages$pinned_len <- c(b7_len, if (trend != 0L) trend else sels[['c6']], expected_d7, expected_final)
  stages$mode <- ifelse(trend == 0L, 'selected', 'fixed')
  if (any(is.na(stages$ic_ratio))) stages$mode[is.na(stages$ic_ratio)] <- 'pinned-chooses-longest-not-claimed'
  stages <- cbind(case = id, stages, row.names = NULL)
  selections <<- rbind(selections, stages)
  if (length(attempts)) msrrows <<- rbind(msrrows, cbind(case = id, do.call(rbind, attempts),
    final_filter = filter, using_default = def, row.names = NULL))
  else msrrows <<- rbind(msrrows, data.frame(case = id, attempt = 0L, count = length(w),
    years_removed = 0L, global_msr = 0, band = 'no_history', final_filter = 5L,
    using_default = TRUE, stringsAsFactors = FALSE))
  picked <- list(B = list(b, c('B7', 'B13', 'B20')), C = list(cstep, c('C7', 'C13', 'C20')),
    D = list(d, c('D1', 'D2', 'D4', 'D5', 'D6', 'D7', 'D8', 'D9', 'D9bis', 'D9_g_bis',
      'D10', 'D10bis', 'D11', 'D11bis', 'D12', 'D13')))
  for (g in picked) for (nm in g[[2]])
    tables <<- rbind(tables, cbind(case = id, grab(g[[1]], nm), row.names = NULL))
  cat('captured', id, 'd7', d7len, 'final', finallen, 'icr', round(as.numeric(stages[1, 'ic_ratio']), 4),
    if (engaged) paste('engaged iter', iter, 'default', def) else '', '\n')
}
# Deterministic input recipes (regenerated here; the fixtures carry the exact
# values and the native tests embed the quarterly/extended subset verbatim).
base_q <- { set.seed(11); n <- 96L
  as.numeric(200 + 5 * sin(2 * pi * (1:n) / 4) + 40 * cumsum(rnorm(n, 0, .03)) + rnorm(n)) }
base_m <- { set.seed(12); n <- 120L
  as.numeric(500 + 20 * sin(2 * pi * (1:n) / 12) + 80 * cumsum(rnorm(n, 0, .02)) + rnorm(n, 0, 6)) }
sl <- (base_q[96L] - base_q[1L]) / 96L
ext_q <- c(base_q[1L] - (3L:1L) * sl, base_q, base_q[96L] + (1L:2L) * sl)
capture('q-s3x9-auto', base_q, 4L, 1L, rep(9L, 4L), 0L)
capture('q-s3x3-auto', base_q, 4L, 1L, rep(3L, 4L), 0L)
capture('q-msr13', base_q, 4L, 1L, rep(-2L, 4L), 13L)
capture('q-msrauto', base_q, 4L, 1L, rep(-2L, 4L), 0L)
capture('q-x11d-auto', base_q, 4L, 1L, rep(-1L, 4L), 0L)
capture('q-mixed13', base_q, 4L, 1L, c(-2L, -2L, 5L, 5L), 13L)
# Long ambiguous MSR walk (19 pinned evaluations): the jdplus run pins every
# attempt, so the qualified x11_five_year_cap branch is checked against the
# same captured evaluations truncated after the sixth.
capture('q-longmsr', { set.seed(2581); n <- 120L
  as.numeric(100 + 6 * sin(2 * pi * (1:n) / 4) + 20 * cumsum(rnorm(n, 0, .04)) + rnorm(n, 0, 9)) },
  4L, 1L, rep(-2L, 4L), 13L)
capture('m-msrauto', base_m, 12L, 1L, rep(-2L, 12L), 0L)
capture('m-s3x9-auto', base_m, 12L, 1L, rep(9L, 12L), 0L)
capture('q-exh20', { set.seed(5); n <- 20L
  as.numeric(100 + 8 * sin(2 * pi * (1:n) / 4) + rnorm(n, 0, 14)) }, 4L, 1L, rep(-2L, 4L), 13L)
capture('q-iter0-19', { set.seed(1); n <- 19L
  as.numeric(100 + 8 * sin(2 * pi * (1:n) / 4) + rnorm(n, 0, 14)) }, 4L, 1L, rep(-2L, 4L), 13L)
capture('qext-msr13', ext_q, 4L, 2L, rep(-2L, 4L), 13L, 3L, 2L)
capture('qext-auto', ext_q, 4L, 2L, rep(9L, 4L), 0L, 3L, 2L)
# Unsupported controls: the pinned kernel turns the degenerate I/C ratio into
# the longest trend filter and a decisive 3.84 MSR value; the qualified pass
# stops with the explicit unsupported status instead. Captured for contrast.
capture('const-msr', rep(100, 60L), 4L, 1L, rep(-2L, 4L), 0L)
capture('const-fixed', rep(100, 60L), 4L, 1L, rep(5L, 4L), 0L)
fixtures_fmt <- function(d) {
  data.frame(lapply(d, function(col) if (is.numeric(col)) sprintf('%.17g', col) else col),
    stringsAsFactors = FALSE, check.names = FALSE)
}
write.csv(fixtures_fmt(inputs), file.path(output, 'inputs.csv'), row.names = FALSE)
write.csv(fixtures_fmt(cases), file.path(output, 'cases.csv'), row.names = FALSE)
write.csv(fixtures_fmt(tables), file.path(output, 'tables.csv'), row.names = FALSE)
write.csv(fixtures_fmt(selections), file.path(output, 'selections.csv'), row.names = FALSE)
write.csv(fixtures_fmt(msrrows), file.path(output, 'msr.csv'), row.names = FALSE)
# Structural guard: every CSV must carry the header plus exactly the frame rows,
# one row per line (guards against matrix-coerced single-line serialisation).
for (spec in list(list('inputs.csv', inputs), list('cases.csv', cases), list('tables.csv', tables),
                  list('selections.csv', selections), list('msr.csv', msrrows))) {
  lines <- readLines(file.path(output, spec[[1]]))
  header <- gsub('^"|"$', '', strsplit(lines[1L], '","', fixed = TRUE)[[1L]])
  stopifnot(length(lines) == nrow(spec[[2]]) + 1L, identical(header, names(spec[[2]])))
}
files <- normalizePath(script)
java <- list(version = .jcall('java/lang/System', 'Ljava/lang/String;', 'getProperty', 'java.version'),
  vendor = .jcall('java/lang/System', 'Ljava/lang/String;', 'getProperty', 'java.vendor'),
  runtime_version = .jcall('java/lang/System', 'Ljava/lang/String;', 'getProperty', 'java.runtime.version'),
  vm_name = .jcall('java/lang/System', 'Ljava/lang/String;', 'getProperty', 'java.vm.name'),
  architecture = .jcall('java/lang/System', 'Ljava/lang/String;', 'getProperty', 'os.arch'),
  oracle = paste0('rjd3 3.9.0/3.9.1 pinned jars via rjd3jars classpath; X11Kernel.process(TsData, X11Spec, ProcessingLog.dummy) with per-phase SeasonalFilterOption requests, fixed or automatic (0) Henderson length, ',
    mode_label, ' mode, zero or hand-extended horizons; step getters B7/B13/B20, C7/C13/C20, D1..D13, getSeasFilter, getD9filter, getD9msriter, isD9default, getD7HendersonFilterLength, getFinalHendersonFilterLength, getICRatio; pinned AutomaticHenderson.calcICR/selectFilter and standalone MsrFilterSelection.doMSR cross-checks on the kernel context; pinned FilteredMeanEndPoints/MsrTable.of attempt walk reproducing every doMSR iteration',
    if (logadd) '; the kernel applies the DoubleSeq.log() input transform, so every captured step table is log-domain and the doMSR/MSR cross-checks run in the additive (isMultiplicative() == false) form' else ''))
metadata <- list(schema_version = 1L,
  captured_utc = format(Sys.time(), tz = 'UTC', usetz = TRUE),
  source_sha256 = setNames(reference_sha256(files), basename(files)),
  java = java, R = R.version.string, cases = nrow(cases),
  public_scope = switch(mode_name,
    Multiplicative = 'Pinned X11Kernel fixed multiplicative selection flow only: staged fixed/X11Default/MSR seasonal requests resolved by the pinned context, automatic Henderson length at B7 (with the pinned inline ratio >= 1 -> period + 1 branch), C7, D7 and the D final step (selectFilter thresholds), the always-computed D final iCRatio, the global-MSR D-final filter decision (doMSR attempts with the mode-aware calcComponents irregular x / seas and the MsrTable.of relative flag set exactly when the mode is Multiplicative, iteration count, exhausted default and seasFilter fill) and the post-selection B/C/D tables of the same run. The constant-series case pins the pinned degenerate choices (NaN I/C ratio -> longest filter, degenerate global MSR -> 3x5 default) as controls the qualified pass deliberately does not reproduce; cross-validation, log-additive and pseudo-additive modes, the period-specific corrector and ARIMA-regression inputs are not claimed',
    LogAdditive = 'Pinned X11Kernel fixed log-additive selection flow only: staged fixed/X11Default/MSR seasonal requests resolved by the pinned context, automatic Henderson length at B7 (with the pinned inline ratio >= 1 -> period + 1 branch), C7, D7 and the D final step (selectFilter thresholds), the always-computed D final iCRatio, the global-MSR D-final filter decision (doMSR attempts with the additive-form calcComponents irregular x - seas and the MsrTable.of relative flag FALSE, because isMultiplicative() is false under LogAdditive, iteration count, exhausted default and seasFilter fill) and the post-selection B/C/D tables of the same run: the tables are the log-domain step tables (additive arithmetic on DoubleSeq.log() of the strictly positive input); the results-builder exp of D10..D13 and the bias-correction skip are export-stage behaviour and are not claimed. The constant-series case pins the pinned degenerate choices (NaN I/C ratio -> longest filter, degenerate global MSR -> 3x5 default) as controls the qualified pass deliberately does not reproduce; cross-validation, multiplicative and pseudo-additive modes, the period-specific corrector and ARIMA-regression inputs are not claimed',
    'Pinned X11Kernel additive selection flow only: staged fixed/X11Default/MSR seasonal requests resolved by the pinned context, automatic Henderson length at B7 (with the pinned inline ratio >= 1 -> period + 1 branch), C7, D7 and the D final step (selectFilter thresholds), the always-computed D final iCRatio, the global-MSR D-final filter decision (doMSR attempts, iteration count, exhausted default and seasFilter fill) and the post-selection B/C/D tables of the same run. The constant-series cases pin the pinned degenerate choices (NaN I/C ratio -> longest filter, MSR 3.844... -> 3x5) as controls the qualified pass deliberately does not reproduce; cross-validation, multiplicative modes, the period-specific corrector and ARIMA-regression inputs are not claimed'))
writeLines(sub('[[:blank:]]+$', '', capture.output(dput(metadata))), file.path(output, 'metadata.R'))
captured <- file.path(output, c('inputs.csv', 'cases.csv', 'tables.csv', 'selections.csv', 'msr.csv', 'metadata.R'))
write.csv(data.frame(file = basename(captured), sha256 = reference_sha256(captured)),
  file.path(output, 'SHA256SUMS.csv'), row.names = FALSE)
cat('Captured', nrow(cases), 'pinned', mode_label, 'selection cases;', nrow(tables), 'table rows\n')
