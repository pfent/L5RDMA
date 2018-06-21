# L5RDMA: A low level, low latency library for RDMA
L5RDMA enables adaptive selection of best available connection type between processes or machines.

## Supported connections
* Domain sockets (`AF_UNIX`, `SOCK_STREAM`)
* TCP sockets (`AF_INET`, `SOCK_STREAM`)
  * As one-to-one channel
  * As many-to-one channels, using `poll` (1 server, N clients)
* Shared memory
* RDMA, whith latency optimized message processing
  * As one-to-one channel
  * As many-to-one channel

## Building
Building the library requires a reasonably modern compiler (C++17). Ubuntu 17.10 or newer works.

### Prerequisites
Necessary libraries:
* libibverbs (on Ubuntu >= 18.04 install `rdma-core`, <= 17.10 `libibverbs`, `librdmacm` and drivers for your Infiniband card)
* Intel tbb (`libtbb2`)

### Building
```bash
git submodule update --init --recursive
mkdir build
cd build
ccmake .. # configure Debug or Release
make -j
```

You can run tests, if everything was correctly configured with:
```bash
make test
```
If all tests pass, you're good to go.
However some tests might fail with, like in the example shown below:
```
The following tests FAILED:
          2 - librdmacmTest (Failed)
          3 - rdmaLargeTest (Failed)
          4 - rdmaTest (Failed)
Errors while running CTest
```
You can run the tests separately to get detailed output. In this case the kernel components of `libibverbs` were missing.
You can still use the other connection types, however using RDMA won't work in this case.

## Benchmarking
To get reliable results, it is recommended to use a tool like `numactl`, to avoid thread migration between CPUs:
```
NODE=1; numactl --membind=$NODE --cpunodebind=$NODE ./point2PointBench server
NODE=1; numactl --membind=$NODE --cpunodebind=$NODE ./point2PointBench client
```

For output, you'll get CSV data, which is much more pleasurable to read using `column`
```
NODE=1; numactl --membind=$NODE --cpunodebind=$NODE ./point2PointBench client > output.csv
cat output.csv | column -s, -t
```
