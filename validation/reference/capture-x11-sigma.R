# Pinned-reference capture for item 22b.3a: moving five-year dispersion windows
# and two-pass sigma weights. The oracle is the pinned public JVM class
# jdplus.x13.base.core.x11.extremevaluecorrector.DefaultExtremeValuesCorrector
# from the installed rjd3 3.9.0 artifacts, driven exactly as the X-11 B/C/D
# steps drive it (analyse on the current series with context.getPosition(0)).
# The protected pass primitives calcStdev/extremeValuesDetection and the
# private removeExtremes exclusion helper are re-run after analyse to record
# every pass's weights, exclusion evidence and dispersion arrays.
# No old corpus and no production output is input.
args <- commandArgs(trailingOnly = FALSE)
script <- sub('^--file=', '', args[startsWith(args, '--file=')][[1L]])
tooling <- dirname(normalizePath(script)); source(file.path(tooling, 'common.R'))
root <- reference_root()
args <- commandArgs(trailingOnly = TRUE)
if (length(args) != 1L || !startsWith(args, '--output='))
  stop('Usage: capture-x11-sigma.R --output=<existing empty directory>')
output <- normalizePath(sub('^--output=', '', args), mustWork = TRUE)
if (!dir.exists(output) || length(list.files(output, all.files = TRUE, no.. = TRUE)))
  stop('Output must be empty')
