# Pinned-reference capture for item 22b.3b: same-phase replacement values,
# neighbor provenance, correction factors, period averages and the
# applyCorrections identity of DefaultExtremeValuesCorrector (pinned rjd3
# 3.9.0 jars, driven through its public API after the 22b.3a analyse pass;
# the private neighbor walk is resolved through JNI for provenance only).
# No old corpus and no production output is input.
args <- commandArgs(trailingOnly = FALSE)
script <- sub('^--file=', '', args[startsWith(args, '--file=')][[1L]])
tooling <- dirname(normalizePath(script)); source(file.path(tooling, 'common.R'))
root <- reference_root()
args <- commandArgs(trailingOnly = TRUE)
if (length(args) != 1L || !startsWith(args, '--output='))
  stop('Usage: capture-x11-replacement.R --output=<existing empty directory>')
output <- normalizePath(sub('^--output=', '', args), mustWork = TRUE)
if (!dir.exists(output) || length(list.files(output, all.files = TRUE, no.. = TRUE)))
  stop('Output must be empty')
fixtures <- file.path(root, 'validation/fixtures/native', 'x11-replacement-cases.R')
for (fixture in fixtures) source(fixture)
suppressWarnings(suppressMessages({
  library(rjd3jars); library(rjd3toolkit); library(rjd3x13); library(rJava)
}))
.jinit()
mode_add <- J('jdplus.sa.base.api.DecompositionMode')$valueOf('Additive')
mode_mul <- J('jdplus.sa.base.api.DecompositionMode')$valueOf('Multiplicative')
bias_ratio <- J('jdplus.x13.base.api.x11.BiasCorrection')$valueOf('Ratio')
corrector_class <- 'jdplus/x13/base/core/x11/extremevaluecorrector/DefaultExtremeValuesCorrector'
corrector_dots <- 'jdplus.x13.base.core.x11.extremevaluecorrector.DefaultExtremeValuesCorrector'
inputs <- cases <- weights <- replacements <- kinds <- neighbors <- list()
factors <- corrected <- averages <- list()
for (ca in x11_replacement_cases()) {
  y <- x11_replacement_input(ca)
  values <- y$values; values[y$missing] <- NaN
  seqi <- .jcast(J('jdplus.toolkit.base.api.data.Doubles')$of(.jarray(as.double(values))),
    'jdplus/toolkit/base/api/data/DoubleSeq')
  ctx <- J('jdplus.x13.base.core.x11.X11Context')$builder()$
    mode(if (ca$multiplicative) mode_mul else mode_add)$
    period(as.integer(ca$frequency))$firstPeriod(as.integer(ca$start_period - 1L))$
    seasonal(TRUE)$lowerSigma(ca$lsig)$upperSigma(ca$usig)$
    backcastHorizon(as.integer(ca$backcasts))$forecastHorizon(as.integer(ca$forecasts))$
    excludefcast(ca$exclude)$bias(bias_ratio)$build()
  cec <- .jnew(corrector_class)
  .jcall(cec, 'V', 'analyse', seqi, as.integer(ca$start_period - 1L), ctx)
  pending <- .jgetEx(TRUE) # void calls do not raise in rJava; fail loudly instead
  if (!is.null(pending)) stop('pinned analyse failed for case ', ca$id, ': ', pending$message)
  w <- cec$getObservationWeights()$toArray()
  stopifnot(length(w) == ca$n, all(is.finite(w)))
  corr0 <- cec$computeCorrections(seqi, FALSE)
  corr1 <- cec$computeCorrections(seqi, TRUE)
  r0 <- corr0$toArray(); r1 <- corr1$toArray()
  fc <- cec$getCorrectionFactors()$toArray()
  ac <- cec$applyCorrections(seqi, corr0)$toArray()
  av <- .jcall(J(corrector_dots), '[D', 'periodAverages', seqi, as.integer(ca$frequency))
  n <- ca$n; f <- ca$frequency
  # computeCorrections(s, TRUE) masks [backcastHorizon, n - forecastHorizon)
  # using the horizons stored by analyse, independent of excludefcast itself.
  lo <- ca$backcasts; hi <- n - ca$forecasts
  stopifnot(length(r0) == n, length(r1) == n, length(fc) == n, length(ac) == n,
    length(av) == f)
  kind0 <- integer(n); kind1 <- integer(n); nlist <- list()
  for (row in seq_len(n)) {
    i <- row - 1L
    outside <- i < lo || i >= hi
    kind1[row] <- if (outside) 3L else 0L
    if (w[row] == 1) { kind0[row] <- 0L; next }
    kind0[row] <- NA_integer_; if (!outside) kind1[row] <- NA_integer_
    pos <- .jcall(cec, '[I', 'searchPositionsForOutlierCorrection',
      as.integer(i), as.integer(f))
    if (is.null(pos)) {
      kind0[row] <- 2L
      if (is.na(kind1[row])) kind1[row] <- 2L
      nlist[[length(nlist) + 1L]] <- data.frame(case = ca$id, row = i, count = 0L,
        slot = NA_integer_, position = NA_integer_)
    } else {
      kind0[row] <- 1L
      if (is.na(kind1[row])) kind1[row] <- 1L
      nlist[[length(nlist) + 1L]] <- data.frame(case = ca$id, row = i,
        count = length(pos), slot = seq_along(pos) - 1L, position = as.integer(pos))
    }
  }
  stopifnot(all(!is.na(kind0)), all(!is.na(kind1)))
  # The pinned replacement decisions must agree with the recorded kinds.
  for (row in seq_len(n)) {
    i <- row - 1L
    if (kind0[row] == 0L) stopifnot(is.nan(r0[row]))
    if (kind0[row] == 2L) stopifnot(identical(r0[row], av[(i %% f) + 1L]) ||
      (is.nan(r0[row]) && is.nan(av[(i %% f) + 1L])))
    if (kind1[row] %in% c(0L, 3L)) stopifnot(is.nan(r1[row]))
    if (kind1[row] == 2L) stopifnot(identical(r1[row], av[(i %% f) + 1L]) ||
      (is.nan(r1[row]) && is.nan(av[(i %% f) + 1L])))
    stopifnot(identical(ac[row], values[row]) ||
      (is.na(ac[row]) && is.na(values[row])) || identical(ac[row], r0[row]))
    expected_factor <- if (w[row] < 1 && is.finite(w[row])) {
      if (ca$multiplicative) values[row] / (1 + w[row] * (values[row] - 1))
      else values[row] * (1 - w[row])
    } else if (ca$multiplicative) 1 else 0
    stopifnot(identical(fc[row], expected_factor) ||
      (is.na(fc[row]) && is.na(expected_factor)))
  }
  cases[[ca$id]] <- data.frame(case = ca$id, category = ca$category,
    frequency = as.integer(f), n = as.integer(n),
    start_period = as.integer(ca$start_period), backcasts = as.integer(ca$backcasts),
    forecasts = as.integer(ca$forecasts), exclude = ca$exclude, lsig = ca$lsig,
    usig = ca$usig, multiplicative = ca$multiplicative,
    flagged = sum(w != 1), neighbors_null = sum(vapply(nlist, function(d) d$count[1] == 0L,
      logical(1))))
  inputs[[ca$id]] <- data.frame(case = ca$id, row = seq_len(n) - 1L,
    value = y$values, missing = y$missing)
  weights[[ca$id]] <- data.frame(case = ca$id, row = seq_len(n) - 1L, weight = w)
  replacements[[ca$id]] <- data.frame(case = rep(ca$id, 2L * n),
    row = rep(seq_len(n) - 1L, 2L), mode = rep(c(0L, 1L), each = n),
    replacement = c(r0, r1))
  kinds[[ca$id]] <- data.frame(case = rep(ca$id, 2L * n),
    row = rep(seq_len(n) - 1L, 2L), mode = rep(c(0L, 1L), each = n),
    kind = c(kind0, kind1))
  if (length(nlist)) neighbors[[ca$id]] <- do.call(rbind, nlist)
  factors[[ca$id]] <- data.frame(case = ca$id, row = seq_len(n) - 1L, factor = fc)
  corrected[[ca$id]] <- data.frame(case = ca$id, row = seq_len(n) - 1L, value = ac)
  averages[[ca$id]] <- data.frame(case = ca$id, phase = seq_along(av) - 1L,
    average = av)
}
# 17 significant digits round-trip IEEE doubles exactly; R's write.csv default
# 15-digit format would silently quantize the pinned values.
fixtures_fmt <- function(d) {
  data.frame(lapply(d, function(col) if (is.numeric(col)) sprintf('%.17g', col) else col),
    stringsAsFactors = FALSE, check.names = FALSE)
}
write.csv(fixtures_fmt(do.call(rbind, inputs)), file.path(output, 'inputs.csv'), row.names = FALSE)
write.csv(fixtures_fmt(do.call(rbind, cases)), file.path(output, 'cases.csv'), row.names = FALSE)
write.csv(fixtures_fmt(do.call(rbind, weights)), file.path(output, 'weights.csv'), row.names = FALSE)
write.csv(fixtures_fmt(do.call(rbind, replacements)), file.path(output, 'replacements.csv'), row.names = FALSE)
write.csv(fixtures_fmt(do.call(rbind, kinds)), file.path(output, 'kinds.csv'), row.names = FALSE)
neighbor_table <- if (length(neighbors)) do.call(rbind, neighbors) else
  data.frame(case = character(), row = integer(), count = integer(),
    slot = integer(), position = integer())
