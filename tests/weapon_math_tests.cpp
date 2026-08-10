#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>

#include "game/weapon_math.h"
#include "sdk/user_cmd.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char *expression, int line) {
    if (!condition) {
        std::cerr << "FAIL line " << line << ": " << expression << '\n';
        ++g_failures;
    }
}

#define CHECK(expression) Check((expression), #expression, __LINE__)

bool Near(float lhs, float rhs, float epsilon) {
    return std::fabs(lhs - rhs) <= epsilon;
}

std::uintptr_t AddressOf(const void *value) {
    return reinterpret_cast<std::uintptr_t>(value);
}

std::size_t Offset(const void *object, const void *field) {
    return static_cast<std::size_t>(AddressOf(field) - AddressOf(object));
}

void TestCommandSeeds() {
    CHECK(game::Seed8(0x12345678) == 0x78);
    CHECK(game::Seed8(-1) == 0xff);

    // Known MD5_PseudoRandom vectors from the Source command-seed path.
    CHECK(game::CommandRandomSeed(0) == 0x2d863277u);
    CHECK(game::CommandRandomSeed(1) == 0x770b7539u);
    CHECK(game::CommandRandomSeed(2) == 0x4e9e4160u);
    CHECK(game::CommandRandomSeed(12345) == 0x02259f87u);
}

void TestCommandSeedResolution() {
    std::uint32_t seed = 0;
    bool fromStored = false;

    CHECK(!game::ResolveCommandRandomSeed(nullptr, seed, fromStored));

    CUserCmd command{};
    command.command_number = 0;
    command.random_seed = 0;
    CHECK(!game::ResolveCommandRandomSeed(&command, seed, fromStored));

    command.command_number = -1;
    CHECK(!game::ResolveCommandRandomSeed(&command, seed, fromStored));

    command.command_number = 12345;
    CHECK(game::ResolveCommandRandomSeed(&command, seed, fromStored));
    CHECK(seed == game::CommandRandomSeed(12345));
    CHECK(!fromStored);

    command.random_seed = 0x12345678;
    CHECK(game::ResolveCommandRandomSeed(&command, seed, fromStored));
    CHECK(seed == 0x12345678u);
    CHECK(fromStored);
}

void TestNextAccuracyPenalty() {
    float penalty = 0.0f;
    CHECK(game::PredictNextAccuracyPenalty(0, 10.0f, true, 0.01f, 1.0f,
                                           0.02f, penalty));
    CHECK(Near(penalty, 0.11f, 1e-6f));

    CHECK(game::PredictNextAccuracyPenalty(2, 10.0f, false, 0.01f, 10.0f,
                                           0.02f, penalty));
    CHECK(Near(penalty, 2.71f, 1e-6f));

    CHECK(game::PredictNextAccuracyPenalty(4, 10.0f, true, 0.01f, 0.5f,
                                           0.02f, penalty));
    CHECK(Near(penalty, 0.5f, 1e-6f));

    CHECK(game::PredictNextAccuracyPenalty(3, -1.0f, true, 0.0f, 1.0f,
                                           0.37f, penalty));
    CHECK(Near(penalty, 0.37f, 1e-6f));
    CHECK(!game::PredictNextAccuracyPenalty(-1, 10.0f, true, 0.0f, 1.0f,
                                            0.0f, penalty));
    CHECK(!game::PredictNextAccuracyPenalty(0, 0.0f, true, 0.0f, 1.0f,
                                            0.0f, penalty));
}

void TestPreFireStateDecay() {
    float penalty = 0.0f;
    CHECK(game::PredictAccuracyPenaltyDecay(
        0.03f, 0.007f, 0.4f, 0.015f, -0.7675284f, penalty));
    const float expected =
        std::exp((-0.7675284f / 0.4f) * 0.015f) * 0.023f + 0.007f;
    CHECK(Near(penalty, expected, 1e-7f));

    CHECK(game::PredictAccuracyPenaltyDecay(
        0.004f, 0.007f, 0.4f, 0.015f, -2.3025851f, penalty));
    CHECK(Near(penalty, 0.007f, 1e-7f));
    CHECK(!game::PredictAccuracyPenaltyDecay(
        0.03f, 0.007f, 0.0f, 0.015f, -2.3025851f, penalty));

    const Vector3 punch{-4.631455f, 1.23305f, 0.0f};
    Vector3 decayed{};
    CHECK(game::PredictCssPunchDecay(punch, 0.015f, decayed));
    const float length = std::sqrt(punch.x * punch.x +
                                   punch.y * punch.y + 1e-10f);
    const float scale =
        std::max(length - (length * 0.5f + 10.0f) * 0.015f, 0.0f) /
        length;
    CHECK(Near(decayed.x, punch.x * scale, 1e-6f));
    CHECK(Near(decayed.y, punch.y * scale, 1e-6f));
    CHECK(!game::PredictCssPunchDecay(
        punch, std::numeric_limits<float>::quiet_NaN(), decayed));
}

