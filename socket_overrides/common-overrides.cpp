#include <errno.h>
#include <cstdarg>
#include <fcntl.h>
#include "common-overrides.h"

/******************** COMMON OVERRIDES ********************/

int fcntl(int fd, int command, ...) {
    va_list argument;

    // Takes the argument pointer and the last positional argument
    // Makes the argument pointer point to the first optional argument
    va_start(argument, command);

    if (command == F_SETFL || command == F_SETFD) {
        return fcntl_set(fd, command, va_arg(argument, int));
    } else if (command == F_GETFL || command == F_GETFD) {
        return fcntl_get(fd, command);
    } else {
        // Sorry, don't know what to do for other commands :(
        // If necessary: handle all cases of arguments ...
        return -1;
    }
}

pid_t fork() {
    bridge_add_user(&bridge);
    return real_fork();
}

