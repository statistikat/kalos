# Pinned-reference capture for item 21-I.3: the automatic calendar-detection
# machines of the pinned jdplus 3.9.0 stack, observed by
# CalendarDetectionProbe.java. Every case runs in its own JVM process
# (per-case isolation; the host has a history of killing long-lived JVMs)
# with the G2 recipe (-XX:DisableIntrinsic=_dexp/_dlog, so Math.log/Math.exp
# go through the fdlibm/StrictMath route the freeze rules pin).
#
# GOLDEN PAYLOADS RIDE RAW BITS: Java's String.format %.17g proved NOT a
# faithful 17-digit renderer during implementation (it prints
# 0x3fb52536ff790458 as 0.082599103328008800 where the correctly-rounded
# rendering is 0.082599103328008794 - the SAME double), so every dump double
# carries Double.doubleToRawLongBits hex and the golden generator parses
# hex. Decimal thresholds never cross the capture boundary unchecked.
#
# Corpus families (the probe javadoc is the grammar authority): CM the
# TradingDaysRegressionComparator ladder algebra (statistics rows, bestModel
# aicc/bic, waldTest), TA the TRAMO AutomaticRegressionTest /
# AutomaticWaldRegressionTest ladders (sel aicc/bic/wald, the per-row
# createTestModel hand-overs), FT the TRAMO AutomaticFRegressionTest (Info
# models, branch models, the branch walk), SA the sa
# AutomaticTradingDaysRegressionTest / AutomaticTradingDaysWaldTest (ladders
# all/nested, the desc-vars update arms including the early-return quirks),
# FS the FastRegressionTest calendar branch (23-E seam activated), DS the
# DefaultRegressionTest calendar fields (T arm, joinTest arm with the FULL
# unscaledCovariance hand-over, the Unprocessed guard), JT the JointTest
# quadratic form, PT the full TramoKernel on TRfull with the tradingDays
# AutoMethod override (the six-machine chain firing live), PX the full
# X13Kernel on rsa4 with tradingDays.auto. Pass --repro=<dir> for the
# byte-identity cross-check between independent process runs (capA/capB).
#
# Usage: capture-calendar-detection.R [--repro=<captured directory>]
#        --output=<existing empty directory>
# Run with JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64 and
# _JAVA_OPTIONS="-XX:+UnlockDiagnosticVMOptions -XX:DisableIntrinsic=_dexp
# -XX:DisableIntrinsic=_dlog".
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
  stop('Usage: capture-calendar-detection.R [--repro=<captured directory>] --output=<existing empty directory>')
output <- normalizePath(sub('^--output=', '', args), mustWork = TRUE)
if (!dir.exists(output) || length(list.files(output, all.files = TRUE, no.. = TRUE)))
  stop('Output must be empty')

javahome <- Sys.getenv('JAVA_HOME')
javac <- if (nzchar(javahome)) file.path(javahome, 'bin', 'javac') else Sys.which('javac')
javabin <- if (nzchar(javahome)) file.path(javahome, 'bin', 'java') else Sys.which('java')
if (!nzchar(javac) || !file.exists(javac)) stop('javac is required (JAVA_HOME/bin)')
jver <- system2(javabin, c('-version'), stderr = TRUE)
jvm_major <- as.integer(sub('^openjdk version "?([0-9]+).*', '\\1', jver[[1L]]))
if (is.na(jvm_major) || jvm_major != 21L)
  stop('the pinned capture JVM is OpenJDK 21, got: ', jver[[1L]])

jarfiles <- function(pkg)
  file.path(system.file('java', package = pkg),
    list.files(system.file('java', package = pkg), pattern = '[.]jar$'))
jars <- c(jarfiles('rjd3jars'), jarfiles('rjd3toolkit'), jarfiles('rjd3x13'),
  jarfiles('rjd3tramoseats'))
missing <- setdiff(c('jdplus-sa-base-core-3.9.0.jar',
  'jdplus-tramoseats-base-core-3.9.0.jar', 'jdplus-x13-base-core-3.9.0.jar',
  'protobuf-java-4.36.1.jar'), basename(jars))
