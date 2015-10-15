//
// This file is part of the gda cpp utility library.
// Copyright (c) 2012 Alexander van Renen. All rights reserved.
//
// Purpose - Represents a folder in a filesystem.
// -------------------------------------------------------------------------------------------------
#include "gda/fs/Folder.hpp"
#include "gda/fs/File.hpp"
#include "gda/String.hpp"
#include "gda/fs/FileSystem.hpp"
#include <dirent.h>
#include <cassert>
#include <iostream>
#include <algorithm>
// -------------------------------------------------------------------------------------------------
using namespace std;
// -------------------------------------------------------------------------------------------------
namespace gda {
// -------------------------------------------------------------------------------------------------
Folder::Folder(const string& in_path)
{
   // Make nice path
   path = in_path;
   if (path.size() == 0 || path[0] != '/')
      path = FileSystem::getWorkingPath() + path;
   path = (path.size() != 0 && path[path.size() - 1] != '/') ? path + "/" : path;

   // Load sub folders and files
   if (exists()) {
      DIR* directory = opendir(path.c_str());
      dirent* entry = readdir(directory);
      while (entry) {
         if (entry->d_type == 8)
            files.push_back(File(entry->d_name, *this));
         else
            subDirectories.push_back(entry->d_name);
         entry = readdir(directory);
      }
      closedir(directory);
   }
}
// -------------------------------------------------------------------------------------------------
Folder::~Folder()
{

}
// -------------------------------------------------------------------------------------------------
void Folder::sort()
{
   ::sort(files.begin(), files.end(), [](const File& lhs, const File& rhs) { return lhs.getPathWithEnding() < rhs.getPathWithEnding(); });
   ::sort(subDirectories.begin(), subDirectories.end());
   ::sort(subDirectoriesCache.begin(), subDirectoriesCache.end(), [](const Folder& lhs, const Folder& rhs) { return lhs.getPath() < rhs.getPath(); });
}
// -------------------------------------------------------------------------------------------------
vector<File> Folder::getFiles(const string& ending) const
{
   vector<File> result = files;
   filterVector(result, ending);
   return result;
}
// -------------------------------------------------------------------------------------------------
const vector<File>& Folder::getAllFiles() const
{
   return files;
}
// -------------------------------------------------------------------------------------------------
const vector<Folder>& Folder::getAllSubDirectories() const
{
   if (subDirectoriesCache.size() != subDirectories.size()) {
      subDirectoriesCache.clear();
      for (auto& iter : subDirectories)
         subDirectoriesCache.push_back(Folder(path + iter));
   }
   return subDirectoriesCache;
}
// -------------------------------------------------------------------------------------------------
const string& Folder::getPath() const
{
   return path;
}
// -------------------------------------------------------------------------------------------------
const string Folder::getBaseName() const
{
   string result = path.substr(0, path.size() - 1);
   size_t pos = result.find_last_of("/");
   if (pos == string::npos)
      return result;
   return result.substr(pos + 1, string::npos);
}
// -------------------------------------------------------------------------------------------------
bool Folder::exists() const
{
   DIR* pDir = opendir(path.c_str());
   if (pDir != NULL) {
      (void) closedir(pDir);
      return true;
   }
   return false;
}
// -------------------------------------------------------------------------------------------------
bool Folder::isSpecial(const string& str) const
{
   return str == ".." || str == ".";
}
// -------------------------------------------------------------------------------------------------
void Folder::filterVector(vector<File>& vec, const string& ending)
{
   // removes all strings from "vec" with "ending" at the end
   for (vector<File>::iterator iter = vec.begin(); iter != vec.end(); /*inside*/)
      if (!endsWith(iter->getPathWithEnding(), ending))
         iter = vec.erase(iter);
      else
         iter++;
}
// -------------------------------------------------------------------------------------------------
} // End of namespace gda
// -------------------------------------------------------------------------------------------------
