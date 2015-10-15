//
// This file is part of the gda cpp utility library.
// Copyright (c) 2014 Alexander van Renen. All rights reserved.
//
// Purpose - Represents a file in a filesystem.
// -------------------------------------------------------------------------------------------------
#include "gda/fs/Folder.hpp"
#include "gda/fs/File.hpp"
#include "gda/String.hpp"
#include "gda/MemoryRef.hpp"
#include "gda/fs/FileSystem.hpp"
#include <dirent.h>
#include <cassert>
#include <fstream>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstdlib>
#include <string>
#include <limits>
// -------------------------------------------------------------------------------------------------
using namespace std;
// -------------------------------------------------------------------------------------------------
namespace gda {
// -------------------------------------------------------------------------------------------------
File::File(const string& path, const Folder& folder)
: path(path)
{
   assert(path.size() > 0);

   if (this->path[0] != '/')
      this->path = folder.getPath() + this->path;
}
// -------------------------------------------------------------------------------------------------
const string& File::getPathWithEnding() const
{
   return path;
}
// -------------------------------------------------------------------------------------------------
const string File::getPathNoEnding() const
{
   size_t pos = path.find_last_of('.');
   if (pos == string::npos)
      return path;
   return path.substr(0, pos);
}
// -------------------------------------------------------------------------------------------------
const string File::getBaseNameWithEnding() const
{
   size_t pos = path.find_last_of('/');
   if (pos == string::npos)
      return path;
   return path.substr(pos + 1, string::npos);
}
// -------------------------------------------------------------------------------------------------
const string File::getBaseNameNoEnding() const
{
   string path = getBaseNameWithEnding();
   size_t pos = path.find_last_of('.');
   if (pos == string::npos)
      return path;
   return path.substr(0, pos);
}
// -------------------------------------------------------------------------------------------------
bool File::exists() const
{
   fstream file(path);
   return file.is_open() && file.good();
}
// -------------------------------------------------------------------------------------------------
uint32_t File::getFileLength() const throw(InvalidFileSystemRequest)
{
   int fileFD = open(path.c_str(), O_RDWR);
   if (fcntl(fileFD, F_GETFL) == -1)
      throw InvalidFileSystemRequest("Can not open '" + path + "'");
   struct stat st;
   fstat(fileFD, &st);
   close(fileFD);
   if (st.st_size > numeric_limits<int32_t>::max())
      throw InvalidFileSystemRequest("File too big for library '" + path + "'");
   return static_cast<uint32_t>(st.st_size);
}
// -------------------------------------------------------------------------------------------------
string File::loadFileToString() const throw(InvalidFileSystemRequest)
{
   uint32_t length = getFileLength();
   string data(length, 'a');
   ifstream in(path);
   in.read(&data[0], length);
   if (!in.good() || !in.is_open())
      throw InvalidFileSystemRequest("Can not read '" + path + "'");
   return move(data);
}
// -------------------------------------------------------------------------------------------------
vector<char> File::loadFileToVector() const throw(InvalidFileSystemRequest)
{
   uint32_t length = getFileLength();
   vector<char> data(length);
   ifstream in(path);
   in.read(&data[0], length);
   if (!in.good() || !in.is_open())
      throw InvalidFileSystemRequest("Can not read '" + path + "'");
   return data;
}
// -------------------------------------------------------------------------------------------------
void File::writeMemoryToFile(const MemoryRef& mem) const throw(InvalidFileSystemRequest)
{
   if (mem.size() > numeric_limits<uint32_t>::max())
      throw InvalidFileSystemRequest("Memory to big, can not write to '" + path + "'");
   ofstream out(path);
   if (!out.good() || !out.is_open())
      throw InvalidFileSystemRequest("Can not write to '" + path + "'");
   out.write(mem.data(), static_cast<uint32_t>(mem.size()));
}
// -------------------------------------------------------------------------------------------------
void File::writeStringToFile(const string& mem) const throw(InvalidFileSystemRequest)
{
   if (mem.size() > numeric_limits<uint32_t>::max())
      throw InvalidFileSystemRequest("Memory to big, can not write to '" + path + "'");
   ofstream out(path);
   if (!out.good() || !out.is_open())
      throw InvalidFileSystemRequest("Can not write to '" + path + "'");
   out.write(mem.data(), static_cast<uint32_t>(mem.size()));
}
// -------------------------------------------------------------------------------------------------
void File::writeVectorToFile(const vector<char>& mem) const throw(InvalidFileSystemRequest)
{
   if (mem.size() > numeric_limits<uint32_t>::max())
      throw InvalidFileSystemRequest("Memory to big, can not write to '" + path + "'");
   ofstream out(path);
   if (!out.good() || !out.is_open())
      throw InvalidFileSystemRequest("Can not write to '" + path + "'");
   out.write(mem.data(), static_cast<uint32_t>(mem.size()));
}
// -------------------------------------------------------------------------------------------------
} // End of namespace gda
// -------------------------------------------------------------------------------------------------
