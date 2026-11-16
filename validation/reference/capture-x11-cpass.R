# Pinned-reference capture for items 22b.4 S2 (additive), 22b.5 M3a
# (multiplicative), 22b.5 M3b-i (log-additive) and 22b.5 M3b-ii
# (pseudo-additive): the complete fixed C-pass tables of
# jdplus.x13.base.core.x11.X11CStep resp. pseudoadd.X11CStepPseudoAdd
# (pinned rjd3 3.9.0 jars),
# composed exactly like X11Kernel: X11BStep.process(series, ctx) followed by
# X11CStep.process(series, bstep.getB20(), ctx) with fixed per-phase seasonal
# filters and a fixed Henderson trend filter. No old corpus and no production
# output is input; the capture re-derives every additive identity from the
# captured tables and the captured B pass itself and refuses to write on
# disagreement.
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
  stop('Usage: capture-x11-cpass.R [--mode=Additive|Multiplicative|LogAdditive|PseudoAdditive] --output=<existing empty directory>')
stopifnot(mode_name %in% c('Additive', 'Multiplicative', 'LogAdditive', 'PseudoAdditive'))
# The pinned truth table: isMultiplicative() is (mode == Multiplicative ||
# mode == PseudoAdditive), so LogAdditive keeps the additive ctx.remove,
# the 0-valued mode default and no makePositivity guards, while
# PseudoAdditive keeps the multiplicative context (ratio removes on the
# sites it does NOT override, mode default 1, makePositivity guards) and
# only replaces c1/c6/c11 with the PseudoAddUtility forms below.
mult <- mode_name == 'Multiplicative'
logadd <- mode_name == 'LogAdditive'
padd <- mode_name == 'PseudoAdditive'
mode_label <- switch(mode_name, Additive = 'additive', Multiplicative = 'multiplicative',
  LogAdditive = 'log-additive', PseudoAdditive = 'pseudo-additive')
dflt <- if (mult || padd) 1 else 0 # the pinned mode default (DummyFilter/correction factor)
rem <- function(a, b) if (mult || padd) a / b else a - b # pinned ctx.remove
# PseudoAddUtility forms (bytecode PADD_UTIL.txt): removeSeasonalAdjusted
# (a, b, c) is a - c*(b - 1) (the lambda captures (arg0, arg2, arg1));
# adjustRefSeries(ref, corr, s, r) is ref + s*(r/corr - r);
# removeSeasonalAdjustedWithBorders(x, s, f, drop) keeps the ratio x/s at
# the border rows and takes x - f*(s - 1) on the dropped interior rows.
padd_seasonal_adjusted <- function(a, b, c) a - c * (b - 1)
padd_adjust_ref <- function(ref, corr, s, r) ref + s * (r / corr - r)
padd_with_borders <- function(x, s, f, t) {
  out <- x / s
  i <- (t + 1L):(length(x) - t)
  out[i] <- x[i] - f * (s[i] - 1)
  out
}
output <- normalizePath(sub('^--output=', '', args), mustWork = TRUE)
if (!dir.exists(output) || length(list.files(output, all.files = TRUE, no.. = TRUE)))
  stop('Output must be empty')
