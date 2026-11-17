# An R_PROFILE_USER file that makes R CMD INSTALL compile the closures of a
# package with crbcc. R reads it at the start of each R process. For the
# functions that use it, refer to ?crbcc::enable_r_cmd_install_hook.
#
# The environment variable CRBCC_MODE sets the mode:
#
#   (not set)    Compile with `crbcc::cmpfun`. If crbcc cannot compile a
#                closure, compile it with GNU R. The hook writes no output.
#   bench_gnur   Compile with `compiler::cmpfun` and measure the time. If
#                CRBCC_SAVE_BYTECODE is a directory, also write the byte code
#                of each closure to <directory>/<package>.rds.
#   bench_crbcc  Compile with `crbcc::cmpfun` and measure the time.
#   check        Compile with `crbcc::cmpfun`. Compare the byte code of each
#                closure with the GNU R byte code in
#                <CRBCC_BASELINE>/<package>.rds. The bench modes do not
#                compare, because the comparison adds time.
#
# In the bench modes and in check, if the library CRBCC_PREBUILT_LIB has the
# package, the hook copies the shared object of the package from it. Then
# R CMD INSTALL does not compile src/ again.
#
# inst/benchmarks/crbcc_bench.py and the benchmark Docker image use these
# modes. The bench modes and check write these lines to stdout:
#
#   RBC bytecode: <s>  The time in the byte-code compiler. There is one line
#                      for each R process that compiles a closure.
#   RBC native: <s>    The time in `tools:::.shlib_internal`, which compiles
#                      and links src/.
#   RBC fn<TAB><key><TAB><s><TAB><check><TAB><size>
#                      One line for each closure. <s> is the compile time. It
#                      does not include the work for the other values.
#                      <key> is the md5 of the formals and the body of the
#                      closure, thus a closure has the same key with each
#                      compiler and in each R process. If R compiles closures
#                      with the same key, the key has the suffix #2, #3 and so
#                      on.
#                      <check> (check): "same", "differs", or "unknown" if
#                      the baseline has no byte code for the key. "-" in the
#                      bench modes.
#                      <size> (check): the size of the closure in AST nodes
#                      (`crbcc:::function_size`). NA in the bench modes.
#   RBC differs: <key> <part>
#                      The crbcc byte code is not the same as the GNU R byte
#                      code. <part> is "code", "consts" (a different number of
#                      constants) or "const <index>".
#
# The key and the stored byte code use a canonical form (`canon`):
#
#   - The byte code becomes lists (disassemble). The serialized byte code
#     records which objects are shared, but `identical()` ignores this.
#   - An environment becomes a string. serialize() writes its contents, but
#     `identical()` compares environments by identity. For example, the
#     srcfile of a srcref is an environment with a temporary path.
#   - A closure becomes a list of its formals and body, without its
#     environment. For example, an S4 method can have a closure (`.local`)
#     in its body.
#   - An external pointer becomes a string, because it is not the same
#     pointer after readRDS().
#
# To install a package with crbcc:
#
#   R_PROFILE_USER=$(Rscript -e 'cat(crbcc::r_cmd_install_hook_path())') \
#     R CMD INSTALL foo_1.0.tar.gz

