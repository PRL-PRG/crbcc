# An R_PROFILE_USER file for R CMD INSTALL. R reads it at the start of each
# R process of R CMD INSTALL. The environment variable RBC_HOOK sets the mode:
#
#   gnur   Compile the closures of the package with `compiler::cmpfun`.
#   crbcc  Compile the closures of the package with `crbcc::cmpfun`.
#   check  Compile each closure with GNU R and crbcc, compare the results and
#          measure the compile time of each compiler
#          (`crbcc:::bench_closure`). RBC_ITERATIONS is the number of batches
#          of `crbcc:::compile_time`. The default is 10.
#
# If RBC_HOOK has a different value, the file does nothing.
#
# The output on stdout:
#   RBC bytecode: <s>  gnur, crbcc: the time in the byte-code compiler. There
#                      is one line for each R process that compiles a closure.
#   RBC native: <s>    gnur, crbcc: the time in `tools:::.shlib_internal`,
#                      which compiles and links src/.
#   RBC fn ...         check: one line for each closure. The key is
#                      <process id>:<index> (see `crbcc:::bench_closure`).
#   RBC differs: ...   check: a closure with different results.
#
# To install a package with crbcc as the byte-code compiler:
#
#   R_PROFILE_USER=$(Rscript -e 'cat(system.file("r-cmd-install-hook.R", package = "crbcc"))') \
#     RBC_HOOK=crbcc R CMD INSTALL foo_1.0.tar.gz

local({
    mode <- Sys.getenv("RBC_HOOK")
    if (!mode %in% c("gnur", "crbcc", "check"))
        return(invisible())

    now <- function() unclass(Sys.time())

    ## Replace a function in a sealed namespace.
    patch <- function(pkg, name, fun) {
        ns <- asNamespace(pkg)
        unlockBinding(name, ns)
        assign(name, fun, envir = ns)
        lockBinding(name, ns)
    }

    ## Native: R CMD INSTALL compiles and links src/ in .shlib_internal.
    setHook(packageEvent("tools", "onLoad"), function(...) {
        shlib <- tools:::.shlib_internal
        patch("tools", ".shlib_internal", function(args) {
            t0 <- now()
            on.exit(cat(sprintf("RBC native: %.6f\n", now() - t0)))
            shlib(args)
        })
    })

    ## Bytecode: R uses `compiler:::tryCmpfun` to compile each closure of the
    ## package. Compile the functions of the hook with `bc`. If you do not,
    ## the JIT of R compiles them with `tryCmpfun`, and the hook counts them.
    setHook(packageEvent("compiler", "onLoad"), function(...) {
        bc <- compiler::cmpfun
        ## crbcc does not read the options that R CMD INSTALL sets.
        opts <- list(suppressAll = FALSE, suppressUndefined = TRUE,
                     suppressNoSuperAssignVar = TRUE)

        if (mode == "check") {
            bench <- crbcc:::bench_closure  # Load crbcc before the first closure.
            k <- as.integer(Sys.getenv("RBC_ITERATIONS", "10"))
            n <- 0
            patch("compiler", "tryCmpfun", bc(function(f) {
                n <<- n + 1
                bench(f, sprintf("%d:%d", Sys.getpid(), n), k, opts)
            }))
            return(invisible())
        }

        if (mode == "gnur")
            cmpfun <- compiler::cmpfun
        else {
            cmp <- crbcc::cmpfun  # Load crbcc before the first time measurement.
            cmpfun <- bc(function(f) cmp(f, options = opts))
        }
        total <- 0
        patch("compiler", "tryCmpfun", bc(function(f) {
            t0 <- now()
            f <- cmpfun(f)
            total <<- total + now() - t0
            f
        }))
        ## Write the total when the R process stops.
        reg.finalizer(environment(), onexit = TRUE, bc(function(e)
            if (total > 0) cat(sprintf("RBC bytecode: %.6f\n", total))))
    })
})
