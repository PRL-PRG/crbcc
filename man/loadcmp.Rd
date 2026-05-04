\name{loadcmp}
\alias{loadcmp}
\title{Load a Compiled R File}
\description{
Loads bytecode compiled by \code{\link{cmpfile}} into an environment.
}
\usage{
loadcmp(file, envir = .GlobalEnv)
}
\arguments{
  \item{file}{Path to the compiled \code{.Rc} file.}
  \item{envir}{The environment to load into.}
}
\value{
Invisible \code{NULL}.
}
\examples{
tmp_r <- tempfile(fileext = ".R")
tmp_rc <- tempfile(fileext = ".Rc")
writeLines("f <- function(x) x * 2", tmp_r)
cmpfile(tmp_r, tmp_rc)
loadcmp(tmp_rc)
f(3)
}
