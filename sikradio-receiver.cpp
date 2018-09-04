
#include <boost/program_options/options_description.hpp>
#include <netinet/in.h>
#include <boost/program_options/variables_map.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include <arpa/inet.h>
#include <fcntl.h>
#include <variant>
#include <regex>
#include <cstdlib>
#include <malloc.h>
#include <queue>
#include <fstream>
#include "common.h"
#include "err.h"
#include "RexmitEvent.h"
#include "sikradio-receiver.h"
#include "gui.h"
#include "poll.h"
#include <thread>
#include <mutex>
#include <condition_variable>

#define DEBUG true

const long ident_req_space = 5000;
const long response_wait_time = 20000;
static const int reply_buffer_size = 200;

namespace po = boost::program_options;
int bsize;
long rtime;
in_port_t ctrl_port,ui_port;
in_addr discover_address;
struct sockaddr_in remote_address;
std::string name;

int discover_sock,listen_sock;

std::deque<PackWrapper> read_packets;
std::mutex read_packets_lock;
std::queue<RexmitEvent> rexmit_events;

ssize_t packet_data_size = 0; // 0 means that we haven't read the first packet yet
uint64_t byte0;

std::vector<Station> stations;
int current_station = -1;

std::ofstream log_file;

std::condition_variable cv;
std::mutex play_mut;

int packages=0;

uint64_t max_last_packet = 0;

bool operator==(Station a, Station b) {
    return a.name == b.name &&
           (uint32_t)a.address.s_addr == (uint32_t)b.address.s_addr &&
           a.port == b.port;
}

void parse_args(int argc, const char *argv[]) {
    po::options_description desc{"Options"};
    try
    {
        desc.add_options()
                ("help,h", "Ekran pomocy")
                ("discover_address,d", po::value<std::string>()->default_value("255.255.255.255"), "Address used to detect active stations")
                ("name,n", po::value<std::string>()->default_value("#"), "Name of the desired station. If not given then first detected station will be played")
                ("bsize,b", po::value<int>()->default_value(65536),"Size of buffer in bytes")
                ("ctrl_port,C", po::value<in_port_t>()->default_value(30000 + (student_id % 10000)),"Port used for UDP control packets trasmission")
                ("ui_port,U", po::value<in_port_t>()->default_value(10000 + (student_id % 10000)),"Port used for TCP connection with GUI")
                ("rtime,R", po::value<int>()->default_value(250),"Time between subsequent retransmissions")
                ;

        po::variables_map vm;
        store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);

        if (vm.count("help")) {
            std::cout << desc << '\n';
            exit(0);
        }
        bsize = vm["bsize"].as<int>();
        ctrl_port = vm["ctrl_port"].as<in_port_t>();
        ui_port = vm["ui_port"].as<in_port_t>();
        rtime = vm["rtime"].as<int>();
        name = vm["name"].as<std::string>();


        std::string disc_address_str = vm["discover_address"].as<std::string>();
        if (inet_aton(disc_address_str.c_str(), &discover_address) == 0) {
            throw po::error("wrong IP address");
        }
    }
    catch (const po::error &ex)
    {
        std::cerr << ex.what() << '\n';
    }
    catch (...) {
        std::cerr << desc << '\n';
    }
}
void init_discover() {
    const int TTL_VALUE = 4;
    int optval;

    /* open socket */
    discover_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (discover_sock < 0)
        syserr("socket");

    /* activate broadcast */
    optval = 1;
    if (setsockopt(discover_sock, SOL_SOCKET, SO_BROADCAST, (void*)&optval, sizeof optval) < 0)
        syserr("setsockopt broadcast");

    /* Set TTL for datagrams */
    optval = TTL_VALUE;
    if (setsockopt(discover_sock, IPPROTO_IP, IP_MULTICAST_TTL, (void*)&optval, sizeof optval) < 0)
        syserr("setsockopt multicast ttl");

    remote_address.sin_family = AF_INET;
    remote_address.sin_port = htons(ctrl_port);

    remote_address.sin_addr = discover_address;
}
void init_udp() {
    //maybe we don't need to init anything, and reuse discover_sock instead
}
void init_listen_sock() {
    listen_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sock < 0)
        syserr("socket");
}

