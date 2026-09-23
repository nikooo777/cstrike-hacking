#include "math/vector.h"

#include <cmath>

#define PI 3.14159265358f

std::ostream &operator<<(std::ostream &os, const Vector3 &v) {
    os << "x: " << v.x << ", y: " << v.y << ", z: " << v.z
       << " - drawcross " << v.x << " " << v.y << " " << v.z;
    return os;
}

float Vector3::Distance(const Vector3 &other) const {
    auto deltaX = this->x - other.x;
    auto deltaY = this->y - other.y;
    auto deltaZ = this->z - other.z;
    return sqrtf(deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ);
}

Vector3 Vector3::CalcAngle(const Vector3 &other) const {
    Vector3 angles{0, 0, 0};

    angles.x = (-atan2f(other.z - z, sqrtf(powf(other.y - y, 2) + powf(other.x - x, 2))) * (180.0f / PI));
    angles.y = (atan2f(other.y - y, other.x - x)) * (180.0f / PI);
    angles.z = 0.0f;

    return angles;
}

void Vector3::ClampAngles() {
    if (x > 89.0f) {
        x = 89.0f;
    }
    if (x < -89.0f) {
        x = -89.0f;
    }
}

void Vector3::NormalizeAngles() {
    while (y > 180) {
        y -= 360;
    }
    while (y < -180) {
        y += 360;
    }

    y = std::remainderf(y, 360.f);
}

Vector3 Vector3::operator+(const Vector3 &other) const {
    return {x + other.x, y + other.y, z + other.z};
}

Vector3 Vector3::operator-(const Vector3 &other) const {
    return {x - other.x, y - other.y, z - other.z};
}

Vector3 Vector3::operator*(float scalar) const {
    return {x * scalar, y * scalar, z * scalar};
}

float Vector3::Length() const {
    return sqrtf(x * x + y * y + z * z);
}

Vector3 Vector3::Normalized() const {
    const float length = Length();
    if (length <= 1e-6f) {
        return {0.0f, 0.0f, 0.0f};
    }
    const float inv = 1.0f / length;
    return {x * inv, y * inv, z * inv};
}

void Vector3::Zero() {
    x = y = z = 0.0f;
}

void AngleVectors(const Vector3 &angles, Vector3 *forward, Vector3 *right,
                  Vector3 *up) {
    const float pitch = angles.x * (PI / 180.0f);
    const float yaw = angles.y * (PI / 180.0f);
    const float roll = angles.z * (PI / 180.0f);

    const float sp = sinf(pitch);
    const float cp = cosf(pitch);
    const float sy = sinf(yaw);
    const float cy = cosf(yaw);
    const float sr = sinf(roll);
    const float cr = cosf(roll);

    if (forward != nullptr) {
        *forward = {cp * cy, cp * sy, -sp};
    }
    if (right != nullptr) {
        *right = {
            -1.0f * sr * sp * cy + -1.0f * cr * -sy,
            -1.0f * sr * sp * sy + -1.0f * cr * cy,
            -1.0f * sr * cp,
        };
    }
    if (up != nullptr) {
        *up = {
            cr * sp * cy + -sr * -sy,
            cr * sp * sy + -sr * cy,
            cr * cp,
        };
    }
}

Vector3 VectorAngles(const Vector3 &forward) {
    Vector3 angles{0.0f, 0.0f, 0.0f};
    if (forward.x == 0.0f && forward.y == 0.0f) {
        angles.x = forward.z > 0.0f ? -90.0f : 90.0f;
        angles.y = 0.0f;
        return angles;
    }

    angles.x = -atan2f(forward.z, sqrtf(forward.x * forward.x +
                                        forward.y * forward.y)) *
               (180.0f / PI);
    angles.y = atan2f(forward.y, forward.x) * (180.0f / PI);
    angles.z = 0.0f;
    angles.NormalizeAngles();
    return angles;
}

bool IsFinite(const Vector3 &vector) {
    return std::isfinite(vector.x) && std::isfinite(vector.y) &&
           std::isfinite(vector.z);
}
