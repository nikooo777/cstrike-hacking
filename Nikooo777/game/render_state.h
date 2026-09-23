#pragma once

#include "sdk/view_setup.h"

namespace game {

void CaptureViewSetup(const CViewSetup &viewSetup);
bool GetCapturedViewSetup(CViewSetup &viewSetup);

} // namespace game
