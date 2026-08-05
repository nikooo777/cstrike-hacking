#pragma once

#include <Windows.h>
#include <cstdint>

#include "sdk/client_state.h"
#include "sdk/client_mode.h"
#include "sdk/base_client.h"

namespace game {

ClientState *GetClientState();
ClientMode *GetClientMode();
BaseClient *GetBaseClient();
std::uintptr_t GetClientStateAddress();

} // namespace game
