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
#include "MemoryRef.hpp"
#include <string>
#include <array>
//---------------------------------------------------------------------------
using namespace std;
//---------------------------------------------------------------------------
namespace util {
//---------------------------------------------------------------------------
namespace {
array<char, 16> numToHex{{'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'}};
}
//---------------------------------------------------------------------------
const string MemoryRef::to_hex() const
{
   string result;
   for (uint32_t i = 0; i < size(); i++) {
      result += numToHex[*(uint8_t*) data(i) >> 4];
      result += numToHex[*(uint8_t*) data(i) & 0x0f];
   }
   return result;
}
//---------------------------------------------------------------------------
} // End of namespace util
//---------------------------------------------------------------------------
