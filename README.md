# Networking / Interprocess communication library for low latency communication

## Building
Needs a reasonably modern compiler (C++17), Ubuntu 17.10 or newer works.
```bash
git submodule update --init --recursive
mkdir build
cd build
ccmake .. # configure Debug or Release
make -j
```
Run some tests with:
```bash
make test
```

## Benchmarking
You probably want to use something like `numactl` to get predictable results:
```
NODE=1; numactl --membind=$NODE --cpunodebind=$NODE ./point2PointBench server
NODE=1; numactl --membind=$NODE --cpunodebind=$NODE ./point2PointBench client
```

For output, you'll get CSV data, which is much more pleasurable to read using `column`
```
NODE=1; numactl --membind=$NODE --cpunodebind=$NODE ./point2PointBench client > output.csv
cat output.csv | column -s, -t
```