void init_gui() {
    initialize_socket(ui_port);
}

void init() {
    init_discover();
    init_udp();
    init_gui();

    add_socket(discover_sock);
    fcntl(discover_sock, F_SETFL, O_NONBLOCK);
}
void drop_multicast_membership(const Station &station) {
    struct ip_mreq ip_mreq;
    ip_mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    ip_mreq.imr_multiaddr = station.address;
    if (setsockopt(listen_sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (void*)&ip_mreq, sizeof ip_mreq) < 0)
        syserr("setsockopt2a");
    remove_socket(listen_sock);
    close(listen_sock);
}
void add_multicast_membership(Station& new_station) {
    init_listen_sock();
    struct ip_mreq ip_mreq;
    ip_mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    ip_mreq.imr_multiaddr = new_station.address;
    u_int val = 1;
    if (setsockopt(listen_sock,SOL_SOCKET,SO_REUSEADDR,&val, sizeof(val)) < 0)
        syserr("setsockopt1b");
    if (setsockopt(listen_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void*)&ip_mreq, sizeof ip_mreq) < 0)
        syserr("setsockopt2c");
    fcntl(listen_sock, F_SETFL, O_NONBLOCK);
    add_socket(listen_sock);

    struct sockaddr_in local_address;
    local_address.sin_family = AF_INET;
    local_address.sin_addr.s_addr = htonl(INADDR_ANY);
    local_address.sin_port = htons(new_station.port);
    if (bind(listen_sock, (struct sockaddr *)&local_address, sizeof local_address) < 0)
        syserr("bind 1");
}
void start_listen(bool changed, bool begin, int previous_station=-1) {

    if (changed)
        max_last_packet = 0;
    for(auto packet : read_packets) {
        packet.free();
    }
    read_packets.clear();
    assert(read_packets.empty());

    rexmit_events = std::queue<RexmitEvent>();
    if (changed) {
        if (!begin) {
            drop_multicast_membership(stations[previous_station]);
        }
        add_multicast_membership(stations[current_station]);
    }
    packet_data_size = 0;
}

void add_station_or_update(const std::string &address, const std::string &port,
                           const std::string &name, sockaddr_in rexmit_addr) {
    if (::name == "#" || name == ::name) {
        in_addr addr_t;
        if (inet_aton(address.c_str(), &addr_t) == 0) {
            syserr("inet_aton");
        }

        Station station(name, addr_t, (in_port_t) stoi(port),
                        get_current_time(),rexmit_addr);
        bool found = false;
        for (auto it = stations.begin(); it != stations.end(); it++) {
            if (station == *it) {
                it->last_reply_time = get_current_time();
                found = true;
                break;
            }
        }
        if (found == false) {
            bool inserted = false;
            if (!stations.empty() && name < stations[0].name) {
                stations.insert(stations.begin(),station);
                current_station++;
                inserted = true;
            } else {
                for (int i = 0; i < (int) stations.size() - 1; i++)
                    if (stations[i].name < name &&
                        name < stations[i + 1].name) {
                        stations.insert(stations.begin() + i + 1, station);
                        if (current_station >= i + 1)
                            current_station++;
                        inserted = true;
                    }
            }
            if (!inserted) {
                stations.push_back(station);
            }
            if (current_station == -1) {
                current_station = 0;
                start_listen(true, true);
            }
            display();
        }
    }
}

// *****************************************************************************

void send_lookup() {
    const char *lookup = "ZERO_SEVEN_COME_IN\n";
    int sflags = 0;
    ssize_t snd_len = sendto(discover_sock, lookup, strlen(lookup), sflags,
                             (struct sockaddr *) &remote_address, sizeof(remote_address));
    if (snd_len != (int)strlen(lookup))
        if (errno != 11)
            syserr("error on sending datagram to client socket");
}

