#include <iostream>
#include <algorithm>
#include <vector>

#include "rdma/Network.hpp"
#include "rdma/CompletionQueuePair.hpp"
#include "rdma/QueuePair.hpp"
#include "rdma/WorkRequest.hpp"
#include "rdma/MemoryRegion.hpp"

#include <infiniband/verbs.h>

using namespace std;
using namespace rdma;

int main(int argc, char **argv) {
    if (argc != 2) {
        throw;
    }
    bool isClient = argv[1][0] == 'c';

    Network network;
    CompletionQueuePair completionQueue(network);
    QueuePair queuePair(network, completionQueue);

    cout << "network.getLID() = " << network.getLID() << endl;
    cout << "queuePairs[i]->getQPN() = " << queuePair.getQPN() << endl;

    cout << "enter qpn:" << endl;
    uint32_t qpn;
    cin >> qpn;

    Address address{network.getLID(), qpn};
    queuePair.connect(address);

    if (isClient) {
        string data = "hello my world !";
        MemoryRegion sharedMR((void *) data.data(), data.length(), network.getProtectionDomain(),
                              MemoryRegion::Permission::All);

        RemoteMemoryRegion rmr;
        cout << "enter remote key:" << endl;
        cin >> rmr.key;
        cout << "enter remote address:" << endl;
        cin >> rmr.address;

        WriteWorkRequest workRequest;
        workRequest.setLocalAddress(sharedMR);
        workRequest.setRemoteAddress(rmr);
        workRequest.setCompletion(true);

        queuePair.postWorkRequest(workRequest);
        auto a = completionQueue.waitForCompletion();
        cout << a.first << " " << a.second << endl;
    } else {
        vector<uint8_t> receiveBuffer(128);
        MemoryRegion sharedMR(receiveBuffer.data(), receiveBuffer.size(), network.getProtectionDomain(),
                              MemoryRegion::Permission::All);

        cout << sharedMR.key->rkey << endl;
        cout << (uint64_t) sharedMR.address << endl;

        int qwe;
        cout << "waiting for data ..." << endl;
        cin >> qwe;

        cout << (char *) receiveBuffer.data() << endl;
    }

    int blub;
    cout << "exit ?" << endl;
    cin >> blub;

    return 0;
}