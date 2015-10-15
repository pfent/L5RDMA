//
// This file is part of the gda cpp utility library.
// Copyright (c) 2012 Alexander van Renen. All rights reserved.
//
// Purpose - Implements MD5 hash.
// Contributions - Stefan Marcik
// -------------------------------------------------------------------------------------------------
#include "gda/hash/MD5Hash.hpp"
#include <stdint.h>
#include <cstdlib>
#include <string>
#include <cstring>
#include <iostream>
#include <cassert>
#include <vector>
// -------------------------------------------------------------------------------------------------
using namespace std;
// -------------------------------------------------------------------------------------------------
namespace gda {
// -------------------------------------------------------------------------------------------------
namespace {
// -------------------------------------------------------------------------------------------------
uint32_t kSeeds[64] = {
7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
};
// -------------------------------------------------------------------------------------------------
uint32_t kMoreSeeds[64] = {
0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
};
// -------------------------------------------------------------------------------------------------
inline uint32_t leftRotate(uint32_t x, uint32_t c)
{
   return ((x << c) | (x >> (32 - c)));
}
// -------------------------------------------------------------------------------------------------
} // End of anonymous namespace
// -------------------------------------------------------------------------------------------------
void MD5Hash::md5CalcBlocks2(MD5Hash* hash, void* data, int blocks)
{
   //Initialize variables
   uint32_t h0 = hash->value32[0];
   uint32_t h1 = hash->value32[1];
   uint32_t h2 = hash->value32[2];
   uint32_t h3 = hash->value32[3];

   struct chunk512 {
      uint32_t w[16];
   };

   // For each 512-Bit/64-Byte chunk
   chunk512* data512 = (chunk512*) data;
   for (int dataround = 0; dataround < blocks; dataround++) {
      uint32_t a = h0;
      uint32_t b = h1;
      uint32_t c = h2;
      uint32_t d = h3;
      uint32_t f = 0;
      uint32_t g = 0;

      for (int i = 0; i <= 15; i++) {
         f = (b & c) | ((~b) & d);
         g = i;

         uint32_t temp = d;
         d = c;
         c = b;
         b = b + leftRotate((a + f + kMoreSeeds[i] + data512[dataround].w[g]), kSeeds[i]);
         a = temp;
      }

      for (int i = 16; i <= 31; i++) {
         f = (d & b) | ((~d) & c);
         g = (5 * i + 1) % 16;

         uint32_t temp = d;
         d = c;
         c = b;
         b = b + leftRotate((a + f + kMoreSeeds[i] + data512[dataround].w[g]), kSeeds[i]);
         a = temp;
      }

      for (int i = 32; i <= 47; i++) {
         f = b ^ c ^ d;
         g = (3 * i + 5) % 16;

         uint32_t temp = d;
         d = c;
         c = b;
         b = b + leftRotate((a + f + kMoreSeeds[i] + data512[dataround].w[g]), kSeeds[i]);
         a = temp;
      }

      for (int i = 48; i <= 63; i++) {
         f = c ^ (b | (~d));
         g = (7 * i) % 16;

         uint32_t temp = d;
         d = c;
         c = b;
         b = b + leftRotate((a + f + kMoreSeeds[i] + data512[dataround].w[g]), kSeeds[i]);
         a = temp;
      }

      h0 = h0 + a;
      h1 = h1 + b;
      h2 = h2 + c;
      h3 = h3 + d;
   }

   hash->value32[0] = h0;
   hash->value32[1] = h1;
   hash->value32[2] = h2;
   hash->value32[3] = h3;
}
// -------------------------------------------------------------------------------------------------
// Initializes a datablock for the input data which is already prepared for md5 computing
// That means, it expands the data with 0x80 and 0x00 until it is a complete 64 byte block
// with a 64-bit (uint64_t) value for the original data len at the end
void* initData(int datalen, int* blocks)
{
   // Alloc data
   void* data = malloc(datalen + 128);

   // Expand input data with one 0x80 and some 0x00s
   int curlen = datalen;
   ((unsigned char*) (data))[curlen++] = 0x80;
   while ((curlen % 64) != 56) ((unsigned char*) (data))[curlen++] = 0x00;

   // Add data size (in bits)
   *((uint64_t*) (&((unsigned char*) (data))[curlen])) = (uint64_t) (datalen * 8);
   curlen = curlen + 8;

   // Return
   *blocks = (curlen / 64);
   return data;
}
// -------------------------------------------------------------------------------------------------
MD5Hash::MD5Hash(const string& str)
{
   value32[0] = 0;
   value32[1] = 0;
   value32[2] = 0;
   value32[3] = 0;
   hash(str);
}
// -------------------------------------------------------------------------------------------------
MD5Hash::MD5Hash()
{
   value32[0] = 0;
   value32[1] = 0;
   value32[2] = 0;
   value32[3] = 0;
}
// -------------------------------------------------------------------------------------------------
void MD5Hash::hash(const string& str)
{
   int numBlocks = 0;
   void* data = initData(str.size(), &numBlocks);
   memcpy(data, str.c_str(), str.size());
   md5CalcBlocks2(this, data, numBlocks);
   free(data);
}
// -------------------------------------------------------------------------------------------------
ostream& operator<<(ostream& os, const MD5Hash& data)
{
   os << data.value32[0] << " " << data.value32[1] << " " << data.value32[2] << " " << data.value32[3];
   return os;
}
// -------------------------------------------------------------------------------------------------
istream& operator>>(istream& is, MD5Hash& data)
{
   is >> data.value32[0] >> data.value32[1] >> data.value32[2] >> data.value32[3];
   return is;
}
// -------------------------------------------------------------------------------------------------
bool MD5Hash::operator==(const MD5Hash& other) const
{
   return value32[0] == other.value32[0] && value32[1] == other.value32[1] && value32[2] == other.value32[2] && value32[3] == other.value32[3];
}
// -------------------------------------------------------------------------------------------------
bool MD5Hash::operator!=(const MD5Hash& other) const
{
   return value32[0] != other.value32[0] || value32[1] != other.value32[1] || value32[2] != other.value32[2] || value32[3] != other.value32[3];
}
// -------------------------------------------------------------------------------------------------
MD5Hash MD5Hash::getHashOfFile(std::istream& is)
{
   assert(is.tellg() == static_cast<size_t>(0));
   is.seekg(0, ios::end);
   size_t length = is.tellg();
   is.seekg(0, ios::beg);
   vector<char> data(length);
   is.read(data.data(), length);
   is.seekg(0, ios::beg);
   return MD5Hash(data.data());
}
// -------------------------------------------------------------------------------------------------
uint64_t MD5Hash::toNumber() const
{
   uint64_t result = value32[0] + (static_cast<uint64_t>(value32[2]) << 16);
   result += value32[2] + (static_cast<uint64_t>(value32[3]) << 16);
   return result;
}
// -------------------------------------------------------------------------------------------------
} // end of namespace gda
// -------------------------------------------------------------------------------------------------
