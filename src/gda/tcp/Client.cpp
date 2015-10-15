//
// This file is part of the gda cpp utility library.
// Copyright (c) 2012, 2013 Alexander van Renen. All rights reserved.
//
// Purpose - Wrapper for a tcp client socket.
// -------------------------------------------------------------------------------------------------
#include "gda/tcp/Client.hpp"
#include "gda/tcp/NetworkBuffer.hpp"
#include <limits>
#include <netdb.h>
#include <iostream>
#include <unistd.h>
#include <cassert>
// -------------------------------------------------------------------------------------------------
using namespace std;
// -------------------------------------------------------------------------------------------------
namespace gda {
// -------------------------------------------------------------------------------------------------
namespace tcp {
// -------------------------------------------------------------------------------------------------
Client::Client(const string& ip, uint32_t port)
: clientSocket(-1)
, clientIp(ip)
, clientPort(port)
, state(true)
{
   restore();
}
// -------------------------------------------------------------------------------------------------
Client::~Client()
{
   close();
}
// -------------------------------------------------------------------------------------------------
void Client::restore()
{
   // Create socket
   clientSocket = socket(AF_INET, SOCK_STREAM, 0);
   if (clientSocket < 0) {
      state = false;
      return;
   }

   // Resolve host name
   hostent* server = gethostbyname(clientIp.c_str());
   if (server == NULL) {
      state = false;
      return;
   }

   // Connect socket
   bzero((char*) &inClientAddr, sizeof(inClientAddr));
   inClientAddr.sin_family = AF_INET;
   bcopy((char*) server->h_addr, (char*) &inClientAddr.sin_addr.s_addr, server->h_length);
   inClientAddr.sin_port = htons(clientPort);
   int connectSuc = connect(clientSocket, (struct sockaddr*) &inClientAddr, sizeof(inClientAddr));
   if (connectSuc < 0) {
      state = false;
      return;
   }
}
// -------------------------------------------------------------------------------------------------
void Client::close()
{
   if (clientSocket != -1) if (::close(clientSocket) != 0) {
      assert(false && "failed closing tcp client");
      throw;
   }
   clientSocket = -1;
}
// -------------------------------------------------------------------------------------------------
bool Client::read(NetworkBuffer& buffer, int64_t timeInMs)
{
   // Check state
   assert(good());
   assert(buffer.getWriteSpace() > 0);

   // Wait if needed
   if (timeInMs >= 0) {
      if (timeInMs / 1000 > numeric_limits<int32_t>::max())
         throw "specified time is too large";

      // Set time
      struct timeval tv;
      tv.tv_sec = timeInMs / 1000;
      tv.tv_usec = timeInMs % 1000 * 1000;

      // Add the socket of this client to the set on which select will listen
      fd_set rfds;
      FD_ZERO(&rfds);
      FD_SET(clientSocket, &rfds);
      int retval = select(clientSocket + 1, &rfds, NULL, NULL, &tv);

      if (retval == -1 || !retval) { // => no input
         return false;
      }
   }

   // Read from client
   int readBytes = ::read(clientSocket, buffer.getWritePointer(), buffer.getWriteSpace());
   if (readBytes <= 0)
      return (state = false);
   buffer.increaseWritePointer(readBytes);
   return true;
}
// -------------------------------------------------------------------------------------------------
bool Client::write(const char* ptr, uint32_t len)
{
   assert(good());

   // Send
   int writeSuc = ::write(clientSocket, ptr, len);
   return (state = (writeSuc > 0));
}
// -------------------------------------------------------------------------------------------------
bool Client::good() const
{
   return state;
}
// -------------------------------------------------------------------------------------------------
} // End of namespace tcp
// -------------------------------------------------------------------------------------------------
} // End of namespace gda
// -------------------------------------------------------------------------------------------------