void receive_reply() {
    std::regex regex_reply("BOREWICZ_HERE (([0-9]{1,3}\\.){3}[0-9]{1,3}) ([0-9]{1,5}) (.{1,64})\n");

    char rcv_buffer[reply_buffer_size];

    sockaddr_in srvr_address;
    socklen_t rcva_len = sizeof(struct sockaddr_in);
    int flags = 0; // we do not request anything special
    ssize_t rcv_len = recvfrom(discover_sock, rcv_buffer, reply_buffer_size, flags,
                       (struct sockaddr *) &srvr_address, &rcva_len);
    if (rcv_len < 0) {
        if (errno == 11 || errno == 22)
            return;
        syserr("read receive reply");
    }
    rcv_buffer[rcv_len] = 0;
    if (std::regex_match ( rcv_buffer, regex_reply) ) {
        std::cmatch cm;

        std::regex_match ( rcv_buffer, cm, regex_reply);

        std::string address = cm[1].str();
        std::string port = cm[3].str();
        std::string name = cm[4].str();

        add_station_or_update(address, port, name, srvr_address);
    }
}

void push_packet_back(const PackWrapper& packet) {
    read_packets.push_back(packet);
    while ((int)read_packets.size() * packet_data_size > bsize) {

        read_packets.front().free();
        read_packets.pop_front();
    }
    const uint64_t last_byte =
            read_packets.back().first_byte_num + packet_data_size - 1;
    if (last_byte >= byte0 + (bsize * 3) / 4 && last_byte <= byte0 + (uint64_t)bsize) {
        std::unique_lock<std::mutex> lck(play_mut);
        cv.notify_one();
    }
}

void fill_with_empty_packets(const Pack *pack_buffer) {
    int empty_packets = 0;
    while (!read_packets.empty() &&
           pack_buffer->first_byte_num - packet_data_size >
           read_packets.back().first_byte_num) {
        uint64_t first_byte_num = read_packets.back().first_byte_num;
        const uint64_t new_packet_number =
                first_byte_num + packet_data_size;
        push_packet_back(PackWrapper(new_packet_number));
        rexmit_events.push(RexmitEvent(get_current_time() + rtime,
                                       new_packet_number));
        empty_packets++;
    }

}

void listen() {
    static uint64_t session_id;
    static size_t read_limit = bsize+100;
    static Pack *pack_buffer = (Pack*)malloc(read_limit);

    read_packets_lock.lock();
    if (!stations.empty()) {

        ssize_t read_len = read(listen_sock,pack_buffer,read_limit);

        if (read_len < 0 && errno != 11)
            syserr("read listen");
        if (read_len >= 2*(int)sizeof(uint64_t)) {

            pack_buffer->first_byte_num = __bswap_64(pack_buffer->first_byte_num);
            pack_buffer->session_id = __bswap_64(pack_buffer->session_id);

            if (packet_data_size == 0) {
                packet_data_size = read_len-2*sizeof(uint64_t);
                byte0=pack_buffer->first_byte_num;
                session_id = pack_buffer->session_id;
            }
            const PackWrapper &packWrapper = PackWrapper(pack_buffer, packet_data_size);

            max_last_packet = max(max_last_packet, packWrapper.first_byte_num);
            if (!read_packets.empty() && packWrapper.first_byte_num < read_packets.front().first_byte_num) {
                read_packets_lock.unlock();
                return;
            }
            if (packWrapper.session_id > session_id) {
                start_listen(false,false);
                read_packets_lock.unlock();
                return;

            }
            int index = get_pack_index_by_number(pack_buffer->first_byte_num,read_packets,packet_data_size);

            if (0 <= index && index < (int)read_packets.size()) {
                assert(read_packets[index].first_byte_num == packWrapper.first_byte_num);
                read_packets[index] = packWrapper;
            } else {
                fill_with_empty_packets(pack_buffer);
                packages++;
                push_packet_back(packWrapper);
            }
        }
    }
    read_packets_lock.unlock();
}

