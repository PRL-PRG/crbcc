library(dplyr)
library(ggplot2)

TARGET_PACKAGES <- c("base", "compiler", "stats", "utils", "tools")

file_crbcc <- "test_results/crbcc.csv"
file_gnur  <- "test_results/gnur.csv"

df_crbcc <- read.csv(file_crbcc, stringsAsFactors = FALSE)
df_gnur  <- read.csv(file_gnur, stringsAsFactors = FALSE)

df_merged <- inner_join(df_gnur, df_crbcc, 
                        by = c("package", "name"), 
                        suffix = c(".gnur", ".crbcc"))

df_filtered <- df_merged %>%
  filter(package %in% TARGET_PACKAGES) %>%
  filter(time.crbcc > 0, time.gnur > 0)

plot <- ggplot(df_filtered, aes(x = time.crbcc, y = time.gnur, color = package)) +
  geom_point(alpha = 0.6, size = 1.5) +
  geom_abline(intercept = 0, slope = 1, linetype = "dashed", color = "black") +
  scale_x_log10() +
  scale_y_log10() +
  labs(
    title = "Compilation Time Comparison: CRBCC vs GNU-R",
    subtitle = paste("Packages:", paste(TARGET_PACKAGES, collapse = ", ")),
    x = "CRBCC Time (log10)",
    y = "GNU-R Time (log10)",
    color = "Package"
  ) +
  theme_minimal() +
  theme(
    legend.position = "right",
    plot.title = element_text(face = "bold")
  )

ggsave("test_results/speedup_scatter.png", plot = plot, width = 8, height = 6, dpi = 300)