void TestUserCmdLayout() {
    CUserCmd command{};
    const std::size_t pointerSize = sizeof(void *);
    const std::size_t base = pointerSize;

    // The vptr is followed by the same field sequence on both ABIs. Pointer
    // alignment changes the total size and the absolute offsets, which is why
    // this test derives them from sizeof(void*) instead of hardcoding x64.
    CHECK(sizeof(CUserCmd) == (pointerSize == 8 ? 0x48u : 0x40u));
    CHECK(Offset(&command, &command.command_number) == base);
    CHECK(Offset(&command, &command.tick_count) == base + 0x04);
    CHECK(Offset(&command, &command.viewangles) == base + 0x08);
    CHECK(Offset(&command, &command.buttons) == base + 0x20);
    CHECK(Offset(&command, &command.weaponselect) == base + 0x28);
    CHECK(Offset(&command, &command.random_seed) == base + 0x30);
    CHECK(Offset(&command, &command.hasbeenpredicted) == base + 0x38);
}

void TestConeDeterminismAndValidation() {
    game::ConeOffsets first{};
    game::ConeOffsets second{};
    game::ConeOffsets different{};

    CHECK(game::PredictConeOffsets(0x770b7539, 0.0356347f, 0.0356347f,
                                   first));
    CHECK(game::PredictConeOffsets(0x770b7539, 0.0356347f, 0.0356347f,
                                   second));
    CHECK(first.ok && second.ok);
    CHECK(first.sx == second.sx && first.sy == second.sy);

    CHECK(game::PredictConeOffsets(0x4e9e4160, 0.0356347f, 0.0356347f,
                                   different));
    CHECK(first.sx != different.sx || first.sy != different.sy);
    // Deterministic vectors for the current CS polar path: seed8+1, one
    // inaccuracy sample, and one spread sample combined for pellet 0.
    CHECK(Near(first.sx, 0.00823590f, 1e-6f));
    CHECK(Near(first.sy, -0.0295979f, 1e-6f));
    CHECK(Near(different.sx, 0.00784939f, 1e-6f));
    CHECK(Near(different.sy, 0.0112452f, 1e-6f));

    // Keep the radii independent; collapsing them into an equal X/Y scalar
    // would fail this runtime-shaped rifle sample.
    game::ConeOffsets unequalRadii{};
    CHECK(game::PredictConeOffsets(0x7e036e50, 0.0259585f, 0.0006f,
                                   unequalRadii));
    CHECK(Near(unequalRadii.sx, -0.00781786f, 1e-6f));
    CHECK(Near(unequalRadii.sy, -0.00764438f, 1e-6f));

    game::ConeOffsets invalid{1.0f, 2.0f, true};
    CHECK(!game::PredictConeOffsets(
        0, -1.0f, 0.1f, invalid));
    CHECK(!invalid.ok && invalid.sx == 0.0f && invalid.sy == 0.0f);

    CHECK(!game::PredictConeOffsets(
        0, std::numeric_limits<float>::quiet_NaN(), 0.1f, invalid));
    CHECK(!invalid.ok);
}

void TestCompensation() {
    const Vector3 intended{2.2411f, -93.4511f, 0.0f};
    game::ConeOffsets cone{};
    CHECK(game::PredictConeOffsets(0x770b7539, 0.0356347f, 0.0356347f,
                                   cone));

    game::CompensationResult result{};
    CHECK(game::CompensateAngles(intended, cone.sx, cone.sy, result));
    CHECK(result.ok);
    CHECK(result.iterations > 0);
    CHECK(result.forwardErrorDeg < 0.01f);

    Vector3 intendedForward{};
    Vector3 compensatedDirection{};
    AngleVectors(intended, &intendedForward, nullptr, nullptr);
    CHECK(game::ForwardSpreadDirection(result.angles, cone.sx, cone.sy,
                                       compensatedDirection));
    CHECK(game::DirectionErrorDegrees(compensatedDirection,
                                      intendedForward) < 0.01f);

    game::CompensationResult neutral{};
    CHECK(game::CompensateAngles(intended, 0.0f, 0.0f, neutral));
    CHECK(neutral.forwardErrorDeg < 0.01f);
}

