//
// This file is part of the gda cpp utility library.
// Copyright (c) 2012, 2013 Alexander van Renen. All rights reserved.
//
// Purpose - Wrapper for a tcp client socket.
// -------------------------------------------------------------------------------------------------
#pragma once
// -------------------------------------------------------------------------------------------------
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <string>
#include <vector>
#include <sys/types.h>
// -------------------------------------------------------------------------------------------------
namespace gda {
// -------------------------------------------------------------------------------------------------
namespace tcp {
// -------------------------------------------------------------------------------------------------
class NetworkBuffer;
// -------------------------------------------------------------------------------------------------
class Client {
public:
   /// Constructor for TCP sockets
   Client(const std::string& ip, uint32_t port);
   /// Destructor
   ~Client();

   /// Recreate this TCP client
   void restore();

   /// Terminate connection (respects destructor)
   void close();

   /// Read and write from the underlying socket
   bool read(NetworkBuffer& buffer, int64_t timeInMs = -1); // True iff stuff was read

   /// Read and write from the underlying socket
   bool write(const std::string& msg) { return write(msg.c_str(), msg.size()); }
   bool write(const std::vector<char>& msg) { return write(msg.data(), msg.size()); }
   bool write(const char* ptr, uint32_t len);

   /// For checking client
   bool good() const;

private:
   /// Common client data
   int32_t clientSocket;
   const std::string clientIp;
   sockaddr_in inClientAddr;
   const uint32_t clientPort;

   /// Client state
   bool state;
};
// -------------------------------------------------------------------------------------------------
}
// -------------------------------------------------------------------------------------------------
}
// -------------------------------------------------------------------------------------------------
