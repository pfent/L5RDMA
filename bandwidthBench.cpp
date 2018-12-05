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
void doRun(const std::string &name, bool isClient, std::string connection, size_t size) {
   std::vector<uint8_t> testdata(size);

   if (isClient) {
      sleep(1);
      auto client = Client();

      std::cout << "connecting to " << connection << "..." << std::flush;
      for (int i = 0;; ++i) {
         try {
            client.connect(connection);
            break;
         } catch (...) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            if (i > 10) throw;
         }
      }
      std::cout << "connected\n";

      client.read(testdata.data(), size);
      DoNotOptimize(testdata);
      ClobberMemory();

   } else { // server
      std::cout << name << ", " << std::flush;
      auto server = Server(connection);
      server.accept();

      RandomString rand;
      rand.fill(size, reinterpret_cast<char*>(testdata.data()));

      // measure bytes / s
      bench(testdata.size(), [&] {
         server.write(testdata.data(), testdata.size());
      }, printResults);
   }
}

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

   const auto size = 1024 * 1024;// todo

   if (not isClient) std::cout << "connection, MB, time, MB/s, user, system, total\n";
   if (isLocal) {
      //doRun<DomainSocketsTransportServer, DomainSocketsTransportClient>("domainSocket", isClient, "/tmp/testSocket",
      //                                                                  size);
      doRun<SharedMemoryTransportServer, SharedMemoryTransportClient>("shared memory", isClient, "/tmp/testSocket",
                                                                      size);
   }
   doRun<TcpTransportServer, TcpTransportClient>("tcp", isClient, connection, size);
   if (not isLocal) {
      doRun<RdmaTransportServer, RdmaTransportClient>("rdma", isClient, connection, size);
   }
}
