# Pinned-reference capture for item 21-D: the d/D selection machine and the
# mean-correction test of the pinned jdplus 3.9.0 x13 DifferencingModule
# (rjd3 jars) through the public-API observer DifferencingDecisionProbe.java.
# The observer runs the real pinned module (no decision rule is re-implemented
# here); this script only builds deterministic case series, parses the printed
# outputs and writes the corpus. The module is constructed through the pinned
# X13SpecDecoder.readAutoModel chain over the default AutoModelSpec: cancel
# 0.1, ub1 = 1/1.0309278350515465, ub2 = 1/1.1363636363636365 (the divisions
# run in the JVM), precision 1e-7, maxd/maxbd builder defaults 2/1, ml never
# set; cases.csv stores the effective scalars exactly as the module held them.
# Series are complete and finite: pure-constant and non-finite responses are
# out of the qualified scope (the pinned raw HannanRissanen pipeline keeps
# running on NaN while the qualified Kalos seam rejects them - documented in
# docs/contracts/differencing-decision-contract.md). The qualified period
# domain is {4, 12}: period 1 / 2 batteries are out of scope (the pinned
# module accepts them, the qualified seam throws). Pass --repro=<dir> to
# cross-check the written corpus against an earlier capture of the same cases
# from an independent JVM process.
# JVM ground truth (21-D.2 qualification): the estimation machine reaches
# Math.exp / Math.log, which the pinned build intrinsifies to the build- and
# CPU-specific StubRoutines::dexp/dlog assembly at hot call sites (1 ulp off
# the bytecode's own FdLibm semantics at scattered arguments, and dependent on
# JIT compilation timing). The contract fixes the bytecode semantics: capture
# runs with
#   _JAVA_OPTIONS="-XX:+UnlockDiagnosticVMOptions -XX:DisableIntrinsic=_dexp -XX:DisableIntrinsic=_dlog"
# so that Math.exp/log equal StrictMath.exp/log == FdLibm.Exp/Log.compute, the
# transcribed kernels of the Kalos machine (validated bit-exact against the
# same JVM on a 230k-input fuzz).
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
  stop('Usage: capture-differencing-decision.R [--repro=<captured directory>] --output=<existing empty directory>')
output <- normalizePath(sub('^--output=', '', args), mustWork = TRUE)
if (!dir.exists(output) || length(list.files(output, all.files = TRUE, no.. = TRUE)))
  stop('Output must be empty')
suppressWarnings(suppressMessages({
  library(rjd3jars); library(rjd3toolkit); library(rJava)
}))
.jinit()
# The rJava JVM fixes the loadable class-file version; compile the observer
# for exactly that release (javac on PATH may be newer). rJava selects the
# JVM through JAVA_HOME, so the capture pins it there; without it rJava may
# pick up a system default that is not the pinned build.
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
probe <- tempfile('kalos-differencing-decision-'); dir.create(probe)
jars <- c(list.files(system.file('java', package = 'rjd3jars'), pattern = '[.]jar$',
  full.names = TRUE),
  list.files(system.file('java', package = 'rjd3x13'), pattern = '[.]jar$',
    full.names = TRUE))
classpath <- paste(jars, collapse = .Platform$path.sep)
status <- system2(javac, c('--release', jvm_major, '-d', shQuote(probe),
  '-cp', shQuote(classpath),
  shQuote(file.path(tooling, 'DifferencingDecisionProbe.java'))))
if (status != 0L) stop('probe compilation failed')
.jaddClassPath(c(jars[grep('rjd3x13', jars)], probe))
if (inherits(try(.jfindClass('DifferencingDecisionProbe'), silent = TRUE), 'try-error'))
  stop('observer class not loadable in the rJava JVM')
