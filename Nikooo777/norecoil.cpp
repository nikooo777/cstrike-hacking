//
// Created by Niko on 7/22/2021.
//
#include "norecoil.h"
#include "common.h"
#include "Helper.h"
#include "CCSPlayer.h"

void ClampAngles(Vector3 &angle) {
    if (angle.x > 89.0f) { angle.x = 89.0f; }
    if (angle.x < -89.0f) { angle.x = -89.0f; }
}

void NormalizeAngles(Vector3 &angle) {
    while (angle.y > 180) {
        angle.y -= 360;
    }
    while (angle.y < -180) {
        angle.y += 360;
    }

    angle.y = std::remainderf(angle.y, 360.f);
}

static Vector3 oldPunch = {0, 0, 0};

void noRecoil() {
    auto helper = Helper::getInstance();
    auto shotsFired = ((CCSPlayer *) helper->GetLocalPlayer())->m_iShotsFired;
    auto punchAngle = helper->GetLocalPlayer()->m_Local.m_vecPunchAngle;
    auto viewAngles = &helper->GetClientState()->m_vViewAngles;

    Vector3 tempAngle = {0, 0, 0};
    if (shotsFired > 0) {
        tempAngle.x = (viewAngles->x + oldPunch.x) - (punchAngle.x * 2);
        tempAngle.y = (viewAngles->y + oldPunch.y) - (punchAngle.y * 2);
        NormalizeAngles(tempAngle);
        ClampAngles(tempAngle);
        oldPunch.x = punchAngle.x * 2;
        oldPunch.y = punchAngle.y * 2;
        *viewAngles = tempAngle;
    } else {
        oldPunch = {0, 0, 0}; // reset old punch
    }
}
