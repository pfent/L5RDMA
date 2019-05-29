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
   static constexpr auto numMessages = size_t(1e6);
   if (isClient) {
      auto rand = Random32();
      auto msgs = std::vector<uint32_t>();
      msgs.reserve(numMessages);
      std::generate_n(std::back_inserter(msgs), numMessages, [&] { return rand.next(); });

      auto numThreads = std::min(concurrentInFlight, size_t(std::thread::hardware_concurrency()));
      auto concurrentPerThread = concurrentInFlight / numThreads;
      auto messagesPerThread = numMessages / numThreads;
      auto threads = std::vector<std::thread>();
      threads.reserve(numThreads);
      for (size_t threadId = 0; threadId < numThreads; ++threadId) {
         bool needsExtraConcurrent = (concurrentInFlight % numThreads) > threadId;
         bool needsExtraMessage = (numMessages % numThreads) > threadId;
         threads.emplace_back([thisThreadConcurrent = concurrentPerThread + needsExtraConcurrent, thisThreadMessages = messagesPerThread + needsExtraMessage, &msgs, &connection] {
            auto clients = std::vector<std::unique_ptr<Client>>();
            emplace_initialize_n(clients, thisThreadConcurrent, [&](Client& client) {
               for (int i = 0;; ++i) {
                  try {
                     client.connect(connection);
                     break;
                  } catch (...) {
                     if (i > 1000) throw;
                     std::this_thread::sleep_for(std::chrono::milliseconds(20));
                  }
               }
            });

            auto inFlight = std::deque<std::tuple<Client&, uint32_t>>();
            size_t done = 0;
            for (size_t i = 0; i < thisThreadMessages; ++i) {
               if (i >= thisThreadConcurrent) {
                  uint32_t response = 0;
                  auto [finClient, expected] = inFlight.front();
                  inFlight.pop_front();
                  finClient.read(response);
                  if (expected != response) throw std::runtime_error("unexpected value!");
                  ++done;
               }

               auto current = i % thisThreadConcurrent;
               auto value = msgs[i];
               auto& client = *clients[current];
               client.write(value);
               inFlight.emplace_back(client, value);
            }
            for (; done < thisThreadMessages; ++done) {
               uint32_t response = 0;
               auto [finClient, expected] = inFlight.front();
               inFlight.pop_front();
               finClient.read(response);
               if (expected != response) throw std::runtime_error("unexpected value!");
            }
            std::cout << "#";
         });
      }
      for (auto& thread : threads) { thread.join(); }
      std::cout << std::endl;
   } else { // server
      auto server = Server(connection, (concurrentInFlight + 15u) & ~15u); // next multiple of 16
      for (size_t i = 0; i < concurrentInFlight; ++i) { server.accept(); }
      server.finishListen();
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
      std::cout << "Usage: " << argv[0] << " <client / server>  <(IP, optional) 127.0.0.1>" << std::endl;
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

   if (!isClient) std::cout << "concurrent, method, messages, seconds, msgps, user, kernel, total\n";
   for (size_t i = 1; i < 50; ++i) {
      // TODO: MulticlientRDMAMemoryRegions -> Suitable for *few* clients (x < ???)
      // MulticlientRDMADoorbells -> Suitable for *most* clients (??? < x < ???)
      if (!isClient) std::cout << i << ", Doorbells, " << std::flush;
      doRun<MulticlientRDMATransportServer, MultiClientRDMATransportClient>(isClient, connectionString, i);
      // MulticlientRDMARecv -> Suitable for *many* clients (??? < x)
      if (!isClient) std::cout << i << ", Recv, " << std::flush;
      doRun<MulticlientRDMARecvTransportServer, MulticlientRDMARecvTransportClient>(isClient, connectionString, i);
   }
}
