#include <iostream>
#include <thread>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "libibverbscpp/libibverbscpp.h"
#include "util/tcpWrapper.h"
#include "rdma/Network.hpp"
#include "rdma/QueuePair.hpp"
#include "util/bench.h"
#include "rdma/RcQueuePair.h"
#include "rdma/UcQueuePair.h"
#include "rdma/UdQueuePair.h"
#include <random>
#include <immintrin.h>
#include <boost/assert.hpp>

using namespace std;

constexpr uint16_t port = 1234;
const char *ip = "127.0.0.1";
constexpr size_t SHAREDMEM_MESSAGES = 1024 * 1024;
constexpr size_t BIGBADBUFFER_SIZE = 1024 * 1024 * 8; // 8MB

void connectSocket(int socket) {
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);
    for (int i = 0;; ++i) {
        try {
            tcp_connect(socket, addr);
            break;
        } catch (...) {
            std::this_thread::sleep_for(20ms);
            if (i > 10) throw;
        }
    }
}

void setUpListenSocket(int socket) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    tcp_bind(socket, addr);
    tcp_listen(socket);
}

auto createWriteWrWithImm(const ibv::memoryregion::Slice &slice) {
    auto write = ibv::workrequest::Simple<ibv::workrequest::WriteWithImm>{};
    write.setLocalAddress(slice);
    write.setInline();
    write.setSignaled();
    return write;
}

// Write with ImmData as baseline
template<class QueuePair>
void runImmData(bool isClient, uint32_t dataSize) {
    std::string data(dataSize, 'A');
    auto net = rdma::Network();
    auto &cq = net.getSharedCompletionQueue();
    auto qp = QueuePair(net);

    auto recvbuf = std::vector<char>(BIGBADBUFFER_SIZE * 2); // *2 just to be sure everything fits
    auto recvmr = net.registerMr(recvbuf.data(), recvbuf.size(),
                                 {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});
    auto sendbuf = std::vector<char>(data.size());
    auto sendmr = net.registerMr(sendbuf.data(), sendbuf.size(), {});

    auto generator = std::default_random_engine{};
    auto randomDistribution = std::uniform_int_distribution<uint32_t>{0, BIGBADBUFFER_SIZE};

    auto socket = tcp_socket();
    if (isClient) {
        connectSocket(socket);

        std::copy(data.begin(), data.end(), sendbuf.begin());

        auto recv = ibv::workrequest::Recv{};
        recv.setId(42);
        // *first* post recv to always have a recv pending, so incoming send don't get swallowed
        qp.postRecvRequest(recv);

        auto remoteAddr = rdma::Address{qp.getQPN(), net.getLID()};
        tcp_write(socket, &remoteAddr, sizeof(remoteAddr));
        tcp_read(socket, &remoteAddr, sizeof(remoteAddr));
        auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(recvbuf.data()),
                                                         recvmr->getRkey()};
        tcp_write(socket, &remoteMr, sizeof(remoteMr));
        tcp_read(socket, &remoteMr, sizeof(remoteMr));

        qp.connect(remoteAddr);

        auto write = createWriteWrWithImm(sendmr->getSlice());

        bench(SHAREDMEM_MESSAGES, [&]() {
            for (size_t i = 0; i < SHAREDMEM_MESSAGES; ++i) {
                auto destPos = randomDistribution(generator);
                write.setRemoteAddress(remoteMr.offset(destPos));
                write.setImmData(destPos);
                qp.postWorkRequest(write);
                cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);

                auto wc = cq.pollRecvWorkCompletionBlocking();
                auto recvPos = wc.getImmData();

                qp.postRecvRequest(recv);

                auto begin = recvbuf.begin() + recvPos;
                auto end = begin + dataSize;
                // check if the data is still the same
                if (not std::equal(begin, end, data.begin(), data.end())) {
                    throw std::runtime_error("received string not equal");
                }
            }
        }, 1);

    } else {
        setUpListenSocket(socket);

        const auto acced = [&] {
            sockaddr_in ignored{};
            return tcp_accept(socket, ignored);
        }();

        auto recv = ibv::workrequest::Recv{};
        recv.setId(42);
        recv.setSge(nullptr, 0);
        // *first* post recv to always have a recv pending, so incoming send don't get swallowed
        qp.postRecvRequest(recv);

        auto remoteAddr = rdma::Address{qp.getQPN(), net.getLID()};
        tcp_write(acced, &remoteAddr, sizeof(remoteAddr));
        tcp_read(acced, &remoteAddr, sizeof(remoteAddr));
        auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(recvbuf.data()),
                                                         recvmr->getRkey()};
        tcp_write(acced, &remoteMr, sizeof(remoteMr));
        tcp_read(acced, &remoteMr, sizeof(remoteMr));

        qp.connect(remoteAddr);

        auto write = createWriteWrWithImm(sendmr->getSlice());

        bench(SHAREDMEM_MESSAGES, [&]() {
            for (size_t i = 0; i < SHAREDMEM_MESSAGES; ++i) {
                // wait for message being written
                auto wc = cq.pollRecvWorkCompletionBlocking();
                qp.postRecvRequest(recv);

                auto recvPos = wc.getImmData();

                auto begin = recvbuf.begin() + recvPos;
                auto end = begin + dataSize;
                std::copy(begin, end, sendbuf.begin());
                // echo back the received data
                auto destPos = randomDistribution(generator);
                write.setRemoteAddress(remoteMr.offset(destPos));
                write.setImmData(destPos);
                qp.postWorkRequest(write);
                cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
            }
        }, 1);

        tcp_close(acced);
    }

    tcp_close(socket);
}

