#pragma once

#include "vector.h"
#include "padding.h"
#include "UserCmd.h"

class ClientMode {
public:
    char pad_0004[1084]; //0x0004

    virtual void Function0();

    virtual void Function1();

    virtual void Function2();

    virtual void Function3();

    virtual void Function4();

    virtual void Function5();

    virtual void Function6();

    virtual void Function7();

    virtual void Function8();

    virtual void Function9();

    virtual void Function10();

    virtual void Function11();

    virtual void Function12();

    virtual void Function13();

    virtual void Function14();

    virtual void Function15();

    virtual void Function16();

    virtual void Function17();

    virtual void Function18();

    virtual void Function19();

    virtual void Function20();

    virtual bool CreateMove(float flInputSampleTime, CUserCmd *cmd);
}; //Size: 0x0440