# Pinned-reference capture for item 21-H.0: the final-information producer
# of the pinned jdplus 3.9.0 stack (rjd3 jars) - the NumericalDerivatives
# stencil at the final point, the RegArmaProcessor.compute score/information
# scalings and the SymmetricMatrix.inverse asymptotic covariance - through
# the observer FinalInformationProbe.java. The observer drives the REAL
# classes (RegSarimaComputer.optimize for est rows, the pinned producer tail
# RegArmaSsqFunction -> calcResiduals -> RegArmaFunction ->
# NumericalDerivatives -> SymmetricMatrix.inverse for every row); this script
# only builds deterministic inputs, runs the stdin protocol and writes the
# corpus. est rows additionally force the probe to replay the tail at the
# converged point and abort unless the captured point/score/information/
# asymptotic covariance match the estimator's own LogLikelihoodFunction.Point
# bit for bit, so an est row can only be captured when the documented tail
# reproduces the estimator exactly. inj rows inject final points the
# optimiser cannot reach on the pinned jars (boundary / eps-zero / NaN /
# singular-information / negative-nobs families, the 21-A precedent).
# Deviance stencil nodes are recorded verbatim (hex value, or THROW where
# newval's catch turns the failure into fx).
#
# The pinned producer computes Math.exp/log inside the deviance, which the
# pinned build intrinsifies to the build- and CPU-specific
# StubRoutines::dexp/dlog assembly at hot call sites. The contract fixes the
# bytecode semantics: capture runs with
#   _JAVA_OPTIONS="-XX:+UnlockDiagnosticVMOptions -XX:DisableIntrinsic=_dexp -XX:DisableIntrinsic=_dlog"
# so that Math.exp/log equal StrictMath.exp/log == FdLibm.Exp/Log.compute.
#
# Parameters are KALOS raw convention throughout: [phi | bphi | theta |
# btheta] with the SarimaModel storage signs (the AR sign flip from the
# Kalos convention is the caller's, exactly like the 21-H.1 driver will do).
# Pass --repro=<dir> to byte-compare the written corpus against an earlier
# capture of the same cases from an independent JVM process.
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
  stop('Usage: capture-final-information.R [--repro=<captured directory>] --output=<existing empty directory>')
output <- normalizePath(sub('^--output=', '', args), mustWork = TRUE)
if (!dir.exists(output) || length(list.files(output, all.files = TRUE, no.. = TRUE)))
  stop('Output must be empty')
suppressWarnings(suppressMessages({
  library(rjd3jars); library(rjd3toolkit); library(rJava)
}))
.jinit()
# The rJava JVM fixes the loadable class-file version; compile the observer
# for exactly that release (javac on PATH may be newer).
jvm_major <- as.integer(sub('^([0-9]+).*', '\\1',
  .jcall('java/lang/System', 'S', 'getProperty', 'java.version')))
probe <- tempfile('kalos-final-information-'); dir.create(probe)
jars <- c(list.files(system.file('java', package = 'rjd3jars'), pattern = '[.]jar$',
  full.names = TRUE),
  list.files(system.file('java', package = 'rjd3x13'), pattern = '[.]jar$',
    full.names = TRUE))
classpath <- paste(jars, collapse = .Platform$path.sep)
javac <- Sys.which('javac'); if (!nzchar(javac)) stop('javac is required on PATH')
status <- system2(javac, c('--release', jvm_major, '-d', shQuote(probe),
  '-cp', shQuote(classpath),
  shQuote(file.path(tooling, 'FinalInformationProbe.java'))))
if (status != 0L) stop('probe compilation failed')
.jaddClassPath(c(jars[grep('rjd3x13', jars)], probe))
PROBE <- 'jdplus.toolkit.base.core.regarima.estimation.FinalInformationProbe'
# Loadability probe: .jfindClass / Class.forName resolve against the system
# loader, which never sees the rJava .jaddClassPath extension; .jcall resolves
# through JNI FindClass on rJava's own loader, which does. An empty input is
# a no-op round trip through the observer.
if (inherits(try(.jstrVal(.jcall(PROBE, 'S', 'capture', '')), silent = TRUE),
  'try-error'))
  stop('observer class not loadable in the rJava JVM')