if (length(missing)) stop('pinned 3.9.0 jars missing: ', paste(missing, collapse = ', '))
classpath <- paste(c(jars), collapse = .Platform$path.sep)

probe <- tempfile('kalos-cal-detect-'); dir.create(probe)
status <- system2(javac, c('--release', jvm_major, '-d', shQuote(probe),
  '-cp', shQuote(classpath),
  shQuote(file.path(tooling, 'CalendarDetectionProbe.java'))))
if (status != 0L) stop('probe compilation failed')
PROBE_CLASS <- 'jdplus.tramoseats.base.core.tramo.CalendarDetectionProbe'

jvmflags <- c('-XX:+UnlockDiagnosticVMOptions',
  '-XX:DisableIntrinsic=_dexp', '-XX:DisableIntrinsic=_dlog')
Sys.setenv('_JAVA_OPTIONS' = paste(jvmflags[-1L], collapse = ' '))

# Ground-truth guard: the intrinsic-disable flags must be live - Math.exp of
# the 1-ulp witness value must equal StrictMath.exp (bits compared).
local({
  guard <- tempfile('guard-'); dir.create(guard)
  writeLines(c('public class ExpGuard {',
    '  public static void main(String[] a) {',
    '    double x = 0.022970045955171898;',
    '    System.out.println(Long.toHexString(',
    '        Double.doubleToRawLongBits(Math.exp(x))));',
    '    System.out.println(Long.toHexString(',
    '        Double.doubleToRawLongBits(StrictMath.exp(x))));',
    '  }', '}'), file.path(guard, 'ExpGuard.java'))
  if (system2(javac, c('-d', shQuote(guard),
      shQuote(file.path(guard, 'ExpGuard.java')))) != 0L)
    stop('intrinsic guard compilation failed')
  st <- system2(javabin, c(jvmflags, '-cp', shQuote(guard), 'ExpGuard'),
    stdout = TRUE, stderr = TRUE)
  st <- st[grepl('^[0-9a-f]{16}$', st)]
  if (length(st) != 2L || !identical(st[[1L]], st[[2L]]))
    stop('capture JVM still uses the intrinsic Math.exp: ',
      paste(st, collapse = ' '))
})

summaries <- list()
case <- function(id, note, hdr) {
  cf <- tempfile('calcase-'); on.exit(unlink(cf))
  writeLines(hdr, cf)
  st <- system2(javabin, c(jvmflags, '-cp',
    paste(c(probe, jars), collapse = .Platform$path.sep),
    shQuote(PROBE_CLASS), shQuote(cf)), stdout = TRUE, stderr = TRUE)
  rc <- attr(st, 'status'); rc <- if (is.null(rc)) 0L else rc
  out <- st
  out <- out[nzchar(out) & !grepl('^(Picked up|NOTE:|WARNING:|^OpenJDK)', out)]
  if (rc != 0L || any(startsWith(out, 'fatal ')))
    stop('case JVM failed for ', id, ': ',
      paste(utils::tail(out, 4L), collapse = ' | '))
  writeLines(out, file.path(output, paste0(id, '.txt')))
  summaries[[length(summaries) + 1L]] <<-
    data.frame(id = id, note = note, lines = length(out), stringsAsFactors = FALSE)
  message('case ', id, ' ok (', length(out), ' lines)')
}

