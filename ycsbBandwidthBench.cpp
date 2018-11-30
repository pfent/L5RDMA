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
   const auto lookupKeys = generateZipfLookupKeys(ycsb_tx_count * 10);
   YcsbDataSet data{};

   // measure bytes / seconds
   std::cout << "none, ";
   bench(ycsb_tx_count * 10 * sizeof(data), [&] {
      for (auto lookupKey : lookupKeys) {
         DoNotOptimize(data);
         database.lookup(lookupKey, data.begin());
         ClobberMemory();
      }
   }, printResults);
}

template<class Server, class Client>
void doRun(bool isClient, std::string connection) {
   struct ReadMessage {
      YcsbKey lookupKey;
   };

   struct ReadResponse {
      YcsbDataSet data;
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

      const auto lookupKeys = generateZipfLookupKeys(ycsb_tx_count);
      auto response = ReadResponse{};

      for (const auto lookupKey: lookupKeys) {
         const auto message = ReadMessage{lookupKey};
         client.write(message);
         DoNotOptimize(response);
         client.read(response);
         ClobberMemory();
      }
   } else { // server
      auto server = Server(connection);
      const auto database = YcsbDatabase();
      server.accept();
      // measure bytes / s
      bench(ycsb_tx_count * sizeof(ReadResponse), [&] {
         for (size_t i = 0; i < ycsb_tx_count; ++i) {
            auto message = ReadMessage{};
            server.read(message);
            server.write([&](auto begin) {
               database.lookup(message.lookupKey, begin);
               return sizeof(ReadResponse);
            });
         }
      }, printResults);
   }
}

void doRunSharedMemory(bool isClient) {
   struct ReadMessage {
      YcsbKey lookupKey;
   };

   struct ReadResponse {
      YcsbDataSet data;
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

      const auto lookupKeys = generateZipfLookupKeys(ycsb_tx_count);

      for (const auto lookupKey : lookupKeys) {
         auto message = ReadMessage{lookupKey};
         auto response = ReadResponse();
         client.write(message);
         DoNotOptimize(response);
         client.read(response); // TODO this is probably a bug in the Shared memory transport
         ClobberMemory();
      }
   } else {
      auto server = SharedMemoryTransportServer("/dev/shm/pingPong");
      const auto database = YcsbDatabase();
      server.accept();
      // measure bytes / s
      bench(ycsb_tx_count * sizeof(ReadResponse), [&]() {
         for (size_t i = 0; i < ycsb_tx_count; ++i) {
            auto message = ReadMessage{};
            server.read(message);
            auto response = ReadResponse();
            database.lookup(message.lookupKey, response.data.begin());

            server.write(response);
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
   //doRun<DomainSocketsTransportServer, DomainSocketsTransportClient>(isClient, "/tmp/testSocket");
   //std::cout << "shared memory, ";
   //doRunSharedMemory(isClient);
   //std::cout << "tcp, ";
   //doRun<TcpTransportServer, TcpTransportClient>(isClient, connection);
   //std::cout << "rdma, ";
   //doRun<RdmaTransportServer, RdmaTransportClient>(isClient, connection);
}
