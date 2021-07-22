//
// Created by Niko on 7/22/2021.
//

#pragma once

#include <map>
#include "common.h"
#include "ClientState.h"
#include "CBasePlayer.h"

class Helper {
private:
    /* Private constructor to prevent instancing. */
    Helper();

    std::map<const char *, DWORD> modules;

    ClientState *clientState = nullptr;
    DWORD clientStateAddr = 0x0;

public:
    /* Static access method. */
    static Helper *getInstance();

    DWORD GetModule(const char *module);

    ClientState *GetClientState();

    CBasePlayer *GetLocalPlayer();

    CBasePlayer *GetPlayer(int index);

    DWORD GetClientStateAddress();
};

