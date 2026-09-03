#pragma once

#include <cmath>
#include <cstdint>

namespace dino {

constexpr float DT = 1.0f / 60.0f;
constexpr float PI = 3.14159265358979323846f;
constexpr float TAU = 2.0f * PI;

constexpr float MAX_THROTTLE_DELTA = 0.15f;
constexpr float MAX_STEER_DELTA = 0.12f;
constexpr float MAX_PITCH_DELTA = 0.12f;
constexpr uint32_t MIN_STEER_DWELL_TICKS = 4;
constexpr uint32_t MIN_LOCO_DWELL_TICKS = 6;
constexpr float STEER_FLIP_EPS = 0.05f;

constexpr float CALORIES_PER_MASS = 12.0f;
constexpr float FEED_RATE_PER_SEC = 25.0f;
constexpr float FEED_RANGE = 2.5f;
constexpr float CARCASS_DECAY_PER_SEC = 0.8f;

inline float wrap_angle(float a) {
    float x = std::fmod(a, TAU);
    if (x <= -PI) x += TAU;
    if (x > PI) x -= TAU;
    return x;
}

inline float clamp(float x, float lo, float hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}

inline float lerp(float a, float b, float t) { return a + (b - a) * t; }

inline float signum(float x) {
    if (x > 0.0f) return 1.0f;
    if (x < 0.0f) return -1.0f;
    return 0.0f;
}

inline uint32_t sat_add(uint32_t x, uint32_t d = 1) {
    if (x > UINT32_MAX - d) return UINT32_MAX;
    return x + d;
}

inline uint32_t sat_sub(uint32_t x, uint32_t d = 1) {
    return x < d ? 0u : x - d;
}

struct Vec2 {
    float x = 0;
    float z = 0;

    static Vec2 zero() { return {}; }
    static Vec2 make(float x, float z) { return {x, z}; }

    Vec2 add(Vec2 o) const { return {x + o.x, z + o.z}; }
    Vec2 sub(Vec2 o) const { return {x - o.x, z - o.z}; }
    Vec2 scale(float s) const { return {x * s, z * s}; }
    float dot(Vec2 o) const { return x * o.x + z * o.z; }
    float length_sq() const { return dot(*this); }
    float length() const { return std::sqrt(length_sq()); }
    Vec2 normalized() const {
        float len = length();
        if (len < 1e-8f) return {};
        return scale(1.0f / len);
    }
    float heading() const { return std::atan2(z, x); }
};

inline Vec2 heading_vec(float heading) { return {std::cos(heading), std::sin(heading)}; }
inline float angle_diff(float a, float b) { return wrap_angle(a - b); }
inline float abs_angle_diff(float a, float b) { return std::fabs(angle_diff(a, b)); }

}  // namespace dino
