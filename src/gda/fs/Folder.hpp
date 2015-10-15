//
// This file is part of the gda cpp utility library.
// Copyright (c) 2012 Alexander van Renen. All rights reserved.
//
// Purpose - Represents a folder in a filesystem.
// -------------------------------------------------------------------------------------------------
#pragma once
// -------------------------------------------------------------------------------------------------
#include "FileSystem.hpp"
#include <string>
#include <vector>
// -------------------------------------------------------------------------------------------------
namespace gda {
// -------------------------------------------------------------------------------------------------
class File;
// -------------------------------------------------------------------------------------------------
class Folder {
public:
   /// ctor
   Folder(const std::string& path);
   ~Folder();

   /// Access files and do filtering
   std::vector<File> getFiles(const std::string& ending) const;
   const std::vector<File>& getAllFiles() const;
   void sort();

   /// Access subDirectories
   const std::vector<Folder>& getAllSubDirectories() const;

   /// Always absolute
   const std::string& getPath() const;
   const std::string getBaseName() const;

   // Check if folder exists
   bool exists() const;

   /// Check if this is a special file or directory under current os
   bool isSpecial(const std::string& str) const;
private:
   std::string path;
   std::vector<File> files;
   std::vector<std::string> subDirectories;
   mutable std::vector<Folder> subDirectoriesCache;

   static void filterVector(std::vector<File>& vec, const std::string& ending);
};
// -------------------------------------------------------------------------------------------------
} // End of namespace gda
// -------------------------------------------------------------------------------------------------
