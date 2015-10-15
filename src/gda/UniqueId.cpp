//
// This file is part of the gda cpp utility library.
// Copyright (c) 2013 Alexander van Renen. All rights reserved.
//
// Purpose - This files provides different ways to obtain unique ids.
// -------------------------------------------------------------------------------------------------
#include "gda/UniqueId.hpp"
#include <chrono>
#include <ostream>
#include <iostream>
#include <chrono>
// -------------------------------------------------------------------------------------------------
using namespace std;
// -------------------------------------------------------------------------------------------------
namespace gda {
// -------------------------------------------------------------------------------------------------
UniqueTimestamp::UniqueTimestamp()
: lastValueUsed(0)
{
}
// -------------------------------------------------------------------------------------------------
uint64_t UniqueTimestamp::getNext()
{
   uint64_t val = std::chrono::system_clock::now().time_since_epoch().count();
   if (val <= lastValueUsed)
      val = ++lastValueUsed;
   return lastValueUsed = val;
}
// -------------------------------------------------------------------------------------------------
} // End of namespace gda
// -------------------------------------------------------------------------------------------------