auto createWriteWr(const ibv::memoryregion::Slice &slice) {
    auto write = ibv::workrequest::Simple<ibv::workrequest::Write>{};
    write.setLocalAddress(slice);
    write.setInline();
    write.setSignaled();
    return write;
}

template<class QueuePair, class F>
void bigBuffer(bool isClient, size_t dataSize, uint16_t pollPositions, F pollFunc) {
    std::string data(dataSize, 'A');
    auto net = rdma::Network();
    auto &cq = net.getSharedCompletionQueue();
    auto qp = QueuePair(net);

    auto recvbuf = std::vector<char>(BIGBADBUFFER_SIZE * 2); // *2 just to be sure everything fits
    auto recvmr = net.registerMr(recvbuf.data(), recvbuf.size(),
                                 {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});
    auto recvPosBuf = std::vector<int32_t>(pollPositions);
    auto recvPosMr = net.registerMr(recvPosBuf.data(), recvPosBuf.size() * sizeof(int32_t),
                                    {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});

    auto sendbuf = std::vector<char>(data.size());
    auto sendmr = net.registerMr(sendbuf.data(), sendbuf.size(), {});

    auto sendPosBuf = std::vector<int32_t>(1);
    auto sendPosMr = net.registerMr(sendPosBuf.data(), sendPosBuf.size() * sizeof(int32_t),
                                    {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});

    auto generator = std::default_random_engine{};
    auto randomDistribution = std::uniform_int_distribution<uint32_t>{0, BIGBADBUFFER_SIZE};

    auto socket = tcp_socket();
    if (isClient) {
        connectSocket(socket);

        std::copy(data.begin(), data.end(), sendbuf.begin());

        // invalidate recv address
        std::fill(recvPosBuf.begin(), recvPosBuf.end(), -1);

        auto remoteAddr = rdma::Address{qp.getQPN(), net.getLID()};
        tcp_write(socket, &remoteAddr, sizeof(remoteAddr));
        tcp_read(socket, &remoteAddr, sizeof(remoteAddr));
        auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(recvbuf.data()),
                                                         recvmr->getRkey()};
        tcp_write(socket, &remoteMr, sizeof(remoteMr));
        tcp_read(socket, &remoteMr, sizeof(remoteMr));
        // TODO: vary the send address
        auto remotePosMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(&recvPosBuf.back()),
                                                            recvPosMr->getRkey()};
        tcp_write(socket, &remotePosMr, sizeof(remotePosMr));
        tcp_read(socket, &remotePosMr, sizeof(remotePosMr));

        qp.connect(remoteAddr);

        auto write = createWriteWr(sendmr->getSlice());
        auto posWrite = createWriteWr(sendPosMr->getSlice());
        posWrite.setRemoteAddress(remotePosMr);

        bench(SHAREDMEM_MESSAGES, [&]() {
            for (size_t i = 0; i < SHAREDMEM_MESSAGES; ++i) {
                auto destPos = randomDistribution(generator);
                sendPosBuf[0] = destPos;
                write.setRemoteAddress(remoteMr.offset(destPos));

                qp.postWorkRequest(write);
                qp.postWorkRequest(posWrite);
                cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
                cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);

                // wait for incoming message
                const auto[sender, recvPos] = pollFunc(recvPosBuf.data(), pollPositions);
                std::ignore = sender;

                auto begin = recvbuf.begin() + recvPos;
                auto end = begin + dataSize;
                // check if the data is still the same
                if (not std::equal(begin, end, data.begin(), data.end())) {
                    throw std::runtime_error("received string not equal");
                }
            }
        }, 1);

    } else {
        setUpListenSocket(socket);

        const auto acced = [&] {
            sockaddr_in ignored{};
            return tcp_accept(socket, ignored);
        }();

        // invalidate recv address
        std::fill(recvPosBuf.begin(), recvPosBuf.end(), -1);

        auto remoteAddr = rdma::Address{qp.getQPN(), net.getLID()};
        tcp_write(acced, &remoteAddr, sizeof(remoteAddr));
        tcp_read(acced, &remoteAddr, sizeof(remoteAddr));
        auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(recvbuf.data()),
                                                         recvmr->getRkey()};
        tcp_write(acced, &remoteMr, sizeof(remoteMr));
        tcp_read(acced, &remoteMr, sizeof(remoteMr));
        // TODO: vary the send address
        auto remotePosMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(&recvPosBuf.back()),
                                                            recvPosMr->getRkey()};
        tcp_write(acced, &remotePosMr, sizeof(remotePosMr));
        tcp_read(acced, &remotePosMr, sizeof(remotePosMr));

        qp.connect(remoteAddr);

        auto write = createWriteWr(sendmr->getSlice());
        auto posWrite = createWriteWr(sendPosMr->getSlice());
        posWrite.setRemoteAddress(remotePosMr);

        bench(SHAREDMEM_MESSAGES, [&]() {
            for (size_t i = 0; i < SHAREDMEM_MESSAGES; ++i) {
                // wait for incoming message
                const auto[sender, recvPos] = pollFunc(recvPosBuf.data(), pollPositions);
                std::ignore = sender;

                auto begin = recvbuf.begin() + recvPos;
                auto end = begin + dataSize;
                std::copy(begin, end, sendbuf.begin());
                // echo back the received data
                auto destPos = randomDistribution(generator);
                sendPosBuf[0] = destPos;
                write.setRemoteAddress(remoteMr.offset(destPos));
                qp.postWorkRequest(write);
                qp.postWorkRequest(posWrite);
                cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
                cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
            }
        }, 1);

        tcp_close(acced);
    }

    tcp_close(socket);
}

