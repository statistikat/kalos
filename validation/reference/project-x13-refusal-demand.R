# Project a completed X13 result to the historical refusal-demand text format.
suppressPackageStartupMessages(library(rjd3x13))
stopifnot(packageVersion("rjd3x13") == "3.9.0", packageVersion("rjd3toolkit") == "3.9.1",
          packageVersion("rjd3jars") == "0.0.6", packageVersion("RProtoBuf") == "0.4.28")
invisible(rjd3x13::x13_spec("rsa0"))
root <- commandArgs(TRUE)[1L]
g <- function(v) if (is.null(v) || !length(v)) "NULL" else
  paste(sprintf("%.17g", as.numeric(v)), collapse = ";")
pickv <- function(x) if (is.null(x) || !length(x)) NULL else
  unlist(x[seq(1, length(x), 2)])
demand_capture <- function(tpl, id) {
  base <- file.path(root,paste0(tpl,"-",id))
  path <- paste0(base,".pb")
  protobuf <- RProtoBuf::read(RProtoBuf::P("x13.X13Results"), readBin(path,"raw",n=file.size(path)))
  f <- rjd3x13:::.p2r_x13_rslts(protobuf)
  nforecast <- as.integer(readLines(paste0(base, ".nf"), warn = FALSE))
  d <- f$preprocessing$description
  names_ <- vapply(d$variables, function(v) v$name, character(1))
  nonconst <- names_ != "const"
  fs <- f$decomposition$final_seasonal
  c(paste0("template=", tpl),
    paste0("series=", id),
    paste0("log=", d$log),
    paste0("d=", d$arima$d, " bd=", d$arima$bd),
    paste0("p=", length(d$arima$phi) %/% 2L),
    paste0("q=", length(d$arima$theta) %/% 2L),
    paste0("bp=", length(d$arima$bphi) %/% 2L),
    paste0("bq=", length(d$arima$btheta) %/% 2L),
    paste0("theta=", g(pickv(d$arima$theta))),
    paste0("btheta=", g(pickv(d$arima$btheta))),
    paste0("phi=", g(pickv(d$arima$phi))),
    paste0("bphi=", g(pickv(d$arima$bphi))),
    paste0("nvars=", sum(nonconst)),
    paste0("vars=", paste(names_[nonconst], collapse = "|")),
    paste0("lsr_coef=", g(unlist(lapply(d$variables[nonconst],
                                        function(v) v$coef$value)))),
    paste0("coef_types=", paste(vapply(d$variables[nonconst],
                                       function(v) v$coef$type[1], character(1)),
                                collapse = "|")),
    paste0("nforecast=", nforecast),
    paste0("final_henderson=", g(f$decomposition$final_henderson)),
    paste0("final_seasonal=", paste(as.character(fs), collapse = "|")),
    paste0("final_seasonal_n=", length(fs)))
}
for (tpl in c("rsa0", "rsa1", "rsa2c")) for (id in c("air", "ao", "seas")) {
  path <- file.path(root, sprintf("demand-%s-%s.txt", tpl, id))
  con <- file(path, "wb")
  writeLines(demand_capture(tpl, id), con)
  close(con)
}
