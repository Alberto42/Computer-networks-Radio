#include <boost/program_options.hpp>
#include <iostream>
#include <string>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <queue>
#include <fcntl.h>
#include <regex>
#include <chrono>
#include <fstream>
#include <thread>
#include <mutex>
#include <set>

#include "err.h"
#include "common.h"

#define DEBUG true
namespace po = boost::program_options;

std::string name,mcast_address_str;
int mcast_sock, discover_sock, direct_sock;

in_addr mcast_address,discover_address;
int psize,fsize,rtime;
in_port_t ctrl_port,data_port;

ssize_t packet_data_size;
std::deque<PackWrapper> last_packs;
std::mutex last_packs_lock;
std::set<uint64_t>rexmit_packs;
std::mutex rexmit_lock;
long last_rexmit_time;

int sent_packages=0;

std::ofstream log_file;
uint64_t next_byte = 0;
time_t seconds_past_epoch;

void parse_args(int argc, const char *argv[]) {
    po::options_description desc{"Options"};
    try
    {
        desc.add_options()
                ("name,n",po::value<std::string>()->default_value("Nienazwany Nadajnik"), "Radio station name")
                ("help,h", "Ekran pomocy")
                ("address,a", po::value<std::string>(), "Multicast address")
                ("psize,p", po::value<int>()->default_value(512),"Package size in bytes")
                ("fsize,f", po::value<int>()->default_value(128*1024),"FIFO queue size in bytes")
                ("ctrl_port,C", po::value<in_port_t>()->default_value(30000 + (student_id % 10000)),"Port used for UDP control packets trasmission")
                ("data_port,P", po::value<in_port_t>()->default_value(20000 + (student_id % 10000)),"Port used for UDP data trasmission")
                ("rtime,R", po::value<int>()->default_value(250),"Time between subsequent retransmissions")
                ;

        po::variables_map vm;
        store(parse_command_line(argc, argv, desc), vm);
        po::notify(vm);

        if (vm.count("help")) {
            std::cout << desc << '\n';
            exit(0);
        }
        name = vm["name"].as<std::string>();
        psize = vm["psize"].as<int>();
        fsize = vm["fsize"].as<int>();
        ctrl_port = vm["ctrl_port"].as<in_port_t>();
        data_port = vm["data_port"].as<in_port_t>();
        rtime = vm["rtime"].as<int>();

        std::string address_str = vm["address"].as<std::string>();
        mcast_address_str = address_str;
        if (inet_aton(address_str.c_str(), &mcast_address) == 0) {
            throw po::error("wrong IP address");
        }

    }
    catch (const po::error &ex)
    {
        std::cerr << ex.what() << '\n';
        exit(1);
    }
    catch (...) {
        std::cerr << desc << '\n';
        exit(1);
    }
}

void init_mcast() {
    const int TTL_VALUE = 4;
    int optval;
    struct sockaddr_in remote_address;

    /* open socket */
    mcast_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (mcast_sock < 0)
        syserr("socket");

    /* activate broadcast */
    optval = 1;
    if (setsockopt(mcast_sock, SOL_SOCKET, SO_BROADCAST, (void*)&optval, sizeof optval) < 0)
        syserr("setsockopt broadcast");

    /* Set TTL for datagrams */
    optval = TTL_VALUE;
    if (setsockopt(mcast_sock, IPPROTO_IP, IP_MULTICAST_TTL, (void*)&optval, sizeof optval) < 0)
        syserr("setsockopt multicast ttl");

    remote_address.sin_family = AF_INET;
    remote_address.sin_port = htons(data_port);

    remote_address.sin_addr = mcast_address;

    if (connect(mcast_sock, (struct sockaddr *)&remote_address, sizeof remote_address) < 0)
        syserr("connect");
}

void init() {
    init_mcast();
    init_multicast_listen(discover_sock, discover_address, ctrl_port);

    seconds_past_epoch = time(0);

    direct_sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (direct_sock < 0)
        syserr("socket");

    packet_data_size = sizeof(uint64_t)*2 + psize;

    last_rexmit_time = get_current_time();
}

