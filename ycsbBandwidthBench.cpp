#include <include/DomainSocketsTransport.h>
#include <include/SharedMemoryTransport.h>
#include "include/RdmaTransport.h"
#include <thread>
#include <include/TcpTransport.h>
#include "util/ycsb.h"
#include "util/bench.h"

using namespace l5::transport;

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
      // 4KB response size -> similar to how MSSQL cursors work
      std::array<YcsbDataSet, 4> data;
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

      auto message = ReadMessage{};
      auto responses = ReadResponse{};
      auto data = YcsbDataSet{};
      for (size_t i = 0; i < ycsb_tuple_count;) {
         client.write(message);
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
         auto message = ReadMessage{};
         auto responses = ReadResponse{};
         for (auto lookupIt = database.database.begin(); lookupIt != database.database.end();) {
            server.read(message);

            for (auto &response : responses.data) {
               std::copy(lookupIt->second.begin(), lookupIt->second.end(), response.begin());
               ++lookupIt;
            }
            server.write(responses);
         }
      }, printResults);
   }
}

void doRunSharedMemory(bool isClient) {
   struct ReadMessage {
      YcsbKey lookupKey;
   };

   struct ReadResponse {
      std::array<YcsbDataSet, 4> data;
   };

   if (isClient) {
      sleep(1);
      auto client = SharedMemoryTransportClient();

      for (int i = 0;; ++i) {
         try {
            client.connect("/dev/shm/pingPong");
            break;
         } catch (...) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            if (i > 10) throw;
         }
      }

      std::cout << "connected to /dev/shm/pingPong\n";

      auto message = ReadMessage{};
      auto responses = ReadResponse{};
      auto data = YcsbDataSet{};
      for (size_t i = 0; i < ycsb_tuple_count;) {
         client.write(message);
         client.read(responses);
         for (auto &response : responses.data) {
            ++i;
            DoNotOptimize(data);
            std::copy(response.begin(), response.end(), data.begin());
            ClobberMemory();
         }
      }
   } else {
      auto server = SharedMemoryTransportServer("/dev/shm/pingPong");
      const auto database = YcsbDatabase();
      server.accept();
      // measure bytes / s
      bench(ycsb_tuple_count * sizeof(YcsbDataSet), [&] {
         auto message = ReadMessage{};
         auto responses = ReadResponse{};
         for (auto lookupIt = database.database.begin(); lookupIt != database.database.end();) {
            server.read(message);

            for (auto &response : responses.data) {
               std::copy(lookupIt->second.begin(), lookupIt->second.end(), response.begin());
               ++lookupIt;
            }
            server.write(responses);
         }
      }, printResults);
   }
}

/**
 * TODO: Bandwidth benchmark should do pagination
 * i.e. request batching
 * only do a actual new request every X bytes of data
 * probably try to keep the value as close possible to MSSQL -> 4096B per TDS packet
 */
int main(int argc, char** argv) {
   if (argc < 2) {
      std::cout << "Usage: " << argv[0] << " <client / server> <(IP, optional) 127.0.0.1>" << std::endl;
      return -1;
   }
   const auto isClient = argv[1][0] == 'c';
   std::string connection;
   if (isClient) {
      connection = ip + std::string(":") + std::to_string(port);
   } else {
      connection = std::to_string(port);
   }
   if (argc > 2) {
      ip = argv[2];
   }
   if (!isClient) std::cout << "connection, MB, time, MB/s, user, system, total\n";
   if (!isClient) doRunNoCommunication();
   std::cout << "domainSocket, ";
   doRun<DomainSocketsTransportServer, DomainSocketsTransportClient>(isClient, "/tmp/testSocket");
   std::cout << "shared memory, ";
   doRunSharedMemory(isClient);
   std::cout << "tcp, ";
   doRun<TcpTransportServer, TcpTransportClient>(isClient, connection);
   //std::cout << "rdma, ";
   //doRun<RdmaTransportServer, RdmaTransportClient>(isClient, connection);
}
