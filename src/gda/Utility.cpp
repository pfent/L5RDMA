//
// This file is part of the gda cpp utility library.
// Copyright (c) 2012 Alexander van Renen. All rights reserved.
//
// Purpose - General helper functions.
// -------------------------------------------------------------------------------------------------
#include "gda/Utility.hpp"
#include <unistd.h>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <sstream>
#include <cmath>
// -------------------------------------------------------------------------------------------------
using namespace std;
// -------------------------------------------------------------------------------------------------
namespace gda {
// -------------------------------------------------------------------------------------------------
vector<string> split(const string& str, char delimiter)
{
   vector<string> result;
   istringstream ss(str);
   string buffer;
   while (getline(ss, buffer, delimiter))
      result.push_back(buffer);
   return result;
}
// -------------------------------------------------------------------------------------------------
uint64_t getMemorySizeInBytes()
{
   // first line should look like this: MemTotal:       1056859000 kB
   ifstream in("/proc/meminfo", ios::in);
   string ignore;
   uint64_t maxMemory;
   in >> ignore >> maxMemory;
   return maxMemory;
}
// -------------------------------------------------------------------------------------------------
namespace {
// -------------------------------------------------------------------------------------------------
uint64_t applyPrecision(uint64_t input, uint32_t precision)
{
   uint32_t digits = log10(input) + 1;
   if(digits <= precision)
      return input;
   uint32_t invalidDigits = pow(10, digits - precision);
   return (uint64_t)((double)input/invalidDigits+.5f)*invalidDigits;
}
// -------------------------------------------------------------------------------------------------
} // End of anonymous
// -------------------------------------------------------------------------------------------------
string formatTime(chrono::nanoseconds ns, uint32_t precision)
{
   ostringstream os;

   uint64_t timeSpan = applyPrecision(ns.count(), precision);

   // Convert to right unit
   if(timeSpan < 1000ll)
      os << timeSpan << "ns";
   else if(timeSpan < 1000ll * 1000ll)
      os << timeSpan/1000.0f << "us";
   else if(timeSpan < 1000ll * 1000ll * 1000ll)
      os << timeSpan / 1000.0f / 1000.0f << "ms";
   else if(timeSpan < 60l * 1000ll * 1000ll * 1000ll)
      os << timeSpan / 1000.0f / 1000.0f / 1000.0f << "s";
   else if(timeSpan < 60l * 60l * 1000ll * 1000ll * 1000ll)
      os << timeSpan/1000.0f / 1000.0f / 1000.0f / 60.0f << "m";
   else
      os << timeSpan/1000.0f / 1000.0f / 1000.0f / 60.0f / 60.0f<< "h";

   return os.str();
}
// -------------------------------------------------------------------------------------------------
} // End of namespace gda
// -------------------------------------------------------------------------------------------------
