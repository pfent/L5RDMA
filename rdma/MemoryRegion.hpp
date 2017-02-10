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
#pragma once
//---------------------------------------------------------------------------
#include <stdint.h>
#include <cstdlib>
#include <type_traits>
#include <ostream>
//---------------------------------------------------------------------------
struct ibv_mr;
struct ibv_pd;
//---------------------------------------------------------------------------
namespace rdma {
//---------------------------------------------------------------------------
/// A region of main memory pinned to avoid swapping to disk
//---------------------------------------------------------------------------
class MemoryRegion {
public:
   enum class Permission : uint8_t {
      None = 0,
      LocalWrite = 1 << 0,
      RemoteWrite = 1 << 1,
      RemoteRead = 1 << 2,
      RemoteAtomic = 1 << 3,
      MemoryWindowBind = 1 << 4,
      All = LocalWrite | RemoteWrite | RemoteRead | RemoteAtomic | MemoryWindowBind
   };

    struct Slice {
        friend class MemoryRegion;

        void *address;
        size_t size;
        uint32_t lkey;
        Slice(void *address, size_t size, uint32_t lkey) : address(address), size(size), lkey(lkey) {}
    };

   ibv_mr *key;
    void *address;
   const size_t size;

   /// Constructor
   MemoryRegion(void *address, size_t size, ibv_pd *protectionDomain, Permission permissions);
   /// Destructor
   ~MemoryRegion();

    /// Get a slice of the memory to pass on
    Slice slice(size_t offset, size_t size);

   MemoryRegion(MemoryRegion const &) = delete;
   void operator=(MemoryRegion const &) = delete;
};
//---------------------------------------------------------------------------
inline MemoryRegion::Permission operator|(MemoryRegion::Permission a, MemoryRegion::Permission b) {
   return static_cast<MemoryRegion::Permission>(static_cast<std::underlying_type<MemoryRegion::Permission>::type>(a) | static_cast<std::underlying_type<MemoryRegion::Permission>::type>(b));
}
//---------------------------------------------------------------------------
inline MemoryRegion::Permission operator&(MemoryRegion::Permission a, MemoryRegion::Permission b) {
   return static_cast<MemoryRegion::Permission>(static_cast<std::underlying_type<MemoryRegion::Permission>::type>(a) & static_cast<std::underlying_type<MemoryRegion::Permission>::type>(b));
}
//---------------------------------------------------------------------------
std::ostream &operator<<(std::ostream& os, const MemoryRegion& memoryRegion);
//---------------------------------------------------------------------------
} // End of namespace rdma
//---------------------------------------------------------------------------
