print_system_info <- function() {
	cat(R.version.string, "\n\n")

	cat("System:\n")
	cat(sprintf("OS: %s\n", Sys.info()[["sysname"]]))
	cat(sprintf("Release: %s\n", Sys.info()[["release"]]))
	cat(sprintf("Version: %s\n", Sys.info()[["version"]]))
	cat(sprintf("Machine: %s\n\n", Sys.info()[["machine"]]))

	cat("Hardware:\n")
	if (nzchar(Sys.which("lscpu"))) {
		if (nzchar(Sys.which("grep"))) {
			system("lscpu | grep -E 'Architecture|Model name|CPU\\(s\\)|Thread\\(s\\) per core|Core\\(s\\) per socket|Socket\\(s\\)|CPU max MHz'")
		} else {
			cat("grep not available; showing full lscpu output\n")
			system("lscpu")
		}
	} else {
		cat("lscpu not available\n")
	}

	cat("\nMemory:\n")
	if (nzchar(Sys.which("free"))) {
		system("free -h")
	} else {
		cat("free not available\n")
	}

	cat("\n")
}

print_system_info()

source("tests/compare.R")
source("tests/perf.R")