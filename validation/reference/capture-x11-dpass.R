# Pinned-reference capture for item 22b.4 S3: the fixed additive D pass of
# jdplus.x13.base.core.x11.X11DStep (pinned rjd3 3.9.0 jars), composed exactly
# like X11Kernel: X11BStep.process(series, ctx), X11CStep.process(series, B20,
# ctx, cv) and X11DStep.process(series, C20, ctx, cv) with fixed per-phase
# seasonal filters and a fixed Henderson trend filter. The 18 fixed-additive
# B/C cases are captured at zero horizon (original calendar) and four of them
# are additionally captured hand-extended (deterministic linear back/forecast
# rows prepended and appended to the same inputs, context horizons and shifted
# first periods), pinning the extended-calendar behaviour, including the MSRE
# diagnostic table computed on the dropped original calendar at
# getPosition(backcast). No old corpus and no production output is input; the
# capture re-derives every additive identity from the captured tables
# themselves and refuses to write on disagreement.
args <- commandArgs(trailingOnly = FALSE)
script <- sub('^--file=', '', args[startsWith(args, '--file=')][[1L]])
tooling <- dirname(normalizePath(script)); source(file.path(tooling, 'common.R'))
root <- reference_root()
args <- commandArgs(trailingOnly = TRUE)
if (length(args) != 1L || !startsWith(args, '--output='))
  stop('Usage: capture-x11-dpass.R --output=<existing empty directory>')
output <- normalizePath(sub('^--output=', '', args), mustWork = TRUE)
if (!dir.exists(output) || length(list.files(output, all.files = TRUE, no.. = TRUE)))
  stop('Output must be empty')
fixtures <- file.path(root, 'validation/fixtures/native', 'x11-bpass-cases.R')
for (fixture in fixtures) source(fixture)
cpass <- read.csv(file.path(root, 'validation/fixtures/native', 'x11-cpass', 'tables.csv'))
suppressWarnings(suppressMessages({
  library(rjd3jars); library(rjd3toolkit); library(rjd3x13); library(rJava)
}))
.jinit()
mode_add <- J('jdplus.sa.base.api.DecompositionMode')$valueOf('Additive')
option <- 'jdplus/x13/base/api/x11/SeasonalFilterOption'
enums <- function(spec) .jarray(lapply(spec, function(x) .jfield(option,
  paste0('L', option, ';'), if (x == 0L) 'Stable' else paste0('S3X', x))),
  contents.class = option)
resolved <- function(ctx, which) {
  a <- .jcall(ctx, '[Ljdplus/x13/base/api/x11/SeasonalFilterOption;', which)
  vapply(a, function(e) {
    s <- .jcall(e, 'Ljava/lang/String;', 'toString')
    switch(s, Stable = 0L, S3X1 = 1L, S3X3 = 3L, S3X5 = 5L, S3X9 = 9L,
      S3X15 = 15L, stop('unexpected resolved filter ', s))
  }, integer(1))
}
resolved_array <- function(arr) vapply(arr, function(e) {
  s <- .jcall(e, 'Ljava/lang/String;', 'toString')
  switch(s, Stable = 0L, S3X1 = 1L, S3X3 = 3L, S3X5 = 5L, S3X9 = 9L, S3X15 = 15L)
}, integer(1))
same <- function(a, bb) length(a) == length(bb) && all((a == bb) | (is.nan(a) & is.nan(bb)))
names <- c('D1', 'D2', 'D4', 'D5', 'D6', 'D7', 'D8', 'D9', 'D9bis', 'D9_g_bis',
  'D10', 'D10bis', 'D11', 'D11bis', 'D12', 'D13')
# base case id -> (backcast, forecast); the extended rows are deterministic
# linear continuations of the input, so the original calendar is a pure slice.
extensions <- list(
  c('basic-12-108-1-13-1.5-2.5', 4, 3), c('rotation-12-96-7-13-1.5-2.5', 5, 2),
  c('basic-4-36-1-13-1.5-2.5', 3, 2), c('masked-4-37-2-13-2-3', 5, 3))
