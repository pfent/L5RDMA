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
#include <memory>
#include <iostream>
#include <array>
#include <vector>
#include <set>
#include <functional>
//---------------------------------------------------------------------------
#include "rdma/Network.hpp"
#include "util/NotAssignable.hpp"
#include "dht/Common.hpp"
//---------------------------------------------------------------------------
namespace util { // Utility
//---------------------------------------------------------------------------
template<class T> struct FreeListElement {
   T *next;
};
//---------------------------------------------------------------------------
template<class T> class FreeListAllocator {
public:

   FreeListAllocator(std::vector<T *> &&mem)
           : mem(std::move(mem))
             , nextFreeElement(nullptr)
   {
      for (uint64_t i = 0; i<this->mem.size(); ++i) {
         free(this->mem[i]);
      }
   }

   T *allocate()
   {
      assert(nextFreeElement != NULL);

      T *result = nextFreeElement;
      nextFreeElement = nextFreeElement->next;
      return result;
   }

   void free(T *element)
   {
      element->next = nextFreeElement;
      nextFreeElement = element;
   }

private:
   std::vector<T*> mem;
   T *nextFreeElement;
};
//---------------------------------------------------------------------------
static inline void testFreeListAllocator()
{
   struct Sample : public FreeListElement<Sample> {
      int a;
   };

   const int kEntries = 128;

   std::vector<Sample *> data(kEntries);
   for (int i = 0; i<kEntries; ++i) {
      data[i] = new Sample();
      data[i]->a = i + 1;
   }

   FreeListAllocator<Sample> allocator(std::move(data));

   // Alloc everything and free it again
   for (int j = 0; j<100; ++j) {
      // Alloc
      int sum = 0;
      std::set<Sample *> dataPtr;
      for (int i = 0; i<kEntries; ++i) {
         Sample *sample = allocator.allocate();
         sum += sample->a;
         dataPtr.insert(sample);
      }
      assert(sum == kEntries * (kEntries + 1) / 2);

      // Free
      for (auto iter : dataPtr) {
         allocator.free(iter);
      }
   }
}
//---------------------------------------------------------------------------
} // End of namespace util
//---------------------------------------------------------------------------
