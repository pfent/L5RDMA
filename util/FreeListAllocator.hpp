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
#include <array>
#include <vector>
#include <zmq.hpp>
//---------------------------------------------------------------------------
#include "rdma/Network.hpp"
#include "util/NotAssignable.hpp"
#include "dht/Common.hpp"
//---------------------------------------------------------------------------
struct ibv_send_wr;
//---------------------------------------------------------------------------
namespace util { // Utility
//---------------------------------------------------------------------------
template<class T> class FreeListAllocator { // TODO: copied from other project .. did not read it
public:
   T *allocate()
   {
      if (nextFreeElement != NULL) {
         T *result = (T *) nextFreeElement;
         nextFreeElement = nextFreeElement->next;
         return result;
      }

      if (positionInCurrentChunk>=Chunk::chunkSize) {
         Chunk *lastChunk = currentChunk;
         currentChunk = new Chunk();
         lastChunk->next = currentChunk;
         positionInCurrentChunk = 0;
      }

      return (T *) (currentChunk->mem + (positionInCurrentChunk++ * sizeof(T)));
   }

   void free(void *data)
   {
      static_cast<FreeElement *>(data)->next = nextFreeElement;
      nextFreeElement = static_cast<FreeElement *>(data);
   }

private:
   struct Chunk {
      Chunk()
      {
         mem = new uint8_t[chunkSize * sizeof(T)];
         next = NULL;
      }
      ~Chunk()
      {
         delete[] mem;
         if (next != NULL)
            delete next;
      }
      static const uint64_t chunkSize = 64;
      uint8_t *mem;
      Chunk *next;
   };

   struct FreeElement {
      FreeElement *next;
   };

   static FreeElement *nextFreeElement;
   static Chunk firstChunk;
   static Chunk *currentChunk;
   static uint32_t positionInCurrentChunk;
};
//---------------------------------------------------------------------------
} // End of namespace util
//---------------------------------------------------------------------------
