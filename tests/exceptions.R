source("synthetic/warnings-errors.R")

capture_compile <- function(compile_fn, fn, options) {
  warns <- character(0)
  msgs <- character(0)
  out <- character(0)
  err <- NULL

  withCallingHandlers(
    tryCatch({
      out <- capture.output({
        compiled <- compile_fn(fn, options = options)
        invisible(compiled)
      })
    }, error = function(e) {
      err <<- conditionMessage(e)
    }),
    warning = function(w) {
      warns <<- c(warns, conditionMessage(w))
      invokeRestart("muffleWarning")
    },
    message = function(m) {
      msgs <<- c(msgs, conditionMessage(m))
      invokeRestart("muffleMessage")
    }
  )

  list(warnings = warns, messages = msgs, output = out, error = err)
}

canon_lines <- function(x) {
  if (length(x) == 0) {
    return(character(0))
  }
  y <- trimws(x)
  y <- y[nzchar(y)]
  unname(y)
}

canon_error <- function(x) {
  if (is.null(x)) {
    return(NULL)
  }
  trimws(unname(x))
}

compare_one <- function(name, fn, options) {
  crbcc_res <- capture_compile(crbcc::cmpfun, fn, options)
  compiler_res <- capture_compile(compiler::cmpfun, fn, options)

  crbcc_warnings <- canon_lines(crbcc_res$warnings)
  compiler_warnings <- canon_lines(compiler_res$warnings)
  crbcc_messages <- canon_lines(crbcc_res$messages)
  compiler_messages <- canon_lines(compiler_res$messages)
  crbcc_output <- canon_lines(crbcc_res$output)
  compiler_output <- canon_lines(compiler_res$output)
  crbcc_error <- canon_error(crbcc_res$error)
  compiler_error <- canon_error(compiler_res$error)

  same_warnings <- identical(crbcc_warnings, compiler_warnings)
  same_messages <- identical(crbcc_messages, compiler_messages)
  same_output <- identical(crbcc_output, compiler_output)
  same_error <- identical(crbcc_error, compiler_error)

  list(
    name = name,
    crbcc = list(
      warnings = crbcc_warnings,
      messages = crbcc_messages,
      output = crbcc_output,
      error = crbcc_error
    ),
    compiler = list(
      warnings = compiler_warnings,
      messages = compiler_messages,
      output = compiler_output,
      error = compiler_error
    ),
    same_warnings = same_warnings,
    same_messages = same_messages,
    same_output = same_output,
    same_error = same_error,
    match = same_warnings && same_messages && same_output && same_error
  )
}

results <- lapply(names(warning_error_corpus), function(name) {
  compare_one(name, warning_error_corpus[[name]], compiler_options_warnings)
})

for (res in results) {
  if (res$match) {
    next
  }

  cat(sprintf("\n[%s] MISMATCH\n", res$name))

  cat(sprintf(
    "diff buckets: warnings=%s, messages=%s, output=%s, error=%s\n",
    if (res$same_warnings) "ok" else "diff",
    if (res$same_messages) "ok" else "diff",
    if (res$same_output) "ok" else "diff",
    if (res$same_error) "ok" else "diff"
  ))

  cat("crbcc warnings:\n")
  if (length(res$crbcc$warnings) > 0) {
    cat(paste0("- ", res$crbcc$warnings), sep = "\n")
    cat("\n")
  } else {
    cat("- (none)\n")
  }

  cat("compiler warnings:\n")
  if (length(res$compiler$warnings) > 0) {
    cat(paste0("- ", res$compiler$warnings), sep = "\n")
    cat("\n")
  } else {
    cat("- (none)\n")
  }

  cat("crbcc messages:\n")
  if (length(res$crbcc$messages) > 0) {
    cat(paste0("- ", res$crbcc$messages), sep = "\n")
    cat("\n")
  } else {
    cat("- (none)\n")
  }

  cat("compiler messages:\n")
  if (length(res$compiler$messages) > 0) {
    cat(paste0("- ", res$compiler$messages), sep = "\n")
    cat("\n")
  } else {
    cat("- (none)\n")
  }

  cat("crbcc output:\n")
  if (length(res$crbcc$output) > 0) {
    cat(paste0("- ", res$crbcc$output), sep = "\n")
    cat("\n")
  } else {
    cat("- (none)\n")
  }

  cat("compiler output:\n")
  if (length(res$compiler$output) > 0) {
    cat(paste0("- ", res$compiler$output), sep = "\n")
    cat("\n")
  } else {
    cat("- (none)\n")
  }

  if (!is.null(res$crbcc$error) || !is.null(res$compiler$error)) {
    cat(sprintf("crbcc error: %s\n", if (is.null(res$crbcc$error)) "<none>" else res$crbcc$error))
    cat(sprintf("compiler error: %s\n", if (is.null(res$compiler$error)) "<none>" else res$compiler$error))
  }
}

mismatches <- vapply(results, function(x) !isTRUE(x$match), logical(1))

if (any(mismatches)) {
  bad <- names(warning_error_corpus)[mismatches]
  stop(sprintf("Found %d mismatch(es): %s", length(bad), paste(bad, collapse = ", ")))
}

cat(sprintf("\n[EXCPETIONS/WARNINGS] All %d cases matched compiler::cmpfun exactly.\n", length(results)))