# Ground-truth guard: the estimation machine reaches Math.exp; the contract
# pins the bytecode (FdLibm) semantics, so the build-specific exp/log
# intrinsics must be disabled in the capture JVM (see the header). The probe
# argument is an argument where the intrinsic differs from the FdLibm kernel
# by 1 ulp on x86 builds; equality proves the disable flags took effect.
local({
  # Bits 3f978575b0a90ad5: an argument where the x86 dexp intrinsic deviates
  # 1 ulp from the FdLibm kernel on affected builds.
  x <- 0.022970045955171898
  em <- .jcall('java/lang/Math', 'D', 'exp', x)
  es <- .jcall('java/lang/StrictMath', 'D', 'exp', x)
  if (!identical(em, es))
    stop('capture JVM still uses the intrinsic Math.exp: run with ',
      '_JAVA_OPTIONS="-XX:+UnlockDiagnosticVMOptions -XX:DisableIntrinsic=_dexp ',
      '-XX:DisableIntrinsic=_dlog"')
})

g <- function(x) sprintf('%.17g', x)
das <- function(v) .jarray(as.double(v))
j <- function(v) paste(v, collapse = ';')

cases_in <- list(); cases_out <- list()

case <- function(id, note, y, period) {
  n <- length(y)
  res <- .jcall('DifferencingDecisionProbe', 'S', 'decide', as.integer(period),
    das(y))
  f <- strsplit(res, '|', fixed = TRUE)[[1L]]
  cases_in[[length(cases_in) + 1L]] <<- data.frame(id = id, note = note,
    period = as.integer(period), n = as.integer(n),
    y = j(vapply(y, g, character(1))), stringsAsFactors = FALSE)
  if (!identical(f[[1L]], 'ok')) {
    cases_out[[length(cases_out) + 1L]] <<- data.frame(id = id, ok = 'error',
      d = '', bd = '', mu = '', tmean = '', maxd = '', maxbd = '', ub1 = '',
      ub2 = '', cancel = '', eps = '', ml = '', iter = '', mlused = '',
      useml = '', rmax = '', rsmax = '', c_ = '', spec_orders = '',
      params = '', error = substr(res, 7L, 220L), stringsAsFactors = FALSE)
    return(invisible(NULL))
  }
  stopifnot(length(f) == 21L)
  cases_out[[length(cases_out) + 1L]] <<- data.frame(id = id, ok = f[[2L]],
    d = f[[3L]], bd = f[[4L]], mu = f[[5L]], tmean = f[[6L]],
    maxd = f[[7L]], maxbd = f[[8L]], ub1 = f[[9L]], ub2 = f[[10L]],
    cancel = f[[11L]], eps = f[[12L]], ml = f[[13L]], iter = f[[14L]],
    mlused = f[[15L]], useml = f[[16L]], rmax = f[[17L]], rsmax = f[[18L]],
    c_ = f[[19L]], spec_orders = f[[20L]], params = f[[21L]],
    error = '', stringsAsFactors = FALSE)
  invisible(f)
}

# -- input recipes (deterministic under set.seed) ------------------------------
rw <- function(n, seed, drift = 0, sd = 1, mu = 0) {
  set.seed(seed); mu + cumsum(rnorm(n, drift, sd))
}
wn <- function(n, seed, mu = 0, sd = 1) {
  set.seed(seed); mu + rnorm(n, 0, sd)
}
sw <- function(n, seed, b = 0, sd = 1, lag = 12) { # seasonal random walk
  set.seed(seed); e <- rnorm(n, b, sd)
  x <- e; if (n > lag) for (t in (lag + 1):n) x[t] <- x[t - lag] + e[t]
  x
}
growth_seasonal <- function(n, seed) {
  set.seed(seed); t <- seq_len(n)
  as.numeric(exp(cumsum(rnorm(n, .01, .02)) + .2 * sin(2 * pi * t / 12)))
}
ar1 <- function(n, seed, a, sd = 1) {
  set.seed(seed); e <- rnorm(n, 0, sd); x <- numeric(n)
  for (t in seq_len(n)) x[t] <- if (t == 1) e[t] else a * x[t - 1] + e[t]
  x
}
sar1 <- function(n, seed, a, sd = 1) { # seasonal AR(1) at lag 12
  set.seed(seed); e <- rnorm(n, 0, sd); x <- numeric(n)
  for (t in seq_len(n)) x[t] <- if (t <= 12) e[t] else a * x[t - 12] + e[t]
  x
}
arma11 <- function(n, seed, a, th, sd = 1) {
  set.seed(seed); e <- rnorm(n, 0, sd); x <- numeric(n)
  for (t in seq_len(n)) x[t] <- (if (t == 1) 0 else a * x[t - 1]) + e[t] +
    (if (t == 1) 0 else th * e[t - 1])
  x
}
sarma11 <- function(n, seed, a, th, sd = 1) { # seasonal ARMA at lag 12
  set.seed(seed); e <- rnorm(n, 0, sd); x <- numeric(n)
  for (t in seq_len(n)) x[t] <- (if (t <= 12) 0 else a * x[t - 12]) + e[t] +
    (if (t <= 12) 0 else th * e[t - 12])
  x
}

