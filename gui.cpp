#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <cstdlib>
#include <ctype.h>
#include <string>
#include <vector>
#include <stdexcept>
#include <deque>
#include <fcntl.h>
#include <mutex>
#include <vector>

#include "poll.h"
#include "err.h"
#include "sikradio-receiver.h"
#include "gui.h"

using namespace std;

#define BUFFER_SIZE   2000
#define QUEUE_LENGTH     5
#define DEBUG false

int server_sock;
in_port_t port;

deque<u_int8_t> data_received;

void handle_client_connect(int msg_sock) {
    negotiate(msg_sock);
    display();
}

vector<u_int8_t> up = {0x1b, 0x5b, 0x41};

vector<u_int8_t> down = {0x1b, 0x5b, 0x42};
vector<u_int8_t> enter = {0xd, 0};

bool is_prefix(deque<u_int8_t> word, vector<u_int8_t> prefix) {
    if (prefix.size() > word.size())
        return false;
    for (unsigned i = 0; i < prefix.size(); i++) {
        if (prefix[i] != word[i])
            return false;
    }
    return true;
}

void onKeyPressed(deque<u_int8_t> &key) {

    unsigned long previous_size = 100000;
    while (key.size() < previous_size) {
        previous_size = key.size();
        if (is_prefix(key, up)) {
            key.erase(key.begin(), key.begin() + up.size());
            read_packets_lock.lock();
            int previous_station = current_station;
            current_station = max(0,current_station-1);
            start_listen(true,false,previous_station);
            read_packets_lock.unlock();
            display();
        } else if (is_prefix(key, down)) {
            key.erase(key.begin(), key.begin() + down.size());
            read_packets_lock.lock();
            int previous_station = current_station;
            current_station = min((int)stations.size()-1,current_station+1);
            start_listen(true,false,previous_station);
            read_packets_lock.unlock();
            display();
        }  else if (key.size() >= 3) {
            key.clear();
        }
    }
}

ssize_t handle_client_key(int msg_sock) {
    u_int8_t buffer[BUFFER_SIZE];
    ssize_t len;

    len = read(msg_sock, buffer, sizeof(buffer));
    if (len < 0) {
        if (errno == 11)
            return len;
        syserr("reading from client socket");
    }
    else {
        buffer[len] = 0;

        for (int i = 0; i < len; i++)
            data_received.push_back(buffer[i]);

        onKeyPressed(data_received);
    }
    return len;
}

void display() {
    string buffer;
    const char *const move_cursor_to_left_upper_corner = "\033[2J\033[0;0H";
    buffer += move_cursor_to_left_upper_corner;

    std::string header = std::string(
            "\r------------------------------------------------------------------------\n") +
                         "\r  SIK Radio\n" +
                         "\r------------------------------------------------------------------------\n";
    buffer += header;
    for (int i = 0; i < (int) stations.size(); i++) {
        buffer += "\r";
        if (current_station == i)
            buffer += "  > ";
        else
            buffer += "    ";
        buffer += stations[i].name + "\n";
    }
    std::string footer = "\r------------------------------------------------------------------------\n";
    buffer += footer;
    const char *c_buffer = buffer.c_str();
    write_to_all(c_buffer, strlen(c_buffer));
}

void negotiate(int msg_sock) {
    u_int8_t negotiation_message[] = {255, 251, 1,
                                      255, 251, 3};
    const int length = 6;
    ssize_t snd_len = write(msg_sock, negotiation_message, length);
    if (snd_len != length)
        syserr("writing to client socket");
}

void initialize_socket(in_port_t port) {
    ::port = port;

    struct sockaddr_in server_address;

    server_sock = socket(PF_INET, SOCK_STREAM, 0); // creating IPv4 TCP socket
    if (server_sock < 0)
        syserr("socket");
    // after socket() call; we should close(sock) on any execution path;
    // since all execution paths exit immediately, sock would be closed when program terminates

    server_address.sin_family = AF_INET; // IPv4
    server_address.sin_addr.s_addr = htonl(
            INADDR_ANY); // listening on all interfaces
    server_address.sin_port = htons(port); // listening on port PORT_NUM

    // bind the socket to a concrete address
    if (bind(server_sock, (struct sockaddr *) &server_address,
             sizeof(server_address)) < 0)
        syserr("bind gui");

    // switch to listening (passive open)
    if (listen(server_sock, QUEUE_LENGTH) < 0)
        syserr("listen");

    fcntl(server_sock, F_SETFL, O_NONBLOCK);

    init(server_sock);

}
