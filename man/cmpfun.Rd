\name{cmpfun}
\alias{cmpfun}
\title{Compile an R Function to Bytecode}
\description{
Compiles an R closure to bytecode using the C-based compiler backend.
}
\usage{
cmpfun(fun, options = list())
}
\arguments{
  \item{fun}{A closure to compile to bytecode. If \code{fun} is a builtin or
    special function, \code{cmpfun} returns it without a change.}
  \item{options}{A named list of compilation options. If \code{options} is
    \code{NULL}, \code{cmpfun} uses the default options.}
}
\value{
A bytecode-compiled function.
}
\examples{
f <- function(x) x + 1
cf <- cmpfun(f)
cf(10)
}
