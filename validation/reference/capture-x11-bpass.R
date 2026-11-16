# Pinned-reference capture for item 22b.4 S1: the complete fixed additive
# B-pass tables of jdplus.x13.base.core.x11.X11BStep (pinned rjd3 3.9.0 jars,
# driven through its public API with fixed per-phase seasonal filters and a
# fixed Henderson trend filter). No old corpus and no production output is
# input; the capture re-derives every additive identity from the captured
# tables itself and refuses to write on disagreement.
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
  stop('Usage: capture-x11-bpass.R [--mode=Additive|Multiplicative] --output=<existing empty directory>')
stopifnot(mode_name %in% c('Additive', 'Multiplicative'))
mult <- mode_name == 'Multiplicative'
dflt <- if (mult) 1 else 0 # the pinned mode default (DummyFilter/correction factor)
rem <- function(a, b) if (mult) a / b else a - b # pinned ctx.remove
output <- normalizePath(sub('^--output=', '', args), mustWork = TRUE)
if (!dir.exists(output) || length(list.files(output, all.files = TRUE, no.. = TRUE)))
  stop('Output must be empty')
fixtures <- file.path(root, 'validation/fixtures/native',
  if (mult) 'x11-bpass-mult-cases.R' else 'x11-bpass-cases.R')
