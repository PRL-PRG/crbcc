\name{compile}
\alias{compile}
\title{Compile a Generic Language Object}
\description{
Compiles a generic R language object (such as an expression or function)
to bytecode using the C-based compiler backend.
}
\usage{
compile(e, env = .GlobalEnv, options = NULL, srcref = NULL)
}
\arguments{
  \item{e}{An R language object to compile.}
  \item{env}{The environment in which to evaluate the object.}
  \item{options}{A list of compilation options, or \code{NULL}.}
  \item{srcref}{Optional source reference information.}
}
\value{
The compiled object, or invisible \code{NULL} if compilation is only for side effect.
}
\examples{
expr <- quote({ x <- 1; x + 2 })
compile(expr)
}