__always_inline
static std::tuple<size_t, int32_t> poll(int32_t *doorBells, size_t count) {
    for (;;) {
        for (size_t i = 0; i < count; ++i) {
            int32_t writePos = *reinterpret_cast<volatile int32_t *>(&doorBells[i]);
            if (writePos != -1) {
                doorBells[i] = -1;
                return {i, writePos};
            }
        }
    }
}

#ifdef __AVX2__

__always_inline
static std::tuple<size_t, int32_t> SIMDPoll(int32_t *doorBells, size_t count) {
    assert(count % 8 == 0);
    const auto invalid = _mm256_set1_epi32(-1);

    for (;;) {
        for (size_t i = 0; i < count; i += 8) {
            auto data = *reinterpret_cast<volatile __m256i *>(&doorBells[i]);
            auto cmp = _mm256_cmpeq_epi32(invalid, data); // sadly no neq
            uint32_t cmpMask = compl _mm256_movemask_epi8(cmp);
            auto lzcnt = __builtin_clz(cmpMask);
            if (lzcnt != 32) {
                // 4 bits per value, since cmpeq32, but movemask8
                auto sender = i + ((32 - lzcnt) / 4 - 1);
                auto writePos = doorBells[sender];
                doorBells[sender] = -1;
                return {sender, writePos};
            }
        }
    }
}

