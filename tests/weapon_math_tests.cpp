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

    CHECK(game::PredictConeOffsets(0x770b7539, 0.0346347f, 0.001f,
                                   first));
    CHECK(game::PredictConeOffsets(0x770b7539, 0.0346347f, 0.001f,
                                   second));
    CHECK(first.ok && second.ok);
    CHECK(first.sx == second.sx && first.sy == second.sy);

    CHECK(game::PredictConeOffsets(0x4e9e4160, 0.0346347f, 0.001f,
                                   different));
    CHECK(first.sx != different.sx || first.sy != different.sy);
    CHECK(Near(first.sx, 0.000680348f, 1e-6f));
    CHECK(Near(first.sy, -0.0251312f, 1e-6f));
    CHECK(Near(different.sx, 0.00960008f, 1e-6f));
    CHECK(Near(different.sy, 0.00828513f, 1e-6f));

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
    CHECK(game::PredictConeOffsets(0x770b7539, 0.0346347f, 0.001f,
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

} // namespace

int main() {
    TestCommandSeeds();
    TestCommandSeedResolution();
    TestUserCmdLayout();
    TestConeDeterminismAndValidation();
    TestCompensation();

    if (g_failures != 0) {
        std::cerr << g_failures << " test assertion(s) failed\n";
        return 1;
    }

    std::cout << "weapon_math_tests: all checks passed\n";
    return 0;
}