for (fixture in fixtures) source(fixture)
suppressWarnings(suppressMessages({
  library(rjd3jars); library(rjd3toolkit); library(rjd3x13); library(rJava)
}))
.jinit()
mode_ref <- J('jdplus.sa.base.api.DecompositionMode')$valueOf(mode_name)
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
tables <- list(); cases <- list(); inputs <- list()
for (ca in x11_bpass_cases()) {
  y <- x11_bpass_input(ca)
  values <- y$values; values[y$missing] <- NaN
  seqi <- .jcast(J('jdplus.toolkit.base.api.data.Doubles')$of(.jarray(values)),
    'jdplus/toolkit/base/api/data/DoubleSeq')
  ctx <- J('jdplus.x13.base.core.x11.X11Context')$builder()$mode(mode_ref)$
    period(as.integer(ca$frequency))$firstPeriod(as.integer(ca$start_period - 1L))$
    seasonal(TRUE)$trendFilterLength(as.integer(ca$trend))$
    initialSeasonalFilter(enums(ca$initial))$finalSeasonalFilter(enums(ca$final))$
    lowerSigma(ca$lsig)$upperSigma(ca$usig)$
    backcastHorizon(0L)$forecastHorizon(0L)$excludefcast(FALSE)$build()
  get_filter <- function(which) resolved(ctx, which)
  resolved_initial <- get_filter('getInitialSeasonalFilter')
  resolved_final <- get_filter('getFinalSeasonalFilter')
  stopifnot(identical(resolved_initial, as.integer(ca$initial)),
    identical(resolved_final, as.integer(ca$final))) # fixed requests pass through
  b <- .jnew('jdplus.x13.base.core.x11.X11BStep')
  .jcall(b, 'V', 'process', seqi, ctx)
  pending <- .jgetEx(TRUE) # void calls do not raise in rJava; fail loudly instead
  if (!is.null(pending)) stop('pinned B pass failed for case ', ca$id, ': ', pending$message)
  stopifnot(is.null(b$getCvSeasonalFilter())) # fixed requests never engage CV
  names <- c('B1', 'B2', 'B3', 'B4', 'B4a', 'B4anorm', 'B4d', 'B4g', 'B5', 'B6',
    'B7', 'B8', 'B9', 'B9g', 'B10', 'B11', 'B13', 'B17', 'B20')
  grab <- function(nm) {
    s <- .jcall(b, 'Ljdplus/toolkit/base/api/data/DoubleSeq;', paste0('get', nm))
    if (is.null(s)) stop('missing table ', nm, ' in case ', ca$id)
    s$toArray()
  }
  tab <- lapply(names, grab); names(tab) <- names
  drop <- b$getB2drop()
  n <- ca$n; f <- ca$frequency
  stopifnot(drop == f %/% 2L)
  cropped <- function(v) v[(drop + 1L):(n - drop)]
  same <- function(a, b) {
    length(a) == length(b) && all((a == b) | (is.nan(a) & is.nan(b)))
  }
  # Public additive identities of the captured tables, checked exactly.
  stopifnot(same(tab$B1, values), length(tab$B2) == n - 2 * drop,
    length(tab$B4a) == n - 2 * drop, length(tab$B5) == n, length(tab$B17) == n,
    length(tab$B20) == n,
    same(tab$B3, rem(cropped(tab$B1), tab$B2)),
    same(tab$B4d, rem(tab$B3, tab$B4anorm)),
    same(tab$B4g, ifelse(is.nan(tab$B4), tab$B3, tab$B4)),
    same(tab$B6, rem(tab$B1, tab$B5)),
    same(tab$B8, rem(tab$B1, tab$B7)),
    same(tab$B9g, ifelse(is.nan(tab$B9), tab$B8, tab$B9)),
    same(tab$B11, rem(tab$B1, tab$B10)),
    same(tab$B13, rem(tab$B11, tab$B7)))
  # The final-stage correction cancels on unit-weight rows and the last two
  # tables follow the pinned getter defaults there.
  unit <- tab$B17 == 1
  stopifnot(all(tab$B20[unit] == dflt))
  # Multiplicative only: rebuild the un-repaired B7 from the pinned filter
  # parts (the same trendFilter/asymmetricTrendFilters/AsymmetricEndPoints
  # calls the b7Step makes) and qualify the pinned makePositivity wiring:
  # repairing the rebuild through the pinned static must equal the getter's
  # B7 exactly, and the repair count records how many rows the guard fixed.
  b7_repair <- 0L
  if (mult) {
    htf <- ctx$trendFilter()
    r <- J('jdplus/x13/base/core/x11/filter/MusgraveFilterFactory')$findR(
      as.integer(ca$trend), as.integer(ca$frequency))
    afs <- ctx$asymmetricTrendFilters(htf, r)
    ep <- .jnew('jdplus/x13/base/core/x11/filter/endpoints/AsymmetricEndPoints', afs, 0L)
    p <- .jcall(htf, 'I', 'length') %/% 2L
    a <- .jarray(rep(NaN, n))
    blkv <- J('jdplus.toolkit.base.core.data.DataBlock')$of(a, p, n - p)
    seq6 <- .jcall(b, 'Ljdplus/toolkit/base/api/data/DoubleSeq;', 'getB6')
    .jcall(htf, 'V', 'apply', seq6, .jcast(blkv, 'jdplus/toolkit/base/api/data/DoubleSeq$Mutable'))
    ep$process(seq6, J('jdplus.toolkit.base.core.data.DataBlock')$of(a))
    if (!is.null(.jgetEx(TRUE))) stop('pinned B7 rebuild failed for case ', ca$id)
    raw7 <- .jcall(J('jdplus.toolkit.base.core.data.DataBlock')$of(a), '[D', 'toArray')
    fixed7 <- .jcall('jdplus/x13/base/core/x11/X11Context',
      'Ljdplus/toolkit/base/api/data/DoubleSeq;', 'makePositivity',
      .jcast(J('jdplus.toolkit.base.api.data.Doubles')$of(.jarray(raw7)),
        'jdplus/toolkit/base/api/data/DoubleSeq'))$toArray()
    b7_repair <- sum(!(raw7 == fixed7 | (is.nan(raw7) & is.nan(fixed7))))
    stopifnot(same(fixed7, tab$B7))
  }
  # Round-2 detection intermediate (pinned protected b9d semantics) rebuilt
  # on the public factory/normalizer/context APIs over the captured B8, so
  # the native correction history is qualified at table level too.
  proc <- J('jdplus/x13/base/core/x11/filter/X11SeasonalFiltersFactory')$filter(
    as.integer(ca$frequency), enums(ca$final))
  seq8 <- .jcall(b, 'Ljdplus/toolkit/base/api/data/DoubleSeq;', 'getB8')
  s9 <- .jcall(proc, 'Ljdplus/toolkit/base/api/data/DoubleSeq;', 'process', seq8,
    .jcall(ctx, 'I', 'getPosition', 0L))
  n9 <- .jcall(J('jdplus/x13/base/core/x11/filter/DefaultSeasonalNormalizer'),
    'Ljdplus/toolkit/base/api/data/DoubleSeq;', 'normalize', s9, 0L, ctx)$toArray()
  seq9 <- .jcast(J('jdplus.toolkit.base.api.data.Doubles')$of(.jarray(n9)),
    'jdplus/toolkit/base/api/data/DoubleSeq')
  d9 <- .jcall(ctx, 'Ljdplus/toolkit/base/api/data/DoubleSeq;', 'remove', seq8, seq9)
  if (is.null(d9)) stop('missing b9d in case ', ca$id)
  tab[["B9d"]] <- d9$toArray()
  stopifnot(length(tab[["B9d"]]) == n, same(tab[["B9d"]], rem(tab$B8, n9)))
  inputs[[ca$id]] <- data.frame(case = ca$id, row = seq_len(n) - 1L,
    value = y$values, missing = y$missing)
  tables[[ca$id]] <- do.call(rbind, lapply(c(names, 'B9d'), function(nm)
    data.frame(case = ca$id, table = nm, row = seq_along(tab[[nm]]) - 1L,
      value = tab[[nm]])))
  cases[[ca$id]] <- data.frame(case = ca$id, category = ca$category,
    frequency = as.integer(f), n = as.integer(n),
    start_period = as.integer(ca$start_period), trend = as.integer(ca$trend),
    lsig = ca$lsig, usig = ca$usig, b2drop = as.integer(drop),
    initial_filters = paste(ca$initial, collapse = ' '),
    final_filters = paste(ca$final, collapse = ' '),
    resolved_initial = paste(resolved_initial, collapse = ' '),
    resolved_final = paste(resolved_final, collapse = ' '),
    replacements_r1 = sum(!is.nan(tab$B4)), replacements_r2 = sum(!is.nan(tab$B9)),
    flagged_r3 = sum(!unit))
  if (mult) cases[[ca$id]]$mode <- mode_name # multiplicative-only columns keep
  if (mult) cases[[ca$id]]$b7_repair <- b7_repair # the additive schema byte-exact
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
files <- c(normalizePath(script), fixtures)
java <- list(version = .jcall('java/lang/System', 'Ljava/lang/String;', 'getProperty', 'java.version'),
  vendor = .jcall('java/lang/System', 'Ljava/lang/String;', 'getProperty', 'java.vendor'),
  runtime_version = .jcall('java/lang/System', 'Ljava/lang/String;', 'getProperty', 'java.runtime.version'),
  vm_name = .jcall('java/lang/System', 'Ljava/lang/String;', 'getProperty', 'java.vm.name'),
  architecture = .jcall('java/lang/System', 'Ljava/lang/String;', 'getProperty', 'os.arch'),
  oracle = paste0('rjd3 3.9.0/3.9.1 pinned jars via rjd3jars classpath; X11BStep.process with fixed per-phase SeasonalFilterOption arrays, fixed Henderson trendFilterLength, ',
    tolower(mode_name), ' mode, zero horizons; all public B getters B1..B20',
    if (mult) ' plus the pinned makePositivity rebuild cross-check of B7' else ''))
metadata <- list(schema_version = 1L,
  captured_utc = format(Sys.time(), tz = 'UTC', usetz = TRUE),
  source_sha256 = setNames(reference_sha256(files), basename(files)),
  java = java, R = R.version.string, cases = length(cases),
  public_scope = if (mult) paste('Pinned X11BStep fixed multiplicative B-pass tables only (B1, B2, B3, B4, B4a, B4anorm, B4d, B4g, B5, B6, B7, B8, B9, B9g, B10, B11, B13, B17, B20) with fixed seasonal filters, fixed Henderson and multiplicative mode, including the b7Step makePositivity repair cross-check and repair counts; the C/D passes, automatic filter choice, cross-validation, log-additive and pseudo-additive modes and ARIMA-regression inputs are not claimed') else 'Pinned X11BStep fixed additive B-pass tables only (B1, B2, B3, B4, B4a, B4anorm, B4d, B4g, B5, B6, B7, B8, B9, B9g, B10, B11, B13, B17, B20) with fixed seasonal filters, fixed Henderson and additive mode; C/D passes, automatic filter choice, cross-validation, multiplicative modes and ARIMA-regression inputs are not claimed')
writeLines(sub('[[:blank:]]+$', '', capture.output(dput(metadata))), file.path(output, 'metadata.R'))
captured <- file.path(output, c('inputs.csv', 'cases.csv', 'tables.csv', 'metadata.R'))
write.csv(data.frame(file = basename(captured), sha256 = reference_sha256(captured)),
  file.path(output, 'SHA256SUMS.csv'), row.names = FALSE)
cat('Captured', length(cases), 'pinned fixed', tolower(mode_name), 'B-pass cases;',
  sum(vapply(tables, nrow, integer(1))), 'table rows\n')
