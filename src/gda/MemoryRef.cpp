//
// This file is part of the gda cpp utility library.
// Copyright (c) 2013 Alexander van Renen. All rights reserved.
//
// Purpose - A non owning reference to a chunk of memory. Similar to a std::string.
// -------------------------------------------------------------------------------------------------
#include "gda/MemoryRef.hpp"
#include <string>
#include <array>
// -------------------------------------------------------------------------------------------------
using namespace std;
// -------------------------------------------------------------------------------------------------
namespace gda {
// -------------------------------------------------------------------------------------------------
namespace {
array<char, 16> numToHex{{'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'}};
}
// -------------------------------------------------------------------------------------------------
const string MemoryRef::to_hex() const
{
   string result;
   for (uint32_t i = 0; i < size(); i++) {
      result += numToHex[*(uint8_t*) data(i) >> 4];
      result += numToHex[*(uint8_t*) data(i) & 0x0f];
   }
   return result;
}
} // End of namespace gda
// -------------------------------------------------------------------------------------------------
