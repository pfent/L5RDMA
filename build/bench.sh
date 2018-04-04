#!/usr/bin/env bash
set -e
if [[ $# -lt 1 ]]; then
    echo "Please specify client / server, optionally IP"
    exit
fi

BENCHMARKS=$(ls | grep "Bench$")
NODE=1
for test in $BENCHMARKS; do
    echo numactl --membind="$NODE" --cpunodebind="$NODE" ./"$test" "$1" "$2" \| tee "$test"_"$1".csv \| column -s, -t
    numactl --membind="$NODE" --cpunodebind="$NODE" ./"$test" "$1" "$2" | tee "$test"_"$1".csv | column -s, -t
done
