#include <zconf.h>
#include <poll.h>
#include <netinet/in.h>
#include <cstdio>
#include <vector>
#include <assert.h>
#include "err.h"
#include "sikradio-receiver.h"

struct pollfd client[_POSIX_OPEN_MAX];
struct sockaddr_in server;
size_t length;
ssize_t rval;
int msgsock, activeClients, i, ret;

void init(int server_sock) {
    for (i = 0; i < _POSIX_OPEN_MAX; ++i) {
        client[i].fd = -1;
        client[i].events = POLLIN;
        client[i].revents = 0;
    }
    activeClients = 0;

    client[0].fd = server_sock;

}

void poll(ssize_t (*callback)(int),void (*callback_new_client)(int),void (*callback_listen)(),
          void (*callback_receive_reply)() ,long timeout) {
    for (i = 0; i < _POSIX_OPEN_MAX; ++i)
        client[i].revents = 0;

    /* Czekamy przez 5000 ms */
    ret = poll(client, _POSIX_OPEN_MAX, timeout);
    if (ret < 0)
        syserr("poll");
    else if (ret > 0) {
        if ((client[0].revents & POLLIN)) {
            msgsock =
                    accept(client[0].fd, (struct sockaddr *) 0,
                           (socklen_t *) 0);
            if (msgsock == -1)
                syserr("accept");
            else {
                for (i = 1; i < _POSIX_OPEN_MAX; ++i) {
                    if (client[i].fd == -1) {
                        client[i].fd = msgsock;
                        activeClients += 1;
                        callback_new_client(msgsock);
                        break;
                    }
                }
                if (i >= _POSIX_OPEN_MAX) {
                    fprintf(stderr, "Too many clients\n");
                    if (close(msgsock) < 0)
                        syserr("close");
                }
            }
        }
        for (i = 1; i < _POSIX_OPEN_MAX; ++i) {
            if (client[i].fd != -1
                && (client[i].revents & (POLLIN | POLLERR))) {
                if (client[i].fd == listen_sock) {
                    callback_listen();
                    rval = 1;
                } else if (client[i].fd == discover_sock) {
                    callback_receive_reply();
                    rval = 1;
                }
                else rval = callback(client[i].fd);
                if (rval < 0) {
                    syserr("Reading stream message");
                    if (close(client[i].fd) < 0)
                        syserr("close");
                    client[i].fd = -1;
                    activeClients -= 1;
                }
                else if (rval == 0) {
                    if (close(client[i].fd) < 0)
                        syserr("close");
                    client[i].fd = -1;
                    activeClients -= 1;
                }
            }
        }
    }
}
void write_to_all(const void *buf, size_t n) {
    for(int i=1; i < _POSIX_OPEN_MAX; ++i) {
        if (client[i].fd != -1) {
            write(client[i].fd,buf,n);
        }
    }
}
void add_socket(int sock) {
    for(int i=1;i< _POSIX_OPEN_MAX; ++i) {
        if (client[i].fd == -1) {
            client[i].fd = sock;
            activeClients += 1;
            break;
        }
    }
}
void remove_socket(int sock) {
    bool found=false;
    for(int i=1;i< _POSIX_OPEN_MAX; ++i) {
        if (client[i].fd == sock) {
            client[i].fd = -1;
            activeClients -= 1;
            found=true;
            break;
        }
    }
    assert(found);
}
