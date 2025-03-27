#pragma once

#include <cmath>

bool floatsEqual(float a, float b, float epsilon = 1e-3) {
    return std::fabs(a - b) < epsilon;
}