fixtures <- file.path(root, 'validation/fixtures/native', 'x11-sigma-cases.R')
for (fixture in fixtures) source(fixture)
suppressWarnings(suppressMessages({
  library(rjd3jars); library(rjd3toolkit); library(rjd3x13); library(rJava)
}))
.jinit()
mode_add <- J('jdplus.sa.base.api.DecompositionMode')$valueOf('Additive')
mode_mul <- J('jdplus.sa.base.api.DecompositionMode')$valueOf('Multiplicative')
bias_ratio <- J('jdplus.x13.base.api.x11.BiasCorrection')$valueOf('Ratio')
inputs <- weights <- dispersions <- dispersions2 <- cases <- list()
for (ca in x11_sigma_cases()) {
  y <- x11_sigma_input(ca)
  values <- y$values; values[y$missing] <- NaN
  seqi <- .jcast(J('jdplus.toolkit.base.api.data.Doubles')$of(.jarray(as.double(values))),
    'jdplus/toolkit/base/api/data/DoubleSeq')
  ctx <- J('jdplus.x13.base.core.x11.X11Context')$builder()$
    mode(if (ca$multiplicative) mode_mul else mode_add)$
    period(as.integer(ca$frequency))$firstPeriod(as.integer(ca$start_period - 1L))$
    seasonal(TRUE)$lowerSigma(ca$lsig)$upperSigma(ca$usig)$
    backcastHorizon(as.integer(ca$backcasts))$forecastHorizon(as.integer(ca$forecasts))$
    excludefcast(ca$exclude)$bias(bias_ratio)$build()
  cec <- .jnew('jdplus/x13/base/core/x11/extremevaluecorrector/DefaultExtremeValuesCorrector')
  .jcall(cec, 'V', 'analyse', seqi, as.integer(ca$start_period - 1L), ctx)
  pending <- .jgetEx(TRUE) # void calls do not raise in rJava; fail loudly instead
  if (!is.null(pending)) stop('pinned analyse failed for case ', ca$id, ': ', pending$toString())
  sig1 <- .jcall(cec, '[D', 'calcStdev', seqi)
  w1 <- .jcall(cec, 'Ljdplus/toolkit/base/api/data/DoubleSeq;', 'extremeValuesDetection',
    seqi, .jarray(sig1))$toArray()
  wf <- cec$getObservationWeights()$toArray()
  passes <- if (any(w1 < 1)) 2L else 1L
  sig2 <- NULL
  if (passes == 2L) {
    # JNI resolves the private exclusion helper and the protected second-pass
    # primitives directly, so the fixture records the pinned per-window
    # exclusion evidence, not just the final weights.
    w1seq <- .jcast(J('jdplus.toolkit.base.api.data.Doubles')$of(.jarray(as.double(w1))),
      'jdplus/toolkit/base/api/data/DoubleSeq')
    s2 <- .jcall(cec, 'Ljdplus/toolkit/base/api/data/DoubleSeq;', 'removeExtremes', seqi, w1seq)
    sig2 <- .jcall(cec, '[D', 'calcStdev', s2)
    w2 <- .jcall(cec, 'Ljdplus/toolkit/base/api/data/DoubleSeq;', 'extremeValuesDetection',
      seqi, .jarray(sig2))$toArray()
    stopifnot(identical(w2, wf))
  } else {
    stopifnot(identical(wf, w1))
  }
  stopifnot(length(w1) == ca$n, length(wf) == ca$n)
  cases[[ca$id]] <- data.frame(case = ca$id, category = ca$category,
    frequency = as.integer(ca$frequency), n = as.integer(ca$n),
    start_period = as.integer(ca$start_period), backcasts = as.integer(ca$backcasts),
    forecasts = as.integer(ca$forecasts), exclude = ca$exclude, lsig = ca$lsig,
    usig = ca$usig, multiplicative = ca$multiplicative, passes = passes,
    window_entries = length(sig1))
  inputs[[ca$id]] <- data.frame(case = ca$id, row = seq_len(ca$n) - 1L,
    value = y$values, missing = y$missing)
  weights[[ca$id]] <- data.frame(case = rep(ca$id, 2L * ca$n),
    row = rep(seq_len(ca$n) - 1L, 2L), pass = rep(c(1L, 2L), each = ca$n),
    weight = c(w1, if (passes == 1L) w1 else wf))
  dispersions[[ca$id]] <- data.frame(case = ca$id, year = seq_along(sig1) - 1L,
    dispersion = sig1)
  if (!is.null(sig2)) dispersions2[[ca$id]] <- data.frame(case = ca$id,
    year = seq_along(sig2) - 1L, dispersion = sig2)
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
write.csv(fixtures_fmt(do.call(rbind, dispersions)), file.path(output, 'dispersions.csv'), row.names = FALSE)
write.csv(fixtures_fmt(do.call(rbind, dispersions2)), file.path(output, 'dispersions-pass2.csv'), row.names = FALSE)
files <- c(normalizePath(script), fixtures)
java <- list(version = .jcall('java/lang/System', 'Ljava/lang/String;', 'getProperty', 'java.version'),
  vendor = .jcall('java/lang/System', 'Ljava/lang/String;', 'getProperty', 'java.vendor'),
  runtime_version = .jcall('java/lang/System', 'Ljava/lang/String;', 'getProperty', 'java.runtime.version'),
  vm_name = .jcall('java/lang/System', 'Ljava/lang/String;', 'getProperty', 'java.vm.name'),
  architecture = .jcall('java/lang/System', 'Ljava/lang/String;', 'getProperty', 'os.arch'),
  oracle = 'rjd3 3.9.0/3.9.1 pinned jars via rjd3jars classpath; DefaultExtremeValuesCorrector (the pinned default rules, equivalent to X11SigmaPolicy::legacy) driven through its public analyse/getObservationWeights API, with its calcStdev/extremeValuesDetection pass primitives and private removeExtremes exclusion helper re-run for per-pass provenance')
metadata <- list(schema_version = 1L,
  captured_utc = format(Sys.time(), tz = 'UTC', usetz = TRUE),
  source_sha256 = setNames(reference_sha256(files), basename(files)),
  java = java, R = R.version.string, cases = length(inputs),
  public_scope = 'Pinned corrector dispersion windows, two-pass weights and exclusions only; captured artifacts reflect the pinned default rules (native X11SigmaPolicy::legacy); the native accurate policy is a deliberate alternative qualified by contract analysis, not by this oracle; no replacement values, correction factors, endpoint policy or decomposition claim')
writeLines(sub('[[:blank:]]+$', '', capture.output(dput(metadata))), file.path(output, 'metadata.R'))
captured <- file.path(output, c('inputs.csv', 'cases.csv', 'weights.csv',
  'dispersions.csv', 'dispersions-pass2.csv', 'metadata.R'))
write.csv(data.frame(file = basename(captured), sha256 = reference_sha256(captured)),
  file.path(output, 'SHA256SUMS.csv'), row.names = FALSE)
cat('Captured', length(inputs), 'pinned dispersion/sigma-weight cases\n')
