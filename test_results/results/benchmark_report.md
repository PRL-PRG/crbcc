# Compiler Benchmark Report

## Package: `base`

### Core Metrics
- **Geometric Average Speedup:** 65.5622x

### Variation
- **Standard Deviation:** 23.4932
- **Variance:** 551.9326
- **Interquartile Range:** 30.2892

### Percentiles
| 1% | 5% | 25% | 50% (Median) | 75% | 95% | 99% |
|---|---|---|---|---|---|---|
| 17.19 | 33.56 | 54.70 | 69.86 | 84.99 | 107.05 | 131.04 |

### Absolute Throughput
- **Total GNU R Time:** 7.6808 seconds
- **Total crbcc Time:** 0.2660 seconds
- **Absolute Speedup:** 28.8799x

### Correlations
- **Lines of Code vs Speedup:** rho = -0.6843 (p = 1.256e-159)
- **Cyclomatic Complexity vs Speedup:** rho = -0.6308 (p = 1.30314e-128)

### Visualization
![Correlation Plot for base](base_correlation_plot.png)

---

## Package: `compiler`

### Core Metrics
- **Geometric Average Speedup:** 64.9487x

### Variation
- **Standard Deviation:** 20.8100
- **Variance:** 433.0560
- **Interquartile Range:** 27.5378

### Percentiles
| 1% | 5% | 25% | 50% (Median) | 75% | 95% | 99% |
|---|---|---|---|---|---|---|
| 24.25 | 38.48 | 53.97 | 65.99 | 81.51 | 103.41 | 123.44 |

### Absolute Throughput
- **Total GNU R Time:** 0.8872 seconds
- **Total crbcc Time:** 0.0165 seconds
- **Absolute Speedup:** 53.6523x

### Correlations
- **Lines of Code vs Speedup:** rho = -0.6294 (p = 1.06131e-16)
- **Cyclomatic Complexity vs Speedup:** rho = -0.4353 (p = 8.54167e-08)

### Visualization
![Correlation Plot for compiler](compiler_correlation_plot.png)

---

## Package: `stats`

### Core Metrics
- **Geometric Average Speedup:** 58.1003x

### Variation
- **Standard Deviation:** 29.0454
- **Variance:** 843.6346
- **Interquartile Range:** 44.1400

### Percentiles
| 1% | 5% | 25% | 50% (Median) | 75% | 95% | 99% |
|---|---|---|---|---|---|---|
| 12.37 | 18.04 | 43.05 | 68.62 | 87.19 | 112.70 | 123.22 |

### Absolute Throughput
- **Total GNU R Time:** 12.8468 seconds
- **Total crbcc Time:** 0.5681 seconds
- **Absolute Speedup:** 22.6132x

### Correlations
- **Lines of Code vs Speedup:** rho = -0.8295 (p = 5.06039e-236)
- **Cyclomatic Complexity vs Speedup:** rho = -0.7878 (p = 1.16164e-196)

### Visualization
![Correlation Plot for stats](stats_correlation_plot.png)

---

## Package: `tools`

### Core Metrics
- **Geometric Average Speedup:** 54.6846x

### Variation
- **Standard Deviation:** 22.2126
- **Variance:** 493.3977
- **Interquartile Range:** 29.5053

### Percentiles
| 1% | 5% | 25% | 50% (Median) | 75% | 95% | 99% |
|---|---|---|---|---|---|---|
| 10.19 | 20.72 | 44.78 | 60.94 | 74.29 | 94.31 | 110.72 |

### Absolute Throughput
- **Total GNU R Time:** 16.6554 seconds
- **Total crbcc Time:** 1.1559 seconds
- **Absolute Speedup:** 14.4093x

### Correlations
- **Lines of Code vs Speedup:** rho = -0.7610 (p = 9.69936e-149)
- **Cyclomatic Complexity vs Speedup:** rho = -0.6457 (p = 1.95805e-93)

### Visualization
![Correlation Plot for tools](tools_correlation_plot.png)

---

## Package: `utils`

### Core Metrics
- **Geometric Average Speedup:** 53.5247x

### Variation
- **Standard Deviation:** 24.8642
- **Variance:** 618.2290
- **Interquartile Range:** 30.4843

### Percentiles
| 1% | 5% | 25% | 50% (Median) | 75% | 95% | 99% |
|---|---|---|---|---|---|---|
| 10.61 | 18.49 | 43.40 | 59.94 | 73.89 | 98.40 | 114.81 |

### Absolute Throughput
- **Total GNU R Time:** 7.1721 seconds
- **Total crbcc Time:** 0.3824 seconds
- **Absolute Speedup:** 18.7551x

### Correlations
- **Lines of Code vs Speedup:** rho = -0.8333 (p = 3.43291e-135)
- **Cyclomatic Complexity vs Speedup:** rho = -0.7361 (p = 1.11448e-89)

### Visualization
![Correlation Plot for utils](utils_correlation_plot.png)

---

## Global Debugging Targets: Top 20 Worst Performers

| Package | Function | LOC | Cyclomatic Complexity | Speedup |
|---|---|---|---|---|
| tools | `.check_package_CRAN_incoming` | 803 | 340 | 3.8074x |
| utils | `install.packages` | 662 | 322 | 5.3588x |
| tools | `.install_packages` | 2084 | 691 | 6.0041x |
| utils | `str.default` | 561 | 208 | 6.9490x |
| stats | `plot.lm` | 347 | 88 | 7.9062x |
| tools | `.check_packages` | 6285 | 1944 | 7.9685x |
| base | `[<-.data.frame` | 283 | 174 | 8.0097x |
| tools | `.check_packages_used` | 303 | 104 | 8.3548x |
| stats | `predict.lm` | 226 | 105 | 8.4843x |
| tools | `httpd` | 524 | 165 | 8.9673x |
| tools | `.shlib_internal` | 376 | 138 | 9.0207x |
| stats | `arima` | 350 | 128 | 9.1260x |
| utils | `hsearch_db` | 244 | 94 | 9.3743x |
| utils | `read.DIF` | 248 | 111 | 9.4429x |
| stats | `arima0` | 225 | 95 | 9.4498x |
| tools | `.check_package_depends` | 216 | 80 | 9.4521x |
| base | `loadNamespace` | 520 | 158 | 9.4816x |
| utils | `read.table` | 208 | 79 | 9.8647x |
| tools | `codoc` | 309 | 60 | 9.8951x |
| stats | `nls` | 168 | 41 | 10.0460x |
