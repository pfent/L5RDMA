//
// This file is part of the gda cpp utility library.
// Copyright (c) 2014 Alexander van Renen. All rights reserved.
//
// Purpose - Represents a file in a filesystem.
// -------------------------------------------------------------------------------------------------
#pragma once
// -------------------------------------------------------------------------------------------------
#include "FileSystem.hpp"
#include "Folder.hpp"
#include <string>
#include <vector>
// -------------------------------------------------------------------------------------------------
namespace gda {
// -------------------------------------------------------------------------------------------------
class MemoryRef;
// -------------------------------------------------------------------------------------------------
class File {
public:
   /// ctor
   File(const std::string& path, const Folder& folder = Folder(""));

   // Access file name
   const std::string& getPathWithEnding() const;
   const std::string getPathNoEnding() const;
   const std::string getBaseNameWithEnding() const;
   const std::string getBaseNameNoEnding() const;

   // File attributes
   uint32_t getFileLength() const throw(InvalidFileSystemRequest);
   bool exists() const;

   /// Loading
   std::string loadFileToString() const throw(InvalidFileSystemRequest);
   std::vector<char> loadFileToVector() const throw(InvalidFileSystemRequest);

   /// Writing
   void writeMemoryToFile(const MemoryRef& mem) const throw(InvalidFileSystemRequest);
   void writeStringToFile(const std::string& mem) const throw(InvalidFileSystemRequest);
   void writeVectorToFile(const std::vector<char>& mem) const throw(InvalidFileSystemRequest);

private:
   std::string path;
};
// -------------------------------------------------------------------------------------------------
} // End of namespace gda
// -------------------------------------------------------------------------------------------------
