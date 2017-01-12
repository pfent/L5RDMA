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
| localhost  | RDMA |             344,330 |               2.90 |
| LAN        | TCP  |              39,541 |              25.29 |
| LAN        | RDMA |             278,790 |               3.59 |

## Remarks
* `WorkRequest::setCompletion()` apparently has no effect, since no matter what I set, I always get a completion in the completion queue. So even with `setCompletion(false)` we need to poll the queue or we run out of memory in the queue.  
* `CompletionQueuePair::waitForCompletion()` slows the roundtrips per second by ~50% (so only do that when really necessary)
* Keeping track of the sent / received messages with a separate AtomicFetchAndAddWorkRequest also slows the RTT by ~50%. Keeping the message in a single WriteRequest seems reasonable.
* RDMA guarantees, that memory is written in order. However, only bytes are written atomically. When reading bigger words, they might be written partially.

## Building
The project can be built with CMake on any platform libibverbs is supported on (Only tested on Linux though) and a reasonably modern compiler (C++14).

```bash
mkdir bin
cd bin
cmake ..
make -j
```
