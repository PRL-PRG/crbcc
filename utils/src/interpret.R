library(ggplot2)

interpret <- function(source_dir, target_dir) {

  # Ensure target and graphs directories exist
  if (!dir.exists(target_dir)) {
    dir.create(target_dir, recursive = TRUE)
  }
  
  graphs_dir <- file.path(target_dir, "graphs")
  if (!dir.exists(graphs_dir)) {
    dir.create(graphs_dir, recursive = TRUE)
  }

  csv_files <- list.files(path = source_dir, pattern = "\\.csv$", full.names = TRUE)

  if (length(csv_files) == 0) {
    stop("No CSV files found in the specified directory.")
  }

  md_file <- file.path(target_dir, "benchmark_report.md")
  cat("# Global Compiler Benchmark Report\n\n", file = md_file)

  # 1. Aggregate all data into a single dataframe
  global_data_list <- list()

  for (file_path in csv_files) {
    pkg_name <- gsub("_benchmark_results\\.csv$", "", basename(file_path))
    df <- read.csv(file_path)
    
    if (nrow(df) > 0) {
      df$package <- pkg_name
      global_data_list[[pkg_name]] <- df
    }
  }

  if (length(global_data_list) == 0) {
    stop("All CSV files are empty.")
  }

  global_df <- do.call(rbind, global_data_list)
  
  # 2. Clean data globally
  global_df <- na.omit(global_df)
  global_df <- global_df[global_df$median_gnu_r > 0 & global_df$median_crbcc > 0, ]

  if (nrow(global_df) < 3) {
    stop("Insufficient valid data points across all packages to perform statistical analysis.")
  }

  global_df$speedup <- global_df$median_gnu_r / global_df$median_crbcc

  # --- Global Computations ---
  geo_mean_speedup <- exp(mean(log(global_df$speedup)))
  
  total_time_gnur <- sum(global_df$median_gnu_r)
  total_time_crbcc <- sum(global_df$median_crbcc)
  absolute_speedup <- total_time_gnur / total_time_crbcc
  
  quantiles <- round(quantile(global_df$speedup, probs = c(0.01, 0.05, 0.25, 0.50, 0.75, 0.95, 0.99)), 2)
  cor_loc <- suppressWarnings(cor.test(global_df$lines_of_code, global_df$speedup, method = "spearman", exact = FALSE))
  cor_cyc <- suppressWarnings(cor.test(global_df$cyclomatic_complexity, global_df$speedup, method = "spearman", exact = FALSE))

  plot_stamp <- sprintf("Global Metrics | Geometric Average Speedup: %.2fx | Absolute Speedup (Sum): %.2fx", 
                        geo_mean_speedup, absolute_speedup)

  # --- Per-Package Computations ---
  global_df$pkg_geo_mean <- ave(global_df$speedup, global_df$package, FUN = function(x) exp(mean(log(x))))
  global_df$pkg_gnu_sum <- ave(global_df$median_gnu_r, global_df$package, FUN = sum)
  global_df$pkg_crbcc_sum <- ave(global_df$median_crbcc, global_df$package, FUN = sum)
  global_df$pkg_abs_speedup <- global_df$pkg_gnu_sum / global_df$pkg_crbcc_sum
  
  global_df$package_label <- sprintf("%s\nGeo: %.2fx\nAbs: %.2fx", 
                                     global_df$package, 
                                     global_df$pkg_geo_mean, 
                                     global_df$pkg_abs_speedup)

  # --- Generate Per-Package Absolute Throughput Table ---
  pkg_summary <- unique(global_df[, c("package", "pkg_gnu_sum", "pkg_crbcc_sum", "pkg_abs_speedup")])
  pkg_summary <- pkg_summary[order(-pkg_summary$pkg_abs_speedup), ] # Sort highest speedup first
  
  pkg_table_md <- "### Per-Package Absolute Throughput\n\n"
  pkg_table_md <- paste0(pkg_table_md, "| Package | Total GNU R Time (s) | Total crbcc Time (s) | Absolute Speedup |\n")
  pkg_table_md <- paste0(pkg_table_md, "|---|---|---|---|\n")
  
  for (i in 1:nrow(pkg_summary)) {
    pkg_table_md <- paste0(pkg_table_md, sprintf("| %s | %.4f | %.4f | %.4fx |\n", 
                                                 pkg_summary$package[i], 
                                                 pkg_summary$pkg_gnu_sum[i], 
                                                 pkg_summary$pkg_crbcc_sum[i], 
                                                 pkg_summary$pkg_abs_speedup[i]))
  }
  pkg_table_md <- paste0(pkg_table_md, "\n")

  # --- Visualizations ---
  
# Create the text label from the correlation test results
# We use 'rho' for Spearman and format the p-value

p_val <- cor_loc$p.value

if (p_val < 2.2e-16) {
  p_display <- "p < 2.2e-16"
} else if (p_val < 0.001) {
  p_display <- "p < 0.001"
} else {
  p_display <- sprintf("p = %.3f", p_val)
}

stats_label <- sprintf("Spearman's rho: %.3f\np-value: %s", 
                       cor_loc$estimate, 
                       p_display)

  # 1. LOC vs Speedup with Stats Overlay
  plot_cyc <- ggplot(global_df, aes(x = lines_of_code, y = speedup)) +
    geom_point(alpha = 0.3, color = "darkorange") +
    geom_smooth(method = "loess", color = "blue", formula = y ~ x) +
    geom_hline(yintercept = 1, linetype = "dashed", color = "red") +
    # --- ADD THIS LAYER ---
    annotate("text", 
            x = Inf, y = Inf,           # Top right of the plot
            label = stats_label, 
            hjust = 1.1, vjust = 1.5,   # Adjust offset from the edges
            size = 4, fontface = "italic", 
            color = "black", 
            bg.color = "white") +       # Some themes support background labels
    # ----------------------
    scale_y_log10() + 
    scale_x_log10() +
    labs(
      title = "Global Speedup vs Lines of Code",
      subtitle = plot_stamp,
      x = "Lines of Code (log10)",
      y = "Speedup (log10)"
    ) +
    theme_minimal()

  suppressMessages(ggsave(filename = file.path(graphs_dir, "global_correlation_plot.png"), 
                          plot = plot_cyc, width = 8, height = 6, dpi = 300))
                          
  # 2. Raw Time Faceted
  plot_raw_time_faceted <- ggplot(global_df, aes(x = median_gnu_r, y = median_crbcc)) +
    geom_point(alpha = 0.4, color = "purple") +
    geom_abline(intercept = 0, slope = 1, linetype = "dashed", color = "red", linewidth = 0.8) +
    scale_x_log10(labels = scales::scientific) +
    scale_y_log10(labels = scales::scientific) +
    facet_wrap(~ package, scales = "free") +
    labs(title = "Absolute Execution Time by Package: GNU R vs crbcc", subtitle = plot_stamp, caption = "Points below the red line indicate crbcc is faster", x = "GNU R Median Time (seconds, log10)", y = "crbcc Median Time (seconds, log10)") +
    theme_minimal() +
    theme(strip.text = element_text(face = "bold", size = 10), panel.border = element_rect(color = "grey80", fill = NA))
  suppressMessages(ggsave(filename = file.path(graphs_dir, "faceted_raw_time_plot.png"), plot = plot_raw_time_faceted, width = 10, height = 8, dpi = 300))

  # 3. Density Plot (log10 Scale)
  plot_density <- ggplot(global_df, aes(x = speedup)) +
    geom_density(fill = "steelblue", alpha = 0.5) +
    geom_vline(xintercept = 1, linetype = "dashed", color = "red", linewidth = 1) +
    scale_x_log10() + 
    labs(title = "Distribution of Global Speedups", subtitle = plot_stamp, caption = "Red line represents 1x (No Speedup)", x = "Speedup (log10)", y = "Density") +
    theme_minimal()
  suppressMessages(ggsave(filename = file.path(graphs_dir, "global_speedup_density.png"), plot = plot_density, width = 8, height = 6, dpi = 300))

  # 4. Complexity vs Speedup
  plot_complex <- ggplot(global_df, aes(x = cyclomatic_complexity, y = speedup)) +
    geom_point(alpha = 0.3, color = "darkorange") +
    geom_smooth(method = "loess", color = "blue", formula = y ~ x) +
    geom_hline(yintercept = 1, linetype = "dashed", color = "red") +
    scale_y_log10() + 
    scale_x_log10() +
    labs(title = "Global Speedup vs Cyclomatic Complexity", subtitle = plot_stamp, x = "Cyclomatic Complexity (log10)", y = "Speedup (log10)") +
    theme_minimal()
  suppressMessages(ggsave(filename = file.path(graphs_dir, "global_complexity_plot.png"), plot = plot_complex, width = 8, height = 6, dpi = 300))

  # 5. Package LOC Violin
  plot_loc_violin <- ggplot(global_df, aes(x = reorder(package_label, lines_of_code, FUN = median), y = lines_of_code)) +
    geom_violin(fill = "skyblue", color = "darkblue", alpha = 0.6) +
    geom_boxplot(width = 0.1, fill = "white", outlier.shape = NA, alpha = 0.5) +
    scale_y_log10() +
    coord_flip() +
    labs(title = "Distribution of Lines of Code by Package", subtitle = plot_stamp, x = "Package", y = "Lines of Code (log10)") +
    theme_minimal()
  suppressMessages(ggsave(filename = file.path(graphs_dir, "global_loc_violin_plot.png"), plot = plot_loc_violin, width = 8, height = 8, dpi = 300))

  # 6. Package Variance Boxplot
  plot_pkg_variance <- ggplot(global_df, aes(x = reorder(package_label, speedup, FUN = median), y = speedup)) +
    geom_boxplot(fill = "lightgreen", outlier.alpha = 0.3) +
    geom_hline(yintercept = 1, linetype = "dashed", color = "red") +
    scale_y_log10() +
    coord_flip() +
    labs(title = "Variance of Speedup by Package", subtitle = plot_stamp, x = "Package", y = "Speedup (log10)") +
    theme_minimal()
  suppressMessages(ggsave(filename = file.path(graphs_dir, "global_speedup_variance_boxplot.png"), plot = plot_pkg_variance, width = 8, height = 8, dpi = 300))

  # --- Markdown Output Construction ---
  md_content <- sprintf(
    "## Overall Aggregate Metrics\n\n### Core Metrics\n- **Total Functions Profiled:** %d\n- **Geometric Average Speedup:** %.4fx\n\n### Variation\n- **Standard Deviation:** %.4f\n- **Variance:** %.4f\n- **Interquartile Range:** %.4f\n\n### Percentiles\n| 1%% | 5%% | 25%% | 50%% (Median) | 75%% | 95%% | 99%% |\n|---|---|---|---|---|---|---|\n| %.2f | %.2f | %.2f | %.2f | %.2f | %.2f | %.2f |\n\n### Absolute Throughput\n- **Total GNU R Time:** %.4f seconds\n- **Total crbcc Time:** %.4f seconds\n- **Absolute Speedup:** %.4fx\n\n%s### Correlations\n- **Lines of Code vs Speedup:** rho = %.4f (p = %g)\n- **Cyclomatic Complexity vs Speedup:** rho = %.4f (p = %g)\n\n### Visualizations\n![Global Correlation Plot](graphs/global_correlation_plot.png)\n\n![Faceted Raw Time by Package](graphs/faceted_raw_time_plot.png)\n\n![Speedup Density](graphs/global_speedup_density.png)\n\n![Complexity vs Speedup](graphs/global_complexity_plot.png)\n\n![LOC Distribution by Package](graphs/global_loc_violin_plot.png)\n\n![Variance of Speedup by Package](graphs/global_speedup_variance_boxplot.png)\n\n---\n\n",
    nrow(global_df),
    geo_mean_speedup,
    sd(global_df$speedup), var(global_df$speedup), IQR(global_df$speedup),
    quantiles[1], quantiles[2], quantiles[3], quantiles[4], quantiles[5], quantiles[6], quantiles[7],
    total_time_gnur, total_time_crbcc, absolute_speedup,
    pkg_table_md,
    cor_loc$estimate, cor_loc$p.value,
    cor_cyc$estimate, cor_cyc$p.value
  )
  
  cat(md_content, file = md_file, append = TRUE)

  cat("\n[INTERPRET] Execution complete. Global Markdown report generated at:\n", md_file, "\n")

}