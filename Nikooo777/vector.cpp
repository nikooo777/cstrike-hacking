//
// Created by Niko on 7/25/2021.
//

#include "vector.h"

#define PI 3.14159265358f

float Vector3::Distance(const Vector3 &other) const {
    auto deltaX = this->x - other.x;
    auto deltaY = this->y - other.y;
    auto deltaZ = this->z - other.z;
    return sqrtf(deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ);
}

Vector3 Vector3::CalcAngle(const Vector3 &other) const {
    Vector3 angles{0, 0, 0};

    angles.x = (-atan2f(other.z - z, sqrtf(powf(other.y - y, 2) + powf(other.x - x, 2))) * (180.0f / PI)); // pitch
    angles.y = (atan2f(other.y - y, other.x - x)) * (180.0f / PI); // yaw
    angles.z = 0.0f;

    return angles;
}

void Vector3::ClampAngles() {
    if (x > 89.0f) { x = 89.0f; }
    if (x < -89.0f) { x = -89.0f; }
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