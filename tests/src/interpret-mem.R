library(ggplot2)

interpret <- function(source_dir, target_dir) {

  if (!dir.exists(target_dir)) dir.create(target_dir, recursive = TRUE)
  
  graphs_dir <- file.path(target_dir, "graphs-mem")
  if (!dir.exists(graphs_dir)) dir.create(graphs_dir, recursive = TRUE)

  csv_files <- list.files(path = source_dir, pattern = "\\.csv$", full.names = TRUE)
  if (length(csv_files) == 0) stop("No CSV files found.")

  md_file <- file.path(target_dir, "mem_report.md")
  cat("# Global Compiler Memory Analysis Report\n\n", file = md_file)

  # 1. Aggregate and Clean
  global_data_list <- list()
  for (file_path in csv_files) {
    pkg_name <- gsub("_benchmark_results\\.csv$", "", basename(file_path))
    df <- read.csv(file_path)
    if (nrow(df) > 0) {
      df$package <- pkg_name
      global_data_list[[pkg_name]] <- df
    }
  }

  global_df <- do.call(rbind, global_data_list)
  global_df <- na.omit(global_df)
  
  # Filter out functions with 0 allocations to avoid division by zero
  global_df <- global_df[global_df$mem_gnu_r > 0 & global_df$mem_crbcc > 0, ]

  # --- TODO: Compute Memory Statistics ---

  # 1. Calculate Relative Overhead (Ratio)
  # Ratio > 1 means crbcc uses more memory than GNU R
  global_df$mem_ratio <- global_df$mem_crbcc / global_df$mem_gnu_r
  
  # 2. Global Aggregates (Absolute)
  total_mem_gnur <- sum(global_df$mem_gnu_r) / (1024^2)   # Convert to MB
  total_mem_crbcc <- sum(global_df$mem_crbcc) / (1024^2) # Convert to MB
  abs_mem_ratio <- total_mem_crbcc / total_mem_gnur
  
  # 3. Distribution Metrics for Memory Ratios
  mem_quantiles <- quantile(global_df$mem_ratio, probs = c(0.01, 0.25, 0.50, 0.75, 0.99))
  
  # 4. Correlation: Does memory usage scale with LOC?
  cor_mem_loc <- suppressWarnings(cor.test(global_df$lines_of_code, global_df$mem_ratio, method = "spearman"))

  # 5. Per-Package Memory Summary
  pkg_mem <- aggregate(cbind(mem_gnu_r, mem_crbcc) ~ package, data = global_df, sum)
  pkg_mem$abs_ratio <- pkg_mem$mem_crbcc / pkg_mem$mem_gnu_r
  pkg_mem <- pkg_mem[order(-pkg_mem$abs_ratio), ]

  # --- Visualizations ---

  # Plot A: Absolute Memory Allocation (Scatter)
  plot_mem_abs <- ggplot(global_df, aes(x = mem_gnu_r, y = mem_crbcc)) +
    geom_point(alpha = 0.3, color = "darkgreen") +
    geom_abline(intercept = 0, slope = 1, linetype = "dashed", color = "red") +
    scale_x_log10(labels = scales::label_bytes()) +
    scale_y_log10(labels = scales::label_bytes()) +
    labs(title = "Absolute Memory Allocation: GNU R vs crbcc",
         subtitle = sprintf("Aggregate Memory Ratio: %.2fx", abs_mem_ratio),
         x = "GNU R Allocation (bytes, log10)", y = "crbcc Allocation (bytes, log10)") +
    theme_minimal()
  ggsave(file.path(graphs_dir, "mem_absolute_comparison.png"), plot_mem_abs, width = 8, height = 6)

  # Plot B: Memory Overhead Distribution
  plot_mem_dist <- ggplot(global_df, aes(x = mem_ratio)) +
    geom_density(fill = "firebrick", alpha = 0.4) +
    geom_vline(xintercept = 1, linetype = "dashed") +
    scale_x_log10() +
    labs(title = "Distribution of Memory Overhead Ratios",
         subtitle = "Values > 1 indicate crbcc is more memory-intensive",
         x = "Memory Ratio (crbcc / GNU R, log10)", y = "Density") +
    theme_minimal()
  ggsave(file.path(graphs_dir, "mem_ratio_density.png"), plot_mem_dist, width = 8, height = 6)

  # Plot C: Memory Ratio vs LOC
  plot_mem_loc <- ggplot(global_df, aes(x = lines_of_code, y = mem_ratio)) +
    geom_point(alpha = 0.2, color = "blue") +
    geom_smooth(method = "loess", formula = y ~ x) +
    scale_x_log10() + scale_y_log10() +
    labs(title = "Memory Overhead Scaling vs Lines of Code",
         x = "Lines of Code (log10)", y = "Memory Ratio (log10)") +
    theme_minimal()
  ggsave(file.path(graphs_dir, "mem_loc_scaling.png"), plot_mem_loc, width = 8, height = 6)

  # --- Markdown Reporting ---

  md_stats <- sprintf(
    "## Global Memory Metrics\n\n- **Total Functions Analyzed:** %d\n- **Total GNU R Allocation:** %.2f MB\n- **Total crbcc Allocation:** %.2f MB\n- **Global Absolute Overhead:** %.4fx\n\n### Memory Ratio Percentiles\n| 1%% | 25%% | 50%% (Median) | 75%% | 99%% |\n|---|---|---|---|---|\n| %.2fx | %.2fx | %.2fx | %.2fx | %.2fx |\n\n### Correlation\n- **LOC vs Memory Ratio (Spearman):** rho = %.4f (p = %g)\n\n",
    nrow(global_df), total_mem_gnur, total_mem_crbcc, abs_mem_ratio,
    mem_quantiles[1], mem_quantiles[2], mem_quantiles[3], mem_quantiles[4], mem_quantiles[5],
    cor_mem_loc$estimate, cor_mem_loc$p.value
  )

  # Per-Package Table
  pkg_table <- "### Per-Package Memory Throughput\n\n| Package | GNU R (MB) | crbcc (MB) | Ratio |\n|---|---|---|---|\n"
  for(i in 1:nrow(pkg_mem)) {
    pkg_table <- paste0(pkg_table, sprintf("| %s | %.2f | %.2f | %.2fx |\n", 
                        pkg_mem$package[i], pkg_mem$mem_gnu_r[i]/(1024^2), 
                        pkg_mem$mem_crbcc[i]/(1024^2), pkg_mem$abs_ratio[i]))
  }

  # Top 10 Memory "Monsters"
  monsters <- head(global_df[order(-global_df$mem_ratio), ], 10)
  monster_table <- "\n### Top 10 Most Memory-Intensive Functions (Relative)\n\n| Package | Function | LOC | Memory Ratio |\n|---|---|---|---|\n"
  for(i in 1:nrow(monsters)) {
    monster_table <- paste0(monster_table, sprintf("| %s | `%s` | %d | %.2fx |\n", 
                            monsters$package[i], monsters$function_name[i], 
                            monsters$lines_of_code[i], monsters$mem_ratio[i]))
  }

  cat(md_stats, pkg_table, monster_table, file = md_file, append = TRUE)
  
  cat("\n### Visualizations\n![Absolute Comparison](graphs-mem/mem_absolute_comparison.png)\n![Overhead Density](graphs-mem/mem_ratio_density.png)\n![Scaling vs LOC](graphs-mem/mem_loc_scaling.png)\n", 
      file = md_file, append = TRUE)

  cat("\n[INTERPRET] Execution complete. Global Markdown report generated at:\n", md_file, "\n")
}