local({
    mode <- Sys.getenv("CRBCC_MODE")
    if (!mode %in% c("", "bench_gnur", "bench_crbcc", "check")) {
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

    ## Native: R CMD INSTALL compiles and links src/ in .shlib_internal. The
    ## objects of a prebuilt source tree are not sufficient, because a
    ## Makevars can delete them (for example "all: clean" in openssl). Thus
    ## copy the shared object from CRBCC_PREBUILT_LIB. If the library does not
    ## have it, run make.
    if (mode != "")
        setHook(packageEvent("tools", "onLoad"), function(...) {
            shlib <- tools:::.shlib_internal
            prebuilt <- Sys.getenv("CRBCC_PREBUILT_LIB")
            patch("tools", ".shlib_internal", function(args) {
                t0 <- now()
                on.exit(cat(sprintf("RBC native: %.6f\n", now() - t0)))
                out <- args[which(args == "-o")[1] + 1]
                so <- file.path(prebuilt, Sys.getenv("R_PACKAGE_NAME"), "libs", out)
                if (nzchar(prebuilt) && !is.na(out) && file.exists(so))
                    return(if (file.copy(so, out, overwrite = TRUE)) 0L else 1L)
                shlib(args)
            })
        })

    ## Bytecode: R uses `compiler:::tryCmpfun` to compile each closure of the
    ## package. Compile the hook functions with `bc`. If you do not, the R
    ## JIT compiles them with `tryCmpfun`, and the bench modes count them.
    setHook(packageEvent("compiler", "onLoad"), function(...) {
        bc <- compiler::cmpfun
        ## crbcc does not read the options that R CMD INSTALL sets.
        opts <- list(suppressAll = FALSE, suppressUndefined = TRUE,
                     suppressNoSuperAssignVar = TRUE)

        if (mode == "") {
            cmp <- crbcc::cmpfun
            gnur <- compiler:::tryCmpfun
            patch("compiler", "tryCmpfun", bc(function(f)
                tryCatch(cmp(f, options = opts), error = function(e) gnur(f))))
            return(invisible())
        }

        md5 <- function(x) unname(tools::md5sum(bytes = serialize(x, NULL)))
        ## Returns x without byte code and environments (see above).
        canon <- function(x) {
            if (typeof(x) == "bytecode") return(canon(.Internal(disassemble(x))))
            if (is.environment(x)) return("<environment>")
            if (typeof(x) == "closure") return(list("<closure>", canon(formals(x)), canon(body(x))))
            if (typeof(x) == "externalptr") return("<externalptr>")
            if (is.list(x)) x[] <- lapply(x, canon)
            else if (typeof(x) == "language")
                for (i in seq_along(x))
                    ## Do not assign an unchanged element. `x[[i]] <- NULL`
                    ## removes the element f(NULL) from the call.
                    if (!identical(x[[i]], quote(expr = )) &&
                        !identical(y <- canon(x[[i]]), x[[i]])) x[[i]] <- y
            if (!is.null(a <- attributes(x))) attributes(x) <- lapply(a, canon)
            x
        }

        ## R CMD INSTALL sets R_PACKAGE_NAME after R reads this file. Thus
        ## read it when R compiles a closure.
        package_file <- function(dir) file.path(dir, paste0(Sys.getenv("R_PACKAGE_NAME"), ".rds"))

        if (mode == "bench_gnur") {
            ## bench_gnur does not load crbcc.
            cmpfun <- compiler::cmpfun
            size <- function(f) NA
            save_dir <- Sys.getenv("CRBCC_SAVE_BYTECODE")
            saved <- new.env(parent = emptyenv())
            check <- function(key, code) {
                if (nzchar(save_dir)) assign(key, code, envir = saved)
                "-"
            }
        } else {
            cmp <- crbcc::cmpfun  # Load crbcc before the first time measurement.
            cmpfun <- bc(function(f) cmp(f, options = opts))
            ## R does not evaluate the argument `code` of check(), thus the
            ## hook does not make its canonical form.
            size <- function(f) NA
            check <- function(key, code) "-"
        }

        if (mode == "check") {
            size <- crbcc:::function_size
            same <- crbcc:::identical_strict
            baseline_dir <- Sys.getenv("CRBCC_BASELINE")
            baseline <- NULL
            check <- function(key, code) {
                if (is.null(baseline)) {
                    f <- package_file(baseline_dir)
                    baseline <<- if (nzchar(baseline_dir) && file.exists(f)) readRDS(f) else list()
                }
                ref <- baseline[[key]]
                if (is.null(ref)) return("unknown")
                if (same(code, ref)) return("same")
                part <- if (!same(code[[2]], ref[[2]])) "code"
                        else if (length(code[[3]]) != length(ref[[3]])) "consts"
                        else paste("const", which(!mapply(same, code[[3]], ref[[3]]))[1])
                cat(sprintf("RBC differs: %s %s\n", key, part))
                "differs"
            }
        }

        seen <- new.env(parent = emptyenv())
        total <- 0
        patch("compiler", "tryCmpfun", bc(function(f) {
            key <- md5(canon(list(formals(f), body(f))))
            t0 <- now()
            g <- cmpfun(f)
            t <- now() - t0
            total <<- total + t
            n <- seen[[key]] <- if (is.null(seen[[key]])) 1 else seen[[key]] + 1
            if (n > 1) key <- paste0(key, "#", n)
            cat(sprintf("RBC fn\t%s\t%.9g\t%s\t%s\n", key, t,
                        check(key, canon(.Internal(bodyCode(g)))), size(f)))
            g
        }))

        ## When the R process stops: write the total and, in bench_gnur, the
        ## byte code. If a previous R process of the same installation wrote
        ## the file, its byte code stays in the file.
        reg.finalizer(environment(), onexit = TRUE, bc(function(e) {
            if (total > 0) cat(sprintf("RBC bytecode: %.6f\n", total))
            if (mode == "bench_gnur" && nzchar(save_dir) && length(keys <- ls(saved, all.names = TRUE))) {
                f <- package_file(save_dir)
                code <- if (file.exists(f)) readRDS(f) else list()
                new <- setdiff(keys, names(code))
                code[new] <- mget(new, envir = saved)
                saveRDS(code, f)
            }
        }))
    })
})
