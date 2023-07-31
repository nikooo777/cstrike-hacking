//
// Created by Niko on 8/3/2021.
//
#pragma once

#include "common.h"
#include "vector.h"

class CUserCmd {
public:
    CUserCmd() {
        Reset();
    }

    virtual ~CUserCmd() {};

    void Reset() {
        command_number = 0;
        tick_count = 0;
        viewangles = {0, 0, 0};
        forwardmove = 0.0f;
        sidemove = 0.0f;
        upmove = 0.0f;
        buttons = 0;
        impulse = 0;
        weaponselect = 0;
        weaponsubtype = 0;
        random_seed = 0;
        mousedx = 0;
        mousedy = 0;

        hasbeenpredicted = false;
    }

    CUserCmd &operator=(const CUserCmd &src) {
        if (this == &src)
            return *this;

        command_number = src.command_number;
        tick_count = src.tick_count;
        viewangles = src.viewangles;
        forwardmove = src.forwardmove;
        sidemove = src.sidemove;
        upmove = src.upmove;
        buttons = src.buttons;
        impulse = src.impulse;
        weaponselect = src.weaponselect;
        weaponsubtype = src.weaponsubtype;
        random_seed = src.random_seed;
        mousedx = src.mousedx;
        mousedy = src.mousedy;

        hasbeenpredicted = src.hasbeenpredicted;

        return *this;
    }

    CUserCmd(const CUserCmd &src) {
        *this = src;
    }

    // For matching server and client commands for debugging
    int command_number;

    // the tick the client created this command
    int tick_count;

    // Player instantaneous view angles.
    Vector3 viewangles;
    // Intended velocities
    //	forward velocity.
    float forwardmove;
    //  sideways velocity.
    float sidemove;
    //  upward velocity.
    float upmove;
    // Attack button states
    int buttons;
    // Impulse command issued.
    byte impulse;
    // Current weapon id
    int weaponselect;
    int weaponsubtype;

    int random_seed;    // For shared random functions

    short mousedx;        // mouse accum in x from create move
    short mousedy;        // mouse accum in y from create move

    // Client only, tracks whether we've predicted this command at least once
    bool hasbeenpredicted;
};