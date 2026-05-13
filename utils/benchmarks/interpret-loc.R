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

df_merged <- inner_join(df_gnur, df_crbcc, 
                        by = c("package", "name"), 
                        suffix = c(".gnur", ".crbcc")) %>%
  filter(package %in% TARGET_PACKAGES, time.crbcc > 0) %>%
  mutate(speedup = time.gnur / time.crbcc)

cat("Extracting LoC via namespace reflection. This may take a moment...\n")

df_analysis <- df_merged %>%
  rowwise() %>%
  mutate(loc = get_loc(package, name)) %>%
  ungroup() %>%
  filter(!is.na(loc) & loc > 0)

pkg_metrics <- df_analysis %>%
  group_by(package) %>%
  summarize(
    wall_clock = sum(time.gnur, na.rm = TRUE) / sum(time.crbcc, na.rm = TRUE),
    geom_speedup = exp(mean(log(speedup), na.rm = TRUE))
  ) %>%
  mutate(
    # Create a descriptive label for the facet
    facet_label = sprintf("%s \n Wall-clock: %.2fx \n Geom: %.2fx", 
                          toupper(package), wall_clock, geom_speedup)
  )

df_plot_data <- df_analysis %>%
  inner_join(pkg_metrics, by = "package") %>%
  mutate(facet_label = factor(facet_label, levels = pkg_metrics$facet_label[match(TARGET_PACKAGES, pkg_metrics$package)]))

plot <- ggplot(df_plot_data, aes(x = package, y = loc, fill = package)) +
  geom_violin(trim = FALSE, alpha = 0.7, scale = "width") +
  geom_boxplot(width = 0.1, fill = "white", outlier.shape = NA, alpha = 0.5) +
  scale_y_log10(labels = scales::comma) +
  facet_wrap(~ facet_label, nrow = 1, scales = "free_x") +
  labs(
    x = NULL,
    y = "Lines of Code (log10)"
  ) +
  theme_bw() +
  theme(
    legend.position = "none",
    axis.text.x = element_blank(),
    axis.ticks.x = element_blank(),
    strip.text = element_text(face = "bold", size = 10),
    strip.background = element_rect(fill = "#f0f0f0")
  )

ggsave("test_results/loc_violin_plots.png", plot = plot, width = 10, height = 6, dpi = 300)