#endif

//__always_inline
static std::tuple<size_t, int32_t> SSEPoll(int32_t *doorBells, size_t count) {
    assert(count % 4 == 0);
    const auto invalid = _mm_set1_epi32(-1);
    for (;;) {
        for (size_t i = 0; i < count; i += 4) {
            auto data = *reinterpret_cast<volatile __m128i *>(&doorBells[i]);
            auto cmp = _mm_cmpeq_epi32(invalid, data);
            uint16_t cmpMask = compl _mm_movemask_epi8(cmp);
            auto lzcnt = __builtin_clz(cmpMask);
            if (lzcnt != 32) {
                auto sender = i + ((32 - lzcnt) / 4 - 1);
                auto writePos = doorBells[sender];
                doorBells[sender] = -1;
                return {sender, writePos};
            }
        }
    }
}

template<class QueuePair, class F>
void exclusiveBuffer(bool isClient, size_t dataSize, uint16_t pollPositions, F pollFunc) {
    std::string data(dataSize, 'A');
    auto net = rdma::Network();
    auto &cq = net.getSharedCompletionQueue();
    auto qp = QueuePair(net);

    auto myPos = pollPositions - 1;
    auto recvbuf = std::vector<char>(dataSize * pollPositions); // for each pollPosition one buffer
    auto recvmr = net.registerMr(recvbuf.data(), recvbuf.size(),
                                 {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});
    auto recvDoorBells = std::vector<char>(pollPositions);
    auto recvDoorBellMr = net.registerMr(recvDoorBells.data(), recvDoorBells.size() * sizeof(char),
                                         {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});

    auto sendbuf = std::vector<char>(data.size());
    auto sendmr = net.registerMr(sendbuf.data(), sendbuf.size(), {});

    auto sendDoorBellBuf = std::vector<char>(1);
    auto sendDoorBellMr = net.registerMr(sendDoorBellBuf.data(), sendDoorBellBuf.size() * sizeof(char),
                                         {ibv::AccessFlag::LOCAL_WRITE, ibv::AccessFlag::REMOTE_WRITE});

    auto socket = tcp_socket();
    int commsocket;

    if (isClient) {
        connectSocket(socket);
        commsocket = socket;
    } else {
        setUpListenSocket(socket);

        const auto acced = [&] {
            sockaddr_in ignored{};
            return tcp_accept(socket, ignored);
        }();
        commsocket = acced;
    }

    // invalidate recv door bells
    std::fill(recvDoorBells.begin(), recvDoorBells.end(), '\0');

    auto remoteAddr = rdma::Address{qp.getQPN(), net.getLID()};
    tcp_write(commsocket, &remoteAddr, sizeof(remoteAddr));
    tcp_read(commsocket, &remoteAddr, sizeof(remoteAddr));

    auto remoteMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(&recvbuf[dataSize * myPos]),
                                                     recvmr->getRkey()};
    tcp_write(commsocket, &remoteMr, sizeof(remoteMr));
    tcp_read(commsocket, &remoteMr, sizeof(remoteMr));

    auto remoteDoorbellMr = ibv::memoryregion::RemoteAddress{reinterpret_cast<uintptr_t>(&recvDoorBells[myPos]),
                                                             recvDoorBellMr->getRkey()};
    tcp_write(commsocket, &remoteDoorbellMr, sizeof(remoteDoorbellMr));
    tcp_read(commsocket, &remoteDoorbellMr, sizeof(remoteDoorbellMr));

    qp.connect(remoteAddr);

    auto write = createWriteWr(sendmr->getSlice());
    auto doorBellWrite = createWriteWr(sendDoorBellMr->getSlice());
    doorBellWrite.setRemoteAddress(remoteDoorbellMr);

    if (isClient) {
        std::copy(data.begin(), data.end(), sendbuf.begin());

        bench(SHAREDMEM_MESSAGES, [&]() {
            for (size_t i = 0; i < SHAREDMEM_MESSAGES; ++i) {
                sendDoorBellBuf[0] = 'X'; // indicator that something arrived
                write.setRemoteAddress(remoteMr);

                qp.postWorkRequest(write);
                qp.postWorkRequest(doorBellWrite);
                cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
                cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);

                // wait for incoming message
                const auto sender = pollFunc(recvDoorBells.data(), pollPositions);

                auto begin = recvbuf.begin() + (dataSize * sender);
                auto end = begin + dataSize;
                // check if the data is still the same
                if (not std::equal(begin, end, data.begin(), data.end())) {
                    throw std::runtime_error("received string not equal");
                }
            }
        }, 1);

    } else {
        bench(SHAREDMEM_MESSAGES, [&]() {
            for (size_t i = 0; i < SHAREDMEM_MESSAGES; ++i) {
                // wait for incoming message
                const auto sender = pollFunc(recvDoorBells.data(), pollPositions);

                auto begin = recvbuf.begin() + (dataSize * sender);
                auto end = begin + dataSize;
                std::copy(begin, end, sendbuf.begin());
                // echo back the received data
                sendDoorBellBuf[0] = 'X';
                write.setRemoteAddress(remoteMr);
                qp.postWorkRequest(write);
                qp.postWorkRequest(doorBellWrite);
                cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
                cq.pollSendCompletionQueueBlocking(ibv::workcompletion::Opcode::RDMA_WRITE);
            }
        }, 1);

        tcp_close(commsocket);
    }

    tcp_close(socket);
}