# Intrinsic guard, verified in-process: the probe deviance path must see the
# bytecode (FdLibm) semantics, not the intrinsic StubRoutines assembly.
local({
  x <- 0.66291
  if (!identical(.jcall('java/lang/Math', 'D', 'exp', x),
                 .jcall('java/lang/StrictMath', 'D', 'exp', x)))
    stop('capture JVM still uses the intrinsic Math.exp: run with ',
      '_JAVA_OPTIONS="-XX:+UnlockDiagnosticVMOptions -XX:DisableIntrinsic=_dexp ',
      '-XX:DisableIntrinsic=_dlog"')
})

g <- function(x) sprintf('%.17g', x)
j <- function(v) paste(v, collapse = ';')
dh <- function(v) j(vapply(v, g, character(1)))

# The producer STEP knob: SarimaMapping2's static Math.sqrt(2.220446E-16),
# spelled at the same literal so IEEE gives the identical double.
STEP <- sqrt(2.220446e-16)

# -- series recipes (deterministic under set.seed) --------------------------------
walk_m <- function(n, seed) {
  set.seed(seed); as.numeric(50 + cumsum(rnorm(n, 0, .8)) + rnorm(n))
}
growth_seasonal <- function(n, seed) {
  set.seed(seed); t <- seq_len(n)
  as.numeric(exp(cumsum(rnorm(n, .01, .02)) + .2 * sin(2 * pi * t / 12)))
}
walk_q <- function(n, seed) {
  set.seed(seed); as.numeric(100 + cumsum(rnorm(n, .3, 1.5)) + rnorm(n))
}

# -- case table -------------------------------------------------------------------
# A case is the full probe C/Y/X record; the probe input text and every corpus
# row derive from the same table, so cases.csv always regenerates the capture.
cases <- list()
case <- function(id, note, mode, freq, p, d, q, bp, bd, bq, point, y, x = NULL,
    nobs = 'a', check_all = 1L, ar_limit = 0.99999, ma_limit = 1,
    epsilon = STEP) {
  cases[[length(cases) + 1L]] <<- list(id = id, note = note, mode = mode,
    freq = as.integer(freq), p = as.integer(p), d = as.integer(d),
    q = as.integer(q), bp = as.integer(bp), bd = as.integer(bd),
    bq = as.integer(bq), point = as.double(point), y = as.double(y),
    x = if (is.null(x)) NULL else as.double(x), nobs = nobs,
    check_all = as.integer(check_all), ar_limit = as.double(ar_limit),
    ma_limit = as.double(ma_limit), epsilon = as.double(epsilon))
}

# est rows: the real estimator runs to convergence and the probe self-checks
# its tail replay against the estimator's own final Point.
ym <- walk_m(120, 4101)
case('e_m0101', 'walk, MA-only ARMA (0,1,1)(0,1,1)12',
  'est', 12, 0, 1, 1, 0, 1, 1, c(0.2, 0.3), ym)
ym <- walk_m(144, 4102)
case('e_m111', 'walk, (1,1,1)(0,0,0)12',
  'est', 12, 1, 1, 1, 0, 0, 0, c(0.2, 0.3), ym)
ym <- walk_q(96, 4103)
case('e_q3', 'quarterly walk, three coordinates (0,1,2)(1,1,0)4',
  'est', 4, 0, 1, 2, 1, 1, 0, c(0.2, 0.1, -0.1), ym)
ym <- growth_seasonal(156, 4104)
case('e_m6', 'growth+seasonal, six coefficients (2,1,2)(1,1,1)12',
  'est', 12, 2, 1, 2, 1, 1, 1, c(0.1, 0.05, 0.1, 0.1, 0.05, 0.1), ym)
# (design-column rows excluded: the pinned final block throws at
# LinearModel.calcResiduals for any explicit X under a plain SarimaMapping2 —
# Evaluation.allCoefficients() is regression-free (length 0) while
# asLinearModel().getVariablesCount() counts the design columns (bytecode
# pin: RegArmaProcessor.compute tail vs LinearModel.calcResiduals dimension
# check); the pipeline reaches designs only through CompoundMapping extended
# coordinates, pinned and qualified with the 21-H.1 driver slice.)
ym <- walk_m(36, 4107)
case('e_short', 'short monthly series, (0,1,1)(0,1,1)12',
  'est', 12, 0, 1, 1, 0, 1, 1, c(0.2, 0.3), ym)
ym <- walk_q(96, 4108)
case('e_qtr3', 'quarterly (1,1,0)(1,1,1)4, three coordinates',
  'est', 4, 1, 1, 0, 1, 1, 1, c(0.2, 0.1, -0.1), ym)
ym <- walk_m(144, 4109)
case('e_d2', 'double regular differencing (1,2,1)(0,0,0)12',
  'est', 12, 1, 2, 1, 0, 0, 0, c(0.2, 0.3), ym)
