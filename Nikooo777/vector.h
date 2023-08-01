#pragma once

#include "common.h"

class Vector3 {
public:
	float x, y, z;

	Vector3() : x(0), y(0), z(0) {}

	Vector3(float x, float y, float z) : x(x), y(y), z(z) {};

	friend std::ostream &operator<<(std::ostream &os, const Vector3 &v);

	float Distance(const Vector3 &other) const;

	Vector3 CalcAngle(const Vector3 &other) const;

	Vector3 operator+(const Vector3 &other) const;

	void NormalizeAngles();

	void ClampAngles();

	void Zero();
};

