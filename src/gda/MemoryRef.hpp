//
// This file is part of the gda cpp utility library.
// Copyright (c) 2013 Alexander van Renen. All rights reserved.
//
// Purpose - A non owning reference to a chunk of memory. Similar to a std::string.
// -------------------------------------------------------------------------------------------------
#pragma once
// -------------------------------------------------------------------------------------------------
#include <cstring>
#include <cstdint>
#include <string>
#include <cassert>
// -------------------------------------------------------------------------------------------------
namespace gda {
// -------------------------------------------------------------------------------------------------
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
};
// -------------------------------------------------------------------------------------------------
} // End of namespace util
// -------------------------------------------------------------------------------------------------