ym <- walk_m(120, 4110)
case('e_arlim099', 'est with the arLimit knob at 0.99 instead of 0.99999',
  'est', 12, 1, 1, 1, 0, 1, 1, c(0.2, 0.3, 0.1), ym, ar_limit = 0.99)

# inj rows: final points the optimiser cannot land on (21-A precedent).
yb <- walk_m(120, 4201)
case('i_arlim_pos', 'phi exactly at +arLimit: checkBoundaries eps flip',
  'inj', 12, 1, 1, 0, 1, 1, 0, c(0.99999, 0.5), yb)
case('i_arlim_neg', 'phi exactly at -arLimit',
  'inj', 12, 1, 1, 0, 1, 1, 0, c(-0.99999, 0.5), yb)
case('i_ar_over_pos', 'phi 1.05 outside every limit: checkepsilon zeroes eps',
  'inj', 12, 1, 1, 0, 1, 1, 0, c(1.05, 0.5), yb)
case('i_ar_over_neg', 'phi -1.05 outside every limit',
  'inj', 12, 1, 1, 0, 1, 1, 0, c(-1.05, -0.5), yb)
case('i_bphi_at', 'seasonal phi exactly at +arLimit',
  'inj', 12, 1, 1, 0, 1, 1, 0, c(0.4, 0.99999), yb)
case('i_bphi_over', 'seasonal phi 1.2 outside (|v| > 1 eps relativity)',
  'inj', 12, 1, 1, 0, 1, 1, 0, c(0.4, 1.2), yb)
case('i_nobs0', 'nobs override 0: score and information scale to zero',
  'inj', 12, 1, 1, 0, 1, 1, 0, c(-0.5, 0.2), yb, nobs = 0)
case('i_nobsneg', 'nobs override -5: sign-flipped scalings',
  'inj', 12, 1, 1, 0, 1, 1, 0, c(-0.5, 0.2), yb, nobs = -5)

yma <- walk_m(120, 4202)
case('i_thetab', 'theta exactly at +ma_limit 1.0 with checkAll',
  'inj', 12, 1, 1, 1, 0, 0, 0, c(-0.5, 1), yma)
case('i_thetab_off', 'theta at 1.0 with checkAll false: no eps adjustment',
  'inj', 12, 1, 1, 1, 0, 0, 0, c(-0.5, 1), yma, check_all = 0L)
case('i_thetaneb', 'theta exactly at -ma_limit with checkAll',
  'inj', 12, 1, 1, 1, 0, 0, 0, c(-0.5, -1), yma)
case('i_theta_over', 'theta 1.5 outside with checkAll: eps zeroed',
  'inj', 12, 1, 1, 1, 0, 0, 0, c(-0.5, 1.5), yma)
case('i_nan_phi', 'NaN phi: NaN eps semantics and THROW nodes',
  'inj', 12, 1, 1, 1, 0, 0, 0, c(NaN, 0.3), yma)
case('i_nan_both', 'both coordinates NaN',
  'inj', 12, 1, 1, 1, 0, 0, 0, c(NaN, NaN), yma)
case('i_arlim099_inj', 'phi 0.995 with arLimit 0.99: outside a relaxed limit',
  'inj', 12, 1, 1, 0, 1, 1, 0, c(0.995, 0.5), yb, ar_limit = 0.99)

ym2 <- walk_m(120, 4203)
case('i_eps2x', 'epsilon knob at 2 STEP: checkepsilon halving path',
  'inj', 12, 0, 1, 1, 0, 1, 1, c(0.4, 0.4), ym2, epsilon = 2 * STEP)
yq <- walk_q(96, 4204)
case('i_freq4', 'quarterly three-coordinate injection (1,1,1)(1,1,0)4',
  'inj', 4, 1, 1, 1, 1, 1, 0, c(0.3, -0.2, 0.4), yq)
yg <- growth_seasonal(156, 4205)
case('i_dim6', 'six-coefficient interior injection (2,1,1)(1,1,2)12',
  'inj', 12, 2, 1, 1, 1, 1, 2, c(0.3, 0.15, 0.2, 0.4, -0.2, 0.15), yg)
ym5 <- walk_m(30, 4208)
case('i_short', 'short-series interior injection',
  'inj', 12, 0, 1, 1, 0, 1, 1, c(-0.5, -0.3), ym5)

