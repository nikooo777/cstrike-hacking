#include "game/weapon_math.h"

#include <cmath>
#include <limits>

#include "sdk/user_cmd.h"

namespace game {

namespace {

constexpr float kMaxPlausibleCone = 10.0f;
constexpr int kCompensationIterations = 4;
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;

constexpr std::uint32_t RotateLeft(std::uint32_t value, unsigned amount) {
    return (value << amount) | (value >> (32U - amount));
}

// Source's MD5_PseudoRandom hashes one little-endian 32-bit seed and returns
// the four bytes beginning at digest offset 6. Keeping this one-block
// implementation local avoids calling or reseeding a game-owned RNG/API.
std::uint32_t Md5PseudoRandom(std::uint32_t seed) {
    static constexpr std::uint32_t kConstants[64] = {
        0xd76aa478u, 0xe8c7b756u, 0x242070dbu, 0xc1bdceeeu,
        0xf57c0fafu, 0x4787c62au, 0xa8304613u, 0xfd469501u,
        0x698098d8u, 0x8b44f7afu, 0xffff5bb1u, 0x895cd7beu,
        0x6b901122u, 0xfd987193u, 0xa679438eu, 0x49b40821u,
        0xf61e2562u, 0xc040b340u, 0x265e5a51u, 0xe9b6c7aau,
        0xd62f105du, 0x02441453u, 0xd8a1e681u, 0xe7d3fbc8u,
        0x21e1cde6u, 0xc33707d6u, 0xf4d50d87u, 0x455a14edu,
        0xa9e3e905u, 0xfcefa3f8u, 0x676f02d9u, 0x8d2a4c8au,
        0xfffa3942u, 0x8771f681u, 0x6d9d6122u, 0xfde5380cu,
        0xa4beea44u, 0x4bdecfa9u, 0xf6bb4b60u, 0xbebfbc70u,
        0x289b7ec6u, 0xeaa127fau, 0xd4ef3085u, 0x04881d05u,
        0xd9d4d039u, 0xe6db99e5u, 0x1fa27cf8u, 0xc4ac5665u,
        0xf4292244u, 0x432aff97u, 0xab9423a7u, 0xfc93a039u,
        0x655b59c3u, 0x8f0ccc92u, 0xffeff47du, 0x85845dd1u,
        0x6fa87e4fu, 0xfe2ce6e0u, 0xa3014314u, 0x4e0811a1u,
        0xf7537e82u, 0xbd3af235u, 0x2ad7d2bbu, 0xeb86d391u,
    };
    static constexpr unsigned kShifts[64] = {
        7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
        5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
        4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
        6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21,
    };

    std::uint32_t words[16] = {};
    words[0] = seed;
    words[1] = 0x00000080u;
    words[14] = 32u;

    std::uint32_t a = 0x67452301u;
    std::uint32_t b = 0xefcdab89u;
    std::uint32_t c = 0x98badcfeu;
    std::uint32_t d = 0x10325476u;
    const std::uint32_t initialA = a;
    const std::uint32_t initialB = b;
    const std::uint32_t initialC = c;
    const std::uint32_t initialD = d;

    for (unsigned i = 0; i < 64; ++i) {
        std::uint32_t f = 0;
        unsigned g = 0;
        if (i < 16) {
            f = (b & c) | (~b & d);
            g = i;
        } else if (i < 32) {
            f = (d & b) | (~d & c);
            g = (5 * i + 1) % 16;
        } else if (i < 48) {
            f = b ^ c ^ d;
            g = (3 * i + 5) % 16;
        } else {
            f = c ^ (b | ~d);
            g = (7 * i) % 16;
        }

        const std::uint32_t next = d;
        d = c;
        c = b;
        b += RotateLeft(a + f + kConstants[i] + words[g], kShifts[i]);
        a = next;
    }

    a += initialA;
    b += initialB;
    c += initialC;
    d += initialD;
    const std::uint32_t digestWords[4] = {a, b, c, d};
    const auto *digest = reinterpret_cast<const std::uint8_t *>(digestWords);
    return static_cast<std::uint32_t>(digest[6]) |
           (static_cast<std::uint32_t>(digest[7]) << 8) |
           (static_cast<std::uint32_t>(digest[8]) << 16) |
           (static_cast<std::uint32_t>(digest[9]) << 24);
}

constexpr int kNTab = 32;
constexpr int kIA = 16807;
constexpr int kIM = 2147483647;
constexpr int kIQ = 127773;
constexpr int kIR = 2836;
constexpr int kNDIV = 1 + (kIM - 1) / kNTab;
constexpr float kAM = 1.0f / static_cast<float>(kIM);
constexpr float kEPS = 1.2e-7f;
constexpr float kRNMX = 1.0f - kEPS;

class LocalUniformStream {
public:
    void SetSeed(int seed) {
        m_idum = (seed < 0) ? seed : -seed;
        if (m_idum == 0) {
            m_idum = 1;
        }
        m_iy = 0;
    }

