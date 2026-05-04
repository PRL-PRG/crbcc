# perf claim: R_compute_identical vs speedup

## Setup
- Source data: gnur_new vs crbcc_new compile_benchmark.csv
- Selection: bottom 10 and top 10 speedups with gnur_time >= 0.1s
- Profiler: perf record -F 99 -g, parsed with perf report --stdio

## Summary
- Bottom group avg R_compute_identical: 43.78%
- Top group avg R_compute_identical: 10.15%
- Pearson correlation (speedup vs R_compute_identical share): -0.920 (p = 6.63e-09)

## Bottom 10 (smallest speedups)
| package | function | speedup | R_compute_identical |
|---|---|---:|---:|
| mgcv | bam | 8.96x | 49.42% |
| survival | coxph | 9.74x | 41.90% |
| tools | .check_package_CRAN_incoming | 10.13x | 44.89% |
| mgcv | gam.fit3 | 10.30x | 42.09% |
| mgcv | shash | 11.24x | 47.82% |
| nlme | nlme.formula | 11.69x | 43.58% |
| mgcv | gam.fit4 | 12.18x | 37.91% |
| mgcv | bgam.fitd | 12.44x | 43.16% |
| mgcv | bgam.fit | 13.03x | 42.23% |
| mgcv | smoothCon | 13.22x | 44.76% |

## Top 10 (largest speedups)
| package | function | speedup | R_compute_identical |
|---|---|---:|---:|
| tools | makeJSS | 68.36x | 0.00% |
| mgcv | fix.family.link.family | 68.12x | 17.96% |
| mgcv | betar | 62.93x | 0.00% |
| mgcv | tw | 60.23x | 19.27% |
| mgcv | cpois | 58.74x | 11.90% |
| tools | format.check_package_CRAN_incoming | 53.51x | 0.00% |
| tools | checkRd | 52.32x | 15.93% |
| mgcv | gammals | 47.40x | 15.34% |
| tools | Rd2latex | 46.74x | 0.00% |
| tools | Rd2txt | 46.38x | 21.07% |

## Files
- targets: perf_claim/targets.csv
- perf data: perf_claim/perf-*.data
- summary: perf_claim/perf_rcident_summary.csv
