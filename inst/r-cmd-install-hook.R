# An R_PROFILE_USER file that makes R CMD INSTALL compile the closures of a
# package with crbcc. If crbcc cannot compile a closure, GNU R compiles it.
# For the functions that use this file, refer to
# ?crbcc::enable_r_cmd_install_hook.
#
#   R_PROFILE_USER=$(Rscript -e 'cat(crbcc::r_cmd_install_hook_path())') \
#     R CMD INSTALL foo_1.0.tar.gz

## R CMD INSTALL compiles each closure with `compiler:::tryCmpfun`.
setHook(packageEvent("compiler", "onLoad"), function(...) {
    gnur <- compiler:::tryCmpfun
    cmp <- crbcc::cmpfun
    ## crbcc does not read the options that R CMD INSTALL sets.
    opts <- list(suppressAll = FALSE, suppressUndefined = TRUE,
                 suppressNoSuperAssignVar = TRUE)
    ## R calls the replacement for each closure, thus give it byte code.
    cmpfun <- compiler::cmpfun(function(f)
        tryCatch(cmp(f, options = opts), error = function(e) gnur(f)))
    ns <- asNamespace("compiler")
    unlockBinding("tryCmpfun", ns)
    assign("tryCmpfun", cmpfun, envir = ns)
    lockBinding("tryCmpfun", ns)
})
