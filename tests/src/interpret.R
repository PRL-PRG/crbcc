interpret <- function(source_dir, target_dir) {

  # Ensure target directory exists
  if (!dir.exists(target_dir)) {
    dir.create(target_dir, recursive = TRUE)
  }

  csv_files <- list.files(path = source_dir, pattern = "\\.csv$", full.names = TRUE)

  if (length(csv_files) == 0) {
    stop("No CSV files found in the specified directory.")
  }

  # Initialize Markdown File & Global Data List
  md_file <- file.path(target_dir, "benchmark_report.md")
  cat("# Compiler Benchmark Report\n\n", file = md_file)

  global_data_list <- list()

  # 3. Iterate and Analyze
  for (file_path in csv_files) {
    
    pkg_name <- gsub("_benchmark_results\\.csv$", "", basename(file_path))
    
    df <- read.csv(file_path)
    df <- na.omit(df)
    df <- df[df$median_gnu_r > 0 & df$median_crbcc > 0, ]
    
    if (nrow(df) < 3) {
      msg <- sprintf("## Package: `%s`\n\n*Insufficient valid data points to perform statistical analysis.*\n\n---\n\n", pkg_name)
      cat(msg, file = md_file, append = TRUE)
      next
    }
    
    df$speedup <- df$median_gnu_r / df$median_crbcc
    
    # Store relevant metrics for global worst-performer analysis
    df$package <- pkg_name
    global_data_list[[pkg_name]] <- df[, c("package", "function_name", "lines_of_code", "cyclomatic_complexity", "speedup")]
    
    # --- Visualization ---
    plot_cyc <- ggplot(df, aes(x = cyclomatic_complexity, y = speedup)) +
      geom_point(alpha = 0.5) +
      geom_smooth(method = "loess", color = "blue", formula = y ~ x) +
      geom_hline(yintercept = 1, linetype = "dashed", color = "red") +
      scale_y_log10() + 
      scale_x_log10() +
      labs(
        title = sprintf("Speedup vs Cyclomatic Complexity: %s", pkg_name),
        x = "Cyclomatic Complexity (log10)",
        y = "Speedup (log10)"
      ) +
      theme_minimal()
    
    plot_filename <- paste0(pkg_name, "_correlation_plot.png")
    plot_filepath <- file.path(target_dir, plot_filename)
    suppressMessages(ggsave(filename = plot_filepath, plot = plot_cyc, width = 8, height = 6, dpi = 300))
    
    # --- Computations ---
    geo_mean_speedup <- exp(mean(log(df$speedup)))
    quantiles <- round(quantile(df$speedup, probs = c(0.01, 0.05, 0.25, 0.50, 0.75, 0.95, 0.99)), 2)
    
    total_time_gnur <- sum(df$median_gnu_r)
    total_time_crbcc <- sum(df$median_crbcc)
    absolute_speedup <- total_time_gnur / total_time_crbcc
    
    cor_loc <- suppressWarnings(cor.test(df$lines_of_code, df$speedup, method = "spearman", exact = FALSE))
    cor_cyc <- suppressWarnings(cor.test(df$cyclomatic_complexity, df$speedup, method = "spearman", exact = FALSE))
    
    # --- Markdown Output Construction ---
    md_content <- sprintf(
      "## Package: `%s`\n\n### Core Metrics\n- **Geometric Average Speedup:** %.4fx\n\n### Variation\n- **Standard Deviation:** %.4f\n- **Variance:** %.4f\n- **Interquartile Range:** %.4f\n\n### Percentiles\n| 1%% | 5%% | 25%% | 50%% (Median) | 75%% | 95%% | 99%% |\n|---|---|---|---|---|---|---|\n| %.2f | %.2f | %.2f | %.2f | %.2f | %.2f | %.2f |\n\n### Absolute Throughput\n- **Total GNU R Time:** %.4f seconds\n- **Total crbcc Time:** %.4f seconds\n- **Absolute Speedup:** %.4fx\n\n### Correlations\n- **Lines of Code vs Speedup:** rho = %.4f (p = %g)\n- **Cyclomatic Complexity vs Speedup:** rho = %.4f (p = %g)\n\n### Visualization\n![Correlation Plot for %s](%s)\n\n---\n\n",
      pkg_name,
      geo_mean_speedup,
      sd(df$speedup), var(df$speedup), IQR(df$speedup),
      quantiles[1], quantiles[2], quantiles[3], quantiles[4], quantiles[5], quantiles[6], quantiles[7],
      total_time_gnur, total_time_crbcc, absolute_speedup,
      cor_loc$estimate, cor_loc$p.value,
      cor_cyc$estimate, cor_cyc$p.value,
      pkg_name, plot_filename
    )
    
    cat(md_content, file = md_file, append = TRUE)
  }

  # 4. Global Monster Isolation
  if (length(global_data_list) > 0) {
    global_df <- do.call(rbind, global_data_list)
    
    # Sort ascending by speedup, extract top 20 worst performers
    worst_performers <- head(global_df[order(global_df$speedup), ], 20)
    
    # Construct Markdown Table
    cat("## Global Debugging Targets: Top 20 Worst Performers\n\n", file = md_file, append = TRUE)
    cat("| Package | Function | LOC | Cyclomatic Complexity | Speedup |\n", file = md_file, append = TRUE)
    cat("|---|---|---|---|---|\n", file = md_file, append = TRUE)
    
    for (i in 1:nrow(worst_performers)) {
      row_md <- sprintf("| %s | `%s` | %d | %d | %.4fx |\n",
                        worst_performers$package[i],
                        worst_performers$function_name[i],
                        worst_performers$lines_of_code[i],
                        worst_performers$cyclomatic_complexity[i],
                        worst_performers$speedup[i])
      cat(row_md, file = md_file, append = TRUE)
    }
  }

  cat("\n[INTERPRET] Execution complete. Markdown report generated at:\n", md_file, "\n")

}