\name{enable_r_cmd_install_hook}
\alias{enable_r_cmd_install_hook}
\alias{r_cmd_install_hook_path}
\title{Install Packages with crbcc}
\description{
\code{R CMD INSTALL} compiles the closures of a package to byte code. The
R CMD INSTALL hook makes it use crbcc for this compilation. The hook is an
\code{R_PROFILE_USER} file, which R reads at the start of each R process.

\code{r_cmd_install_hook_path} returns the path of the hook.

\code{enable_r_cmd_install_hook} sets the environment variable
\code{R_PROFILE_USER} of this R session to the hook. The R processes that
this R session starts read this variable. Thus \code{install.packages()}
compiles the packages with crbcc. \code{enable_r_cmd_install_hook(FALSE)}
sets \code{R_PROFILE_USER} back to its previous value.
}
\usage{
r_cmd_install_hook_path()
enable_r_cmd_install_hook(enable = TRUE)
}
\arguments{
  \item{enable}{\code{TRUE} enables the hook. \code{FALSE} disables it.}
}
\details{
crbcc compiles only the packages that R installs from source. A binary
package is already compiled. On Windows and with the R for macOS from CRAN,
\code{install.packages()} installs binary packages by default. To install
from source, use \code{type = "source"}.

If crbcc cannot compile a closure, the hook compiles it with the compiler
package.

When the hook is enabled, R does not read the usual \code{.Rprofile} file
in the R processes that this R session starts.

The environment variable \code{CRBCC_MODE} sets a benchmark mode of the
hook. The benchmarks of crbcc use these modes. For the modes, refer to the
comments in the hook.
}
\value{
\code{r_cmd_install_hook_path} returns a path.
\code{enable_r_cmd_install_hook} returns the previous state, \code{TRUE} or
\code{FALSE}, invisibly.
}
\examples{
r_cmd_install_hook_path()

\dontrun{
enable_r_cmd_install_hook()
install.packages("MASS", type = "source")
enable_r_cmd_install_hook(FALSE)
}

## From a shell:
## R_PROFILE_USER=$(Rscript -e 'cat(crbcc::r_cmd_install_hook_path())') \\
##   R CMD INSTALL foo_1.0.tar.gz
}