write.csv(fixtures_fmt(neighbor_table), file.path(output, 'neighbors.csv'), row.names = FALSE)
write.csv(fixtures_fmt(do.call(rbind, factors)), file.path(output, 'factors.csv'), row.names = FALSE)
write.csv(fixtures_fmt(do.call(rbind, corrected)), file.path(output, 'corrected.csv'), row.names = FALSE)
write.csv(fixtures_fmt(do.call(rbind, averages)), file.path(output, 'averages.csv'), row.names = FALSE)
files <- c(normalizePath(script), fixtures)
java <- list(version = .jcall('java/lang/System', 'Ljava/lang/String;', 'getProperty', 'java.version'),
  vendor = .jcall('java/lang/System', 'Ljava/lang/String;', 'getProperty', 'java.vendor'),
  runtime_version = .jcall('java/lang/System', 'Ljava/lang/String;', 'getProperty', 'java.runtime.version'),
  vm_name = .jcall('java/lang/System', 'Ljava/lang/String;', 'getProperty', 'java.vm.name'),
  architecture = .jcall('java/lang/System', 'Ljava/lang/String;', 'getProperty', 'os.arch'),
  oracle = 'rjd3 3.9.0/3.9.1 pinned jars via rjd3jars classpath; DefaultExtremeValuesCorrector public computeCorrections/applyCorrections/getCorrectionFactors/periodAverages after analyse, with the private searchPositionsForOutlierCorrection neighbor walk re-run for provenance')
metadata <- list(schema_version = 1L,
  captured_utc = format(Sys.time(), tz = 'UTC', usetz = TRUE),
  source_sha256 = setNames(reference_sha256(files), basename(files)),
  java = java, R = R.version.string, cases = length(inputs),
  public_scope = 'Pinned corrector replacement values (both projection-masking modes), neighbor provenance, correction factors, period averages and applyCorrections identity only; the neighbor walk feeds provenance columns, B17/B20 table placement and CV corrections are not claimed')
writeLines(sub('[[:blank:]]+$', '', capture.output(dput(metadata))), file.path(output, 'metadata.R'))
captured <- file.path(output, c('inputs.csv', 'cases.csv', 'weights.csv', 'replacements.csv',
  'kinds.csv', 'neighbors.csv', 'factors.csv', 'corrected.csv', 'averages.csv', 'metadata.R'))
write.csv(data.frame(file = basename(captured), sha256 = reference_sha256(captured)),
  file.path(output, 'SHA256SUMS.csv'), row.names = FALSE)
cat('Captured', length(inputs), 'pinned replacement/correction-factor cases\n')
