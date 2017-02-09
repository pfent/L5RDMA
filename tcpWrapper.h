#ifndef TCPWRAPPER_H
#define TCPWRAPPER_H

struct sockaddr_in;

int tcp_socket();

void tcp_listen(int sock);

void tcp_connect(int sock, sockaddr_in &addr);

void tcp_write(int sock, void *buffer, std::size_t size);

void tcp_read(int sock, void *buffer, std::size_t size);

void tcp_bind(int sock, sockaddr_in &addr);

int tcp_accept(int sock, sockaddr_in &inAddr);

void tcp_setBlocking(int sock);

void tcp_close(int sock);

#endif //TCPWRAPPER_H