__always_inline
static size_t exPoll(char *doorBells, size_t count) {
    for (;;) {
        for (size_t i = 0; i < count; ++i) {
            auto data = *reinterpret_cast<volatile char *>(&doorBells[i]);
            if (data != '\0') {
                doorBells[i] = '\0';
                return i;
            }
        }
    }
}

#ifdef __AVX2__

__always_inline
static size_t exPollSIMD(char *doorBells, size_t count) {
    assert(count % 32 == 0);
    const auto zero = _mm256_setzero_si256();
    for (;;) {
        for (size_t i = 0; i < count; i += 32) { // _mm256_cmpeq_epi8
            auto data = *reinterpret_cast<volatile __m256i *>(&doorBells[i]);
            auto cmp = _mm256_cmpeq_epi8(zero, data);
            uint32_t cmpMask = compl _mm256_movemask_epi8(cmp);
            auto lzcnt = __builtin_clz(cmpMask);
            if (lzcnt != 32) {
                auto sender = 32 - (lzcnt + 1) + i;
                doorBells[sender] = -1;
                return sender;
            }
        }
    }
}

#endif

__always_inline
static size_t exPollSSE(char *doorBells, size_t count) {
    assert(count % 16 == 0);
    const auto zero = _mm_set1_epi8('\0');
    for (;;) {
        for (size_t i = 0; i < count; i += 16) {
            auto data = *reinterpret_cast<volatile __m128i *>(&doorBells[i]);
            auto cmp = _mm_cmpeq_epi8(zero, data);
            uint16_t cmpMask = compl _mm_movemask_epi8(cmp);
            auto lzcnt = __builtin_clz(cmpMask);
            if (lzcnt != 32) {
                auto sender = 32 - (lzcnt + 1) + i;
                doorBells[sender] = -1;
                return sender;
            }
        }
    }
}

__always_inline
static size_t exPollPCMP(char *doorBells, size_t count) {
    assert (count % 16 == 0);
    const auto needle = _mm_set1_epi8('\0');
    for (;;) {
        for (size_t i = 0; i < count; i += 16) {
            const auto haystack = *reinterpret_cast<volatile __m128i *>(&doorBells[i]);
            const auto mask = _mm_cmpestrm(needle, 1, haystack, 16,
                                           _SIDD_UBYTE_OPS | _SIDD_CMP_EQUAL_ANY | _SIDD_NEGATIVE_POLARITY |
                                           _SIDD_LEAST_SIGNIFICANT);
            const auto res = _mm_extract_epi16(mask, 0);
            if (res != 0) {
                const auto lzcnt = __builtin_clz(res);
                const size_t sender = (31 - lzcnt) + i;
                doorBells[sender] = '\0';
                return sender;
            }
        }
    }
}

