//
// This file is part of the gda cpp utility library.
// Copyright (c) 2013 Alexander van Renen. All rights reserved.
//
// Purpose - Helps reading data received via tcp in a nice way.
// -------------------------------------------------------------------------------------------------
#include "gda/tcp/NetworkBuffer.hpp"
// -------------------------------------------------------------------------------------------------
namespace gda {
// -------------------------------------------------------------------------------------------------
namespace tcp {
// -------------------------------------------------------------------------------------------------
uint32_t NetworkBuffer::compactify()
{
   uint32_t movedBytes = writePosition - readPosition;
   memmove(mem.data(), mem.data() + readPosition, writePosition - readPosition);
   writePosition = writePosition - readPosition;
   readPosition = 0;
   return movedBytes;
}
// -------------------------------------------------------------------------------------------------
void NetworkBuffer::increaseSizeBy(uint32_t size)
{
   mem.resize(mem.size() + size);
}
// -------------------------------------------------------------------------------------------------
uint32_t NetworkBuffer::size()
{
   return mem.size();
}
// -------------------------------------------------------------------------------------------------
} // End of namespace tcp
// -------------------------------------------------------------------------------------------------
} // End of namespace gda
// -------------------------------------------------------------------------------------------------
