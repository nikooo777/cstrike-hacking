//
// Created by Niko on 7/22/2021.
//
#include "bhop.h"
#include "common.h"
#include "Helper.h"

void bhop() {
    auto helper = Helper::getInstance();
    if (GetAsyncKeyState(VK_SPACE) & BUTTON_DOWN) {
        uintptr_t buffer = 4;
        if (helper->GetLocalPlayer()->m_iFlags & FL_ONGROUND) {
            buffer = 5;
        }
        *(DWORD *) (helper->GetModule("client.dll") + dwForceJump) = buffer;
    }
}