void test() {
    const auto dimension = 64;
    int32_t positions[dimension][dimension];
    for (auto &position : positions) {
        for (int &j : position) {
            j = -1;
        }
    }

    auto rearmPositions = [&]() {
        for (int i = 0; i < dimension; ++i) {
            for (int j = 0; j < dimension; ++j) {
                if (i == j) {
                    positions[i][j] = i;
                }
            }
        }
    };

    for (int i = 0; i < dimension; ++i) {
        {
            rearmPositions();
            auto[s, w] = poll(positions[i], dimension);
            if (w != i || static_cast<int>(s) != i) throw std::runtime_error("poll");
        }
#ifdef __AVX2__
        {
            rearmPositions();
            auto[s, w] = SIMDPoll(positions[i], dimension);
            if (w != i || static_cast<int>(s) != i) throw std::runtime_error("SIMDPoll");
        }
#endif
        {
            rearmPositions();
            auto[s, w] = SSEPoll(positions[i], dimension);
            if (w != i || static_cast<int>(s) != i) throw std::runtime_error("SSEPoll");
        }
    }

    char markers[dimension][dimension];
    for (auto &marker : markers) {
        for (char &j : marker) {
            j = '\0';
        }
    }

    auto rearmMarkers = [&]() {
        for (int i = 0; i < dimension; ++i) {
            for (int j = 0; j < dimension; ++j) {
                if (i == j) {
                    markers[i][j] = 'X';
                }
            }
        }
    };

    for (size_t i = 0; i < dimension; ++i) {
        rearmMarkers();
        if (exPoll(markers[i], dimension) != i) throw std::runtime_error("exPoll");
#ifdef __AVX2__
        rearmMarkers();
        if (exPollSIMD(markers[i], dimension) != i) throw std::runtime_error("exPollSIMD");
#endif
        rearmMarkers();
        if (exPollPCMP(markers[i], dimension) != i) throw std::runtime_error("exPollPCMP");
        rearmMarkers();
        if (exPollSSE(markers[i], dimension) != i) throw std::runtime_error("exPollSSE");
    }
}

int main(int argc, char **argv) {
    test();
    if (argc < 2) {
        cout << "Usage: " << argv[0] << " <client / server> <(optional) 127.0.0.1>" << endl;
        return -1;
    }
    const auto isClient = argv[1][0] == 'c';
    if (argc > 2) {
        ip = argv[2];
    }

    cout << "size, connection, clients, messages, seconds, msgps, user, kernel, total" << '\n';
    const auto length = 64;
    for (const int clients : {1, 2, 4, 8, 16, 32, 64, 128, 192, 256}) {
        cout << length << ", ImmData " << clients << ", ";
        runImmData<rdma::RcQueuePair>(isClient, length);
        cout << length << ", scalar_poll " << clients << ", ";
        bigBuffer<rdma::RcQueuePair>(isClient, length, clients, poll);
#ifdef __AVX2__
        if (clients >= 8) {
            cout << length << ", simd_poll " << clients << ", ";
            bigBuffer<rdma::RcQueuePair>(isClient, length, clients, SIMDPoll);
        }
#endif
        if (clients >= 4) {
            cout << length << ", sse_poll " << clients << ", ";
            bigBuffer<rdma::RcQueuePair>(isClient, length, clients, SSEPoll);
        }
        cout << length << ", exclusive_buffer " << clients << ", ";
        exclusiveBuffer<rdma::RcQueuePair>(isClient, length, clients, exPoll);
#ifdef __AVX2__
        if (clients >= 32) {
            cout << length << ", exclusive_simd " << clients << ", ";
            exclusiveBuffer<rdma::RcQueuePair>(isClient, length, clients, exPollSIMD);
        }
#endif
        if (clients >= 16) {
            cout << length << ", exclusive_pcmp " << clients << ", ";
            exclusiveBuffer<rdma::RcQueuePair>(isClient, length, clients, exPollPCMP);
        }
        if (clients >= 16) {
            cout << length << ", exclusive_sse " << clients << ", ";
            exclusiveBuffer<rdma::RcQueuePair>(isClient, length, clients, exPollSSE);
        }
    }
}
