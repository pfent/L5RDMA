//
// This file is part of the gda cpp utility library.
// Copyright (c) 2014 Alexander van Renen. All rights reserved.
//
// Purpose - Helper functions for a file system.
// -------------------------------------------------------------------------------------------------
#include "gda/fs/FileSystem.hpp"
#include <unistd.h>
#include <algorithm>
// -------------------------------------------------------------------------------------------------
using namespace std;
// -------------------------------------------------------------------------------------------------
namespace gda {
// -------------------------------------------------------------------------------------------------
namespace FileSystem {
// -------------------------------------------------------------------------------------------------
string getWorkingPath()
{
   char cCurrentPath[FILENAME_MAX];
   if(getcwd(cCurrentPath, sizeof(cCurrentPath)) == nullptr)
      throw "can not get working directory";
   return string(cCurrentPath) + "/";
}
// -------------------------------------------------------------------------------------------------
bool hasNewEvents(uint32_t fileDescriptor, int64_t waitTime)
{
   //set time
   timeval tv;
   if(waitTime/1000 > numeric_limits<int32_t>::max())
      throw "specified time is too large";
   tv.tv_sec = waitTime/1000;
   tv.tv_usec = (waitTime%1000)*1000;

   //add the socket of this client to the set on which select will listen
   fd_set rfds;
   FD_ZERO(&rfds);
   FD_SET(fileDescriptor, &rfds);
   int retval = select(fileDescriptor+1, &rfds, NULL, NULL, &tv);

   return !(retval == -1 || !retval);
}
// -------------------------------------------------------------------------------------------------
bool hasNewEvents(vector<uint32_t> fileDescriptors, int64_t waitTime)
{
   //set time
   timeval tv;
   if(waitTime/1000 > numeric_limits<int32_t>::max())
      throw "specified time is too large";
   tv.tv_sec = waitTime/1000;
   tv.tv_usec = (waitTime%1000)*1000;

   //add the socket of this client to the set on which select will listen
   fd_set rfds;
   FD_ZERO(&rfds);
   for(uint32_t i=0; i<fileDescriptors.size(); i++)
      FD_SET(fileDescriptors[i], &rfds);
   int retval = select(fileDescriptors[fileDescriptors.size()-1]+1, &rfds, NULL, NULL, &tv);

   return !(retval == -1 || !retval);
}
// -------------------------------------------------------------------------------------------------
} // End of namespace FileSystem
// -------------------------------------------------------------------------------------------------
} // End of namespace gda
// -------------------------------------------------------------------------------------------------
