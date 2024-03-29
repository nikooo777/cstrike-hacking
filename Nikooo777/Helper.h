//
// Created by Niko on 7/22/2021.
//

#pragma once

#include "common.h"
#include "ClientState.h"
#include "CBasePlayer.h"
#include "ClientMode.h"

class Helper {
private:
    /* Private constructor to prevent instancing. */
    Helper();

    std::map<const std::string, DWORD> modules;

    ClientState *clientState = nullptr;
    ClientMode *clientMode = nullptr;
    DWORD clientStateAddr = 0x0;

public:
    /* Static access method. */
    static Helper *getInstance();

    DWORD GetModule(const std::string &module);

    ClientState *GetClientState();

    ClientMode *GetClientMode();

    CBasePlayer *GetLocalPlayer();

    CBasePlayer *GetPlayer(int index);

    DWORD GetClientStateAddress();

    int GetPlayerCount();

    int GetMaxPlayerCount();
};