# -- stationary responses (stMean path) ----------------------------------------
case('wn50', 'white noise n=50 period 12: stationary, stMean threshold 1.96',
  wn(50, 4101), 12)
case('wn_shift100', 'white noise + mean 0.3, n=100: stMean threshold 2.0, mu true',
  wn(100, 4102, mu = 0.3), 12)
case('wn_shift250', 'white noise + mean 0.25, n=250: stMean threshold 2.55',
  wn(250, 4103, mu = 0.25), 12)
case('wn36_short', 'white noise n=36 period 12: short stationary response',
  wn(36, 4104), 12)
case('ar_stationary', 'AR(1) a=0.6 monthly n=120: stationary, no differencing',
  ar1(120, 4105, 0.6), 12)
case('const_noise', 'near-constant level 50 + 1e-9 noise (tiny variance, HR normalizer)',
  50 + 1e-9 * (wn(144, 4106) %% 1), 12)

# -- quarterly -------------------------------------------------------------------
case('qtr_rw', 'quarterly random walk with drift n=100 (period 4)',
  rw(100, 4121, drift = 0.5, sd = 1.2), 4)
case('qtr_sw', 'quarterly seasonal random walk n=120 drift (period 4)',
  sw(120, 4122, b = 0.15, lag = 4), 4)

# -- monthly core decisions -------------------------------------------------------
case('m_rw144_drift', 'monthly random walk with drift n=144',
  rw(144, 4131, drift = 0.2), 12)
case('m_rw144_flat', 'monthly random walk no drift n=144',
  rw(144, 4132), 12)
case('m_growth', 'exponential growth with seasonality n=132 (levels)',
  growth_seasonal(132, 4133), 12)
case('m_sw_drift', 'seasonal random walk with drift n=144 (airline-type)',
  sw(144, 4134, b = 0.1), 12)
case('m_sa_additive', 'additive seasonal sine + slow trend + noise n=156',
  { set.seed(4135); t <- seq_len(156)
    as.numeric(0.02 * t + sin(2 * pi * t / 12) + 0.4 * rnorm(156)) }, 12)
case('m_n25_sw', 'seasonal random walk n=25: filter length forces short HR windows',
  sw(25, 4136), 12)
case('m_n20_sw', 'seasonal random walk n=20', sw(20, 4137), 12)
case('m_n15_sw', 'seasonal random walk n=15 (near-period length)', sw(15, 4138), 12)

# -- explosive responses (unstable HR -> default fill -> trial fit) ---------------
case('m_explosive', 'explosive 1.02^t + noise n=120', {
  set.seed(4141); as.numeric(1.02^seq_len(120) + rnorm(120))
}, 12)
case('m_explosive_alt', 'alternating explosive (-1.05)^t + noise n=120', {
  set.seed(4142); as.numeric((-1.05)^seq_len(120) + rnorm(120))
}, 12)
case('m_seas_explosive', 'seasonal explosive x_t = 1.3 x_{t-12} + e, n=144',
  sar1(144, 4143, 1.3), 12)

