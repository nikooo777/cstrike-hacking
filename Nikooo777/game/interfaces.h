#pragma once

#include <Windows.h>
#include <cstddef>
#include <cstdint>

#include "sdk/client_state.h"
#include "sdk/client_mode.h"
#include "sdk/base_client.h"
#include "sdk/client_entity_list.h"
#include "sdk/engine_client.h"
#include "sdk/engine_trace.h"
#include "sdk/render_view.h"

namespace config {
struct Signature;
}

namespace game {

char *FindConfiguredSignature(const config::Signature &signature,
                              std::size_t &matchCount);
ClientState *GetClientState();
ClientMode *GetClientMode();
BaseClient *GetBaseClient();
IClientEntityList *GetClientEntityList();
EngineClient *GetEngineClient();
sdk::trace::EngineTrace *GetEngineTrace();
sdk::render::RenderView *GetRenderView();
sdk::render::ModelInfo *GetModelInfo();
void *GetVguiSurface();
bool GetWorldToProjection(const CViewSetup &viewSetup,
                          sdk::render::Matrix4x4 &worldToProjection);
bool GetViewAngles(Vector3 &angles);
bool SetViewAngles(Vector3 &angles);
bool TraceLine(const Vector3 &start, const Vector3 &end,
               const void *skipFirst, const void *skipSecond, float &fraction);
std::uintptr_t GetClientStateAddress();

} // namespace game