tables <- list(); cases <- list(); inputs <- list(); msrrows <- list()
capture <- function(ca, back, fc) {
  y <- x11_bpass_input(ca)
  series_values <- y$values; missing <- y$missing
  series_values[missing] <- NaN # the pinned reference carries NaN, not placeholders
  if (back > 0L || fc > 0L) {
    n0 <- length(series_values)
    slope <- (series_values[n0] - series_values[1]) / n0
    series_values <- c(series_values[1] - (back:1L) * slope, series_values,
      series_values[n0] + (1L:fc) * slope)
    missing <- c(rep(FALSE, back), missing, rep(FALSE, fc))
  }
  first0 <- (ca$start_period - 1L - back) %% ca$frequency
  seqi <- .jcast(J('jdplus.toolkit.base.api.data.Doubles')$of(.jarray(series_values)),
    'jdplus/toolkit/base/api/data/DoubleSeq')
  ctx <- J('jdplus.x13.base.core.x11.X11Context')$builder()$mode(mode_add)$
    period(as.integer(ca$frequency))$firstPeriod(as.integer(first0))$
    seasonal(TRUE)$trendFilterLength(as.integer(ca$trend))$
    initialSeasonalFilter(enums(ca$initial))$finalSeasonalFilter(enums(ca$final))$
    lowerSigma(ca$lsig)$upperSigma(ca$usig)$
    backcastHorizon(as.integer(back))$forecastHorizon(as.integer(fc))$excludefcast(FALSE)$build()
  stopifnot(identical(resolved(ctx, 'getInitialSeasonalFilter'), as.integer(ca$initial)),
    identical(resolved(ctx, 'getFinalSeasonalFilter'), as.integer(ca$final)))
  b <- .jnew('jdplus.x13.base.core.x11.X11BStep')
  .jcall(b, 'V', 'process', seqi, ctx)
  if (!is.null(.jgetEx(TRUE))) stop('pinned B pass failed for case ', ca$id)
  cstep <- .jnew('jdplus.x13.base.core.x11.X11CStep')
  .jcall(cstep, 'V', 'process', seqi,
    .jcall(b, 'Ljdplus/toolkit/base/api/data/DoubleSeq;', 'getB20'), ctx, NULL)
  if (!is.null(.jgetEx(TRUE))) stop('pinned C pass failed for case ', ca$id)
  cvf <- .jcall(cstep, '[Ljdplus/x13/base/api/x11/SeasonalFilterOption;', 'getCvSeasonalFilter')
  c13 <- cstep$getC13()$toArray(); c17 <- cstep$getC17()$toArray()
  c20 <- cstep$getC20()$toArray()
  dstep <- .jnew('jdplus.x13.base.core.x11.X11DStep')
  .jcall(dstep, 'V', 'process', seqi,
    .jcall(cstep, 'Ljdplus/toolkit/base/api/data/DoubleSeq;', 'getC20'), ctx,
    .jnull('[Ljdplus/x13/base/api/x11/SeasonalFilterOption;'))
  if (!is.null(.jgetEx(TRUE))) stop('pinned D pass failed for case ', ca$id)
  # Fixed requests keep cross-validation, the MSRE filter selection and the
  # period-specific corrector disengaged: none of them may produce output.
  stopifnot(is.null(dstep$getCvSeasonalFilter()), dstep$getResultCV()$isEmpty(),
    is.null(dstep$getD9filter()), dstep$getD9msriter() == 0L,
    !dstep$isD9default(), is.null(dstep$getD9g()))
  grab <- function(nm) {
    s <- .jcall(dstep, 'Ljdplus/toolkit/base/api/data/DoubleSeq;', paste0('get', nm))
    if (is.null(s)) stop('missing table ', nm, ' in case ', ca$id)
    s$toArray()
  }
  tab <- lapply(names, grab); names(tab) <- names
  n <- length(series_values); f <- ca$frequency; drop <- f %/% 2L
  stopifnot(dstep$getD2drop() == drop,
    dstep$getD7HendersonFilterLength() == ca$trend,
    dstep$getFinalHendersonFilterLength() == ca$trend,
    identical(resolved_array(.jcall(dstep,
      '[Ljdplus/x13/base/api/x11/SeasonalFilterOption;', 'getSeasFilter')),
      as.integer(ca$final)),
    same(dstep$getRefSeries()$toArray(), series_values),
    length(tab$D2) == n - 2 * drop, length(tab$D4) == n - 2 * drop,
    all(vapply(setdiff(names, c('D2', 'D4')), function(nm) length(tab[[nm]]) == n,
      logical(1))))
  cropped <- function(v) v[(drop + 1L):(n - drop)]
  # Additive identities of the captured tables, checked exactly against the
  # captured input, the captured C20 and each other. The D9 mask is the
  # pinned onMapping pair with dcmpg semantics (a NaN difference keeps D9bis).
  m9 <- abs(tab$D9bis - tab$D8)
  d9want <- ifelse(is.na(m9) | m9 >= 1e-9, tab$D9bis, NaN)
  stopifnot(same(tab$D1, series_values - c20),
    same(tab$D4, cropped(tab$D1) - tab$D2),
    same(tab$D6, tab$D1 - tab$D5),
    same(tab$D8, series_values - tab$D7),
    same(tab$D9bis, tab$D1 - tab$D7),
    same(tab$D9_g_bis, tab$D9bis),
    same(tab$D9, d9want),
    same(tab$D11, series_values - tab$D10),
    same(tab$D11bis, tab$D1 - tab$D10),
    same(tab$D13, tab$D11 - tab$D12))
  # The pinned D2 filter is the same annual trend-cycle filter re-applied
  # here independently through the toolkit's Mutable view convention.
  tf <- J('jdplus/x13/base/core/x11/filter/X11TrendCycleFilterFactory')$
    makeTrendFilter(as.integer(f))
  side <- .jcall(tf, 'I', 'length') %/% 2L
  blk <- J('jdplus.toolkit.base.core.data.DataBlock')$of(
    .jarray(rep(NaN, n - 2 * side)), 0L, as.integer(n - 2 * side))
  .jcall(tf, 'V', 'apply', .jcast(J('jdplus.toolkit.base.api.data.Doubles')$of(
    .jarray(tab$D1)), 'jdplus/toolkit/base/api/data/DoubleSeq'),
    .jcast(blk, 'jdplus/toolkit/base/api/data/DoubleSeq$Mutable'))
  if (!is.null(.jgetEx(TRUE))) stop('pinned D2 filter failed for case ', ca$id)
  stopifnot(side == drop, same(.jcall(blk, '[D', 'toArray'), tab$D2))
  # D7 and D12 recomposed through the pinned filter parts (fixed Henderson
  # with mmsre endpoints on both sides) to qualify which series each filters.
  htf <- ctx$trendFilter()
  r <- J('jdplus/x13/base/core/x11/filter/MusgraveFilterFactory')$findR(
    as.integer(ca$trend), as.integer(f))
  afs <- ctx$asymmetricTrendFilters(htf, r)
  recompose <- function(target, got) {
    ep <- .jnew('jdplus/x13/base/core/x11/filter/endpoints/AsymmetricEndPoints', afs, 0L)
    p <- .jcall(htf, 'I', 'length') %/% 2L
    a <- .jarray(rep(NaN, n))
    blkv <- J('jdplus.toolkit.base.core.data.DataBlock')$of(a, p, n - p)
    .jcall(htf, 'V', 'apply', .jcast(J('jdplus.toolkit.base.api.data.Doubles')$of(
      .jarray(target)), 'jdplus/toolkit/base/api/data/DoubleSeq'),
      .jcast(blkv, 'jdplus/toolkit/base/api/data/DoubleSeq$Mutable'))
    ep$process(.jcast(J('jdplus.toolkit.base.api.data.Doubles')$of(
      .jarray(target)), 'jdplus/toolkit/base/api/data/DoubleSeq'),
      J('jdplus.toolkit.base.core.data.DataBlock')$of(a))
    if (!is.null(.jgetEx(TRUE))) stop('pinned endpoint recomposition failed for ', ca$id)
    same(.jcall(J('jdplus.toolkit.base.core.data.DataBlock')$of(a), '[D', 'toArray'), got)
  }
  stopifnot(recompose(tab$D6, tab$D7), recompose(tab$D11bis, tab$D12))
  # MSRE diagnostic table: computed on the dropped original calendar at the
  # original-calendar position, reproduced through the pinned factory.
  msr <- dstep$getD9msr()
  position <- (first0 + back) %% f
  mt <- J('jdplus.x13.base.core.x11.X11Utility')$defaultMsrTable(
    dstep$getD9_g_bis()$drop(as.integer(back), as.integer(fc)), as.integer(f),
    as.integer(position), mode_add)
  ri <- msr$getMeanIrregularEvolutions(); rs <- msr$getMeanSeasonalEvolutions()
  msrphase <- vapply(seq_len(f) - 1L, function(p) msr$getMsr(as.integer(p)), double(1))
  ok <- function(a, b) all((a == b) | (is.na(a) & is.na(b)))
  # Per-phase difference counts are the pinned stride counts; MsrTable keeps
  # them private, so they are recorded from the pinned loop arithmetic
  # (count = floor((m - 1 - start) / f) over the dropped calendar).
  m <- n - back - fc
  start <- ((seq_len(f) - 1L - position) %% f) + 1L
  counts <- pmax(0L, as.integer((m - start) %/% f))
  stopifnot(msr$getCount() == f, length(ri) == f, ok(mt$getMeanIrregularEvolutions(), ri),
    ok(mt$getMeanSeasonalEvolutions(), rs), ok(mt$getGlobalMsr(), msr$getGlobalMsr()),
    ok(msrphase, ri / rs), all(counts >= 0))
  if (back == 0L && fc == 0L) {
    # Regression linkage: the C pass this D pass consumed is exactly the
    # installed x11-cpass fixture for the same case.
    for (nm in c('C13', 'C17', 'C20')) {
      want <- as.numeric(cpass$value[cpass$case == ca$id & cpass$table == nm])
      got <- switch(nm, C13 = c13, C17 = c17, C20 = c20)
      res <- (got == want) | (is.nan(got) & is.nan(want)) | (is.na(got) & is.na(want))
      if (!all(res %in% TRUE) || length(got) != length(want)) {
        d <- which(!(res %in% TRUE))[1]
        if (is.na(d)) d <- length(want) + 1L
        stop('C fixture link differs: ', ca$id, ' ', nm, ' row ', d - 1L,
          ' got[', d, ']=', got[d], ' want[', d, ']=', want[d],
          ' len ', length(got), '/', length(want), call. = FALSE)
      }
    }
  } else {
    # The original calendar of the extended capture is the untouched base input.
    vv <- y$values; vv[y$missing] <- NaN
    stopifnot(same(series_values[(back + 1L):(n - fc)], vv))
  }
  id <- if (back == 0L && fc == 0L) ca$id else paste0('ext-', ca$id)
  stored <- series_values
  stored[missing] <- y$values[missing] # inputs.csv keeps finite placeholders with the flag
  inputs[[id]] <<- data.frame(case = id, row = seq_len(n) - 1L,
    value = stored, missing = missing)
  tables[[id]] <<- do.call(rbind, lapply(names, function(nm)
    data.frame(case = id, table = nm, row = seq_along(tab[[nm]]) - 1L,
      value = tab[[nm]])))
  msrrows[[id]] <<- data.frame(case = id, phase = seq_len(f) - 1L, ri = ri, rs = rs,
    msr = msrphase, count = counts)
  cases[[id]] <<- data.frame(case = id, category = if (back == 0L) ca$category else 'extended',
    frequency = as.integer(f), n = as.integer(n), start_period = as.integer(first0 + 1L),
    trend = as.integer(ca$trend), lsig = ca$lsig, usig = ca$usig,
    d2drop = as.integer(dstep$getD2drop()), initial_filters = paste(ca$initial, collapse = ' '),
    final_filters = paste(ca$final, collapse = ' '), back = as.integer(back),
    fc = as.integer(fc), orig_n = as.integer(length(y$values)),
    orig_start_period = as.integer(ca$start_period),
    global_msr = msr$getGlobalMsr(), d9_nonnan = sum(!is.nan(tab$D9)),
    corrected_rc = sum(c20 != 0))
}
for (ca in x11_bpass_cases()) {
  capture(ca, 0L, 0L)
  ext <- Filter(function(e) e[1] == ca$id, extensions)
  if (length(ext)) capture(ca, as.integer(ext[[1]][2]), as.integer(ext[[1]][3]))
}
# 17 significant digits round-trip IEEE doubles exactly; R's write.csv default
# 15-digit format would silently quantize the pinned values.
fixtures_fmt <- function(d) {
  data.frame(lapply(d, function(col) if (is.numeric(col)) sprintf('%.17g', col) else col),
    stringsAsFactors = FALSE, check.names = FALSE)
}
write.csv(fixtures_fmt(do.call(rbind, inputs)), file.path(output, 'inputs.csv'), row.names = FALSE)
write.csv(fixtures_fmt(do.call(rbind, cases)), file.path(output, 'cases.csv'), row.names = FALSE)
write.csv(fixtures_fmt(do.call(rbind, tables)), file.path(output, 'tables.csv'), row.names = FALSE)
write.csv(fixtures_fmt(do.call(rbind, msrrows)), file.path(output, 'msr.csv'), row.names = FALSE)
files <- c(normalizePath(script), fixtures,
  file.path(root, 'validation/fixtures/native/x11-cpass/tables.csv'))
