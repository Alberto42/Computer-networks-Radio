#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <deque>
#include <assert.h>
#include "err.h"
#include "common.h"

long get_current_time() {
    std::chrono::milliseconds ms = std::chrono::duration_cast< std::chrono::milliseconds >(
            std::chrono::system_clock::now().time_since_epoch()
    );
    return ms.count();
}

void init_multicast_listen(int &sock, in_addr address,
                           in_port_t port) {
    struct sockaddr_in local_address;


    /* otworzenie gniazda */
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
        syserr("socket");

    u_int val = 1;
    if (setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&val, sizeof(val)) < 0)
        syserr("setsockopt1");

    local_address.sin_family = AF_INET;
    local_address.sin_addr.s_addr = htonl(INADDR_ANY);
    local_address.sin_port = htons(port);
    if (bind(sock, (struct sockaddr *)&local_address, sizeof local_address) < 0)
        syserr("bind listen");
}

// Returns index of element in packs that has first_byte_number = num
int get_pack_index_by_number(uint64_t num,const std::deque<PackWrapper> &packs, int psize) {
    if (packs.empty())
        return -2;
    int64_t reversed_index = ((int64_t)packs.back().first_byte_num - (int64_t)num)/(int64_t)psize;
    int64_t index = packs.size()-1-reversed_index;
    if (index < 0) {
        return -1;
    }
    assert(index >= (int)packs.size() || packs[index].first_byte_num == num);
    return (int)index;
}