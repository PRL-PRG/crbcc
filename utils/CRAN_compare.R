N_CRAN <- 50
CRAN_WHEN <- "last-week"
CRAN_FALLBACK_TO_STATIC <- FALSE

# If TRUE, install missing target packages before running checks.
INSTALL_MISSING_PACKAGES <- TRUE

# If TRUE, remove newly installed CRAN packages after run.
# Base/recommended packages are never removed.
REMOVE_PACKAGES_AFTER_TEST <- FALSE

OPTIONS <- list(
  optimize=2
)

######################################

if (!requireNamespace("cranlogs", quietly = TRUE)) stop("Package 'cranlogs' is required.")

resolve_packages <- function() {

  top <- cranlogs::cran_top_downloads(when = CRAN_WHEN, count = N_CRAN)
  pkgs <- unique(top$package)
  cat(sprintf("[INFO] Using cranlogs top %d packages (%s).\n", length(pkgs), CRAN_WHEN))
  
  pkgs

}

install_missing_packages <- function(pkgs) {

  missing <- pkgs[!sapply(pkgs, requireNamespace, quietly = TRUE)]  # ← was installed.packages()

  if (length(missing) == 0) {
    cat("[INFO] All target packages already installed and loadable.\n")
    return(character(0))
  }

  if (!INSTALL_MISSING_PACKAGES) {
    cat(sprintf("[INFO] %d package(s) not loadable, will be skipped (INSTALL_MISSING_PACKAGES=FALSE).\n", length(missing)))
    cat(sprintf("[INFO] Missing: %s\n", paste(missing, collapse=", ")))
    return(character(0))
  }

  cat(sprintf("[INFO] Installing %d missing/broken package(s)...\n", length(missing)))
  for (pkg in missing) {
    tryCatch({
      utils::install.packages(pkg, dependencies = TRUE)  # ← also grab deps
      cat(sprintf("[INFO] Installed: %s\n", pkg))
    }, error = function(e) {
      cat(sprintf("[WARN] Failed to install %s: %s\n", pkg, conditionMessage(e)))
    })
  }

  installed_after <- pkgs[sapply(pkgs, requireNamespace, quietly = TRUE)]
  newly_installed <- intersect(missing, installed_after)
  cat(sprintf("[INFO] Newly loadable this run: %d\n", length(newly_installed)))

  newly_installed
}

remove_newly_installed_packages <- function(pkgs) {
  
  if ( ! REMOVE_PACKAGES_AFTER_TEST ) {
    return(invisible(NULL))
  }

  installed_now <- rownames(installed.packages())
  removable <- intersect(pkgs, installed_now)

  cat(sprintf("[CLEANUP] Removing %d newly installed package(s)...\n", length(removable)))

  for (pkg in removable) {
    tryCatch({
      utils::remove.packages(pkg)
      cat(sprintf("[CLEANUP] Removed: %s\n", pkg))
    }, error = function(e) {
      cat(sprintf("[CLEANUP] Failed to remove %s: %s\n", pkg, conditionMessage(e)))
    })
  }

}

grand_total_funs  <- 0L
grand_total_lines <- 0L
grand_total_instrs <- 0L

bytecode_size <- function(xs) {
  if (is.list(xs)) {
    sum(vapply(as.list(xs), bytecode_size, integer(1L)))
  } else if (is.function(xs) || typeof(xs) == "bytecode") {
    tryCatch({
      capture.output(body <- compiler::disassemble(xs))
      bc     <- body[[2]]
      ops    <- vapply(as.list(bc), is.symbol, logical(1L))
      consts <- body[[3]]
      sum(ops) + bytecode_size(consts)
    }, error = function(e) 0L)
  } else {
    0L
  }
}

compare_compilers <- function(prog) {

  res_compiler <- compiler::cmpfun(prog, options=OPTIONS) 
  res_crbcc <- crbcc::cmpfun(prog, options=OPTIONS)

  grand_total_instrs <<- grand_total_instrs + bytecode_size(res_crbcc)

  identical(res_compiler, res_crbcc,
    num.eq          = FALSE,  
    single.NA       = FALSE,  
    attrib.as.set   = FALSE,  
    ignore.bytecode = FALSE,  
    ignore.environment = FALSE,
    ignore.srcref   = FALSE,  
    extptr.as.ref   = TRUE  
  )

}

test_package <- function(package, torture=FALSE) {
  if (!requireNamespace(package, quietly = TRUE)) {
    cat(sprintf("\n[SKIP] Package not available: %s\n", package))
    return(invisible(NULL))
  }
  ns <- getNamespace(package)
  symbols <- ls(ns, all.names=T)
  objects <- lapply(symbols, get, envir=ns)
  is_closure <- sapply(objects, \(x) typeof(x) == "closure")
  funs <- objects[is_closure]
  func_names <- symbols[is_closure]
  uncompiled_funs <- lapply(funs, function(f) {
    res <- f
    body(res) <- body(f)
    return(res)
  })
  names(uncompiled_funs) <- func_names
  compiled_ok <- 0L
  total_lines <- 0L

  for (i in seq_along(uncompiled_funs)) {
    func_id <- names(uncompiled_funs)[i]
    tryCatch({
      res <- compare_compilers(uncompiled_funs[[i]])
      if (!res) {
        cat(sprintf("[MISMATCH] Compilers are NOT identical!\n"))
        cat(sprintf("Stopped at: %s\n", func_id))
        stop("Test suite halted due to fatal crash.")
      }
    }, error = function(e) {
      cat(sprintf("[FATAL] Compiler crashed!\n"))
      cat(sprintf("Stopped at: %s\n", func_id))
      cat(sprintf("Error: %s\n", e$message))
      readLines(con = "stdin", n = 1)
      stop("Test suite halted due to fatal crash.")
    })

    total_lines <- total_lines + length(deparse(body(uncompiled_funs[[i]])))
    compiled_ok <- compiled_ok + 1L
  }

  grand_total_funs  <<- grand_total_funs  + compiled_ok   # ← superassign
  grand_total_lines <<- grand_total_lines + total_lines    # ← superassign

  cat(sprintf("\n[OK] %d functions | %d lines compiled identically\n", compiled_ok, total_lines))
}

pkgs <- resolve_packages()
new <- install_missing_packages(pkgs)
on.exit(remove_newly_installed_packages(new))

for (i in pkgs) {
  cat(sprintf("\n[START] Compiling package: %s ", i))
  test_package(i)
}

cat(sprintf("\n%s\n[TOTAL] %d functions | %d lines | %d opcodes compiled across %d packages\n%s\n",
  strrep("=", 60),
  grand_total_funs,
  grand_total_lines,
  grand_total_instrs,
  length(pkgs),
  strrep("=", 60)
))