loadwitness <- function(id, hdr, mustload) {
  cf <- tempfile('lcase-'); on.exit(unlink(cf))
  writeLines(hdr, cf)
  cl <- tempfile('classload-'); on.exit(unlink(cl), add = TRUE)
  st <- system2(javabin, c(jvmflags,
    paste0('-Xlog:class+load=info:file=', cl), '-cp',
    paste(c(probe, jars), collapse = .Platform$path.sep),
    shQuote(PROBE_CLASS), shQuote(cf)), stdout = TRUE, stderr = TRUE)
  rc <- attr(st, 'status'); rc <- if (is.null(rc)) 0L else rc
  if (rc != 0L) stop('load-witness JVM failed for ', id)
  log <- readLines(cl, warn = FALSE)
  loaded <- sort(unique(gsub('0x[0-9a-fA-F]+', '0xADDR',
    sub(' source:.*$', '', sub('^.*class,load\\] ', '',
      grep(' source: file:.*[.]jar', log, value = TRUE))))))
  loaded <- loaded[grepl('^jdplus[.]', loaded)]
  writeLines(loaded, file.path(output, paste0('loadlog-', id, '.txt')))
  for (m in mustload)
    if (!any(grepl(m, loaded)))
      stop('load witness ', id, ' never loaded ', m)
  message('load-witness ', id, ' ok (', length(loaded), ' rows)')
}

# --- case matrix -------------------------------------------------------------
ORD <- '2,1,1,0,0,1'
# CM: the shared comparator algebra (the ladder every ladder machine runs).
case('cm_all', 'comparator: lp, four TD columns, wald leg',
  paste('CM cm_all clean y', ORD, '1e-5 y TD2,TD3,TD4,TD7 0.01 0.1 y'))
case('cm_nolp', 'comparator: no lp arm (absent row 1)',
  paste('CM cm_nolp clean y', ORD, '1e-5 n TD2,TD7 0.01 0.1 y'))
case('cm_tdeff', 'comparator: strong-TD series (wald picks a column)',
  paste('CM cm_tdeff tdeff y', ORD, '1e-5 y TD2,TD7 0.01 0.1 y'))
case('cm_nowald', 'comparator: aicc/bic only, additive series',
  paste('CM cm_nowald clean n', ORD, '1e-5 y TD2,TD3,TD4,TD7 0.01 0.1 n'))
# TA: the TRAMO AICc/BIC/Wald ladder machines.
for (sel in c('aicc', 'bic', 'wald'))
  case(paste0('ta_', sel), paste('TRAMO ladder machine sel=', sel),
    paste('TA ta_', sel, ' clean y ', ORD, ' 1e-5 ', sel, ' 0.01 0.1 y y y y -', sep = ''))
case('ta_tdeff', 'TRAMO ladder machine on the strong-TD series',
  paste('TA ta_tdeff tdeff y', ORD, '1e-5 aicc 0.01 0.1 y y y y -'))
case('ta_nolp', 'TRAMO ladder machine without the leapYear wiring',
  paste('TA ta_nolp clean y', ORD, '1e-5 aicc 0.01 0.1 y y y n -'))
case('ta_nomean', 'TRAMO ladder machine with testMean off',
  paste('TA ta_nomean clean y', ORD, '1e-5 aicc 0.01 0.1 y n y y -'))
case('ta_noea', 'TRAMO ladder machine without the easter field',
  paste('TA ta_noea clean y', ORD, '1e-5 aicc 0.01 0.1 n y y y -'))
# FT: the TRAMO F-test machine (branch walk A/c/n/q + the Info models).
for (fp in c('0.01', '0.05'))
  case(paste0('ft_fp', sub('^0[.]', '', fp)), paste('TRAMO FTEST fp=', fp),
    paste('FT ft_fp', sub('^0[.]', '', fp), ' clean y ', ORD, ' 1e-5 y y y ', fp, ' -', sep = ''))
case('ft_tdeff', 'TRAMO FTEST on the strong-TD series (branch A witness)',
  paste('FT ft_tdeff tdeff y', ORD, '1e-5 y y y 0.05 -'))
case('ft_nomean', 'TRAMO FTEST with testMean off',
  paste('FT ft_nomean clean y', ORD, '1e-5 y n y 0.05 -'))
case('ft_noea', 'TRAMO FTEST without the easter field',
  paste('FT ft_noea clean y', ORD, '1e-5 n y y 0.05 -'))
