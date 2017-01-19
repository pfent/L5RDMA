int connect(int fd, const sockaddr* address, socklen_t length) {
    int use_tssx;

    if (real_connect(fd, address, length) == ERROR) {
        return ERROR;
    }

    if ((use_tssx = check_tssx_usage(fd, CLIENT)) == ERROR) {
        print_error("Could not check if socket uses TSSX");
        return ERROR;
    } else if (!use_tssx) {
        return SUCCESS;
    }

    return _setup_tssx(fd);
}

ssize_t read(int fd, void* destination, size_t requested_bytes) {
    // clang-format off
    return connection_read(
            fd,
            destination,
            requested_bytes,
            SERVER_BUFFER
    );
    // clang-format on
}

ssize_t write(int fd, const void* source, size_t requested_bytes) {
    // clang-format off
    return connection_write(
            fd,
            source,
            requested_bytes,
            CLIENT_BUFFER
    );
    // clang-format on
}