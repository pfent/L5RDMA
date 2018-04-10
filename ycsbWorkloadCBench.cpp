#include <cstddef>
#include <util/Random32.h>
#include <string_view>
#include <algorithm>
#include <array>
#include <unordered_map>
#include <vector>
#include <cassert>
#include <chrono>
#include <x86intrin.h>
#include <benchmark/benchmark.h>


/// YCSB Benchmark workload, based on Alexander van Renen's version
static constexpr size_t ycsb_tuple_count = 100000;
static constexpr size_t ycsb_field_count = 10;
static constexpr size_t ycsb_field_length = 100;
static constexpr size_t ycsb_tx_count = 100000;
using YcsbKey = uint32_t;

struct YcsbDataSet;
using YcsbValue = YcsbDataSet;

struct RandomString {
    Random32 rand;

    void fill(size_t len, char *dest) {
        using namespace std::literals::string_view_literals;
        static constexpr auto alphanum = "0123456789"
                                         "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                         "abcdefghijklmnopqrstuvwxyz"sv;
        std::generate(&dest[0], &dest[len - 1], [&] { return alphanum[rand.next() % alphanum.size()]; });
        dest[len - 1] = '\0';
    }

    template<typename CharContainer>
    void fill(CharContainer &container) {
        fill(container.size(), container.data());
    }
};

struct YcsbDataSet {
    template<class T, size_t ROWS, size_t COLUMNS>
    using Matrix = std::array<std::array<T, COLUMNS>, ROWS>;

    Matrix<char, ycsb_field_count, ycsb_field_length> rows{};

    std::array<char, ycsb_field_length> &operator[](size_t i) {
        return rows[i];
    }

    YcsbDataSet() = default;

    explicit YcsbDataSet(RandomString &gen) {
        for (auto &row : rows) {
            gen.fill(row);
        }
    }
};

std::vector<YcsbKey> generateLookupKeys(size_t count, size_t maxValue) {
    auto rand = Random32();
    auto res = std::vector<YcsbKey>();
    res.reserve(count);
    std::generate_n(std::back_inserter(res), count, [&] { return rand.next() % maxValue; });
    return res;
}

class TimeOut {
    static constexpr size_t check_rdtsc = 2'000'000; // Roughly every ms
    std::chrono::high_resolution_clock::time_point begin{};
    std::chrono::microseconds max_us;
    uint64_t last_check = 0;

public:
    template<class Unit>
    explicit TimeOut(std::chrono::duration<int64_t, Unit> duration): max_us(duration) {}

    void start() {
        begin = std::chrono::high_resolution_clock::now();
        last_check = __rdtsc();
    }

    bool shouldContinue() {

        if (__rdtsc() - last_check < check_rdtsc) {
            return true;
        }
        last_check = __rdtsc();

        auto now = std::chrono::high_resolution_clock::now();
        return (now - begin) < max_us;
    }
};

int main(int, char **) {
    auto rand = Random32();
    auto database = std::unordered_map<YcsbKey, YcsbDataSet>{};
    database.reserve(ycsb_tuple_count);

    auto gen = RandomString();
    auto nextKey = uint32_t(0);
    for (size_t i = 0; i < ycsb_tuple_count; ++i) {
        database.emplace(nextKey++, YcsbDataSet(gen));
    }

    const auto lookupKeys = generateLookupKeys(ycsb_tx_count, ycsb_tuple_count);
    auto resultSet = YcsbDataSet();

    for (const auto lookupKey: lookupKeys) {
        const auto field = YcsbKey(rand.next() % ycsb_field_count);
        const auto fieldPtr = database.find(lookupKeys[lookupKey]);
        if (fieldPtr == database.end()) {
            throw;
        }
        const auto begin = fieldPtr->second[field].begin();
        const auto end = fieldPtr->second[field].end();
        auto target = resultSet[field].begin();
        benchmark::DoNotOptimize(std::copy(begin, end, target));
        assert(std::equal(resultSet[field].begin(), resultSet[field].end(), fieldPtr->second[field].begin()));
    }
}