# The strictly positive multiplicative shapes are the only inventory with
# the positivity the pinned kernel requires of the log-additive mode; the
# pseudo-additive pipeline runs the ratio context too, so it reuses them.
fixtures <- file.path(root, 'validation/evidence/native',
  if (mult || logadd || padd) 'x11-bpass-mult-cases.R' else 'x11-bpass-cases.R')
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
  if (mult || logadd) { # the pinned X11Kernel.check() positivity gate
    stopifnot(all(values[!is.nan(values)] > 0))
    # The pinned X11Kernel.process input transform: both steps run on
    # DoubleSeq.log() under LogAdditive; inputs.csv keeps the raw values.
    if (logadd) values <- log(values)
  }
  seqi <- .jcast(J('jdplus.toolkit.base.api.data.Doubles')$of(.jarray(values)),
    'jdplus/toolkit/base/api/data/DoubleSeq')
  ctx <- J('jdplus.x13.base.core.x11.X11Context')$builder()$mode(mode_ref)$
    period(as.integer(ca$frequency))$firstPeriod(as.integer(ca$start_period - 1L))$
    seasonal(TRUE)$trendFilterLength(as.integer(ca$trend))$
    initialSeasonalFilter(enums(ca$initial))$finalSeasonalFilter(enums(ca$final))$
    lowerSigma(ca$lsig)$upperSigma(ca$usig)$
    backcastHorizon(0L)$forecastHorizon(0L)$excludefcast(FALSE)$build()
  resolved_initial <- resolved(ctx, 'getInitialSeasonalFilter')
  resolved_final <- resolved(ctx, 'getFinalSeasonalFilter')
  stopifnot(identical(resolved_initial, as.integer(ca$initial)),
    identical(resolved_final, as.integer(ca$final))) # fixed requests pass through
  b <- .jnew(if (padd) 'jdplus.x13.base.core.x11.pseudoadd.X11BStepPseudoAdd'
    else 'jdplus.x13.base.core.x11.X11BStep')
  .jcall(b, 'V', 'process', seqi, ctx)
  pending <- .jgetEx(TRUE) # void calls do not raise in rJava; fail loudly instead
  if (!is.null(pending)) stop('pinned B pass failed for case ', ca$id, ': ', pending$message)
  stopifnot(is.null(b$getCvSeasonalFilter())) # fixed requests never engage CV
  bnames <- c('B2', 'B13', 'B20', if (padd) 'B7') # B7 feeds the pseudo C-step
  bgrab <- function(nm) {
    s <- .jcall(b, 'Ljdplus/toolkit/base/api/data/DoubleSeq;', paste0('get', nm))
    if (is.null(s)) stop('missing table ', nm, ' in case ', ca$id)
    s$toArray()
  }
  btab <- lapply(bnames, bgrab); names(btab) <- bnames
  # X11CStepPseudoAdd carries the kernel's finished B7/B13 (constructor);
  # its c1 = adjustRefSeries(ref, B20, B7, B13) override consumes them.
  cstep <- if (padd) .jnew('jdplus.x13.base.core.x11.pseudoadd.X11CStepPseudoAdd',
      .jcall(b, 'Ljdplus/toolkit/base/api/data/DoubleSeq;', 'getB7'),
      .jcall(b, 'Ljdplus/toolkit/base/api/data/DoubleSeq;', 'getB13'))
    else .jnew('jdplus.x13.base.core.x11.X11CStep')
  .jcall(cstep, 'V', 'process', seqi, .jcall(b, 'Ljdplus/toolkit/base/api/data/DoubleSeq;', 'getB20'), ctx)
  pending <- .jgetEx(TRUE)
  if (!is.null(pending)) stop('pinned C pass failed for case ', ca$id, ': ', pending$message)
  stopifnot(is.null(cstep$getCvSeasonalFilter())) # CV stays disengaged in the C pass
  names <- c('C1', 'C2', 'C4', 'C5', 'C5a', 'C6', 'C7', 'C9', 'C10', 'C10a',
    'C11', 'C13', 'C17', 'C20')
  grab <- function(nm) {
    s <- .jcall(cstep, 'Ljdplus/toolkit/base/api/data/DoubleSeq;', paste0('get', nm))
    if (is.null(s)) stop('missing table ', nm, ' in case ', ca$id)
    s$toArray()
  }
  tab <- lapply(names, grab); names(tab) <- names
  ref <- cstep$getRefSeries()$toArray()
  dropc <- cstep$getC2drop()
  n <- ca$n; f <- ca$frequency; drop <- f %/% 2L
  stopifnot(dropc == drop)
  same <- function(a, bb) {
    length(a) == length(bb) && all((a == bb) | (is.nan(a) & is.nan(bb)))
  }
  cropped <- function(v) v[(drop + 1L):(n - drop)]
  # Public additive identities of the captured tables, checked exactly against
  # the captured input, the captured B20 and each other; the pseudo-additive
  # overrides (c1, c6, c11) take their PseudoAddUtility forms and every
  # other site keeps the ratio context the subclass inherits.
  stopifnot(same(ref, values),
    length(tab$C1) == n, length(tab$C2) == n - 2 * drop, length(tab$C17) == n,
    length(tab$C20) == n,
    length(tab$C4) == n - 2 * drop, length(tab$C5a) == n - 2 * drop,
    length(tab$C5) == n, length(tab$C10a) == n,
    same(tab$C1, if (padd) padd_adjust_ref(values, btab$B20, btab$B7, btab$B13)
      else rem(values, btab$B20)),
    same(tab$C4, rem(cropped(tab$C1), tab$C2)),
    same(tab$C6, if (padd) padd_with_borders(tab$C1, tab$C5, tab$C2, drop)
      else rem(tab$C1, tab$C5)),
    same(tab$C9, rem(tab$C1, tab$C7)),
    same(tab$C11, if (padd) padd_seasonal_adjusted(values, tab$C10, tab$C7)
      else rem(values, tab$C10)),
    same(tab$C13, rem(tab$C11, tab$C7)))
  # The pinned B2 filter is the same X11TrendCycleFilterFactory filter the C
  # pass applies to C1: qualify the C2 interior against a fresh application.
  tf <- J('jdplus/x13/base/core/x11/filter/X11TrendCycleFilterFactory')$
    makeTrendFilter(as.integer(f))
  c1seq <- .jcast(J('jdplus.toolkit.base.api.data.Doubles')$of(.jarray(tab$C1)),
    'jdplus/toolkit/base/api/data/DoubleSeq')
  side <- .jcall(tf, 'I', 'length') %/% 2L
  blk <- J('jdplus.toolkit.base.core.data.DataBlock')$of(
    .jarray(rep(NaN, n - 2 * side)), 0L, as.integer(n - 2 * side))
  .jcall(tf, 'V', 'apply', c1seq,
    .jcast(blk, 'jdplus/toolkit/base/api/data/DoubleSeq$Mutable'))
  pending <- .jgetEx(TRUE)
  if (!is.null(pending)) stop('pinned C2 filter failed for case ', ca$id, ': ', pending$message)
  interior <- .jcall(blk, '[D', 'toArray')
  stopifnot(side == drop, same(interior, tab$C2))
  # Final-stage correction: unit weights carry zero factors and no NaN in the
  # delivered observation weights.
  unit <- tab$C17 == 1
  stopifnot(all(tab$C20[unit] == dflt), !any(is.nan(tab$C17)))
  # Multiplicative and pseudo-additive (isMultiplicative() is true for both):
  # the c7Step makePositivity wiring, qualified exactly
  # as in the B-pass capture (rebuild C7 from the pinned filter parts over
  # C6, repair through the pinned static, compare to the getter; the repair
  # count records how many rows the guard fixed).
  c7_repair <- 0L
  if (mult || padd) {
    htf <- ctx$trendFilter()
    r <- J('jdplus/x13/base/core/x11/filter/MusgraveFilterFactory')$findR(
      as.integer(ca$trend), as.integer(ca$frequency))
    afs <- ctx$asymmetricTrendFilters(htf, r)
    ep <- .jnew('jdplus/x13/base/core/x11/filter/endpoints/AsymmetricEndPoints', afs, 0L)
    p <- .jcall(htf, 'I', 'length') %/% 2L
    a <- .jarray(rep(NaN, n))
    blkv <- J('jdplus.toolkit.base.core.data.DataBlock')$of(a, p, n - p)
    seq6 <- .jcall(cstep, 'Ljdplus/toolkit/base/api/data/DoubleSeq;', 'getC6')
    .jcall(htf, 'V', 'apply', seq6, .jcast(blkv, 'jdplus/toolkit/base/api/data/DoubleSeq$Mutable'))
    ep$process(seq6, J('jdplus.toolkit.base.core.data.DataBlock')$of(a))
    if (!is.null(.jgetEx(TRUE))) stop('pinned C7 rebuild failed for case ', ca$id)
    raw7 <- .jcall(J('jdplus.toolkit.base.core.data.DataBlock')$of(a), '[D', 'toArray')
    fixed7 <- .jcall('jdplus/x13/base/core/x11/X11Context',
      'Ljdplus/toolkit/base/api/data/DoubleSeq;', 'makePositivity',
      .jcast(J('jdplus.toolkit.base.api.data.Doubles')$of(.jarray(raw7)),
        'jdplus/toolkit/base/api/data/DoubleSeq'))$toArray()
    c7_repair <- sum(!(raw7 == fixed7 | (is.nan(raw7) & is.nan(fixed7))))
    stopifnot(same(fixed7, tab$C7))
  }
  inputs[[ca$id]] <- data.frame(case = ca$id, row = seq_len(n) - 1L,
    value = y$values, missing = y$missing)
  tables[[ca$id]] <- do.call(rbind, lapply(names, function(nm)
    data.frame(case = ca$id, table = nm, row = seq_along(tab[[nm]]) - 1L,
      value = tab[[nm]])))
  cases[[ca$id]] <- data.frame(case = ca$id, category = ca$category,
    frequency = as.integer(f), n = as.integer(n),
    start_period = as.integer(ca$start_period), trend = as.integer(ca$trend),
    lsig = ca$lsig, usig = ca$usig, c2drop = as.integer(dropc),
    initial_filters = paste(ca$initial, collapse = ' '),
    final_filters = paste(ca$final, collapse = ' '),
    resolved_initial = paste(resolved_initial, collapse = ' '),
    resolved_final = paste(resolved_final, collapse = ' '),
    flagged_rc = sum(!unit), corrected_rc = sum(tab$C20 != dflt))
  if (mult || logadd || padd) cases[[ca$id]]$mode <- mode_name # mode-carrying schemas
  if (mult || padd) cases[[ca$id]]$c7_repair <- c7_repair # keep the additive schema byte-exact
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
  oracle = paste0('rjd3 3.9.0/3.9.1 pinned jars via rjd3jars classpath; X11Kernel composition ',
    if (padd) 'X11BStepPseudoAdd.process(series, ctx) + X11CStepPseudoAdd(B7, B13).process(series, B20, ctx)'
      else 'X11BStep.process(series, ctx) + X11CStep.process(series, B20, ctx)', ' on',
    if (logadd) ' DoubleSeq.log() of the strictly positive input (the pinned kernel input transform) with' else '',
    ' fixed per-phase SeasonalFilterOption arrays, fixed Henderson trendFilterLength, ',
    mode_label, ' mode, zero horizons; all public C getters C1..C20 and getC2drop',
    if (padd) ' (the pseudo-additive overrides c1/c6/c11 re-derived through the pinned PseudoAddUtility forms)' else '',
    if (mult || padd) ' plus the pinned makePositivity rebuild cross-check of C7' else ''))
