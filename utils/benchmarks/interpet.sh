#!/usr/bin/env bash

# Target data files
FILE_CRBCC="./test_results/crbcc.csv"
FILE_GNUR="./test_results/gnur.csv"

# Verify existence of required data
if [[ ! -f "$FILE_CRBCC" ]] || [[ ! -f "$FILE_GNUR" ]]; then
    echo "ERROR: Missing required data files." >&2
    echo "Check that both '$FILE_CRBCC' and '$FILE_GNUR' exist in the current directory." >&2
    echo "If not, run benchmark.sh"
    exit 1
fi

echo "Ok, data exists"

# Execute the R scripts sequentially
Rscript interpret.R
Rscript interpret-graphics.R
Rscript interpret-loc.R

echo "Done"