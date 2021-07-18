//
// Created by Niko on 7/12/2021.
//

#ifndef CSTRIKE_HAX_CLIENTSTATE_H
#define CSTRIKE_HAX_CLIENTSTATE_H

#define STR_MERGE_IMPL(a, b) a##b
#define STR_MERGE(a, b) STR_MERGE_IMPL(a, b)
#define MAKE_PAD(size) STR_MERGE(_pad, __COUNTER__)[size]
#define DEFINE_MEMBER_N(type, name, offset) struct {unsigned char MAKE_PAD(offset); type name;}


class ClientState {
public:
    union {
        //              Type     Name    Offset
        DEFINE_MEMBER_N(Vector3, m_vViewAngles, 0x4b84);
    };


};


#endif //CSTRIKE_HAX_CLIENTSTATE_H
