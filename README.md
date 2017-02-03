# A project to test and play with Infiniband / RDMA

In this repo, I experiment with libibverbs and the C++ RDMA wrapper [roediger](https://github.com/roediger) and [alexandervanrenen](https://github.com/alexandervanrenen) wrote.  
The ultimate goal is to create a wrapper for TCP sockets (like [TSSX](https://github.com/goldsborough/tssx) did for domain sockets).

## Microbenchmarks

Reproducible with
```bash
tcpPingPong
rdmaPingPong
```

| Where      | What | RoundTrips / second | avg. latency in µs |
| ---------- | ---- | ------------------: | -----------------: |
| localhost  | TCP  |              81,781 |              12.23 |
| localhost  | RDMA |             472,415 |               2.12 |
| network    | TCP  |              39,541 |              25.29 |
| network    | RDMA |             381,520 |               2.62 |

## Remarks
* Keeping track of the sent / received messages with a separate AtomicFetchAndAddWorkRequest also slows the RTT by ~50%. Keeping the message in a single WriteRequest seems reasonable.
* RDMA guarantees, that memory is written in order. However, only bytes are written atomically. When reading bigger words, they might be written partially.

## calling `fork()`
`fork()`-ing libibverbs should be avoided. However, the [man pages](https://linux.die.net/man/3/ibv_fork_init) suggest, that forking can be done when calling `ibv_fork_init()` before forking, or simply setting `IBV_FORK_SAFE=1`.  
However, trying to get this to work with postgres results in a segfault in the server process.

There is a (quite hacky) solution in place to allow correct operation with forking programs, by setting `RDMA_FORKGEN`:
```bash
RDMA_FORKGEN=1 USE_RDMA=127.0.0.1 LD_PRELOAD=/home/fent/rdma_tests/bin/libTest.so ./forkingPingPong server 1234
RDMA_FORKGEN=0 USE_RDMA=127.0.0.1 LD_PRELOAD=/home/fent/rdma_tests/bin/libTest.so ./forkingPingPong client 1234 127.0.0.1
```

Please note, that you need to know in which generation your program stops to fork and set the environment variable accordingly.

## Executing postgres with the preload library

```bash
# Server
RDMA_FORKGEN=1 USE_RDMA=10.0.0.11 LD_PRELOAD=/home/fent/rdma_tests/bin/libTest.so ./bin/postgres -D ../tmp/ -p 4567
# Client
RDMA_FORKGEN=0 USE_RDMA=10.0.0.16 LD_PRELOAD=/home/fent/rdma_tests/bin/libTest.so ./bin/psql -h scyper16 -p 4567 -d postgres
```

## Building
The project can be built with CMake on any platform libibverbs is supported on (Only tested on Linux though) and a reasonably modern compiler (C++14).

```bash
mkdir bin
cd bin
cmake ..
make -j
```
