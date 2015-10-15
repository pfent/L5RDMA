//
// This file is part of the gda cpp utility library.
// Copyright (c) 2012 Alexander van Renen. All rights reserved.
//
// Purpose - Implements MD5 hash.
// Contributions - Stefan Marcik
// -------------------------------------------------------------------------------------------------
#pragma once
// -------------------------------------------------------------------------------------------------
#include <string>
#include <sstream>
#include <stdint.h>
// -------------------------------------------------------------------------------------------------
namespace gda {
// -------------------------------------------------------------------------------------------------
class MD5Hash {
public:
   MD5Hash();
   MD5Hash(const std::string& str);

   bool operator==(const MD5Hash& other) const;
   bool operator!=(const MD5Hash& other) const;

   static MD5Hash getHashOfFile(std::istream& is);

   uint64_t toNumber() const;
private:
   void hash(const std::string& str);

   union {
      uint8_t value[16];
      uint32_t value32[4];
   };

   static void md5CalcBlocks2(MD5Hash* hash, void* data, int blocks);

   friend std::ostream& operator<<(std::ostream& os, const MD5Hash& data);
   friend std::istream& operator>>(std::istream& is, MD5Hash& data);
};
// -------------------------------------------------------------------------------------------------
} // End of namespace gda
// -------------------------------------------------------------------------------------------------
