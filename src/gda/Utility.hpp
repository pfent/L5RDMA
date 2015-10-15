//
// This file is part of the gda cpp utility library.
// Copyright (c) 2012 Alexander van Renen. All rights reserved.
//
// Purpose - General helper functions.
// -------------------------------------------------------------------------------------------------
#pragma once
// -------------------------------------------------------------------------------------------------
#include <memory>
#include <vector>
#include <stdint.h>
#include <chrono>
// -------------------------------------------------------------------------------------------------
namespace gda {
// -------------------------------------------------------------------------------------------------
/// Creates a unique ptr
template<class T, class... Arg>
std::unique_ptr<T> make_unique(Arg&& ...args)
{
   return std::unique_ptr<T>(new T(std::forward<Arg>(args)...));
}
// -------------------------------------------------------------------------------------------------
std::vector<std::string> split(const std::string &str, char delimiter);
// -------------------------------------------------------------------------------------------------
/// Tries to figure out available memory
uint64_t getMemorySizeInBytes();
// -------------------------------------------------------------------------------------------------
/// Converts the given time in ns into a usable unit depending on its size
std::string formatTime(std::chrono::nanoseconds ns, uint32_t precision);
// -------------------------------------------------------------------------------------------------
} // End of namespace gda
// -------------------------------------------------------------------------------------------------