metadata <- list(schema_version = 1L,
  captured_utc = format(Sys.time(), tz = 'UTC', usetz = TRUE),
  source_sha256 = setNames(reference_sha256(files), basename(files)),
  java = java, R = R.version.string, cases = length(cases),
  public_scope = switch(mode_name,
    Multiplicative = 'Pinned X11CStep fixed multiplicative C-pass tables only (C1, C2, C4, C5, C5a, C6, C7, C9, C10, C10a, C11, C13, C17, C20) composed from the fixed multiplicative B pass exactly like X11Kernel, with fixed seasonal filters, fixed Henderson and multiplicative mode, including the c7Step makePositivity repair cross-check and repair counts; the D pass, automatic filter choice, cross-validation, log-additive and pseudo-additive modes and ARIMA-regression inputs are not claimed',
    LogAdditive = 'Pinned X11CStep fixed log-additive C-pass tables only (C1, C2, C4, C5, C5a, C6, C7, C9, C10, C10a, C11, C13, C17, C20) composed from the fixed log-additive B pass exactly like X11Kernel, with fixed seasonal filters, fixed Henderson and log-additive mode: the tables are the log-domain step tables of the pinned pipeline (additive arithmetic on DoubleSeq.log() of the strictly positive input, no makePositivity guards because isMultiplicative() is false); the results-builder exp of D10..D13 and the bias-correction skip are export-stage behaviour and are not claimed; the D pass, automatic filter choice, cross-validation, multiplicative and pseudo-additive modes and ARIMA-regression inputs are not claimed',
    PseudoAdditive = 'Pinned X11CStepPseudoAdd fixed pseudo-additive C-pass tables only (C1, C2, C4, C5, C5a, C6, C7, C9, C10, C10a, C11, C13, C17, C20) composed from the fixed pseudo-additive B pass exactly like X11Kernel (the C step carries the B7/B13 of that run), with fixed seasonal filters, fixed Henderson and pseudo-additive mode: the multiplicative ratio context of isMultiplicative() at every site the subclass does not override (C4, C9, C13, the sigma/correction arithmetic with mode default 1, the makePositivity-repaired C7 with repair counts) plus the pinned PseudoAddUtility overrides c1 = ref + B7*(B13/B20 - B13), c6 with ratio border rows and interior C1 - C2*(C5 - 1) and c11 = ref - C7*(C10 - 1), each cross-checked against the same IEEE form; the D pass, automatic filter choice, cross-validation, multiplicative and log-additive modes and ARIMA-regression inputs are not claimed',
    'Pinned X11CStep fixed additive C-pass tables only (C1, C2, C4, C5, C5a, C6, C7, C9, C10, C10a, C11, C13, C17, C20) composed from the fixed additive B pass exactly like X11Kernel, with fixed seasonal filters, fixed Henderson and additive mode; the D pass, automatic filter choice, cross-validation, multiplicative modes and ARIMA-regression inputs are not claimed'))
writeLines(sub('[[:blank:]]+$', '', capture.output(dput(metadata))), file.path(output, 'metadata.R'))
captured <- file.path(output, c('inputs.csv', 'cases.csv', 'tables.csv', 'metadata.R'))
write.csv(data.frame(file = basename(captured), sha256 = reference_sha256(captured)),
  file.path(output, 'SHA256SUMS.csv'), row.names = FALSE)
cat('Captured', length(cases), 'pinned fixed', mode_label, 'C-pass cases;',
  sum(vapply(tables, nrow, integer(1))), 'table rows\n')
