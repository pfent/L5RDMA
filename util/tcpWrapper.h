#ifndef TCPWRAPPER_H
#define TCPWRAPPER_H

#include <cstddef>

struct sockaddr_in;

int tcp_socket();

void tcp_listen(int sock);

void tcp_connect(int sock, const sockaddr_in &addr);

void tcp_write(int sock, const void *buffer, std::size_t size);

template<typename T>
void tcp_write(int sock, const T &buffer) {
    return tcp_write(sock, &buffer, sizeof(T));
}

void tcp_read(int sock, void *buffer, std::size_t size);

template<typename T>
void tcp_read(int sock, T &buffer) {
    return tcp_read(sock, &buffer, sizeof(T));
}

void tcp_bind(int sock, const sockaddr_in &addr);

int tcp_accept(int sock, sockaddr_in &inAddr);

void tcp_setBlocking(int sock);

void tcp_close(int sock);

#endif //TCPWRAPPER_H
