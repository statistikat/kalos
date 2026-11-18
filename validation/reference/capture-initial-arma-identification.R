# Pinned-reference capture for item 21-E: the initial ARMA identification
# machine of the pinned jdplus 3.9.0 x13 ArmaModule (rjd3 jars) through the
# public-API observer InitialArmaIdentificationProbe.java. The observer calls
# the real pinned classes (RegArimaUtility.olsResiduals, RegArmaBic.sort,
# RegArimaUtility.processor(true, eps), ArmaModule.select/getPreferedModels);
# no decision rule is re-implemented here. This script builds deterministic
# raw series, derives the OLS cursor through the pinned olsResiduals (the
# cursor CSVs store exactly the doubles the JVM printed), evaluates the
# three-stage candidate grids on that cursor, and runs the full select with
# the decoder call-site module state (modelsCount 5, max_p/max_q 2,
# max_bp/max_bq 1, balanced false, mixed true, eps 1.0E-7) plus refinement-
# probing variants (balanced true, mixed false, a tight modelsCount). The
# qualified period domain is {4, 12}; the cursor cases run d/bd in {(0,0),
# (1,0), (0,1), (1,1)} with mean on/off and zero/one/two user regressors.
# Pass --repro=<dir> to cross-check the written corpus against an earlier
# capture of the same cases from an independent JVM process.
# JVM ground truth (21-D.2 qualification): the estimation machine reaches
# Math.exp / Math.log, which the pinned build intrinsifies to the build- and
# CPU-specific StubRoutines::dexp/dlog assembly at hot call sites. The
# contract fixes the bytecode semantics: capture runs with
#   _JAVA_OPTIONS="-XX:+UnlockDiagnosticVMOptions -XX:DisableIntrinsic=_dexp -XX:DisableIntrinsic=_dlog"
# so that Math.exp/log equal StrictMath.exp/log == FdLibm.Exp/Log.compute.
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
  stop('Usage: capture-initial-arma-identification.R [--repro=<captured directory>] --output=<existing empty directory>')
output <- normalizePath(sub('^--output=', '', args), mustWork = TRUE)
if (!dir.exists(output) || length(list.files(output, all.files = TRUE, no.. = TRUE)))
  stop('Output must be empty')
suppressWarnings(suppressMessages({
  library(rjd3jars); library(rjd3toolkit); library(rJava)
}))
.jinit()
# The rJava JVM fixes the loadable class-file version; compile the observer
# for exactly that release (javac on PATH may be newer). rJava selects the
# JVM through JAVA_HOME, so the capture pins it there.
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
probe <- tempfile('kalos-initial-arma-'); dir.create(probe)
jars <- c(list.files(system.file('java', package = 'rjd3jars'), pattern = '[.]jar$',
  full.names = TRUE),
  list.files(system.file('java', package = 'rjd3x13'), pattern = '[.]jar$',
    full.names = TRUE))
classpath <- paste(jars, collapse = .Platform$path.sep)
status <- system2(javac, c('--release', jvm_major, '-d', shQuote(probe),
  '-cp', shQuote(classpath),
  shQuote(file.path(tooling, 'InitialArmaIdentificationProbe.java'))))
if (status != 0L) stop('probe compilation failed')
.jaddClassPath(c(jars[grep('rjd3x13', jars)], probe))
if (inherits(try(.jfindClass('InitialArmaIdentificationProbe'), silent = TRUE), 'try-error'))
  stop('observer class not loadable in the rJava JVM')
