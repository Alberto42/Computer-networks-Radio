//
// Created by albert on 04.06.18.
//

#ifndef SIK_2_REXMITEVENT_H
#define SIK_2_REXMITEVENT_H


struct RexmitEvent {
    long time;
    uint64_t pack_number;
    RexmitEvent(long time, uint64_t pack_number):time(time),pack_number(pack_number){}
};


#endif //SIK_2_REXMITEVENT_H
