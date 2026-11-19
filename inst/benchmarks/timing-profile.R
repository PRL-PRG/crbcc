# The R_PROFILE_USER file of the suite install of crbcc_bench.py. It measures
# the time in the native-code compiler (`tools:::.shlib_internal`) and in the
# byte-code compiler (`compiler:::tryCmpfun`). At exit, each R process that
# calls one of them writes its total time to stdout:
#
#   RBC native: <s>
#   RBC bytecode: <s>
#
# If CRBCC_BENCH_COMPILER is "crbcc", the file first reads the shipped hook
# (inst/r-cmd-install-hook.R). Thus the timer measures the function that the
# hook installs. If not, it measures the GNU R function.
#
# The R JIT does not compile a small closure that is not in the global
# environment. Thus keep each function small. If not, the JIT compiles it,
# and the installation does more work than without this file.

local({
    if (Sys.getenv("CRBCC_BENCH_COMPILER") == "crbcc")
        sys.source(system.file("r-cmd-install-hook.R", package = "crbcc", mustWork = TRUE),
                   envir = globalenv())

    ## Returns fun with a timer. .Internal(Sys.time()) is the R clock with the
    ## smallest overhead.
    timed <- function(fun, metric) {
        force(fun)  # Read fun now, because patch() replaces it later.
        total <- 0
        reg.finalizer(environment(), onexit = TRUE, function(e)
            if (total > 0) cat(sprintf("RBC %s: %.6f\n", metric, total)))
        function(x) {
            t0 <- .Internal(Sys.time())
            y <- fun(x)
            total <<- total + (.Internal(Sys.time()) - t0)
            y
        }
    }
    patch <- function(pkg, name, fun) {
        ns <- asNamespace(pkg)
        unlockBinding(name, ns)
        assign(name, fun, envir = ns)
        lockBinding(name, ns)
    }
    setHook(packageEvent("tools", "onLoad"), function(...)
        patch("tools", ".shlib_internal", timed(tools:::.shlib_internal, "native")))
    ## R calls the bytecode timer for each closure, thus give it byte code.
    setHook(packageEvent("compiler", "onLoad"), function(...)
        patch("compiler", "tryCmpfun",
              compiler::cmpfun(timed(compiler:::tryCmpfun, "bytecode"))))
})
