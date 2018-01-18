# Networking / Interprocess communication library for low latency communication

## Building
Needs a reasonably modern compiler (C++17).
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

