//
// Created by albert on 30.05.18.
//

#ifndef SIK_2_COMMON_H
#define SIK_2_COMMON_H

#include <cstdlib>
#include <malloc.h>
#include <iostream>
#include <arpa/inet.h>
#include <fcntl.h>
#include <variant>
#include <string.h>
#include <deque>

long get_current_time();
const int student_id = 370756;

struct Pack {
    uint64_t session_id;
    uint64_t first_byte_num;
    uint8_t audio_data[0];
};
struct PackWrapper {
    uint64_t session_id;
    uint64_t first_byte_num;
    uint8_t* audio_data;
    size_t size;
    bool empty=false;
    PackWrapper(uint64_t first_byte_num, size_t size,const uint8_t* audio_data) {
        this->audio_data = (uint8_t*)malloc(size);
        this->first_byte_num = first_byte_num;
        this->session_id = 42;
        memcpy(this->audio_data,audio_data,size);
        this->size = size;
    }
    PackWrapper(Pack* pack,size_t audio_size) {
        this->session_id = pack->session_id;
        this->first_byte_num = pack->first_byte_num;
        audio_data = (uint8_t*)malloc(audio_size);

        memcpy(this->audio_data,pack->audio_data,audio_size);
    }
    PackWrapper(uint64_t first_byte_num) { // for empty packets
        this->first_byte_num = first_byte_num;
        this->empty = true;
    }
    void* to_raw_data(time_t seonds_past_eopch) {
        void* data = malloc(2*sizeof(uint64_t ) + size);
        Pack* data2 = (Pack*)data;
        data2->session_id = __bswap_64(session_id);
        data2->first_byte_num = __bswap_64(first_byte_num);
        memcpy(data2->audio_data,audio_data,size);
        return data;
    }
    void free() {
        if (!empty)
            ::free(audio_data);
    }
};


void init_multicast_listen(int &sock, in_addr address,
                           in_port_t port);

int get_pack_index_by_number(uint64_t num,const std::deque<PackWrapper>& packs,int psize);

#endif //SIK_2_COMMON_H
