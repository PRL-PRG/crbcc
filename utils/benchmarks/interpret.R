library(dplyr)

# Define file paths
file_crbcc <- "test_results/crbcc.csv"
file_gnur <- "test_results/gnur.csv"
output_tex <- "test_results/metrics.tex"

# Load and merge data
df_crbcc <- read.csv(file_crbcc, stringsAsFactors = FALSE)
df_gnur <- read.csv(file_gnur, stringsAsFactors = FALSE)

df_merged <- inner_join(df_gnur, df_crbcc, 
                        by = c("package", "name"), 
                        suffix = c(".gnur", ".crbcc")) %>%
  filter(time.crbcc > 0) %>%
  mutate(speedup = time.gnur / time.crbcc)

# Helper functions for LaTeX formatting
format_num <- function(x) sprintf("%.2f", x)
make_macro <- function(name, value) {
  sprintf("\\newcommand{\\%s}{%s}", name, format_num(value))
}

macros <- c()

# Global Metrics
t_crbcc_global <- sum(df_merged$time.crbcc, na.rm = TRUE)
t_gnur_global <- sum(df_merged$time.gnur, na.rm = TRUE)
ws_global <- t_gnur_global / t_crbcc_global
gs_global <- exp(mean(log(df_merged$speedup), na.rm = TRUE))

macros <- c(macros,
  "% Global Metrics",
  make_macro("GlobalTotalTimeCRBCC", t_crbcc_global),
  make_macro("GlobalTotalTimeGNUR", t_gnur_global),
  make_macro("GlobalGeomSpeedup", gs_global),
  make_macro("GlobalWallclockSpeedup", ws_global),
  ""
)

# Package-Specific Metrics
target_pkgs <- c("base", "compiler", "stats", "utils", "tools")

for (pkg in target_pkgs) {
  df_pkg <- df_merged %>% filter(package == pkg)
  
  t_c <- sum(df_pkg$time.crbcc, na.rm = TRUE)
  t_g <- sum(df_pkg$time.gnur, na.rm = TRUE)
  ws <- t_g / t_c
  gs <- exp(mean(log(df_pkg$speedup), na.rm = TRUE))
  
  pkg_camel <- paste0(toupper(substr(pkg, 1, 1)), substr(pkg, 2, nchar(pkg)))
  
  macros <- c(macros,
    sprintf("%% Package: %s", pkg),
    make_macro(paste0(pkg_camel, "TotalTimeCRBCC"), t_c),
    make_macro(paste0(pkg_camel, "TotalTimeGNUR"), t_g),
    make_macro(paste0(pkg_camel, "GeomSpeedup"), gs),
    make_macro(paste0(pkg_camel, "WallclockSpeedup"), ws),
    ""
  )
}

# Sum of the Five Specified Packages
df_sum5 <- df_merged %>% filter(package %in% target_pkgs)

t_c_5 <- sum(df_sum5$time.crbcc, na.rm = TRUE)
t_g_5 <- sum(df_sum5$time.gnur, na.rm = TRUE)
ws_5 <- t_g_5 / t_c_5
gs_5 <- exp(mean(log(df_sum5$speedup), na.rm = TRUE))

macros <- c(macros,
  "% Sum of Target 5 Packages (base, compiler, stats, utils, tools)",
  make_macro("TopFiveTotalTimeCRBCC", t_c_5),
  make_macro("TopFiveTotalTimeGNUR", t_g_5),
  make_macro("TopFiveGeomSpeedup", gs_5),
  make_macro("TopFiveWallclockSpeedup", ws_5),
  ""
)

# Worst Performing Function Speedup
worst_speedup <- min(df_merged$speedup, na.rm = TRUE)

macros <- c(macros,
  "% Worst Performing Function",
  make_macro("WorstFunctionSpeedup", worst_speedup)
)

# Write output to .tex file
writeLines(macros, output_tex)
cat(sprintf("LaTeX macros successfully written to %s\n", output_tex))