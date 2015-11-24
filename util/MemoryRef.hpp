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
#include <cstring>
#include <cstdint>
#include <string>
#include <cassert>
//---------------------------------------------------------------------------
namespace util {
//---------------------------------------------------------------------------
/// A non-owning reference to a chunk of memory
class MemoryRef {
	/// A pointer to the data
	char* ptr;
	/// The length of the string
	uint32_t len;

public:
	/// Constructor
   MemoryRef(char* data, uint32_t len) : ptr(data),len(len) {}
   /// Return pointer to data
   const char* data() const { return ptr; }
   const char* data(uint32_t offset) const { assert(offset<len); return ptr + offset; }
   char* data() { return ptr; }
   char* data(uint32_t offset) { assert(offset<len); return ptr + offset; }

   /// Create a non owning reference
   MemoryRef subRef(uint32_t offset) { assert(len>=offset); return MemoryRef(data(offset), len-offset); }

	/// Return length
	uint32_t size() const { return len; }

   /// Create a string (copy memory)
   const std::string to_string() const { return std::string(ptr, len); }
   const std::string to_hex() const;

	/// Index operator
	char operator[](uint32_t index) const { return ptr[index]; }
	char& operator[](uint32_t index) { return ptr[index]; }
};
//---------------------------------------------------------------------------
} // End of namespace util
//---------------------------------------------------------------------------