# SA: the sa ATD / Wald machines (ladders all/nested + the desc-var arms).
case('sa_aicc', 'sa ATD sel=aicc, ladder all, clean desc',
  paste('SA sa_aicc clean y', ORD, '0.001 aicc 0.01 0.1 all 2.0 y -'))
case('sa_bic_td', 'sa ATD sel=bic with a stale plain td on the desc',
  paste('SA sa_bic_td clean y', ORD, '0.001 bic 0.01 0.1 all 2.0 y td:TD2:p'))
case('sa_wald_all', 'sa Wald, ladder all',
  paste('SA sa_wald_all clean y', ORD, '0.001 wald 0.01 0.1 all 2.0 y -'))
case('sa_wald_nested', 'sa Wald, ladder nested (no TD2c/TD3c)',
  paste('SA sa_wald_nested clean y', ORD, '0.001 wald 0.05 0.05 nested 2.0 y -'))
case('sa_lpa', 'sa ATD with an ami lp on the desc (tail/quirk arms)',
  paste('SA sa_lpa clean y', ORD, '0.001 bic 0.01 0.1 all 2.0 y lp:a'))
case('sa_td_sel0', 'sa ATD with the selected core already on the desc (keep arm)',
  paste('SA sa_td_sel0 clean y', ORD, '0.001 aicc 0.01 0.1 all 2.0 y td:sel0:a'))
case('sa_tdeff', 'sa ATD on the strong-TD series',
  paste('SA sa_tdeff tdeff y', ORD, '0.001 bic 0.01 0.1 all 2.0 y -'))
# FS: the FastRegressionTest calendar branch (the 23-E seam, activated).
case('fs_all', 'fast step: ami td+lp+easter (mhTest slot)',
  paste('FS fs_all clean y', ORD, '1e-5 1.96 1.96 y td:TD7:a,lp:a,ea:a'))
case('fs_tdonly', 'fast step: ami td only',
  paste('FS fs_tdonly clean y', ORD, '1e-5 1.96 1.96 y td:TD7:a'))
case('fs_tdeff', 'fast step: strong-TD series',
  paste('FS fs_tdeff tdeff y', ORD, '1e-5 1.96 1.96 y td:TD7:a,lp:a,ea:a'))
case('fs_nomean', 'fast step: testMean off',
  paste('FS fs_nomean clean y', ORD, '1e-5 1.96 1.96 n td:TD7:a,lp:a'))
# DS: the DefaultRegressionTest calendar fields (T + joinTest arms).
case('ds_t', 'default step: T arm (join off), wd-shaped TD2',
  paste('DS ds_t clean y', ORD, '1e-5 1.96 y y n TD2 n n -'))
case('ds_join', 'default step: joinTest arm on the TD2 window',
  paste('DS ds_join clean y', ORD, '1e-5 1.96 y y y TD2 n n -'))
case('ds_join_td7', 'default step: joinTest arm on the six-column TD7',
  paste('DS ds_join_td7 clean y', ORD, '1e-5 1.96 y y y TD7 n n -'))
case('ds_lp', 'default step: lp field alongside a kept td',
  paste('DS ds_lp clean y', ORD, '1e-5 1.96 y y n TD2 y n -'))
case('ds_ea', 'default step: easter field (mhTest arm)',
  paste('DS ds_ea clean y', ORD, '1e-5 1.96 y y n TD2 n y -'))
case('ds_unproc', 'default step: the Unprocessed guard',
  paste('DS ds_unproc clean y', ORD, '1e-5 1.96 n n n n n n -'))
# JT: the JointTest quadratic form (window + selection echoes).
case('jt_w16', 'joint test: window pos 1 dim 6, k = 0',
  paste('JT jt_w16 clean y', ORD, '1e-5 1 6 0'))
case('jt_k4', 'joint test: window pos 1 dim 6, k = the free parameters',
  paste('JT jt_k4 clean y', ORD, '1e-5 1 6 4'))
case('jt_all', 'joint test: select-all short circuit',
  paste('JT jt_all clean y', ORD, '1e-5 0 7 0'))
