//
// This file is part of the gda cpp utility library.
// Copyright (c) 2013 Alexander van Renen. All rights reserved.
//
// Purpose - Wraps the socket which is created by the tcp server after a client connects.
// -------------------------------------------------------------------------------------------------
#include "gda/tcp/Connection.hpp"
#include "gda/tcp/NetworkBuffer.hpp"
#include <iostream>
#include <sys/time.h>
#include <limits>
#include <unistd.h>
#include <arpa/inet.h>
#include <cassert>
// -------------------------------------------------------------------------------------------------
using namespace std;
// -------------------------------------------------------------------------------------------------
namespace gda {
// -------------------------------------------------------------------------------------------------
namespace tcp {
// -------------------------------------------------------------------------------------------------
Connection::Connection()
: state(true)
{
}
// -------------------------------------------------------------------------------------------------
Connection::~Connection()
{
   shutdown(socket, 2);
   if(close(socket) != 0) {
      assert(false && "failed closing tcp connection");
      throw;
   }
}
// -------------------------------------------------------------------------------------------------
bool Connection::read(NetworkBuffer& buffer, int64_t timeInMs)
{
   // Check state
   assert(good());
   assert(buffer.getWriteSpace() > 0);

   // Wait if needed
   if(timeInMs >= 0) {
      if(timeInMs/1000 > numeric_limits<int32_t>::max())
         throw "specified time is too large";

      // Set time
      struct timeval tv;
      tv.tv_sec = timeInMs/1000;
      tv.tv_usec = timeInMs%1000*1000;

      // Add the socket of this client to the set on which select will listen
      fd_set rfds;
      FD_ZERO(&rfds);
      FD_SET(socket, &rfds);
      int retval = select(socket+1, &rfds, NULL, NULL, &tv);

      if(retval == -1 || !retval) { // => no input
         return false;
      }
   }

   // Read from client
   int readBytes = ::read(socket, buffer.getWritePointer(), buffer.getWriteSpace());
   if (readBytes <= 0)
      return (state = false);
   buffer.increaseWritePointer(readBytes);
   return true;
}
// -------------------------------------------------------------------------------------------------
bool Connection::write(const std::string& msg)
{
   assert(good());

   // Send
   int writeSuc = ::write(socket, msg.c_str(), msg.size());
   return (state = (writeSuc > 0));
}
// -------------------------------------------------------------------------------------------------
bool Connection::write(const char* ptr, uint32_t len)
{
   assert(good());

   // Send
   int writeSuc = ::write(socket, ptr, len);
   return (state = (writeSuc > 0));
}
// -------------------------------------------------------------------------------------------------
bool Connection::good() const
{
   return state;
}
// -------------------------------------------------------------------------------------------------
uint32_t Connection::getLocalPort() const
{
   return socket;
}
// -------------------------------------------------------------------------------------------------
const string Connection::getRemoteIp() const
{
   string result(addrlen, ' ');
   inet_ntop(AF_INET, &addr.sin_addr, &result[0], addrlen);
   return string(result.c_str()); // need to filter \0 .. stupid c ..
}
// -------------------------------------------------------------------------------------------------
} // End of namespace tcp
// -------------------------------------------------------------------------------------------------
} // End of namespace gda
// -------------------------------------------------------------------------------------------------