void play() {

    read_packets_lock.lock();
    if (!read_packets.empty() &&
            read_packets.back().first_byte_num+packet_data_size-1 >= byte0 + (bsize * 3) / 4) {

        PackWrapper &first_packet = read_packets.front();
        if (first_packet.empty) {
            start_listen(false,false);
            read_packets_lock.unlock();
            return;
        }
        if (write(1,read_packets.front().audio_data,packet_data_size) != packet_data_size) {
            syserr("stdout write");
        } else {
            read_packets.front().free();
            read_packets.pop_front();
        }
        if (read_packets.empty()) {
            start_listen(false,false);
        }
        read_packets_lock.unlock();
    } else {
        read_packets_lock.unlock();
        std::unique_lock<std::mutex> lck(play_mut);
        cv.wait(lck);
    }
}
void clean_stations() {

    read_packets_lock.lock();
    bool any_change = false;
    for (int i = 0; i < (int) stations.size(); i++) {
        if (get_current_time() - stations[i].last_reply_time >
            response_wait_time) {
            if (current_station > i)
                current_station--;
            else if (current_station == i) {
                drop_multicast_membership(stations[i]);
                current_station = -1;
            }
            stations.erase(stations.begin() + i);
            i = -1;
            any_change = true;
        }
    }
    if (current_station == -1 && !stations.empty()) {
        current_station = 0;
        start_listen(true,true);
    }
    if (any_change) {
        display();
    }
    read_packets_lock.unlock();
}
long clean_stations_get_timeout() {
    long min_sleep_time = response_wait_time;
    for (int i = 0; i < (int) stations.size(); i++) {
        min_sleep_time = min(min_sleep_time,
                             response_wait_time - (get_current_time() -
                                                   stations[i].last_reply_time));
    }
    min_sleep_time = max(min_sleep_time, (long)0);
    return min_sleep_time;
}
void send_rexmit_request() {

    while (!rexmit_events.empty() && get_current_time() >= rexmit_events.front().time) {
        const RexmitEvent event = rexmit_events.front();
        rexmit_events.pop();
        int index = get_pack_index_by_number(event.pack_number,
                                             read_packets,
                                             packet_data_size);
        if (0 <= index && index < (int)read_packets.size()) {
            if (read_packets[index].empty == true) {
                rexmit_events.push(RexmitEvent(get_current_time() + rtime,
                                               event.pack_number));
                std::string rexmit2 = "LOUDER_PLEASE " + std::to_string(event.pack_number) + "\n";
                const char *rexmit = rexmit2.c_str();
                int sflags = 0;

                int sock = socket(PF_INET, SOCK_DGRAM, 0);
                if (sock < 0)
                    syserr("socket");

                const sockaddr_in &sender_address = stations[current_station].rexmit_addr;
                ssize_t snd_len = sendto(sock, rexmit, strlen(rexmit), sflags,
                                         (struct sockaddr *) &sender_address, sizeof(sender_address));
                if (snd_len != (int)strlen(rexmit))
                    if (errno != 11)
                        syserr("error on sending datagram to client socket");

                if (close(sock))
                    syserr("close sock");
            }
        }
    }

}


void lookup_thread_fun() {
    while(true) {
        send_lookup();
        std::this_thread::sleep_for(std::chrono::milliseconds(ident_req_space));
    }
}
void receive_reply_thread_fun() {
    while(true) {
        receive_reply();
    }
}
void play_thread_fun() {
    while(true) {
        play();
    }
}
void send_rexmit_request_thread_fun() {
    while(true) {
        long wait_time = rtime;
        read_packets_lock.lock();
        send_rexmit_request();
        if (!rexmit_events.empty()) {
            wait_time = min(rtime,rexmit_events.front().time-get_current_time());
            wait_time = max(wait_time,(long)0);
        }
        read_packets_lock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));
    }
}
int main(int argc, const char *argv[]) {
    parse_args(argc,argv);
    init();
    std::thread lookup_thread{lookup_thread_fun}; //Check existence of radio stations
    lookup_thread.detach();

    //In a regular periods ask radio station for retransmitting lost packages
    std::thread send_rexmit_request_thread{send_rexmit_request_thread_fun};
    send_rexmit_request_thread.detach();

    // Write output from read_packets to stdout
    std::thread play_thread{play_thread_fun};
    play_thread.detach();

    while(true) {
        if (clean_stations_get_timeout() == 0)
            clean_stations(); //Every 20 seconds clean not-responding stations
        long min_sleep_time = clean_stations_get_timeout();
        //Read diffrent types of input: UI, packages from station, information about new stations
        poll(&handle_client_key,&handle_client_connect,&listen,&receive_reply,min_sleep_time);
    }
}