# PT/PX: the full pipelines with the calendar auto-method overrides.
for (m in c('AIC', 'BIC', 'WALD', 'FTEST'))
  case(paste0('pt_', tolower(m)), paste('TRfull pipeline, tradingDays method=', m),
    paste('PT pt_', tolower(m), ' ', m, ' clean', sep = ''))
for (m in c('base', 'AIC', 'WALD5'))
  case(paste0('px_', tolower(m)), paste('rsa4 pipeline, tradingDays.auto=', m),
    paste('PX px_', tolower(m), ' ', m, ' clean', sep = ''))

# Load witnesses: the machine classes must be live in the pipeline/step runs.
loadwitness('lw_pt_ftest', 'PT lw_pt_ftest FTEST clean', 'AutomaticFRegressionTest')
loadwitness('lw_pt_aic', 'PT lw_pt_aic AIC clean', 'AutomaticRegressionTest')
loadwitness('lw_px_aic', 'PX lw_px_aic AIC clean',
  'AutomaticTradingDaysRegressionTest')
loadwitness('lw_ds_join', paste('DS lw_ds_join clean y', ORD, '1e-5 1.96 y y y TD2 n n -'),
  'linearmodel.JointTest')
loadwitness('lw_fs_all', paste('FS lw_fs_all clean y', ORD, '1e-5 1.96 1.96 y td:TD7:a'),
  'tramo.FastRegressionTest')

wcsv <- function(name, df) {
  con <- file(file.path(output, name), 'w')
  write.csv(df, con, row.names = FALSE, na = '')
  close(con)
}
wcsv('cases.csv', do.call(rbind, summaries))

jv <- system2(javabin, c('-version'), stderr = TRUE)[[1L]]
writeLines(c(
  paste0('# metadata for the item 21-I.3 calendar-detection capture ',
    '(generated by validation/reference/capture-calendar-detection.R)'),
  paste0('r_version <- "', paste(R.version$major, R.version$minor, sep = '.'), '"'),
  paste0('jvm_version <- "', jv, '"'),
  paste0('java_options <- "', Sys.getenv('_JAVA_OPTIONS'), '"'),
  paste0('capture_time <- "', format(Sys.time(), tz = 'UTC'), '"'),
  paste0('capture_host <- "', Sys.info()[['nodename']], '"'),
  paste0('capture_user <- "', Sys.info()[['user']], '"'),
  paste0('probe_sha256 <- "', reference_sha256(file.path(root,
    'validation/reference/CalendarDetectionProbe.java')), '"'),
  paste0('script_sha256 <- "', reference_sha256(file.path(root,
    'validation/reference/capture-calendar-detection.R')), '"'),
  'jar_sha256 <- c(',
  paste0('  "', vapply(jars, function(p) paste0(basename(p), '=',
    reference_sha256(p)), character(1)), '"',
    collapse = ',\n  '),
  ')'
), file.path(output, 'metadata.R'))

files <- setdiff(list.files(output), 'SHA256SUMS.csv')
if (nzchar(repro)) {
  data_files <- setdiff(files, 'metadata.R')
  prior <- setdiff(intersect(list.files(repro), data_files),
    c('metadata.R', 'SHA256SUMS.csv'))
  if (!identical(sort(data_files), sort(prior)))
    stop('repro capture has a different file set: ',
      paste(setxor(data_files, prior), collapse = ', '))
  for (f in data_files) {
    a <- readBin(file.path(output, f), 'raw', file.size(file.path(output, f)))
    b <- readBin(file.path(repro, f), 'raw', file.size(file.path(repro, f)))
    if (!identical(a, b)) stop('repro capture differs byte-wise: ', f)
  }
  message('repro check: ', length(data_files), ' data files byte-identical')
}
write.csv(data.frame(file = files,
  sha256 = vapply(files, function(f)
    digest::digest(file = file.path(output, f), algo = 'sha256'), character(1))),
  file.path(output, 'SHA256SUMS.csv'), row.names = FALSE, quote = FALSE)
message('wrote ', length(files), ' files to ', output)