# Ground-truth guard: the machine reaches Math.exp; the contract pins the
# bytecode (FdLibm) semantics, so the intrinsics must be disabled in the
# capture JVM. The probe argument is an argument where the x86 dexp intrinsic
# deviates 1 ulp from the FdLibm kernel on affected builds (bits
# 3f978575b0a90ad5); equality proves the disable flags took effect.
local({
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

ols_in <- list(); ols_out <- list(); cand_in <- list(); cand_out <- list()
sel_in <- list(); sel_out <- list()
cursors <- new.env()
periods <- new.env()

# -- raw input recipes (deterministic under set.seed) ------------------------
wn <- function(n, seed, mu = 0, sd = 1) {
  set.seed(seed); mu + rnorm(n, 0, sd)
}
ar1 <- function(n, seed, a, sd = 1) {
  set.seed(seed); e <- rnorm(n, 0, sd); x <- numeric(n)
  for (t in seq_len(n)) x[t] <- if (t == 1) e[t] else a * x[t - 1] + e[t]
  x
}
ar2 <- function(n, seed, a1, a2, sd = 1) {
  set.seed(seed); e <- rnorm(n, 0, sd); x <- numeric(n)
  for (t in seq_len(n)) {
    x[t] <- e[t] + (if (t >= 2) a1 * x[t - 1] else 0) + (if (t >= 3) a2 * x[t - 2] else 0)
  }
  x
}
walk <- function(n, seed, drift = 0, sd = 1) {
  set.seed(seed); cumsum(rnorm(n, drift, sd))
}
swn <- function(n, seed, b = 0, sd = 1, lag = 12) { # seasonal random walk
  set.seed(seed); e <- rnorm(n, b, sd)
  x <- e; if (n > lag) for (t in (lag + 1):n) x[t] <- x[t - lag] + e[t]
  x
}
trend <- function(n) as.numeric(seq_len(n))
sine12 <- function(n) as.numeric(sin(2 * pi * seq_len(n) / 12))

user_cols <- function(kind, n) switch(kind,
  none = NULL,
  trend = matrix(trend(n), ncol = 1),
  'trend+sine' = cbind(trend(n), sine12(n)))

# -- 1. OLS cursor cases: raw series through the pinned olsResiduals ---------
ols_case <- function(id, note, y, period, d, bd, mean, kind) {
  u <- user_cols(kind, length(y))
  flat <- if (is.null(u)) numeric(0) else as.vector(u) # column-major
  k <- if (is.null(u)) 0L else ncol(u)
  res <- .jcall('InitialArmaIdentificationProbe', 'S', 'ols', as.integer(period),
    as.integer(d), as.integer(bd), as.logical(mean), das(y), das(flat), as.integer(k))
  f <- strsplit(res, '|', fixed = TRUE)[[1L]]
  userstr <- if (k == 0L) '' else
    j(apply(u, 2, function(c) j(vapply(c, g, character(1)))))
  ols_in[[length(ols_in) + 1L]] <<- data.frame(id = id, note = note,
    period = as.integer(period), d = as.integer(d), bd = as.integer(bd),
    mean = as.character(mean), user_kind = kind, n = as.integer(length(y)),
    y = j(vapply(y, g, character(1))), user = userstr, stringsAsFactors = FALSE)
  if (!identical(f[[1L]], 'ok')) {
    ols_out[[length(ols_out) + 1L]] <<- data.frame(id = id, ok = 'error', n = '',
      cursor = '', error = substr(res, 7L, 200L), stringsAsFactors = FALSE)
    return(invisible(FALSE))
  }
  stopifnot(length(f) == 3L)
  assign(id, f[[3L]], envir = cursors) # the exact %.17g strings the JVM printed
  assign(id, as.integer(period), envir = periods)
  ols_out[[length(ols_out) + 1L]] <<- data.frame(id = id, ok = 'ok', n = f[[2L]],
    cursor = f[[3L]], error = '', stringsAsFactors = FALSE)
  invisible(TRUE)
}

cursor_of <- function(cid) as.numeric(strsplit(get(cid, envir = cursors), ';',
  fixed = TRUE)[[1L]])

# -- 2. candidate cases: the three pinned grids on a captured cursor ---------
# Grid shapes follow the module exactly: stage 1 the (bp, bq) grid on the
# (3, 0) regular part, stage 2 the (p, q) grid on the stage-1 winner, stage 3
# the (bp, bq) grid on the stage-2 winner with the bd == 1 bp clamp. The
# winners are the first BIC minima in the module's own grid order (maxP =
# maxQ = 2, maxBp = maxBq = 1, mixed true), so the fed grids equal the grids
# the module itself builds.
bic_case <- function(cid, stage, tag, p, q, bp, bq, eps) {
  cur <- cursor_of(cid)
  res <- .jcall('InitialArmaIdentificationProbe', 'S', 'bic', das(cur),
    as.integer(get(cid, envir = periods)), as.integer(p), as.integer(q),
    as.integer(bp), as.integer(bq), as.double(eps))
  id <- sprintf('%s/%s%d,%d,%d,%d', cid, tag, p, q, bp, bq)
  f <- strsplit(res, '|', fixed = TRUE)[[1L]]
  cand_in[[length(cand_in) + 1L]] <<- data.frame(id = id, cursor = cid,
    stage = as.integer(stage), grid = tag, p = as.integer(p), q = as.integer(q),
    bp = as.integer(bp), bq = as.integer(bq), eps = g(eps),
    stringsAsFactors = FALSE)
  if (!identical(f[[1L]], 'ok')) {
    cand_out[[length(cand_out) + 1L]] <<- data.frame(id = id, ok = 'error',
      p = '', q = '', bp = '', bq = '', bic = '', loglik = '', k = '', neff = '',
      params = '', error = substr(res, 7L, 200L), stringsAsFactors = FALSE)
    return(invisible(NaN))
  }
  if (length(f) != 13L) {
    # the JVM prints trailing empty parameter blocks without the final field
    f <- c(f, character(13L - length(f)))
  }
  cand_out[[length(cand_out) + 1L]] <<- data.frame(id = id, ok = 'ok',
    p = f[[2L]], q = f[[3L]], bp = f[[4L]], bq = f[[5L]], bic = f[[6L]],
    loglik = f[[7L]], k = f[[8L]], neff = f[[9L]],
    params = paste(f[10:13], collapse = '/'), error = '', stringsAsFactors = FALSE)
  invisible(as.numeric(f[[6L]]))
}

run_grid <- function(cid, stage, tag, orders, eps) {
  bics <- vapply(seq_len(nrow(orders)), function(i) with(orders[i, ],
    bic_case(cid, stage, tag, p, q, bp, bq, eps)), numeric(1))
  k <- which.min(bics) # first minimum = the pinned stable sort head
  if (is.na(k)) return(NULL)
  orders[k, ]
}

grids <- function(cid, bd, mixed, eps) {
  tag <- if (mixed) 'm' else 'x'
  keep <- function(df, a, b) if (mixed) df else df[!(df[[a]] > 0 & df[[b]] > 0), ]
  g1 <- keep(expand.grid(bp = 0:1, bq = 0:1), 'bp', 'bq')
  g1 <- data.frame(p = 3L, q = 0L, bp = g1$bp, bq = g1$bq)[order(g1$bp, g1$bq), ]
  w1 <- run_grid(cid, 1L, tag, g1, eps) # pinned loop: bp outer, bq inner
  if (is.null(w1)) return(invisible(NULL))
  g2 <- keep(expand.grid(p = 0:2, q = 0:2), 'p', 'q')
  g2 <- data.frame(p = g2$p, q = g2$q, bp = w1$bp, bq = w1$bq)[order(g2$p, g2$q), ]
  w2 <- run_grid(cid, 2L, tag, g2, eps)
  if (is.null(w2)) return(invisible(NULL))
  m1 <- if (bd == 1) 0 else 1
  g3 <- keep(expand.grid(bp = 0:m1, bq = 0:1), 'bp', 'bq')
  g3 <- data.frame(p = w2$p, q = w2$q, bp = g3$bp, bq = g3$bq)[order(g3$bp, g3$bq), ]
  run_grid(cid, 3L, tag, g3, eps)
  invisible(NULL)
}

# -- 3. select cases: the full three-stage module over a captured cursor -----
select_case <- function(cid, d, bd, variant, nmod, maxP, maxQ, maxBp, maxBq,
    balanced, mixed, wn, eps) {
  cur <- cursor_of(cid)
  res <- .jcall('InitialArmaIdentificationProbe', 'S', 'select', das(cur),
    as.integer(get(cid, envir = periods)), as.integer(d), as.integer(bd),
    as.integer(nmod), as.integer(maxP), as.integer(maxQ), as.integer(maxBp),
    as.integer(maxBq), as.logical(balanced), as.logical(mixed), as.logical(wn),
    as.double(eps))
  id <- sprintf('%s/sel.%s', cid, variant)
  f <- strsplit(res, '|', fixed = TRUE)[[1L]]
  sel_in[[length(sel_in) + 1L]] <<- data.frame(id = id, cursor = cid,
    d = as.integer(d), bd = as.integer(bd), variant = variant, nmod = as.integer(nmod),
    max_p = as.integer(maxP), max_q = as.integer(maxQ), max_bp = as.integer(maxBp),
    max_bq = as.integer(maxBq), balanced = as.character(balanced),
    mixed = as.character(mixed), accept_white_noise = as.character(wn),
    eps = g(eps), stringsAsFactors = FALSE)
  if (!identical(f[[1L]], 'ok')) {
    sel_out[[length(sel_out) + 1L]] <<- data.frame(id = id, ok = 'error',
      p = '', q = '', bp = '', bq = '', merged_n = '', merged = '', ref = '',
      error = substr(res, 7L, 200L), stringsAsFactors = FALSE)
    return(invisible(NULL))
  }
  # ok|p|q|bp|bq|nmod|entry*nmod|REF|p|q|bp|bq (entry = p,q,bp,bq,bic); the
  # panel length comes from the emitted count, not the nmod argument (the
  # pinned array can hold fewer live entries after the dead-tail trim).
  panel_n <- as.integer(f[[6L]])
  if (!identical(f[[7L + panel_n]], 'REF')) {
    cat('BAD SELECT LINE:', substr(res, 1L, 400L), '\n')
    stop('malformed select line')
  }
  stopifnot(length(f) == 11L + panel_n)
  sel_out[[length(sel_out) + 1L]] <<- data.frame(id = id, ok = 'ok',
    p = f[[2L]], q = f[[3L]], bp = f[[4L]], bq = f[[5L]], merged_n = f[[6L]],
    merged = j(f[7L:(6L + panel_n)]),
    ref = paste(f[(8L + panel_n):(11L + panel_n)], collapse = ','), error = '',
    stringsAsFactors = FALSE)
  invisible(NULL)
}

# -- battery ------------------------------------------------------------------
add_cursor <- function(cid, note, y, period, kinds) {
  assign(cid, period, envir = periods)
  for (kind in kinds) for (dd in 0:1) for (db in 0:1) for (mn in c(TRUE, FALSE)) {
    # The user kind is part of the case id: two kinds on one series otherwise
    # collide in the cursor environment and make the candidate ids ambiguous.
    utag <- switch(kind, none = 'u0', trend = 'u1', 'trend+sine' = 'u2')
    oid <- sprintf('%s/%s.o%d%d.%s', cid, utag, dd, db, if (mn) 'm' else 'n')
    ols_case(oid, sprintf('%s: d=%d bd=%d mean=%s user=%s', note, dd, db, mn, kind),
      y, period, dd, db, mn, kind)
  }
}
case_cursors <- function(cid, eps = 1e-7) {
  for (nm in ls(cursors, all.names = TRUE)) {
    if (!startsWith(nm, paste0(cid, '/'))) next
    m <- regmatches(nm, regexpr('[.]o[01][01]', nm))
    dd <- as.integer(substr(m, 3, 3)); db <- as.integer(substr(m, 4, 4))
    grids(nm, db, TRUE, eps)
    grids(nm, db, FALSE, eps)
    select_case(nm, dd, db, 'decoder', 5L, 2L, 2L, 1L, 1L, FALSE, TRUE, FALSE, eps)
    select_case(nm, dd, db, 'balanced', 5L, 2L, 2L, 1L, 1L, TRUE, TRUE, FALSE, eps)
    select_case(nm, dd, db, 'nomixed', 5L, 2L, 2L, 1L, 1L, FALSE, FALSE, FALSE, eps)
    select_case(nm, dd, db, 'nmod3', 3L, 2L, 2L, 1L, 1L, FALSE, TRUE, FALSE, eps)
  }
}

add_cursor('wn36', 'white noise n=36', wn(36, 4201), 12, 'none')
add_cursor('wn120', 'white noise n=120', wn(120, 4202), 12, c('none', 'trend'))
add_cursor('ar120', 'AR(1) a=0.6 n=120', ar1(120, 4203, 0.6), 12, 'trend+sine')
add_cursor('ar284', 'AR(2) a=(0.5,-0.25) n=84', ar2(84, 4204, 0.5, -0.25), 12, 'none')
add_cursor('rw144', 'random walk n=144', walk(144, 4205, 0.05), 12, c('none', 'trend'))
add_cursor('swn96', 'seasonal random walk n=96', swn(96, 4206, 0.05), 12, 'trend')
add_cursor('qtr72', 'quarterly random walk n=72', walk(72, 4207, 0.1), 4, 'trend')

for (cid in c('wn36', 'wn120', 'ar120', 'ar284', 'rw144', 'swn96', 'qtr72')) {
  case_cursors(cid)
}

ols_in <- do.call(rbind, ols_in); ols_out <- do.call(rbind, ols_out)
cand_in <- do.call(rbind, cand_in); cand_out <- do.call(rbind, cand_out)
sel_in <- do.call(rbind, sel_in); sel_out <- do.call(rbind, sel_out)
write.csv(ols_in, file.path(output, 'ols-inputs.csv'), row.names = FALSE)
write.csv(ols_out, file.path(output, 'ols.csv'), row.names = FALSE)
write.csv(cand_in, file.path(output, 'candidates-inputs.csv'), row.names = FALSE)
write.csv(cand_out, file.path(output, 'candidates.csv'), row.names = FALSE)
write.csv(sel_in, file.path(output, 'select-inputs.csv'), row.names = FALSE)
write.csv(sel_out, file.path(output, 'select.csv'), row.names = FALSE)

repro_note <- '# double capture: run this script twice into separate directories and
# compare all csv files; identical bytes prove JVM-process reproducibility.'
if (nzchar(repro)) {
  same <- all(vapply(c('ols-inputs.csv', 'ols.csv', 'candidates-inputs.csv',
      'candidates.csv', 'select-inputs.csv', 'select.csv'), function(f)
    identical(readLines(file.path(output, f)),
              readLines(file.path(repro, f))), logical(1)))
  if (!same) stop('reproduction capture differs from ', repro)
  repro_note <- sprintf(
    '# double capture: byte-identical csv files against the independent JVM
# capture in %s (captured_at differs only in metadata.R).', repro)
}
writeLines(c(
  '# Capture provenance for the item 21-E initial-arma-identification corpus.',
  '# Oracle: the pinned jdplus 3.9.0 x13 ArmaModule through the public-API',
  '# observer InitialArmaIdentificationProbe.java (no decision rule there):',
  '# olsResiduals for the cursor, RegArmaBic.sort for one candidate with the',
  '# private fitted SarimaModel read by reflection, the processor(true, eps)',
  '# route for the likelihood statistics, and ArmaModule.select with its',
  '# getPreferedModels panel. The module is assembled through the pinned',
  '# package-private Builder (X13SpecDecoder.readAutoModel builds the same',
  '# object); the decoder call-site state is modelsCount 5, max_p/max_q 2,',
  '# max_bp/max_bq 1, balanced false, mixed true, acceptWhiteNoise false,',
  '# eps 1.0E-7; variants vary those fields to exercise the refinement rule',
  '# groups and the merge eviction path. The stage grids are fed from the',
  "# JVM's own first-BIC-minimum in the module's grid order, so the",
  '# candidates battery witnesses exactly the grids the module builds.',
  '# Candidates report the specification, BIC2, the raw logLikelihood, the',
  '# estimated-parameters count and the effective n (statistics route), and',
  '# the fitted SarimaModel raw parameters [phi, bphi, theta, btheta] under',
  '# the pinned 1 + phi(B) sign convention.',
  '# The qualified period domain is {4, 12}; the pinned period-1 shortcuts are',
  '# out of scope. Failures are recorded as one error line per case.',
  repro_note,
  sprintf('# R %s', getRversion()),
  '# ground truth: bytecode (FdLibm) semantics; capture JVM runs with',
  '# _JAVA_OPTIONS="-XX:+UnlockDiagnosticVMOptions -XX:DisableIntrinsic=_dexp -XX:DisableIntrinsic=_dlog"',
  '# (guard verified in-process: Math.exp == StrictMath.exp at the probe argument).',
  sprintf('# rjd3jars %s', as.character(packageVersion('rjd3jars'))),
  sprintf('# rJava JVM %s (observer compiled with javac --release %d)',
    .jcall('java/lang/System', 'S', 'getProperty', 'java.version'), jvm_major),
  sprintf('# capture-initial-arma-identification.R sha256 %s', reference_sha256(script)),
  sprintf('# InitialArmaIdentificationProbe.java sha256 %s',
    reference_sha256(file.path(tooling, 'InitialArmaIdentificationProbe.java'))),
  sprintf('captured_at <- "%s"', format(Sys.time(), tz = 'UTC'))),
  file.path(output, 'metadata.R'))
files <- sort(setdiff(list.files(output, full.names = TRUE),
  file.path(output, 'SHA256SUMS.csv')))
write.csv(data.frame(file = basename(files),
  sha256 = reference_sha256(files)),
  file.path(output, 'SHA256SUMS.csv'), row.names = FALSE)
cat('ols:', nrow(ols_in), 'candidates:', nrow(cand_in), 'select:', nrow(sel_in),
  'errors:', sum(ols_out$ok == 'error') + sum(cand_out$ok == 'error') +
    sum(sel_out$ok == 'error'), '\n')
