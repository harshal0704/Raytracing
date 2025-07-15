#ifndef MATH_H
#define MATH_H

#include "vec3.h"
#include <cmath>
#include <cstdint>
#include <algorithm>

#define PI      3.14159265358979323846
#define EPSILON 1e-8
#define INF     1e30

inline double deg2rad(double d) { return d * PI / 180.0; }
inline double clamp(double x, double lo, double hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline double clamp01(double x) { return clamp(x, 0.0, 1.0); }

// XorShift64 RNG
struct XorShift64 {
    uint64_t state;
    XorShift64(uint64_t seed = 123456789ULL) : state(seed ? seed : 1ULL) {}
    uint64_t next() {
        state ^= state << 13; state ^= state >> 7; state ^= state << 17;
        return state;
    }
    double random01() { return (next() >> 11) * (1.0 / (double)(1ULL << 53)); }
};

// Thread-local RNG
static XorShift64 g_rng(987654321ULL);
inline double random01() { return g_rng.random01(); }

inline vec3 random_unit_vector() {
    double a = random01() * 2.0 * PI;
    double z = random01() * 2.0 - 1.0;
    double r = std::sqrt(1.0 - z * z);
    return vec3(r * std::cos(a), r * std::sin(a), z);
}

inline vec3 random_in_unit_sphere() {
    while (true) {
        vec3 p(random01()*2-1, random01()*2-1, random01()*2-1);
        if (p.length_squared() < 1.0) return p;
    }
}

inline vec3 random_in_unit_disk() {
    while (true) {
        vec3 p(random01()*2-1, random01()*2-1, 0);
        if (p.length_squared() < 1.0) return p;
    }
}

inline vec3 reflect(const vec3& v, const vec3& n) { return v - 2*dot(v,n)*n; }

inline vec3 refract(const vec3& uv, const vec3& n, double etai_over_etat) {
    double cos_theta = std::fmin(dot(-uv, n), 1.0);
    vec3 r_out_perp = etai_over_etat * (uv + cos_theta * n);
    vec3 r_out_par  = -std::sqrt(std::fabs(1.0 - r_out_perp.length_squared())) * n;
    return r_out_perp + r_out_par;
}

inline double schlick(double cosine, double ref_idx) {
    double r0 = (1 - ref_idx) / (1 + ref_idx);
    r0 = r0 * r0;
    return r0 + (1 - r0) * std::pow(1 - cosine, 5);
}

inline color3 tonemap_aces(color3 c) {
    const double a = 2.51, b = 0.03, gc = 2.43, d = 0.59, e = 0.14;
    double r = (c.x()*(a*c.x()+b))/(c.x()*(gc*c.x()+d)+e);
    double g = (c.y()*(a*c.y()+b))/(c.y()*(gc*c.y()+d)+e);
    double bl = (c.z()*(a*c.z()+b))/(c.z()*(gc*c.z()+d)+e);
    return color3(clamp01(r), clamp01(g), clamp01(bl));
}

#endif
