//
// This file is part of the gda cpp utility library.
// Copyright (c) 2012, 2013, 2014 Alexander van Renen. All rights reserved.
//
// Purpose - Wraps a tcp server socket.
// -------------------------------------------------------------------------------------------------
#include "gda/tcp/Server.hpp"
#include "gda/Utility.hpp"
#include "gda/tcp/Connection.hpp"
#include <cassert>
#include <iostream>
#include <sys/time.h>
#include <limits>
#include <unistd.h>
#include <arpa/inet.h>
// -------------------------------------------------------------------------------------------------
using namespace std;
// -------------------------------------------------------------------------------------------------
namespace gda {
// -------------------------------------------------------------------------------------------------
namespace tcp {
// -------------------------------------------------------------------------------------------------
Server::Server(uint32_t port)
: serverPort(port)
, state(true)
{
   // Set up server Socket
   bzero((char *) &inServerAddr, sizeof(inServerAddr));
   serverSocket = socket(AF_INET, SOCK_STREAM, 0);
   if(serverSocket<0) {
      state = false;
      return;
   }

   // Avoid the socket is currently used issue
   setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&inServerAddr, sizeof(inServerAddr));

   // Bind server Socket
   inServerAddr.sin_family = AF_INET;
   inServerAddr.sin_addr.s_addr = INADDR_ANY;
   inServerAddr.sin_port = htons(serverPort);
   int bindSuc = ::bind(serverSocket, (sockaddr *) &inServerAddr, sizeof(inServerAddr));
   if (bindSuc<0) {
      state = false;
      return;
   }

   // Listen
   listen(serverSocket,5);
}
// -------------------------------------------------------------------------------------------------
Server::~Server()
{
   //shutdown
   shutdown(serverSocket,2);
   if(close(serverSocket) != 0) {
      assert(false && "failed closing tcp server");
      throw;
   }
}
// -------------------------------------------------------------------------------------------------
std::unique_ptr<Connection> Server::waitForConnection(int64_t timeInMs)
{
   assert(state);

   // Wait for clients
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
      FD_SET(serverSocket, &rfds);
      int retval = select(serverSocket+1, &rfds, NULL, NULL, &tv);

      if(retval == -1 || !retval) // => no new connection
         return nullptr;
   }

   // Accept client
   auto result = make_unique<Connection>();
   result->addrlen = sizeof(result->addr);
   result->socket = accept(serverSocket, (sockaddr*) &result->addr, &result->addrlen);

   if(result->socket >= 0)
      return result; else
      return nullptr;
}
// -------------------------------------------------------------------------------------------------
bool Server::good() const
{
   return state;
}
// -------------------------------------------------------------------------------------------------
uint32_t Server::getPort() const
{
   return serverPort;
}
// -------------------------------------------------------------------------------------------------
} // End of namespace tcp
// -------------------------------------------------------------------------------------------------
} // End of namespace gda
// -------------------------------------------------------------------------------------------------