java <- list(version = .jcall('java/lang/System', 'Ljava/lang/String;', 'getProperty', 'java.version'),
  vendor = .jcall('java/lang/System', 'Ljava/lang/String;', 'getProperty', 'java.vendor'),
  runtime_version = .jcall('java/lang/System', 'Ljava/lang/String;', 'getProperty', 'java.runtime.version'),
  vm_name = .jcall('java/lang/System', 'Ljava/lang/String;', 'getProperty', 'java.vm.name'),
  architecture = .jcall('java/lang/System', 'Ljava/lang/String;', 'getProperty', 'os.arch'),
  oracle = 'rjd3 3.9.0/3.9.1 pinned jars via rjd3jars classpath; X11Kernel composition X11BStep.process(series, ctx) + X11CStep.process(series, B20, ctx, cv) + X11DStep.process(series, C20, ctx, cv) with fixed per-phase SeasonalFilterOption arrays, fixed Henderson trendFilterLength, additive mode; extended cases set backcastHorizon/forecastHorizon with a hand-extended series and shifted firstPeriod; all public D getters D1..D13 (D9g is null in the fixed corrector mode), getD9msr, getSeasFilter, getD2drop and the Henderson lengths')
metadata <- list(schema_version = 1L,
  captured_utc = format(Sys.time(), tz = 'UTC', usetz = TRUE),
  source_sha256 = setNames(reference_sha256(files), basename(files)),
  java = java, R = R.version.string, cases = length(cases),
  public_scope = 'Pinned X11DStep fixed additive D-pass tables only (D1, D2, D4, D5, D6, D7, D8, D9, D9bis, D9_g_bis, D10, D10bis, D11, D11bis, D12, D13, the D9 MSRE diagnostic table and the seasFilter echo) composed from the fixed additive B and C passes exactly like X11Kernel, at zero horizon and on four hand-extended calendars with context backcast/forecast horizons; the MSRE filter selection, automatic Henderson (getICRatio is diagnostic-only and not captured), cross-validation, the period-specific corrector (D9g), multiplicative modes and ARIMA-regression inputs are not claimed')
writeLines(sub('[[:blank:]]+$', '', capture.output(dput(metadata))), file.path(output, 'metadata.R'))
captured <- file.path(output, c('inputs.csv', 'cases.csv', 'tables.csv', 'msr.csv', 'metadata.R'))
write.csv(data.frame(file = basename(captured), sha256 = reference_sha256(captured)),
  file.path(output, 'SHA256SUMS.csv'), row.names = FALSE)
cat('Captured', length(cases), 'pinned fixed additive D-pass cases;',
  sum(vapply(tables, nrow, integer(1))), 'table rows\n')
