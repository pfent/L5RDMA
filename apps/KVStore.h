#ifndef EXCHANGABLETRANSPORTS_KVSTORE_H
#define EXCHANGABLETRANSPORTS_KVSTORE_H

#include <cstdint>
#include <unordered_map>
#include <cstring>
#include "transports/Transport.h"

struct KvInput {
    char command[8]; // 7 chars + \0
    size_t key;
    size_t value;
};

template<typename T>
struct KVStore {
    std::unique_ptr<TransportServer<T>> transport;
    std::unordered_map<uint64_t, uint64_t> store;

    explicit KVStore(std::unique_ptr<TransportServer<T>> t) : transport(std::move(t)) {};

    std::optional<uint64_t> get(uint64_t k) {
        auto res = store.find(k);
        if (res != store.end()) {
            return res->second;
        }
        return std::nullopt;
    }

    void insert(uint64_t k, uint64_t v) { store[k] = v; }

    void deleteKey(uint64_t k) { store.erase(k); }

    void start() {
        transport->accept();
    }

    void respond() {
        static constexpr auto get = "SELECT ";
        static constexpr auto ins = "INSERT ";
        static constexpr auto del = "DELETE ";

        static_assert(strlen(get) == strlen(ins));
        static_assert(strlen(get) == strlen(del));
        static_assert(strlen(get) == 7);

        KvInput input;

        transport->read(reinterpret_cast<uint8_t *>(&input), sizeof(input));

        const auto key = &input.key;
        const auto val = &input.value;
        std::optional < size_t > res;

        switch (input.command[0]) {
            case 'S':
                res = this->get(*key);
                size_t output;
                if (res.has_value()) {
                    output = res.value();
                } else {
                    output = std::numeric_limits<size_t>::max();
                }
                transport->write(reinterpret_cast<uint8_t *>(&output), sizeof(output));
                break;
            case 'I':
                insert(*key, *val);
                break;
            case 'D':
                deleteKey(*key);
                break;
            default:
                throw std::runtime_error{"unknown command from client"};
        }
    }
};

#endif //EXCHANGABLETRANSPORTS_KVSTORE_H
