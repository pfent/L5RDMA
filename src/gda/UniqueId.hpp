//
// This file is part of the gda cpp utility library.
// Copyright (c) 2013 Alexander van Renen. All rights reserved.
//
// Purpose - This files provides different ways to obtain unique ids.
// -------------------------------------------------------------------------------------------------
#pragma once
// -------------------------------------------------------------------------------------------------
#include <cstdint>
// -------------------------------------------------------------------------------------------------
namespace gda {
// -------------------------------------------------------------------------------------------------
class UniqueTimestamp {
public:
   explicit UniqueTimestamp();

   uint64_t getNext();

private:
   uint64_t lastValueUsed;
};
// -------------------------------------------------------------------------------------------------
} // End of namespace gda
// -------------------------------------------------------------------------------------------------
