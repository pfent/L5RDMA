#include "include/MulticlientRDMARecvTransport.h"
#include "include/MulticlientRDMATransport.h"
#include "include/RdmaTransport.h"
#include "util/Random32.h"
#include "util/bench.h"
#include "util/doNotOptimize.h"
#include <array>
#include <deque>
#include <thread>
#include <vector>

using namespace l5::transport;

static constexpr auto numMessages = size_t(1e3);
static constexpr uint16_t port = 1234;
static std::string_view ip = "127.0.0.1";

template <typename Container, typename Size, typename... Args, typename Initializer, typename = std::enable_if_t<std::is_same_v<std::invoke_result_t<Initializer, std::remove_pointer_t<typename Container::value_type::pointer>&>, void>>>
void emplace_initialize_n(Container& container, Size n, Args&&... args, Initializer&& init) {
   container.reserve(n);
   for (Size i = 0; i < n; ++i) {
      init(*container.emplace_back(std::make_unique<typename std::remove_pointer_t<typename Container::value_type::pointer>>(std::forward<Args>(args)...)));
   }
}

template <typename Server, typename Client>
void doRun(bool isClient, const std::string& connection, size_t concurrentInFlight) {
   if (isClient) {
      auto rand = Random32();
      auto msgs = std::vector<uint32_t>();
      msgs.reserve(numMessages);
      std::generate_n(std::back_inserter(msgs), numMessages, [&] { return rand.next(); });

      auto numThreads = std::min(concurrentInFlight, size_t(std::thread::hardware_concurrency()));
      auto concurrentPerThread = concurrentInFlight / numThreads;
      auto threads = std::vector<std::thread>();
      threads.reserve(numThreads);
      for (size_t threadId = 0; threadId < numThreads; ++threadId) {
         threads.emplace_back([&, id = threadId] {
            bool needsExtra = (concurrentInFlight % numThreads) > id;
            auto thisThreadConcurrent = concurrentPerThread + needsExtra;

            auto clients = std::vector<std::unique_ptr<Client>>();
            emplace_initialize_n(clients, thisThreadConcurrent, [&](Client& client) {
               for (int i = 0;; ++i) {
                  try {
                     client.connect(connection);
                     break;
                  } catch (...) {
                     std::this_thread::sleep_for(std::chrono::milliseconds(20));
                     if (i > 1000) throw;
                  }
               }
            });

            auto inFlight = std::deque<std::tuple<Client&, uint32_t>>();
            size_t done = 0;
            for (size_t i = 0; i < numMessages; ++i) {
               auto current = i % concurrentInFlight;
               auto value = msgs[i];
               auto& client = *clients[current];
               client.write(value);
               inFlight.emplace_back(client, value);

               if (i >= concurrentInFlight) {
                  uint32_t response = 0;
                  auto [finClient, expected] = inFlight.front();
                  inFlight.pop_front();
                  finClient.read(response);
                  if (expected != response) throw std::runtime_error("unexpected value!");
                  ++done;
               }
            }
            for (; done < numMessages; ++done) {
               uint32_t response = 0;
               auto [finClient, expected] = inFlight.front();
               inFlight.pop_front();
               finClient.read(response);
               if (expected != response) throw std::runtime_error("unexpected value!");
            }
         });
      }
      for (auto& thread : threads) { thread.join(); }
   } else { // server
      auto server = Server(connection);
      for (size_t i = 0; i < concurrentInFlight; ++i) { server.accept(); }
      bench(numMessages, [&] {
         for (size_t i = 0; i < numMessages; ++i) {
            uint32_t message = {};
            auto client = server.read(message);
            server.write(client, message);
         }
      });
   }
}

int main(int argc, char** argv) {
   if (argc < 2) {
      std::cout << "Usage: " << argv[0] << " <client / server> <(IP, optional) 127.0.0.1>" << std::endl;
      return -1;
   }
   const auto isClient = std::string_view(argv[1]) == "client";
   if (argc >= 2) ip = argv[1];
   std::string connectionString;
   if (isClient) {
      connectionString = std::string(ip) + ":" + std::to_string(port);
   } else {
      connectionString = std::to_string(port);
   }

   std::cout << "concurrent, method, seconds, msgps, user, kernel, total\n";
   for (size_t i = 1; i < 50; ++i) {
      // TODO: MulticlientRDMAMemoryRegions -> Suitable for *few* clients (x < ???)
      // MulticlientRDMADoorbells -> Suitable for *most* clients (??? < x < ???)
      std::cout << i << ", Doorbells";
      doRun<MulticlientRDMATransportServer, MultiClientRDMATransportClient>(isClient, connectionString, i);
      // MulticlientRDMARecv -> Suitable for *many* clients (??? < x)
      std::cout << i << ", Recv";
      doRun<MulticlientRDMARecvTransportServer, MulticlientRDMARecvTransportClient>(isClient, connectionString, i);
   }
}