# -- run the observer --------------------------------------------------------------
stdin_text <- paste(unlist(lapply(cases, function(c) {
  c(sprintf('C;%s;%s;%d;%d;%d;%d;%d;%d;%d;%s;%d;%s;%s;%s;%s', c$id, c$mode,
        c$p, c$bp, c$q, c$bq, c$d, c$bd, c$freq, c$nobs, c$check_all,
        g(c$ar_limit), g(c$ma_limit), g(c$epsilon),
        paste(vapply(c$point, g, character(1)), collapse = ' ')),
    sprintf('Y;%s', dh(c$y)),
    if (is.null(c$x)) character(0) else sprintf('X;%s', dh(c$x)))
})), collapse = '\n')
out <- .jstrVal(.jcall(PROBE, 'S', 'capture', paste0(stdin_text, '\n')))
lines <- strsplit(out, '\n', fixed = TRUE)[[1L]]

# -- parse the blocks --------------------------------------------------------------
blk <- list()   # per id: list of (tag, payload) in order
ids <- character(0)
for (l in lines) {
  f <- strsplit(l, ';', fixed = TRUE)[[1L]]
  if (f[[1L]] %in% c('N', 'P', 'Q', 'O', 'U', 'K', 'E', 'G', 'H', 'S', 'I',
                    'A', 'D', 'V')) {
    id <- f[[2L]]
    if (!(id %in% names(blk))) {
      ids <- c(ids, id)
      blk[[id]] <- list(N = character(0), P = character(0), Q = character(0),
        O = character(0), U = character(0), K = character(0), E = character(0),
        G = character(0), H = character(0), S = character(0), I = character(0),
        A = character(0), D = character(0), V = character(0))
    }
    blk[[id]][[f[[1L]]]] <- c(blk[[id]][[f[[1L]]]], paste(f[-c(1L, 2L)],
      collapse = ';'))
  } else stop('unexpected probe line: ', substr(l, 1, 120))
}
stopifnot(identical(ids, vapply(cases, `[[`, character(1), 'id')))

cases_tbl <- NULL
cap_rows <- list(); node_rows <- list()
for (c in cases) {
  b <- blk[[c$id]]
  stopifnot(length(b$N) == 1L, length(b$P) == 1L, length(b$O) == 1L,
            length(b$U) == 1L, length(b$K) == 1L)
  n <- strsplit(b$N, ';', fixed = TRUE)[[1L]]  # mode dim checkall ar ma eps
  k <- b$K
  rej <- startsWith(k, 'REJECTED;')
  status <- if (rej) paste0('rej:', gsub(';', ':', k))
            else if (b$A == 'INVFAIL') 'invfail' else 'ok'
  stopifnot(rej || (length(b$E) == 1L && length(b$G) == 1L &&
                    length(b$H) == 1L && length(b$S) == 1L &&
                    length(b$I) == 1L && length(b$A) == 1L))
  cap_rows[[length(cap_rows) + 1L]] <- data.frame(id = c$id, status = status,
    dim = as.integer(n[[2L]]), checkall_eff = as.integer(n[[3L]]),
    arlimit_hex = n[[4L]], malimit_hex = n[[5L]], epsilon_hex = n[[6L]],
    yd = gsub(' ', ';', b$P, fixed = TRUE),
    xd = if (length(b$Q)) gsub(' ', ';', b$Q, fixed = TRUE) else '',
    nobs = as.integer(b$O), point_hex = b$U,
    fx_hex = if (rej) '' else k, eps_hex = if (rej) '' else b$E,
    grad_hex = if (rej) '' else b$G, hess_hex = if (rej) '' else b$H,
    score_hex = if (rej) '' else b$S, info_hex = if (rej) '' else b$I,
    asy_hex = if (rej || status == 'invfail') '' else b$A,
    diag_hex = if (rej || status == 'invfail') '' else b$D,
    stringsAsFactors = FALSE)
  for (v in b$V) {
    f <- strsplit(v, ';', fixed = TRUE)[[1L]]
    stopifnot(length(f) == 2L)
    node_rows[[length(node_rows) + 1L]] <- data.frame(id = c$id,
      node = gsub(' ', ';', f[[1L]], fixed = TRUE), value = f[[2L]],
      stringsAsFactors = FALSE)
  }
  cases_tbl <- rbind(cases_tbl, data.frame(id = c$id, mode = c$mode,
    note = c$note, freq = c$freq, p = c$p, d = c$d, q = c$q, bp = c$bp,
    bd = c$bd, bq = c$bq, nobs = c$nobs, check_all = c$check_all,
    ar_limit = g(c$ar_limit), ma_limit = g(c$ma_limit), epsilon = g(c$epsilon),
    point = dh(c$point), y = dh(c$y), x = if (is.null(c$x)) '' else dh(c$x),
    stringsAsFactors = FALSE))
}
cap_tbl <- do.call(rbind, cap_rows)
nodes_tbl <- do.call(rbind, node_rows)

