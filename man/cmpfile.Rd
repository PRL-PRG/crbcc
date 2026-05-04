\name{cmpfile}
\alias{cmpfile}
\title{Compile an R Source File}
\description{
Compiles all top-level expressions in an R source file to bytecode and
saves the result to an output file.
}
\usage{
cmpfile(infile, outfile, ascii = FALSE, env = .GlobalEnv,
        verbose = FALSE, options = NULL, version = NULL)
}
\arguments{
  \item{infile}{Path to the input R source file.}
  \item{outfile}{Path to the output file. If missing, defaults to the input
  file name with extension replaced by \code{.Rc}.}
  \item{ascii}{Logical; whether to write an ASCII file.}
  \item{env}{The top-level environment in which to evaluate expressions.}
  \item{verbose}{Logical; print compilation progress messages.}
  \item{options}{List of compilation options.}
  \item{version}{Optional file format version.}
}
\value{
Invisible \code{NULL}. Output is written to file as a side effect.
}
\examples{
tmp_r <- tempfile(fileext = ".R")
tmp_rc <- tempfile(fileext = ".Rc")
writeLines("x <- 1; x + 1", tmp_r)
cmpfile(tmp_r, tmp_rc)
}
