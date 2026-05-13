library(dplyr)
library(ggplot2)

options(scipen = 999)

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

plot <- ggplot(df_filtered, aes(x = time.crbcc * 1000, y = time.gnur * 1000, color = package)) +
  geom_point(alpha = 0.6, size = 1.5) +
  geom_abline(intercept = 0, slope = 1, linetype = "dashed", color = "black") +
  scale_x_log10() +
  scale_y_log10() +
  labs(
    x = "CRBCC Time (log10) ms",
    y = "GNU-R Time (log10) ms",
    color = "Package"
  ) +
  theme_minimal() +
  theme(
    legend.position = c(0.05, 0.95), 
    legend.justification = c(0, 1),
    legend.background = element_rect(fill = "white", color = "white"),
    plot.title = element_text(face = "bold")
  )

ggsave("test_results/speedup_scatter.png", plot = plot, width = 8, height = 6, dpi = 300)