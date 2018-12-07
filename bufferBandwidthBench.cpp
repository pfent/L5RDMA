#include <include/DomainSocketsTransport.h>
#include <include/SharedMemoryTransport.h>
#include "include/RdmaTransport.h"
#include <thread>
#include <include/TcpTransport.h>
#include <util/doNotOptimize.h>
#include <util/ycsb.h>
#include "util/bench.h"

using namespace l5::transport;

static constexpr uint16_t port = 1234;
static const char* ip = "127.0.0.1";

constexpr size_t operator "" _k(unsigned long long i) { return i * 1024; }

constexpr size_t operator "" _m(unsigned long long i) { return i * 1024 * 1024; }

constexpr size_t operator "" _g(unsigned long long i) { return i * 1024 * 1024 * 1024; }

static constexpr auto printResults = []
      (double workSize, auto avgTime, auto userPercent, auto systemPercent, auto totalPercent) {
   std::cout << workSize / 1e6 << ", "
             << avgTime << ", "
             << (workSize / 1e6 / avgTime) << ", "
             << userPercent << ", "
             << systemPercent << ", "
             << totalPercent << '\n';
};

template<class Server, class Client>
void doRun(const std::string &name, bool isClient, const std::string &connection, size_t size) {
   std::vector<uint8_t> testdata(size);

   if (isClient) {
      std::cout << "connecting to " << connection << "..." << std::flush;

      sleep(1);
      auto client = Client();
      for (int i = 0;; ++i) {
         try {
            client.connect(connection);
            break;
         } catch (...) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            if (i > 10) throw;
         }
      }
      std::cout << " connected!\n";

      std::cout << "receiving " << size << "B data from the server\n";

      client.read(testdata.data(), size);
      DoNotOptimize(testdata);
      ClobberMemory();

   } else { // server
      // immediately accept to not block the client
      auto server = Server(connection);
      server.accept();

      RandomString rand;
      rand.fill(size, reinterpret_cast<char*>(testdata.data()));

      std::cout << name << ", " << Server::buffer_size / 1024. / 1024. << ", " << std::flush;
      bench(testdata.size(), [&] {
         server.write(testdata.data(), testdata.size());
      }, printResults);
   }
}

template<template<size_t> typename Server,
      template<size_t> typename Client, size_t bufferSize, size_t ...bufferSizes>
typename std::enable_if_t<sizeof...(bufferSizes) == 0>
doRunHelper(const std::string &name, bool isClient, const std::string &connection, size_t size) {
   doRun<Server<bufferSize>, Client<bufferSize> >(name, isClient, connection, size);
}

template<template<size_t> typename Server,
      template<size_t> typename Client, size_t bufferSize, size_t ...bufferSizes>
typename std::enable_if_t<sizeof...(bufferSizes) != 0>
doRunHelper(const std::string &name, bool isClient, const std::string &connection, size_t size) {
   doRun<Server<bufferSize>, Client<bufferSize> >(name, isClient, connection, size);
   doRunHelper<Server, Client, bufferSizes...>(name, isClient, connection, size);
}

int main(int argc, char** argv) {
   if (argc < 2) {
      std::cout << "Usage: " << argv[0] << " <client / server> <(optional) 127.0.0.1>" << std::endl;
      return -1;
   }
   const auto isClient = argv[1][0] == 'c';
   const auto isLocal = [&] {
      if (argc > 2) {
         ip = argv[2];
      }
      return strcmp("127.0.0.1", ip) == 0;
   }();
   const auto connection = [&] {
      if (isClient) {
         return ip + std::string(":") + std::to_string(port);
      } else {
         return std::to_string(port);
      }
   }();
   const auto size = 1_g;

   if (not isClient) std::cout << "connection, buffer size [MB], MB, time, MB/s, user, system, total\n";

   if (isLocal) {
      doRunHelper<SharedMemoryTransportServer, SharedMemoryTransportClient,
            1_m, 2_m, 4_m, 8_m, 16_m, 32_m, 64_m, 128_m, 256_m, 512_m, 1_g, 2_g
      >("shared memory", isClient, "/tmp/testSocket", size);
   } else {
      doRunHelper<RdmaTransportServer, RdmaTransportClient,
            1_m, 2_m, 4_m, 8_m, 16_m, 32_m, 64_m, 128_m, 256_m, 512_m, 1_g, 2_g
      >("rdma", isClient, connection, size);
   }
}
