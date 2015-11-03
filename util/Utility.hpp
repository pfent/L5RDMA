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
#include <string>
#include <sstream>
//---------------------------------------------------------------------------
namespace util {
//---------------------------------------------------------------------------
std::string getHostname();
//---------------------------------------------------------------------------
/// The exception which gets thrown when parsing a number/string fails
struct NoNumberOrNoStringDependingOnWhatYouCalled {
};
//---------------------------------------------------------------------------
template<class Number> std::string to_string(const Number &num)
{
   std::ostringstream stream;
   stream << num;
   if (!stream.good())
      throw NoNumberOrNoStringDependingOnWhatYouCalled();
   return stream.str();
}
//---------------------------------------------------------------------------
/// string --> number
template<class Number> Number to_number(const std::string &str)
{
   Number num;
   std::istringstream stream(str);
   stream >> num;
   if (!stream.good() && !stream.eof())
      throw NoNumberOrNoStringDependingOnWhatYouCalled();
   return num;
}
//---------------------------------------------------------------------------
template<typename Number> bool isPowerOfTwo(Number x)
{
   return ((x != 0) && !(x & (x - 1)));
}
//---------------------------------------------------------------------------
inline int getNumberOfSetBits(uint64_t i)
{
   i = i - ((i >> 1) & 0x5555555555555555);
   i = (i & 0x3333333333333333) + ((i >> 2) & 0x3333333333333333);
   return (((i + (i >> 4)) & 0xF0F0F0F0F0F0F0F) * 0x101010101010101) >> 56;
}
//---------------------------------------------------------------------------
} // End of namespace util
//---------------------------------------------------------------------------
