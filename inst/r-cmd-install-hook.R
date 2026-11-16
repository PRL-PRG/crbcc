# An R_PROFILE_USER file that makes R CMD INSTALL compile the closures of a
# package with crbcc. R reads it at the start of each R process. For the
# functions that use it, refer to ?crbcc::enable_r_cmd_install_hook.
#
# The environment variable CRBCC_MODE sets the mode:
#
#   (not set)    Compile with `crbcc::cmpfun`. If crbcc cannot compile a
#                closure, compile it with GNU R. The file writes no output.
#   bench_gnur   Compile with `compiler::cmpfun` and measure the time.
#   bench_crbcc  Compile with `crbcc::cmpfun` and measure the time.
#   bench_both   Compile each closure with GNU R and crbcc, compare the
#                results and measure the compile time of each compiler
#                (`crbcc:::bench_closure`). CRBCC_ITERATIONS is the number of
#                batches of `crbcc:::compile_time`. The default is 10.
#
# inst/benchmarks/crbcc_bench.py uses the bench modes. They write to stdout:
#
#   RBC bytecode: <s>  bench_gnur, bench_crbcc: the time in the byte-code
#                      compiler. There is one line for each R process that
#                      compiles a closure.
#   RBC native: <s>    bench_gnur, bench_crbcc: the time in
#                      `tools:::.shlib_internal`, which compiles and links src/.
#   RBC fn ...         bench_both: one line for each closure. The key is
#                      <process id>:<index> (see `crbcc:::bench_closure`).
#   RBC differs: ...   bench_both: a closure with different results.
#
# To install a package with crbcc:
#
#   R_PROFILE_USER=$(Rscript -e 'cat(crbcc::r_cmd_install_hook_path())') \
#     R CMD INSTALL foo_1.0.tar.gz

local({
    mode <- Sys.getenv("CRBCC_MODE")
    if (!mode %in% c("", "bench_gnur", "bench_crbcc", "bench_both")) {
        warning("CRBCC_MODE has an unknown value \"", mode, "\". crbcc is not used.",
                call. = FALSE)
        return(invisible())
    }

    now <- function() unclass(Sys.time())

    ## Replace a function in a sealed namespace.
    patch <- function(pkg, name, fun) {
        ns <- asNamespace(pkg)
        unlockBinding(name, ns)
        assign(name, fun, envir = ns)
        lockBinding(name, ns)
    }

    ## Native: R CMD INSTALL compiles and links src/ in .shlib_internal.
    if (mode %in% c("bench_gnur", "bench_crbcc"))
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
    ## the JIT of R compiles them with `tryCmpfun`, and the bench modes count
    ## them.
    setHook(packageEvent("compiler", "onLoad"), function(...) {
        bc <- compiler::cmpfun
        ## crbcc does not read the options that R CMD INSTALL sets.
        opts <- list(suppressAll = FALSE, suppressUndefined = TRUE,
                     suppressNoSuperAssignVar = TRUE)

        if (mode == "bench_both") {
            bench <- crbcc:::bench_closure  # Load crbcc before the first closure.
            k <- as.integer(Sys.getenv("CRBCC_ITERATIONS", "10"))
            n <- 0
            patch("compiler", "tryCmpfun", bc(function(f) {
                n <<- n + 1
                bench(f, sprintf("%d:%d", Sys.getpid(), n), k, opts)
            }))
            return(invisible())
        }

        if (mode == "") {
            cmp <- crbcc::cmpfun
            gnur <- compiler:::tryCmpfun
            patch("compiler", "tryCmpfun", bc(function(f)
                tryCatch(cmp(f, options = opts), error = function(e) gnur(f))))
            return(invisible())
        }

        cmpfun <- if (mode == "bench_gnur") compiler::cmpfun else {
            cmp <- crbcc::cmpfun  # Load crbcc before the first time measurement.
            bc(function(f) cmp(f, options = opts))
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