void send_rexmit() {
    int sent_rexmit = 0;
    last_packs_lock.lock();
    rexmit_lock.lock();

    for(auto pack = rexmit_packs.begin();pack != rexmit_packs.end();pack++) {
        int number = get_pack_index_by_number(*pack,last_packs,psize);
        if (0 <= number && number < (int)last_packs.size()) {
            void* audio_data = last_packs[number].to_raw_data(seconds_past_epoch);
            if (write(mcast_sock,audio_data,packet_data_size) != packet_data_size) {
                if (errno != 11)
                    syserr("write rexmit");
            }
            sent_rexmit++;
            free(audio_data);
        }
    }
    rexmit_packs.clear();
    rexmit_lock.unlock();
    last_packs_lock.unlock();
}
bool stdin_read() {
    static std::unique_ptr<uint8_t[]> p{new uint8_t[psize]};
    static uint64_t first_byte = 0;
    static int next_byte = 0;
    int ret = read(STDIN_FILENO,&p[next_byte],psize-next_byte);
    if (ret > 0 )
        next_byte+=ret;
    if (next_byte == psize) {
        last_packs_lock.lock();

        last_packs.push_back(PackWrapper(first_byte,psize,&p[0]));
        first_byte+=psize;
        next_byte = 0;
        if (last_packs.size()*packet_data_size > (unsigned)fsize ) {
            free(last_packs.front().audio_data);
            last_packs.pop_front();
        }

        last_packs_lock.unlock();
    }
    if (ret == 0)
        return false;
    return true;
}
void send_data() {
    last_packs_lock.lock();
    int index = get_pack_index_by_number(next_byte,last_packs,psize);
    if (index == -2)
        return;
    if (index == -1) {
        next_byte = last_packs.front().first_byte_num;
        index = 0;
    }
    for(;index<(int)last_packs.size();index++) {
        void* audio_data = last_packs[index].to_raw_data(seconds_past_epoch);
        next_byte = last_packs[index].first_byte_num+psize;
        ssize_t res_write = write(mcast_sock,audio_data,packet_data_size);
        sent_packages++;
        if (res_write != packet_data_size) {
            syserr("write data");
        }
        free(audio_data);
    }
    last_packs_lock.unlock();
}
void discover_read(int recv_sock) {

    std::cmatch cm;
    socklen_t rcva_len = sizeof(struct sockaddr_in);
    const size_t buffer_size = 20 + (fsize / psize) * 20;
    static std::unique_ptr<char[]> buffer{new char[buffer_size]};
    sockaddr_in client_address;

    char* buffer2 = &buffer[0];
    int flags = 0;

    ssize_t len;
    len = recvfrom(recv_sock, buffer2, buffer_size, flags,
                           (struct sockaddr *) &client_address, &rcva_len);

    socklen_t snda_len = (socklen_t) sizeof(client_address);
    if (len < 0) {
        if (errno == 11)
            return;
    }
    if (len > 0) {
        buffer[len] = 0;
        std::regex regex_rexmit("LOUDER_PLEASE (([0-9]*,)*([0-9]*))\n");
        if (std::regex_match ( buffer2, regex_rexmit)) {
            std::smatch m;
            std::regex e ("\\b[0-9]+\\b");

            std::string s(buffer2);
            while (std::regex_search (s,m,e)) {
                std::istringstream iss(m.str());
                uint64_t value;
                iss >> value;
                rexmit_lock.lock();
                rexmit_packs.insert(value);
                rexmit_lock.unlock();
                s = m.suffix().str();
            }
        }
        if (strcmp(buffer2,"ZERO_SEVEN_COME_IN\n") == 0) {
            int sflags = 0;
            char send_buffer[200];

            sprintf(send_buffer,"BOREWICZ_HERE %s %d %s\n",mcast_address_str.c_str(), data_port, name.c_str());
            ssize_t snd_len = sendto(direct_sock, send_buffer, strlen(send_buffer), sflags,
                                     (struct sockaddr *) &client_address, snda_len);
            if (snd_len != strlen(send_buffer))
                if (errno != 11)
                    syserr("error on sending datagram to client socket");
        }
    }
}

void rexmit() {
    std::this_thread::sleep_for(std::chrono::milliseconds(rtime));
    send_rexmit();
    last_rexmit_time = get_current_time();
}
void discover_thread_lookup_fun() {
    while(true) {
        discover_read(discover_sock);
    }
}
void discover_thread_rexmit_fun() {
    while(true) {
        discover_read(direct_sock);
    }
}
void rexmit_thread_fun() {
    while(true) {
        rexmit();
    }
}
int main(int argc, const char *argv[])
{
    parse_args(argc,argv);
    init();
    std::thread discover_thread_lookup{discover_thread_lookup_fun}; /*Respond to lookups*/
    discover_thread_lookup.detach();

    std::thread discover_thread_rexmit{discover_thread_rexmit_fun}; /*Read rexmit-s */
    discover_thread_rexmit.detach();

    std::thread rexmit_thread{rexmit_thread_fun}; /* Respond to rexmits*/
    rexmit_thread.detach();

    while(stdin_read()) {
        send_data();
    }
    last_packs_lock.lock();
    for(auto i : last_packs)
        i.free();
    last_packs_lock.unlock();

}
