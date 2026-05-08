library(dplyr)
library(ggplot2)
library(purrr)

TARGET_PACKAGES <- c("base", "compiler", "stats", "utils", "tools")
file_crbcc <- "test_results/crbcc.csv"
file_gnur  <- "test_results/gnur.csv"

get_loc <- function(pkg, fn_name) {
  tryCatch({
    ns <- asNamespace(pkg)
    fn <- get(fn_name, envir = ns)
    if (is.function(fn)) {
      # deparse() converts the function closure to a character vector of lines
      return(length(deparse(fn)))
    } else {
      return(NA_integer_)
    }
  }, error = function(e) {
    return(NA_integer_)
  })
}

df_crbcc <- read.csv(file_crbcc, stringsAsFactors = FALSE)
df_gnur  <- read.csv(file_gnur, stringsAsFactors = FALSE)

# Merge datasets and filter to target packages and valid times
df_merged <- inner_join(df_gnur, df_crbcc, 
                        by = c("package", "name"), 
                        suffix = c(".gnur", ".crbcc")) %>%
  filter(package %in% TARGET_PACKAGES, time.crbcc > 0) %>%
  mutate(speedup = time.gnur / time.crbcc)

cat("Extracting LoC via namespace reflection. This may take a moment...\n")

# Apply the LoC extraction row by row
df_analysis <- df_merged %>%
  rowwise() %>%
  mutate(loc = get_loc(package, name)) %>%
  ungroup() %>%
  filter(!is.na(loc) & loc > 0) # Remove unresolved or empty functions

# Calculate package-level metrics for the facet headers
pkg_metrics <- df_analysis %>%
  group_by(package) %>%
  summarize(
    wall_clock = sum(time.gnur, na.rm = TRUE) / sum(time.crbcc, na.rm = TRUE),
    geom_speedup = exp(mean(log(speedup), na.rm = TRUE))
  ) %>%
  mutate(
    # Create a descriptive label for the facet
    facet_label = sprintf("%s | Wall-clock: %.2fx | Geom: %.2fx", 
                          toupper(package), wall_clock, geom_speedup)
  )

# Join the labels back to the main dataframe
df_plot_data <- df_analysis %>%
  inner_join(pkg_metrics, by = "package") %>%
  # Factor the labels to ensure consistent ordering based on TARGET_PACKAGES
  mutate(facet_label = factor(facet_label, levels = pkg_metrics$facet_label[match(TARGET_PACKAGES, pkg_metrics$package)]))

plot <- ggplot(df_plot_data, aes(x = loc, y = package, fill = package)) +
  geom_violin(trim = FALSE, alpha = 0.7, scale = "width") +
  # Add a boxplot inside the violin for statistical context
  geom_boxplot(width = 0.1, fill = "white", outlier.shape = NA, alpha = 0.5) +
  # Log10 scale for LoC because code size typically follows a log-normal distribution
  scale_x_log10(labels = scales::comma) +
  facet_wrap(~ facet_label, ncol = 1, scales = "free_y") +
  labs(
    title = "Function Density by Lines of Code",
    subtitle = "Annotated with package-level Wall-clock and Geometric Mean Speedups",
    x = "Lines of Code (log10 scale)",
    y = NULL
  ) +
  theme_bw() +
  theme(
    legend.position = "none",
    axis.text.y = element_blank(),
    axis.ticks.y = element_blank(),
    strip.text = element_text(face = "bold", size = 10),
    strip.background = element_rect(fill = "#f0f0f0")
  )

ggsave("test_results/loc_violin_plots.png", plot = plot, width = 8, height = 10, dpi = 300)