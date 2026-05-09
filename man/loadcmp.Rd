\name{loadcmp}
\alias{loadcmp}
\title{Load a Compiled R File}
\description{
Loads bytecode compiled by \code{\link{cmpfile}} into an environment.
}
\usage{
loadcmp(file, envir)
}
\arguments{
  \item{file}{Path to the compiled \code{.Rc} file.}
  \item{envir}{The environment to load into, use .GlobalEnv to load the code into current global session}
}
\value{
Invisible \code{NULL}.
}
\examples{
tmp_r <- tempfile(fileext = ".R")
tmp_rc <- tempfile(fileext = ".Rc")
writeLines("f <- function(x) x * 2", tmp_r)
cmpfile(tmp_r, tmp_rc)
loadcmp(tmp_rc, .GlobalEnv)
f(3)
}
