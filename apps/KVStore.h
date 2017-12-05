#ifndef EXCHANGABLETRANSPORTS_KVSTORE_H
#define EXCHANGABLETRANSPORTS_KVSTORE_H

#include <cstdint>
#include <exchangeableTransports/transports/Transport.h>
#include <unordered_map>

template<typename T>
class KVStore {
    std::unique_ptr<TransportServer<T>> transport;
    std::unordered_map<uint64_t, uint64_t> store;

    explicit KVStore(std::unique_ptr<TransportServer<T>> t) : transport(std::move(t)) {};

    std::optional<uint64_t> get(uint64_t k) {
        auto res = store.find(k);
        if (res != store.end())
            return res->second;
        return std::nullopt_t;
    }

    void insert(uint64_t k, uint64_t v) { store[k] = v; }

    void deleteEntry(uint64_t k) { store.erase(k); }

    void
};

#endif //EXCHANGABLETRANSPORTS_KVSTORE_H
