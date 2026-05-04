# gnur_new vs crbcc_new compile_benchmark.csv

## Overall totals
- GNU R total time: 95.590s
- crbcc total time: 3.140s
- Overall speedup (total): 30.45x
- Matched functions: 7,624 in both files (missing gnur: 0, missing crbcc: 0)

## Per-package totals (gnur_total, crbcc_total, speedup)
- KernSmooth: 0.407s vs 0.008s (49.69x)
- MASS: 2.541s vs 0.062s (40.75x)
- Matrix: 2.200s vs 0.050s (44.37x)
- base: 5.903s vs 0.133s (44.43x)
- boot: 1.574s vs 0.035s (45.05x)
- class: 0.164s vs 0.003s (57.10x)
- cluster: 1.072s vs 0.032s (33.80x)
- codetools: 0.297s vs 0.005s (65.24x)
- compiler: 0.666s vs 0.011s (59.87x)
- foreign: 0.602s vs 0.016s (37.46x)
- grDevices: 1.353s vs 0.027s (49.43x)
- graphics: 2.253s vs 0.063s (36.04x)
- grid: 3.037s vs 0.051s (59.13x)
- lattice: 4.574s vs 0.143s (32.06x)
- methods: 3.657s vs 0.079s (46.47x)
- mgcv: 22.529s vs 1.068s (21.10x)
- nlme: 5.936s vs 0.173s (34.36x)
- nnet: 0.373s vs 0.009s (42.45x)
- parallel: 0.568s vs 0.011s (50.46x)
- rpart: 0.797s vs 0.021s (37.81x)
- spatial: 0.276s vs 0.004s (62.33x)
- splines: 0.323s vs 0.007s (49.41x)
- stats: 10.833s vs 0.268s (40.44x)
- stats4: 0.044s vs 0.001s (41.64x)
- survival: 9.908s vs 0.377s (26.26x)
- tools: 13.703s vs 0.484s (28.30x)

## Top time consumers (by GNU R total time)
- mgcv::shash 3.179s vs 0.283s
- tools::.check_packages 2.365s vs 0.131s
- tools::.install_packages 0.815s vs 0.058s
- mgcv::gevlss 0.786s vs 0.044s
- mgcv::gam.fit3 0.513s vs 0.050s
- tools::.check_package_CRAN_incoming 0.508s vs 0.050s
- mgcv::bam 0.456s vs 0.051s
- mgcv::clog 0.420s vs 0.010s
- nlme::nlme.formula 0.413s vs 0.035s
- mgcv::gam.fit4 0.393s vs 0.032s

## Smallest speedups (bottom 20)
- mgcv::bam 8.96x (0.456s → 0.051s)
- survival::coxph 9.74x (0.367s → 0.038s)
- tools::.check_package_CRAN_incoming 10.13x (0.508s → 0.050s)
- mgcv::gam.fit3 10.30x (0.513s → 0.050s)
- mgcv::shash 11.24x (3.179s → 0.283s)
- nlme::nlme.formula 11.69x (0.413s → 0.035s)
- mgcv::gam.fit4 12.18x (0.393s → 0.032s)
- mgcv::bgam.fitd 12.44x (0.326s → 0.026s)
- mgcv::bgam.fit 13.03x (0.252s → 0.019s)
- mgcv::smoothCon 13.22x (0.372s → 0.028s)
- mgcv::newton 13.66x (0.223s → 0.016s)
- mgcv::gam.setup 13.70x (0.272s → 0.020s)
- survival::survfit.coxphms 13.84x (0.175s → 0.013s)
- mgcv::gam.fit5 13.89x (0.334s → 0.024s)
- mgcv::gamm 13.96x (0.219s → 0.016s)
- tools::.install_packages 14.09x (0.815s → 0.058s)
- mgcv::predict.gam 14.45x (0.319s → 0.022s)
- survival::tmerge 15.15x (0.275s → 0.018s)
- survival::survfit.coxph 15.42x (0.156s → 0.010s)
- mgcv::Sl.setup 16.09x (0.260s → 0.016s)
