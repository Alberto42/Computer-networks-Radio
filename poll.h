//
// Created by albert on 05.06.18.
//

#ifndef SIK_2_POLL_H
#define SIK_2_POLL_H

#include <cstdlib>

void init(int server_sock);

void add_socket(int sock);

void remove_socket(int sock);

void write_to_all(const void *__buf, size_t __n);

void poll(ssize_t (*callback)(int),void (*callback_new_client)(int),void (*callback_listen)(),
          void (*callback_receive_reply)() ,long timeout);

#endif //SIK_2_POLL_H
