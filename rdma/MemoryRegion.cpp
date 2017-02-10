//---------------------------------------------------------------------------
// (c) 2015 Wolf Roediger <roediger@in.tum.de>
// Technische Universitaet Muenchen
// Institut fuer Informatik, Lehrstuhl III
// Boltzmannstr. 3
// 85748 Garching
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//---------------------------------------------------------------------------
#include "MemoryRegion.hpp"
#include "Network.hpp"
//---------------------------------------------------------------------------
#include <infiniband/verbs.h>
#include <iostream>
#include <cstring>
//---------------------------------------------------------------------------
using namespace std;
//---------------------------------------------------------------------------
namespace rdma {
//---------------------------------------------------------------------------
    int convertPermissions(MemoryRegion::Permission permissions) {
        int flags = 0;
        if (static_cast<underlying_type<MemoryRegion::Permission>::type>(permissions &
                                                                         MemoryRegion::Permission::LocalWrite)) {
            flags |= IBV_ACCESS_LOCAL_WRITE;
        }
        if (static_cast<underlying_type<MemoryRegion::Permission>::type>(permissions &
                                                                         MemoryRegion::Permission::RemoteWrite)) {
            flags |= IBV_ACCESS_REMOTE_WRITE;
        }
        if (static_cast<underlying_type<MemoryRegion::Permission>::type>(permissions &
                                                                         MemoryRegion::Permission::RemoteRead)) {
            flags |= IBV_ACCESS_REMOTE_READ;
        }
        if (static_cast<underlying_type<MemoryRegion::Permission>::type>(permissions &
                                                                         MemoryRegion::Permission::RemoteAtomic)) {
            flags |= IBV_ACCESS_REMOTE_ATOMIC;
        }
        if (static_cast<underlying_type<MemoryRegion::Permission>::type>(permissions &
                                                                         MemoryRegion::Permission::MemoryWindowBind)) {
            flags |= IBV_ACCESS_MW_BIND;
        }
        return flags;
    }

//---------------------------------------------------------------------------
    MemoryRegion::MemoryRegion(void *address, size_t size, ibv_pd *protectionDomain, Permission permissions) : address(
            address), size(size) {
        key = ::ibv_reg_mr(protectionDomain, address, size, convertPermissions(permissions));
        if (key == nullptr) {
            string reason = "registering memory failed with error " + to_string(errno) + ": " + strerror(errno);
            cerr << reason << endl;
            throw NetworkException(reason);
        }
    }

//---------------------------------------------------------------------------
    MemoryRegion::~MemoryRegion() {
        if (::ibv_dereg_mr(key) != 0) {
            string reason = "deregistering memory failed with error " + to_string(errno) + ": " + strerror(errno);
            cerr << reason << endl;
            throw NetworkException(reason);
        }
    }

    MemoryRegion::Slice MemoryRegion::slice(size_t offset, size_t size) {
        return MemoryRegion::Slice(reinterpret_cast<uint8_t *>(address) + offset, size, key->lkey);
    }

//---------------------------------------------------------------------------
    ostream &operator<<(ostream &os, const MemoryRegion &memoryRegion) {
        return os << "ptr=" << memoryRegion.address << " size=" << memoryRegion.size << " key={..}";
    }
//---------------------------------------------------------------------------
} // End of namepsace rdma
//---------------------------------------------------------------------------