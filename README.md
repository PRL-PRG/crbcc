## CRBCC - Bytecode compiler for R written in C 

part of a bachelor thesis @ FIT CTU

## License

GPL (>= 2)

## Performance

crbcc performance measured against standard GNU-R compiler.
Speedup compiling a function measured by compiling it 10 times, result is the mean value.
Average compiler speedup of a package is the average of speedups on compiling all of its functions.

Measured on Intel Core i5-8400 CPU @ 2.80GHz, RAM 16GB (inside Docker on Windows 11).

`./tests/kit.R` stdout:

```
[START] Compiling package: base 
[OK] 1150 functions compiled identically
[PERF] Average compiler speedup: 49.23x

[START] Compiling package: compiler 
[OK] 139 functions compiled identically
[PERF] Average compiler speedup: 52.87x

[START] Compiling package: tools 
[OK] 782 functions compiled identically
[PERF] Average compiler speedup: 47.79x

[START] Compiling package: stats 
[OK] 926 functions compiled identically
[PERF] Average compiler speedup: 51.39x

[START] Compiling package: utils 
[OK] 519 functions compiled identically
[PERF] Average compiler speedup: 47.47x
```