# -- near-unit-root and cancellation structures -----------------------------------
case('m_ar985', 'AR(1) a=0.985 n=144: root above ub1, searchur counts',
  ar1(144, 4151, 0.985), 12)
case('m_ar96', 'AR(1) a=0.96 n=144: root below ub1, maincondition path',
  ar1(144, 4152, 0.96), 12)
case('m_ar9995', 'AR(1) a=0.9995 n=144: deep unit root',
  ar1(144, 4153, 0.9995), 12)
case('m_arma_cancel', 'ARMA(1,1) a=0.99, ma=0.89 (near regular cancellation) n=144',
  arma11(144, 4154, 0.99, 0.89), 12)
case('m_arma_cancel2', 'ARMA(1,1) a=0.99, ma=0.97 (closer cancellation) n=144',
  arma11(144, 4155, 0.99, 0.97), 12)
case('m_sar99', 'seasonal AR a=0.99 at lag 12 n=144: seasonal root above ub1',
  sar1(144, 4156, 0.99), 12)
case('m_sarma_cancel', 'seasonal ARMA a=0.99 ma=-0.95 (near seasonal cancellation) n=168',
  sarma11(168, 4157, 0.99, -0.95), 12)
case('m_sw_ma_neg', 'seasonal random walk with negative MA at lag 12: bphi-btheta cancellation n=144',
  { set.seed(4158); e <- rnorm(144, 0.1, 1)
    z <- numeric(144); for (t in 13:144) z[t] <- z[t - 12] + e[t] - 0.95 * e[t - 12]
    z }, 12)
case('m_double_rw', 'regular + seasonal random walk n=156: maxd/maxbd pressure',
  { set.seed(4159); e <- rnorm(156, 0.2, 1)
    z <- numeric(156)
    for (t in seq_len(156)) z[t] <- (if (t > 1) z[t - 1] else 0) +
      (if (t > 12) z[t - 12] else 0) - (if (t > 13) z[t - 13] else 0) + e[t]
    z }, 12)
case('m_i2_144', 'doubly integrated random walk n=144: pressure toward d=2',
  { set.seed(4162); as.numeric(cumsum(cumsum(rnorm(144, 0.05, 1)))) }, 12)
case('m_sw_trend', 'seasonal random walk plus strong linear trend n=132',
  { set.seed(4164); tt <- seq_len(132)
    e <- rnorm(132, 0, 1)
    z <- numeric(132); for (s in 13:132) z[s] <- z[s - 12] + e[s]
    as.numeric(z + 0.4 * tt) }, 12)
case('m_level_shift', 'stable noise with a mid-sample level shift n=120', {
  y <- wn(120, 4160); y[61:120] <- y[61:120] + 6; y
}, 12)
case('m_trend_seasonal', 'linear trend + seasonal sine n=120 (mean test with d)',
  { set.seed(4161); t <- seq_len(120)
    as.numeric(0.5 * t + 2 * sin(2 * pi * t / 12) + rnorm(120)) }, 12)

# -- nstMean length/threshold boundaries (d>0 or bd>0 with mean test) ------------
case('m_n80_rw', 'random walk n=80: nstMean threshold boundary 1.96',
  rw(80, 4171), 12)
case('m_n81_rw', 'random walk n=81: nstMean threshold 1.98 band',
  rw(81, 4172), 12)
case('m_n156_rw', 'random walk n=156: nstMean threshold 2.1 band',
  rw(156, 4173), 12)
case('m_n231_rw', 'random walk n=231: nstMean threshold 2.3 band',
  rw(231, 4174), 12)
case('m_n360_rw', 'random walk n=360: nstMean threshold 2.5 band',
  rw(360, 4175), 12)
case('m_n360_rw_shift', 'random walk n=360 with a strong end level shift: mu true',
  { y <- rw(360, 4176); y[241:360] <- y[241:360] + 18; y }, 12)
