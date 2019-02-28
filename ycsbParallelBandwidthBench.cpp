#include "include/RdmaTransport.h"
#include "util/bench.h"
#include "util/ycsb.h"
#include <thread>
#include <include/DomainSocketsTransport.h>
#include <include/MulticlientRDMATransport.h>
#include <include/MulticlientTCPTransport.h>
#include <include/SharedMemoryTransport.h>
#include <include/TcpTransport.h>

using namespace l5::transport;

constexpr size_t operator"" _k(unsigned long long i) { return i * 1024; }

constexpr size_t operator"" _m(unsigned long long i) { return i * 1024 * 1024; }

constexpr size_t operator"" _g(unsigned long long i) { return i * 1024 * 1024 * 1024; }

static double accumulated;
static auto printResults = [](double workSize, auto avgTime,
                              auto userPercent, auto systemPercent,
                              auto totalPercent) {
   std::cout << workSize / 1e6 << ", " << avgTime << ", "
             << (workSize / 1e6 / avgTime) << ", " << userPercent << ", "
             << systemPercent << ", " << totalPercent << '\n';
   accumulated += (workSize / 1e6 / avgTime);
};

static std::atomic<size_t> clientsReady;

static auto database = std::optional<YcsbDatabase>();

template <class Server, class Client>
void doRun(bool isClient, std::string connection, size_t numClientThreadsPerServer, size_t numServerThreads) {
   struct ReadMessage {
      char next = '\0';
   };

   struct ReadResponse {
      // 128KB response size -> optimal for shared memory
      std::array<YcsbDataSet, 128> data;
   };

   if (isClient) {
      std::vector<std::thread> clientThreads;
      for (size_t s = 0; s < numServerThreads; ++s)
         for (size_t c = 0; c < numClientThreadsPerServer; ++c)
            clientThreads.emplace_back([&, s] {
               auto client = Client();
               for (int i = 0;; ++i) {
                  try {
                     client.connect(connection + std::to_string(s));
                     break;
                  } catch (...) {
                     std::this_thread::sleep_for(std::chrono::milliseconds(20));
                     if (i > 10)
                        throw;
                  }
               }

               { // Warm up
                  auto responses = ReadResponse{};
                  auto data = YcsbDataSet{};
                  std::array<uint8_t, 1> request = {0};
                  client.send(request.data(), request.size());
                  for (size_t i = 0; i < ycsb_tuple_count;) {
                     client.read(responses);
                     for (auto& response : responses.data) {
                        ++i;
                        DoNotOptimize(data);
                        std::copy(response.begin(), response.end(), data.begin());
                        ClobberMemory();
                     }
                  }
               }

               { // Benchmark
                  auto responses = ReadResponse{};
                  auto data = YcsbDataSet{};
                  std::array<uint8_t, 1> request = {0};
                  client.send(request.data(), request.size());
                  for (size_t i = 0; i < ycsb_tuple_count;) {
                     client.read(responses);
                     for (auto& response : responses.data) {
                        ++i;
                        DoNotOptimize(data);
                        std::copy(response.begin(), response.end(), data.begin());
                        ClobberMemory();
                     }
                  }
               }
            });
      for (auto& t : clientThreads) { t.join(); }
   } else { // server
      std::vector<std::thread> serverThreads;
      for (size_t s = 0; s < numServerThreads; ++s) serverThreads.emplace_back([&, s] {
         auto server = Server(connection + std::to_string(s));
         for (size_t i = 0; i < numClientThreadsPerServer; ++i) {
            server.accept();
         }

         std::cout << "Warming up\n";
         for (size_t i = 0; i < numClientThreadsPerServer; ++i) {
            char request;
            auto client = server.read(request);
            auto responses = ReadResponse{};
            for (auto lookupIt = database->database.begin();
                 lookupIt != database->database.end();) {
               for (auto& response : responses.data) {
                  std::copy(lookupIt->second.begin(), lookupIt->second.end(), response.begin());
                  ++lookupIt;
                  if (lookupIt == database->database.end()) {
                     break;
                  }
               }
               server.write(client, responses);
            }
         }

         std::cout << "Benchmarking...\n";
         // measure bytes / s
         bench(ycsb_tuple_count * sizeof(YcsbDataSet) * numClientThreadsPerServer, [&] {
            for (size_t i = 0; i < numClientThreadsPerServer; ++i) {
               char request;
               auto client = server.read(request);
               auto responses = ReadResponse{};
               for (auto lookupIt = database->database.begin();
                    lookupIt != database->database.end();) {
                  for (auto& response : responses.data) {
                     std::copy(lookupIt->second.begin(), lookupIt->second.end(), response.begin());
                     ++lookupIt;
                     if (lookupIt == database->database.end()) {
                        break;
                     }
                  }
                  server.write(client, responses);
               }
            }
         },
               printResults);
      });
      for (auto& t : serverThreads) { t.join(); }
      std::cout << accumulated << "MB/s\n";
   }
}

int main(int argc, char** argv) {
   if (argc < 2) {
      std::cout << "Usage: " << argv[0]
                << " <client / server> <#clientThreadsPerServer = 1> <#serverTheads = 1> <IP = 127.0.0.1>" << std::endl;
      return -1;
   }
   const auto isClient = argv[1][0] == 'c';
   const auto numClientThreadsPerServer = argc < 3 ? 1 : std::atoi(argv[2]);
   const auto numServerThreads = argc < 4 ? 1 : std::atoi(argv[3]);

   static constexpr uint16_t port = 123;
   static const char* ip = "127.0.0.1";
   const auto connection = [&] {
      if (isClient) {
         return ip + std::string(":") + std::to_string(port);
      } else {
         return std::to_string(port);
      }
   }();

   if (not isClient) database = YcsbDatabase();
   if (not isClient) std::cout << "connection, MB, time, MB/s, user, system, total\n";
   if (not isClient) std::cout << "tcp, ";
   doRun<MulticlientTCPTransportServer, MulticlientTCPTransportClient>(isClient, connection, numClientThreadsPerServer, numServerThreads);
   if (not isClient) std::cout << "rdma, ";
   doRun<MulticlientRDMATransportServer, MultiClientRDMATransportClient>(isClient, connection, numClientThreadsPerServer, numServerThreads);
}
