#pragma once

#include "math/vector.h"

class CUserCmd;

namespace features {

// Seed-based inverse-cone compensation on CUserCmd (default off in config).
// Does not touch engine view angles; the CreateMove hook suppresses the
// caller's camera copy when silent angles are enabled.
void PerfectNoSpread(CUserCmd *userCmd);

// True when silentAngles is enabled and cmd angles differ from the camera
// intent. The CreateMove hook returns false in this case, so the engine's
// caller does not copy the simulation angles into the render camera.
bool ShouldSuppressCameraUpdate(const CUserCmd *userCmd,
                                const Vector3 &intendedCamera);

} // namespace features