case('m_n96_sw_shift', 'seasonal random walk n=96 with mean offset: mu near threshold',
  { y <- sw(96, 4177); y + 0.12 }, 12)

cases_in <- do.call(rbind, cases_in); cases_out <- do.call(rbind, cases_out)
write.csv(cases_in, file.path(output, 'inputs.csv'), row.names = FALSE)
write.csv(cases_out, file.path(output, 'cases.csv'), row.names = FALSE)

repro_note <- '# double capture: run this script twice into separate directories and
# compare all csv files; identical bytes prove JVM-process reproducibility.'
if (nzchar(repro)) {
  same <- all(vapply(c('inputs.csv', 'cases.csv'), function(f)
    identical(readLines(file.path(output, f)),
              readLines(file.path(repro, f))), logical(1)))
  if (!same) stop('reproduction capture differs from ', repro)
  repro_note <- sprintf(
    '# double capture: byte-identical csv files against the independent JVM
# capture in %s (captured_at differs only in metadata.R).', repro)
}
writeLines(c(
  '# Capture provenance for the item 21-D differencing-decision corpus.',
  '# Oracle: the pinned jdplus 3.9.0 x13 DifferencingModule through the',
  '# public-API observer DifferencingDecisionProbe.java (no decision rule there).',
  '# The module is built as X13SpecDecoder.readAutoModel builds it: cancel',
  '# spec.getCancel() = 0.1, ub1 = 1.0 / spec.getUb1() = 1/1.0309278350515465,',
  '# ub2 = 1.0 / spec.getUb2() = 1/1.1363636363636365 (JVM divisions), precision',
  '# 1.0E-7, maxd/maxbd builder defaults 2/1, ml never set; cases.csv stores the',
  '# effective scalars exactly as the module stored them.',
  '# Each case runs the public process(DoubleSeq, int) on a complete finite',
  '# series and records d, bd, mean, tmean plus the reflective end-state',
  '# observation: iter, mlused, useml, rmax, rsmax, c_, final SarimaOrders',
  '# p,q,bp,bq and the final SarimaModel raw parameters [phi, bphi, theta,',
  '# btheta] (pinned 1 + phi(B) convention). Errors are recorded as one line.',
  '# Pure-constant and non-finite responses are out of scope (the pinned raw',
  '# HannanRissanen pipeline keeps running on NaN; the qualified Kalos seam',
  '# rejects them - see the contract).',
  repro_note,
  sprintf('# R %s', getRversion()),
  '# ground truth: bytecode (FdLibm) semantics; capture JVM runs with',
  '# _JAVA_OPTIONS="-XX:+UnlockDiagnosticVMOptions -XX:DisableIntrinsic=_dexp -XX:DisableIntrinsic=_dlog"',
  '# (guard verified in-process: Math.exp == StrictMath.exp at the probe argument).',
  sprintf('# rjd3jars %s', as.character(packageVersion('rjd3jars'))),
  sprintf('# rJava JVM %s (observer compiled with javac --release %d)',
    .jcall('java/lang/System', 'S', 'getProperty', 'java.version'), jvm_major),
  sprintf('# capture-differencing-decision.R sha256 %s', reference_sha256(script)),
  sprintf('# DifferencingDecisionProbe.java sha256 %s',
    reference_sha256(file.path(tooling, 'DifferencingDecisionProbe.java'))),
  sprintf('captured_at <- "%s"', format(Sys.time(), tz = 'UTC'))),
  file.path(output, 'metadata.R'))
files <- sort(setdiff(list.files(output, full.names = TRUE),
  file.path(output, 'SHA256SUMS.csv')))
write.csv(data.frame(file = basename(files),
  sha256 = reference_sha256(files)),
  file.path(output, 'SHA256SUMS.csv'), row.names = FALSE)
cat('cases:', nrow(cases_in), 'errors:', sum(cases_out$ok == 'error'), '\n')