void TestConfigAwareShotPipeline() {
    const Vector3 desired{8.0f, -45.0f, 0.0f};
    const Vector3 punch{1.25f, -0.75f, 0.0f};

    game::ShotAngleRequest request{};
    request.desiredAngles = desired;
    request.punchAngles = punch;
    request.punchReadable = true;

    game::ShotAngleResult result{};
    CHECK(game::ComposeShotAngles(request, result));
    CHECK(result.ok);
    CHECK(!result.noRecoilApplied && !result.noSpreadApplied);
    CHECK(Near(result.commandAngles.x, desired.x, 1e-6f));
    CHECK(Near(result.commandAngles.y, desired.y, 1e-6f));

    Vector3 naturalFire{};
    AngleVectors(result.fireBaseAngles, &naturalFire, nullptr, nullptr);
    Vector3 expectedNatural{};
    AngleVectors(desired + punch * 2.0f, &expectedNatural, nullptr, nullptr);
    CHECK(game::DirectionErrorDegrees(naturalFire, expectedNatural) < 1e-4f);

    request.noRecoil = true;
    CHECK(game::ComposeShotAngles(request, result));
    CHECK(result.noRecoilApplied && !result.noSpreadApplied);
    Vector3 noRecoilFire{};
    CHECK(game::ForwardSpreadDirection(result.commandAngles + punch * 2.0f,
                                       0.0f, 0.0f, noRecoilFire));
    Vector3 desiredForward{};
    AngleVectors(desired, &desiredForward, nullptr, nullptr);
    CHECK(game::DirectionErrorDegrees(noRecoilFire, desiredForward) < 1e-4f);

    request.noRecoil = false;
    request.noSpread = true;
    request.spreadAvailable = true;
    request.spreadX = 0.018f;
    request.spreadY = -0.011f;
    CHECK(game::ComposeShotAngles(request, result));
    CHECK(result.noSpreadApplied && !result.noRecoilApplied);
    Vector3 noSpreadFire{};
    CHECK(game::ForwardSpreadDirection(result.commandAngles + punch * 2.0f,
                                       request.spreadX, request.spreadY,
                                       noSpreadFire));
    AngleVectors(result.fireBaseAngles, &expectedNatural, nullptr, nullptr);
    CHECK(game::DirectionErrorDegrees(noSpreadFire, expectedNatural) < 0.01f);

    request.noRecoil = true;
    CHECK(game::ComposeShotAngles(request, result));
    CHECK(result.noRecoilApplied && result.noSpreadApplied);
    Vector3 bothFire{};
    CHECK(game::ForwardSpreadDirection(result.commandAngles + punch * 2.0f,
                                       request.spreadX, request.spreadY,
                                       bothFire));
    AngleVectors(desired, &desiredForward, nullptr, nullptr);
    CHECK(game::DirectionErrorDegrees(bothFire, desiredForward) < 0.01f);

    request.noSpread = false;
    CHECK(game::ComposeShotAngles(request, result));
    CHECK(result.noRecoilApplied && !result.noSpreadApplied);
    Vector3 manualRecoilFire{};
    CHECK(game::ForwardSpreadDirection(result.commandAngles + punch * 2.0f,
                                       0.0f, 0.0f, manualRecoilFire));
    CHECK(game::DirectionErrorDegrees(manualRecoilFire, desiredForward) <
          1e-4f);

    request.noRecoil = false;
    request.desiredAngles = desired;
    request.noSpread = true;
    request.spreadAvailable = false;
    CHECK(game::ComposeShotAngles(request, result));
    CHECK(!result.noRecoilApplied && !result.noSpreadApplied);
    CHECK(Near(result.commandAngles.x, desired.x, 1e-6f));
    CHECK(Near(result.commandAngles.y, desired.y, 1e-6f));
}

} // namespace

int main() {
    TestCommandSeeds();
    TestCommandSeedResolution();
    TestNextAccuracyPenalty();
    TestPreFireStateDecay();
    TestUserCmdLayout();
    TestConeDeterminismAndValidation();
    TestCompensation();
    TestConfigAwareShotPipeline();

    if (g_failures != 0) {
        std::cerr << g_failures << " test assertion(s) failed\n";
        return 1;
    }

    std::cout << "weapon_math_tests: all checks passed\n";
    return 0;
}
