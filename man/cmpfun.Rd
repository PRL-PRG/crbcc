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
  \item{fun}{A closure to compile to bytecode.}
  \item{options}{A named list of compilation options.}
}
\value{
A bytecode-compiled function.
}
\examples{
f <- function(x) x + 1
cf <- cmpfun(f)
cf(10)
}
