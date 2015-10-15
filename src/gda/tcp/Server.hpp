//
// This file is part of the gda cpp utility library.
// Copyright (c) 2012, 2013, 2014 Alexander van Renen. All rights reserved.
//
// Purpose - Wraps a tcp server socket.
// -------------------------------------------------------------------------------------------------
#pragma once
// -------------------------------------------------------------------------------------------------
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <memory>
// -------------------------------------------------------------------------------------------------
namespace gda {
// -------------------------------------------------------------------------------------------------
namespace tcp {
// -------------------------------------------------------------------------------------------------
class Connection;
// -------------------------------------------------------------------------------------------------
/// Wrapper class for TCP sockets
class Server {
public:
   // Create & destroy
   Server(uint32_t port);
   ~Server();

   /// Waits for a new connection and place the info about it in the argument
   /// The time to wait is specified in milliseconds. (-1 == forever, 0 == don't wait)
   std::unique_ptr<Connection> waitForConnection(int64_t timeInMs = -1);

   /// Check state of the server
   bool good() const;
   uint32_t getPort() const;

private:
   /// TCP server data
   int32_t serverSocket;
   uint32_t serverPort;
   sockaddr_in inServerAddr;

   /// Server state
   bool state;
};
// -------------------------------------------------------------------------------------------------
}
// -------------------------------------------------------------------------------------------------
}
// -------------------------------------------------------------------------------------------------
