//
// This file is part of the gda cpp utility library.
// Copyright (c) 2013 Alexander van Renen. All rights reserved.
//
// Purpose - Wraps the socket which is created by the tcp server after a client connects.
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
class NetworkBuffer;
// -------------------------------------------------------------------------------------------------
/// Stores data and offers functionality for a single TCP connection.
class Connection {
public:
   // Create & destroy
   Connection();
   ~Connection();

   /// Read and write from the underlying socket
   bool read(NetworkBuffer& buffer, int64_t timeInMs = -1); // True iff stuff was read

   /// Read and write from the underlying socket
   bool write(const std::string& msg);
   bool write(const char* ptr, uint32_t len);

   /// Information about the Connection status
   bool good() const;
   uint32_t getLocalPort() const;
   const std::string getRemoteIp() const;

private:
   /// Data fields representing the internal state
   int32_t socket;
   sockaddr_in addr;
   socklen_t addrlen;
   bool state;

   /// Declare the server a friend because it initializes the connection object
   friend class Server;
};
// -------------------------------------------------------------------------------------------------
} // End of namespace tcp
// -------------------------------------------------------------------------------------------------
} // End of namespace gda
// -------------------------------------------------------------------------------------------------
