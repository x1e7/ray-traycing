#pragma once

#include "interval.h"
#include "vec3.h"

using color = vec3;

inline double linear_to_gamma(double linear_component)
{
    if (linear_component > 0)
        return std::sqrt(linear_component);

    return 0;
}

void write_color(unsigned char* out, const color& pixel_color) {
    auto r = linear_to_gamma(pixel_color.x());
    auto g = linear_to_gamma(pixel_color.y());
    auto b = linear_to_gamma(pixel_color.z());

    static const interval intensity(0.000, 0.999);
    out[0] = static_cast<unsigned char>(256 * intensity.clamp(r));
    out[1] = static_cast<unsigned char>(256 * intensity.clamp(g));
    out[2] = static_cast<unsigned char>(256 * intensity.clamp(b));
}