write.csv(cases_tbl, file.path(output, 'cases.csv'), row.names = FALSE)
write.csv(cap_tbl, file.path(output, 'capture.csv'), row.names = FALSE)
write.csv(nodes_tbl, file.path(output, 'nodes.csv'), row.names = FALSE)

repro_note <- '# double capture: run this script twice into separate directories and
# compare all csv files; identical bytes prove JVM-process reproducibility.'
if (nzchar(repro)) {
  same <- all(vapply(c('cases.csv', 'capture.csv', 'nodes.csv'), function(f)
    identical(readLines(file.path(output, f)),
              readLines(file.path(repro, f))), logical(1)))
  if (!same) stop('reproduction capture differs from ', repro)
  repro_note <- sprintf(
    '# double capture: byte-identical csv files against the independent JVM
# capture in %s (captured_at differs only in metadata.R).', repro)
}
writeLines(c(
  '# Capture provenance for the item 21-H.0 final-information corpus.',
  '# Oracle: pinned jdplus 3.9.0 classes through the observer',
  '# FinalInformationProbe.java (package-private access to RegArmaSsqFunction;',
  '# no stencil, scaling or inverse algorithm is re-implemented there).',
  '# est rows run RegSarimaComputer.optimize with computeExactFinalDerivatives',
  '# (true) and the default LevenbergMarquardtMinimizer, capture the final',
  '# Point, then abort unless a replay of the pinned tail reproduces the',
  '# estimator point/score/information/asymptotic covariance bit for bit.',
  '# inj rows inject the final point (boundary, eps-zero, NaN, negative-nobs',
  '# and singular-information families the optimiser cannot reach on the',
  '# pinned jars, the 21-A precedent).',
  '# Tail: RegArmaSsqFunction.ssqEvaluate -> allCoefficients -> calcResiduals',
  '# -> RegArmaFunction (deviance) -> NumericalDerivatives(fp, false) stencil,',
  '# score grad * ((-0.5 nobs) / fx), information hess * ((0.5 nobs) / fx),',
  '# asymptotic covariance SymmetricMatrix.inverse(information); nobs =',
  '# len(y_d) - xcols - dim unless overridden (nobs column of cases.csv).',
  '# capture.csv: yd/xd are the differenced response/design rows, *_hex rows',
  '# are raw-bit hex of the JVM doubles (row-major flats), status ok/rej:.../',
  '# invfail. nodes.csv holds every stencil node: key = raw-bit hex of the',
  '# node coordinates, value = raw-bit hex or THROW (newval catch -> fx).',
  '# Parameters are KALOS raw convention [phi bphi theta btheta].',
  repro_note,
  sprintf('# R %s', getRversion()),
  sprintf('# rjd3jars %s', as.character(packageVersion('rjd3jars'))),
  sprintf('# rJava JVM %s (observer compiled with javac --release %d)',
    .jcall('java/lang/System', 'S', 'getProperty', 'java.version'), jvm_major),
  '# ground truth: bytecode (FdLibm) semantics; capture JVM runs with',
  '# _JAVA_OPTIONS="-XX:+UnlockDiagnosticVMOptions -XX:DisableIntrinsic=_dexp -XX:DisableIntrinsic=_dlog"',
  '# (guard verified in-process: Math.exp == StrictMath.exp at the probe argument).',
  sprintf('# capture-final-information.R sha256 %s', reference_sha256(script)),
  sprintf('# FinalInformationProbe.java sha256 %s',
    reference_sha256(file.path(tooling, 'FinalInformationProbe.java'))),
  sprintf('captured_at <- "%s"', format(Sys.time(), tz = 'UTC'))),
  file.path(output, 'metadata.R'))
files <- sort(setdiff(list.files(output, full.names = TRUE),
  file.path(output, 'SHA256SUMS.csv')))
write.csv(data.frame(file = basename(files),
  sha256 = reference_sha256(files)),
  file.path(output, 'SHA256SUMS.csv'), row.names = FALSE)
cat('cases:', nrow(cases_tbl), 'nodes:', nrow(nodes_tbl),
    'statuses:', paste(names(table(cap_tbl$status)), table(cap_tbl$status),
      collapse = ' '), '\n')
