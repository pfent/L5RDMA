//
// This file is part of the gda cpp utility library.
// Copyright (c) 2013 Alexander van Renen. All rights reserved.
//
// Purpose - Helps reading data received via tcp in a nice way.
// -------------------------------------------------------------------------------------------------
#pragma once
// -------------------------------------------------------------------------------------------------
#include "gda/MemoryRef.hpp"
#include <vector>
#include <cstdint>
#include <cassert>
#include <string>
// -------------------------------------------------------------------------------------------------
namespace gda {
// -------------------------------------------------------------------------------------------------
namespace tcp {
// -------------------------------------------------------------------------------------------------
/// A buffer for reading messages from the network.
///
/// The network classes (Connection) write into the buffer.
/// The application (Controller) reads from it.
class NetworkBuffer {
public:
   NetworkBuffer(uint32_t size)
   : mem(size, 'x')
   , readPosition(0)
   , writePosition(0) { }
   NetworkBuffer(const std::string& startValue);

   /// The number of bytes which can be read from the buffer.
   uint32_t getReadSpace() const { return writePosition - readPosition; }

   /// Read the specified amount of bytes from the buffer (increases read pointer)
   MemoryRef extractBytes(uint32_t bytes);
   std::string extractString(uint32_t bytes);
   uint32_t extractInt32();

   /// Reuse unused memory (the one that is already read). This will move memory around and therefore destroy all pointers into the buffer.
   uint32_t compactify();
   /// Increases the memory pool. This will move memory around and therefore destroy all pointers into the buffer.
   void increaseSizeBy(uint32_t size);
   /// Complete size of the buffer
   uint32_t size();

private:

   friend class Connection;
   friend class Client;

   /// The number of bytes which can be written from the buffer.
   uint32_t getWriteSpace() const { return mem.size() - writePosition; }
   /// Get a pointer with at least getWriteSpace of free bytes (does not increase the write pointer, use increaseWritePointer)
   char* getWritePointer() { return mem.data() + writePosition; }
   /// Tells the object how many bytes the user wrote into the buffer with getWritePointer
   void increaseWritePointer(uint32_t bytes)
   {
      assert(writePosition + bytes <= mem.size());
      writePosition += bytes;
   }

   std::vector<char> mem;
   uint32_t readPosition;
   uint32_t writePosition;
};
// -------------------------------------------------------------------------------------------------
NetworkBuffer::NetworkBuffer(const std::string& startValue)
: mem(startValue.size())
, readPosition(0)
, writePosition(startValue.size())
{
   memcpy(mem.data(), startValue.data(), startValue.size());
}
// -------------------------------------------------------------------------------------------------
uint32_t NetworkBuffer::extractInt32()
{
   assert(readPosition + sizeof(uint32_t) <= writePosition);
   readPosition += sizeof(uint32_t);
   return *reinterpret_cast<uint32_t*>(mem.data() + readPosition - sizeof(uint32_t));
}
// -------------------------------------------------------------------------------------------------
std::string NetworkBuffer::extractString(uint32_t bytes)
{
   MemoryRef r = extractBytes(bytes);
   return std::string(r.data(), r.size());
}
// -------------------------------------------------------------------------------------------------
MemoryRef NetworkBuffer::extractBytes(uint32_t bytes)
{
   assert(readPosition + bytes <= writePosition);
   readPosition += bytes;
   return MemoryRef(mem.data() + readPosition - bytes, bytes);
}
// -------------------------------------------------------------------------------------------------
}
// -------------------------------------------------------------------------------------------------
}
// -------------------------------------------------------------------------------------------------
