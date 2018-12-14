#include <include/DomainSocketsTransport.h>
#include <include/SharedMemoryTransport.h>
#include "include/RdmaTransport.h"
#include <thread>
#include <include/TcpTransport.h>
#include "util/ycsb.h"
#include "util/bench.h"

using namespace l5::transport;

constexpr size_t operator "" _k(unsigned long long i) { return i * 1024; }

constexpr size_t operator "" _m(unsigned long long i) { return i * 1024 * 1024; }

constexpr size_t operator "" _g(unsigned long long i) { return i * 1024 * 1024 * 1024; }

static constexpr uint16_t port = 1234;
static const char* ip = "127.0.0.1";

static constexpr auto printResults = []
      (double workSize, auto avgTime, auto userPercent, auto systemPercent, auto totalPercent) {
   std::cout << workSize / 1e6 << ", "
             << avgTime << ", "
             << (workSize / 1e6 / avgTime) << ", "
             << userPercent << ", "
             << systemPercent << ", "
             << totalPercent << '\n';
};

void doRunNoCommunication() {
   const auto database = YcsbDatabase();
   YcsbDataSet data{};

   // measure bytes / seconds
   std::cout << "none, ";
   bench(database.database.size() * 10 * sizeof(data), [&] {
      for (int i = 0; i < 10; ++i)
         for (auto &lookup : database.database) {
            DoNotOptimize(data);
            std::copy(lookup.second.begin(), lookup.second.end(), data.begin());
            ClobberMemory();
         }
   }, printResults);
}

template<class Server, class Client>
void doRun(bool isClient, std::string connection) {
   struct ReadMessage {
      char next = '\0';
   };

   struct ReadResponse {
      // 128KB response size -> optimal for shared memory
      std::array<YcsbDataSet, 128> data;
   };

   if (isClient) {
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

      std::cout << "connected to " << connection << '\n';

      auto responses = ReadResponse{};
      auto data = YcsbDataSet{};
      for (size_t i = 0; i < ycsb_tuple_count;) {
         client.read(responses);
         for (auto &response : responses.data) {
            ++i;
            DoNotOptimize(data);
            std::copy(response.begin(), response.end(), data.begin());
            ClobberMemory();
         }
      }
   } else { // server
      auto server = Server(connection);
      const auto database = YcsbDatabase();
      server.accept();
      // measure bytes / s
      bench(ycsb_tuple_count * sizeof(YcsbDataSet), [&] {
         auto responses = ReadResponse{};
         for (auto lookupIt = database.database.begin(); lookupIt != database.database.end();) {
            for (auto &response : responses.data) {
               std::copy(lookupIt->second.begin(), lookupIt->second.end(), response.begin());
               ++lookupIt;
               if (lookupIt == database.database.end()) {
                  break;
               }
            }
            server.write(responses);
         }
      }, printResults);
   }
}

/**
 * Bandwidth benchmark with pagination
 * i.e. request batching
 * only do a actual new request every X bytes of data
 * From previous analysis: 128 KB blocks with a 1 MB buffer
 */
int main(int argc, char** argv) {
   if (argc < 2) {
      std::cout << "Usage: " << argv[0] << " <client / server> <(IP, optional) 127.0.0.1>" << std::endl;
      return -1;
   }
   const auto isClient = argv[1][0] == 'c';
   const auto isLocal = [&] {
      if (argc <= 2) {
         return true;
      }
      ip = argv[2];
      return strcmp("127.0.0.1", ip) == 0;
   }();
   const auto connection = [&] {
      if (isClient) {
         return ip + std::string(":") + std::to_string(port);
      } else {
         return std::to_string(port);
      }
   }();
   if (not isClient) std::cout << "connection, MB, time, MB/s, user, system, total\n";
   if (isLocal) {
      if (not isClient) doRunNoCommunication();
      std::cout << "domainSocket, ";
      doRun<DomainSocketsTransportServer, DomainSocketsTransportClient>(isClient, "/tmp/testSocket");
      std::cout << "shared memory, ";
      doRun<SharedMemoryTransportServer<1_m>, SharedMemoryTransportClient<1_m>>(isClient, "/tmp/testSocket");
   }
   std::cout << "tcp, ";
   doRun<TcpTransportServer, TcpTransportClient>(isClient, connection);
   if (not isLocal) {
      std::cout << "rdma, ";
      doRun<RdmaTransportServer<>, RdmaTransportClient<>>(isClient, connection);
   }
}
