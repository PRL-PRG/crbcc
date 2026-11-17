#' Returns the path of the R CMD INSTALL hook
#'
#' The hook is an R_PROFILE_USER file. It makes R CMD INSTALL compile the
#' closures of a package with crbcc.
#'
#' @export
r_cmd_install_hook_path <- function()
  system.file("r-cmd-install-hook.R", package = "crbcc", mustWork = TRUE)

## The value of R_PROFILE_USER before enable_r_cmd_install_hook(TRUE).
hook_state <- new.env(parent = emptyenv())

#' Enables or disables the R CMD INSTALL hook
#'
#' Sets R_PROFILE_USER to the hook, or sets it back to its previous value.
#' The R processes that this R session starts read R_PROFILE_USER, thus
#' `install.packages()` compiles the packages with crbcc.
#'
#' @param enable TRUE enables the hook. FALSE disables it.
#' @return The previous state (TRUE or FALSE), invisibly.
#' @export
enable_r_cmd_install_hook <- function(enable = TRUE) {
  if (!isTRUE(enable) && !isFALSE(enable))
    stop("'enable' must be TRUE or FALSE")
  path <- r_cmd_install_hook_path()
  old <- identical(Sys.getenv("R_PROFILE_USER"), path)
  if (enable && !old) {
    hook_state$profile <- Sys.getenv("R_PROFILE_USER", NA)
    Sys.setenv(R_PROFILE_USER = path)
  } else if (!enable && old) {
    profile <- hook_state$profile
    if (is.null(profile) || is.na(profile)) Sys.unsetenv("R_PROFILE_USER")
    else Sys.setenv(R_PROFILE_USER = profile)
  }
  invisible(old)
}
