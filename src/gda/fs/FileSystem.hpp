//
// This file is part of the gda cpp utility library.
// Copyright (c) 2014 Alexander van Renen. All rights reserved.
//
// Purpose - Helper functions for a file system.
// -------------------------------------------------------------------------------------------------
#pragma once
// -------------------------------------------------------------------------------------------------
#include <vector>
#include <stdexcept>
#include <string>
// -------------------------------------------------------------------------------------------------
namespace gda {
// -------------------------------------------------------------------------------------------------
struct InvalidFileSystemRequest : public std::runtime_error {
   InvalidFileSystemRequest()
   : runtime_error("InvalidFileSystemRequest")
   {
   }

   InvalidFileSystemRequest(const std::string& reason)
   : runtime_error("InvalidFileSystemRequest: " + reason)
   {
   }
};
// -------------------------------------------------------------------------------------------------
namespace FileSystem {
// -------------------------------------------------------------------------------------------------
std::string getWorkingPath();
// -------------------------------------------------------------------------------------------------
/// Check if there is new input on the file descriptor
bool hasNewEvents(uint32_t fileDescriptor, int64_t waitTime = 0);
bool hasNewEvents(std::vector<uint32_t> fileDescriptors, int64_t waitTime = 0);
// -------------------------------------------------------------------------------------------------
} // End of namespace FileSystem
// -------------------------------------------------------------------------------------------------
} // End of namespace gda
// -------------------------------------------------------------------------------------------------
