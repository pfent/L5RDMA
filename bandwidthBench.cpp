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

constexpr size_t operator "" _K(unsigned long long i) { return i * 1024; }
constexpr size_t operator "" _M(unsigned long long i) { return i * 1024 * 1024; }
constexpr size_t operator "" _G(unsigned long long i) { return i * 1024 * 1024 * 1024; }

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

      std::cout << name << ", " << std::flush;
      bench(testdata.size(), [&] {
         server.write(testdata.data(), testdata.size());
      }, printResults);
   }
}

int main(int argc, char** argv) {
   if (argc < 3) {
      std::cout << "Usage: " << argv[0] << " <client / server> messagesize <(optional) 127.0.0.1>" << std::endl;
      return -1;
   }
   const auto isClient = argv[1][0] == 'c';

   const auto size = [&] {
      auto specifiedSize = std::string(argv[2]);
      size_t multiplier;
      switch (specifiedSize.back()) {
         case 'k':
         case 'K':
            multiplier = 1024;
            break;
         case 'm':
         case 'M':
            multiplier = 1024 * 1024;
            break;
         case 'g':
         case 'G':
            multiplier = 1024 * 1024 * 1024;
            break;
         default:
            multiplier = 1;
      }
      specifiedSize.pop_back();
      return stoi(specifiedSize) * multiplier;
   }();

   const auto isLocal = [&] {
      if (argc > 3) {
         ip = argv[3];
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

   if (not isClient) std::cout << "connection, MB, time, MB/s, user, system, total\n";
   if (isLocal) {
      doRun<DomainSocketsTransportServer,
            DomainSocketsTransportClient
      >("domainSocket", isClient, "/tmp/testSocket", size);
      doRun<SharedMemoryTransportServer<512_M>,
            SharedMemoryTransportClient<512_M>
      >("shared memory", isClient, "/tmp/testSocket", size);
   }
   doRun<TcpTransportServer,
         TcpTransportClient
   >("tcp", isClient, connection, size);
   if (not isLocal) {
      doRun<RdmaTransportServer<512_M>,
            RdmaTransportClient<512_M>
      >("rdma", isClient, connection, size);
   }
}
