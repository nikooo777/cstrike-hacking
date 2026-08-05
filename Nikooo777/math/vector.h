#pragma once

#include <ostream>

// POD on purpose: must be usable inside DEFINE_MEMBER_N unions (GCC/MinGW).
// No user-declared constructors — use Vector3{} or Vector3{x,y,z}.
class Vector3 {
public:
    float x, y, z;

    friend std::ostream &operator<<(std::ostream &os, const Vector3 &v);

    float Distance(const Vector3 &other) const;

    Vector3 CalcAngle(const Vector3 &other) const;

    Vector3 operator+(const Vector3 &other) const;

    void NormalizeAngles();

    void ClampAngles();

    void Zero();
};
