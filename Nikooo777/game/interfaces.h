#pragma once

#include <Windows.h>
#include <cstdint>

#include "sdk/client_state.h"
#include "sdk/client_mode.h"
#include "sdk/base_client.h"
#include "sdk/client_entity_list.h"
#include "sdk/engine_client.h"

namespace game {

ClientState *GetClientState();
ClientMode *GetClientMode();
BaseClient *GetBaseClient();
IClientEntityList *GetClientEntityList();
EngineClient *GetEngineClient();
bool GetViewAngles(Vector3 &angles);
bool SetViewAngles(Vector3 &angles);
std::uintptr_t GetClientStateAddress();

} // namespace game
