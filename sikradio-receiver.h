//
// Created by albert on 04.06.18.
//

#ifndef SIK_2_SIKRADIO_RECEIVER_H_H
#define SIK_2_SIKRADIO_RECEIVER_H_H

#include <mutex>
#include <netinet/in.h>
#include <vector>

struct Station {
    Station(const std::string &name, const in_addr &address, in_port_t port,
            long last_reply_time, sockaddr_in rexmit_addr) : name(name), address(address), port(port),
                                    last_reply_time(last_reply_time),rexmit_addr(rexmit_addr) {
    }

    std::string name;
    in_addr address;
    in_port_t port;
    long last_reply_time;
    sockaddr_in rexmit_addr;
};

bool operator==(Station a, Station b);

void start_listen(bool changed, bool begin, int previous_station);
extern std::vector<Station> stations;
extern int current_station;
extern int listen_sock;
extern int discover_sock;
extern std::mutex read_packets_lock;
#endif //SIK_2_SIKRADIO_RECEIVER_H_H