    float RandomFloat(float flMin, float flMax) {
        float fl = kAM * static_cast<float>(Generate());
        if (fl > kRNMX) {
            fl = kRNMX;
        }
        return fl * (flMax - flMin) + flMin;
    }

private:
    int Generate() {
        int j = 0;
        int k = 0;

        if (m_idum <= 0 || m_iy == 0) {
            if (-m_idum < 1) {
                m_idum = 1;
            } else {
                m_idum = -m_idum;
            }

            for (j = kNTab + 7; j >= 0; --j) {
                k = m_idum / kIQ;
                m_idum = kIA * (m_idum - k * kIQ) - kIR * k;
                if (m_idum < 0) {
                    m_idum += kIM;
                }
                if (j < kNTab) {
                    m_iv[j] = m_idum;
                }
            }
            m_iy = m_iv[0];
        }

        k = m_idum / kIQ;
        m_idum = kIA * (m_idum - k * kIQ) - kIR * k;
        if (m_idum < 0) {
            m_idum += kIM;
        }
        j = m_iy / kNDIV;
        if (j < 0 || j >= kNTab) {
            j = 0;
        }
        m_iy = m_iv[j];
        m_iv[j] = m_idum;
        return m_iy;
    }

    int m_idum = 0;
    int m_iy = 0;
    int m_iv[kNTab] = {};
};

} // namespace

std::uint8_t Seed8(int randomSeed) {
    return static_cast<std::uint8_t>(randomSeed & 0xFF);
}

std::uint32_t CommandRandomSeed(int commandNumber) {
    return Md5PseudoRandom(static_cast<std::uint32_t>(commandNumber)) &
           0x7fffffffu;
}

bool PredictNextAccuracyPenalty(int shotsFired, float divisor,
                                bool quadratic, float offset, float cap,
                                float currentPenalty, float &out) {
    out = 0.0f;
    if (shotsFired < 0 || !std::isfinite(divisor) ||
        !std::isfinite(offset) || !std::isfinite(cap) ||
        !std::isfinite(currentPenalty) || offset < 0.0f || cap < 0.0f ||
        currentPenalty < 0.0f || (divisor != -1.0f && divisor <= 0.0f)) {
        return false;
    }

    if (divisor == -1.0f) {
        out = currentPenalty;
        return true;
    }

    const auto nextShots = static_cast<long long>(shotsFired) + 1;
    if (nextShots > 0 &&
        nextShots > (std::numeric_limits<long long>::max)() / nextShots) {
        return false;
    }
    long long power = nextShots * nextShots;
    if (!quadratic) {
        if (nextShots > 0 &&
            power > (std::numeric_limits<long long>::max)() / nextShots) {
            return false;
        }
        power *= nextShots;
    }
    const float candidate = static_cast<float>(power) / divisor + offset;
    if (!std::isfinite(candidate)) {
        return false;
    }

    out = candidate < cap ? candidate : cap;
    return std::isfinite(out) && out >= 0.0f;
}

bool PredictAccuracyPenaltyDecay(float currentPenalty, float baseline,
                                 float recoveryTime, float intervalPerTick,
                                 float decayConstant, float &out) {
    out = 0.0f;
    if (!std::isfinite(currentPenalty) || !std::isfinite(baseline) ||
        !std::isfinite(recoveryTime) || !std::isfinite(intervalPerTick) ||
        !std::isfinite(decayConstant) || currentPenalty < 0.0f ||
        baseline < 0.0f || recoveryTime <= 0.0f ||
        intervalPerTick <= 0.0f || intervalPerTick > 1.0f ||
        decayConstant >= 0.0f) {
        return false;
    }

    if (currentPenalty < baseline) {
        out = baseline;
        return true;
    }

    const float factor =
        expf((decayConstant / recoveryTime) * intervalPerTick);
    out = factor * (currentPenalty - baseline) + baseline;
    return std::isfinite(out) && out >= 0.0f;
}

bool PredictCssPunchDecay(const Vector3 &currentPunch,
                          float intervalPerTick, Vector3 &out) {
    out = {};
    if (!std::isfinite(currentPunch.x) ||
        !std::isfinite(currentPunch.y) ||
        !std::isfinite(currentPunch.z) ||
        !std::isfinite(intervalPerTick) || intervalPerTick <= 0.0f ||
        intervalPerTick > 1.0f) {
        return false;
    }

    const float lengthSquared = currentPunch.x * currentPunch.x +
                                currentPunch.y * currentPunch.y +
                                currentPunch.z * currentPunch.z + 1e-10f;
    const float length = sqrtf(lengthSquared);
    if (!std::isfinite(length) || length <= 0.0f) {
        return false;
    }

    float nextLength =
        length - (length * 0.5f + 10.0f) * intervalPerTick;
    if (nextLength < 0.0f) {
        nextLength = 0.0f;
    }
    const float scale = nextLength / length;
    out = currentPunch * scale;
    return std::isfinite(out.x) && std::isfinite(out.y) &&
           std::isfinite(out.z);
}

bool ResolveCommandRandomSeed(const CUserCmd *userCmd,
                              std::uint32_t &seedOut,
                              bool &fromStoredSeedOut) {
    seedOut = 0;
    fromStoredSeedOut = false;
    if (userCmd == nullptr) {
        return false;
    }

    if (userCmd->random_seed != 0) {
        seedOut = static_cast<std::uint32_t>(userCmd->random_seed);
        fromStoredSeedOut = true;
        return true;
    }

    if (userCmd->command_number <= 0) {
        return false;
    }

    seedOut = CommandRandomSeed(userCmd->command_number);
    return true;
}

bool PredictConeOffsets(int randomSeed, float inaccuracy, float spread,
                        ConeOffsets &out) {
    out = {};
    if (!std::isfinite(inaccuracy) || !std::isfinite(spread) ||
        inaccuracy < 0.0f || spread < 0.0f ||
        inaccuracy > kMaxPlausibleCone || spread > kMaxPlausibleCone) {
        return false;
    }

    LocalUniformStream stream;
    // The current CS FX/server fire path calls RandomSeed(seed8 + 1), then
    // samples one polar offset at the inaccuracy radius and one at the
    // per-pellet spread radius. The helper returns (sin(theta), cos(theta));
    // the fire call consumes those lanes as (right, up) = (cos, sin).
    stream.SetSeed(static_cast<int>(Seed8(randomSeed)) + 1);

    const auto samplePolar = [&stream](float radius, float &sx, float &sy) {
        const float theta = stream.RandomFloat(0.0f, kTwoPi);
        const float magnitude = stream.RandomFloat(0.0f, radius);
        sx = cosf(theta) * magnitude;
        sy = sinf(theta) * magnitude;
    };

    float inaccuracyX = 0.0f;
    float inaccuracyY = 0.0f;
    float spreadX = 0.0f;
    float spreadY = 0.0f;
    samplePolar(inaccuracy, inaccuracyX, inaccuracyY);
    samplePolar(spread, spreadX, spreadY);

    out.sx = inaccuracyX + spreadX;
    out.sy = inaccuracyY + spreadY;
    out.ok = std::isfinite(out.sx) && std::isfinite(out.sy);
    return out.ok;
}

bool ForwardSpreadDirection(const Vector3 &cmdAngles, float sx, float sy,
                            Vector3 &outDir) {
    Vector3 forward{};
    Vector3 right{};
    Vector3 up{};
    AngleVectors(cmdAngles, &forward, &right, &up);
    outDir = (forward + right * sx + up * sy).Normalized();
    return outDir.Length() > 1e-6f;
}

float DirectionErrorDegrees(const Vector3 &a, const Vector3 &b) {
    const Vector3 na = a.Normalized();
    const Vector3 nb = b.Normalized();
    float dot = na.x * nb.x + na.y * nb.y + na.z * nb.z;
    if (dot > 1.0f) {
        dot = 1.0f;
    }
    if (dot < -1.0f) {
        dot = -1.0f;
    }
    return acosf(dot) * (180.0f / kPi);
}

bool CompensateAngles(const Vector3 &intended, float sx, float sy,
                      CompensationResult &result) {
    result = {};
    Vector3 intendedForward{};
    AngleVectors(intended, &intendedForward, nullptr, nullptr);
    if (intendedForward.Length() <= 1e-6f) {
        return false;
    }

    Vector3 forward{};
    Vector3 right{};
    Vector3 up{};
    AngleVectors(intended, &forward, &right, &up);
    Vector3 aimDir = (forward - right * sx - up * sy).Normalized();
    if (aimDir.Length() <= 1e-6f) {
        return false;
    }

    Vector3 cmdAngles = VectorAngles(aimDir);
    cmdAngles.NormalizeAngles();
    cmdAngles.ClampAngles();

    for (int i = 0; i < kCompensationIterations; ++i) {
        AngleVectors(cmdAngles, &forward, &right, &up);
        Vector3 got = (forward + right * sx + up * sy).Normalized();
        const float err = DirectionErrorDegrees(got, intendedForward);
        result.forwardErrorDeg = err;
        result.iterations = i + 1;
        if (err < 1e-3f) {
            break;
        }

        aimDir = (intendedForward - right * sx - up * sy).Normalized();
        if (aimDir.Length() <= 1e-6f) {
            break;
        }
        cmdAngles = VectorAngles(aimDir);
        cmdAngles.NormalizeAngles();
        cmdAngles.ClampAngles();
    }

    Vector3 finalDir{};
    if (!ForwardSpreadDirection(cmdAngles, sx, sy, finalDir)) {
        return false;
    }
    result.forwardErrorDeg =
        DirectionErrorDegrees(finalDir, intendedForward);
    result.angles = cmdAngles;
    result.ok = std::isfinite(result.angles.x) &&
                std::isfinite(result.angles.y) &&
                std::isfinite(result.forwardErrorDeg);
    return result.ok;
}

namespace {

bool IsFiniteAngles(const Vector3 &angles) {
    return std::isfinite(angles.x) && std::isfinite(angles.y) &&
           std::isfinite(angles.z);
}

Vector3 NormalizeCommandAngles(Vector3 angles) {
    angles.NormalizeAngles();
    angles.ClampAngles();
    return angles;
}

} // namespace

bool ComposeShotAngles(const ShotAngleRequest &request,
                       ShotAngleResult &result) {
    result = {};
    if (!IsFiniteAngles(request.desiredAngles)) {
        return false;
    }

    const bool punchUsable =
        request.punchReadable && IsFiniteAngles(request.punchAngles);
    const Vector3 punchTwice = punchUsable ? request.punchAngles * 2.0f
                                           : Vector3{};

    result.commandAngles = request.desiredAngles;
    result.recoilCommandAngles = request.desiredAngles;
    result.fireBaseAngles = NormalizeCommandAngles(
        request.desiredAngles + (punchUsable ? punchTwice : Vector3{}));
    result.spreadAngles = result.fireBaseAngles;

    if (request.noRecoil && punchUsable) {
        result.noRecoilApplied = true;
        result.recoilCommandAngles = NormalizeCommandAngles(
            request.desiredAngles - punchTwice);
        result.fireBaseAngles = request.desiredAngles;
        result.spreadAngles = result.fireBaseAngles;
    }

    result.commandAngles = result.recoilCommandAngles;

    // Spread compensation is only valid when the feature code supplied a
    // live punch basis and a seed-derived cone. If it fails, retain the recoil
    // stage instead of partially applying an angle transform.
    if (request.noSpread && request.spreadAvailable && punchUsable) {
        CompensationResult compensated{};
        if (CompensateAngles(result.fireBaseAngles, request.spreadX,
                             request.spreadY, compensated) &&
            compensated.ok && compensated.forwardErrorDeg <= 1.0f) {
            result.spreadAngles = compensated.angles;
            result.commandAngles = NormalizeCommandAngles(
                compensated.angles - punchTwice);
            result.spreadResidualDeg = compensated.forwardErrorDeg;
            result.spreadIterations = compensated.iterations;
            result.noSpreadApplied = true;
        }
    }

    result.ok = IsFiniteAngles(result.commandAngles) &&
                IsFiniteAngles(result.recoilCommandAngles) &&
                IsFiniteAngles(result.fireBaseAngles) &&
                IsFiniteAngles(result.spreadAngles);
    return result.ok;
}

} // namespace game
