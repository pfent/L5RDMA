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
//---------------------------------------------------------------------------
struct ibv_send_wr;
//---------------------------------------------------------------------------
namespace rdma {
//---------------------------------------------------------------------------
struct RemoteMemoryRegion;
struct MemoryRegion;
//---------------------------------------------------------------------------
class WorkRequest {
   friend class Network;

protected:
   std::unique_ptr <ibv_send_wr> wr;
   WorkRequest();
   WorkRequest(const WorkRequest &) = delete;
   WorkRequest(WorkRequest &&) = delete;
   const WorkRequest &operator=(const WorkRequest &) = delete;
   ~WorkRequest();

public:
   /// Clear all set data and restore to original state after construction
   void reset();

   /// Assign an arbitrary id (not used, only for 'recognizing' it)
   void setId(uint64_t id);
   uint64_t getId() const;

   /// Should the work request produce a completion event
   void setCompletion(bool flag);
   bool getCompletion() const;
};
//---------------------------------------------------------------------------
class AtomicFetchAndAddWorkRequest : public WorkRequest {
public:
   AtomicFetchAndAddWorkRequest();

   /// The number to be added to the remote address
   void setAddValue(uint64_t value);
   uint64_t getAddValue() const;

   /// Remote memory address (location of the add value)
   void setRemoteAddress(const RemoteMemoryRegion &remoteAddress);

   /// Local memory address (location to write what was at the remote )
   void setLocalAddress(const MemoryRegion &localAddress);
};
//---------------------------------------------------------------------------
} // End of namespace rdma
//---------------------------------------------------------------------------
