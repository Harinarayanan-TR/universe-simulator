#ifndef VEC_MATH_H
#define VEC_MATH_H

#include <math.h>
#include <string.h>

typedef struct { double x, y, z; } vec3;
typedef struct { double x, y, z, w; } vec4;
typedef struct { double m[4][4]; } mat4;

static inline vec3 vec3_new(double x, double y, double z) { vec3 v = {x, y, z}; return v; }
static inline vec3 vec3_add(vec3 a, vec3 b) { return vec3_new(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline vec3 vec3_sub(vec3 a, vec3 b) { return vec3_new(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline vec3 vec3_mul(vec3 v, double s) { return vec3_new(v.x * s, v.y * s, v.z * s); }
static inline double vec3_dot(vec3 a, vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

static inline vec3 vec3_cross(vec3 a, vec3 b) {
    return vec3_new(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

static inline double vec3_len(vec3 v) { return sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

static inline vec3 vec3_norm(vec3 v) {
    double l = vec3_len(v);
    if (l < 1e-30) return vec3_new(0, 0, 0);
    return vec3_mul(v, 1.0 / l);
}

static inline vec3 vec3_lerp(vec3 a, vec3 b, double t) {
    return vec3_new(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t);
}

static inline vec4 vec4_new(double x, double y, double z, double w) {
    vec4 v = {x, y, z, w}; return v;
}

static inline mat4 mat4_identity() {
    mat4 m = {0};
    for (int i = 0; i < 4; i++) m.m[i][i] = 1.0;
    return m;
}

static inline mat4 mat4_mul(mat4 a, mat4 b) {
    mat4 r = {0};
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            for (int k = 0; k < 4; k++)
                r.m[i][j] += a.m[i][k] * b.m[k][j];
    return r;
}

static inline vec4 mat4_mul_v(mat4 m, vec4 v) {
    vec4 r;
    r.x = m.m[0][0] * v.x + m.m[0][1] * v.y + m.m[0][2] * v.z + m.m[0][3] * v.w;
    r.y = m.m[1][0] * v.x + m.m[1][1] * v.y + m.m[1][2] * v.z + m.m[1][3] * v.w;
    r.z = m.m[2][0] * v.x + m.m[2][1] * v.y + m.m[2][2] * v.z + m.m[2][3] * v.w;
    r.w = m.m[3][0] * v.x + m.m[3][1] * v.y + m.m[3][2] * v.z + m.m[3][3] * v.w;
    return r;
}

static inline mat4 mat4_perspective(double fov_rad, double aspect, double near, double far) {
    mat4 m = {0};
    double f = 1.0 / tan(fov_rad * 0.5);
    m.m[0][0] = f / aspect;
    m.m[1][1] = f;
    m.m[2][2] = (far + near) / (near - far);
    m.m[2][3] = 2.0 * far * near / (near - far);
    m.m[3][2] = -1.0;
    return m;
}

static inline mat4 mat4_lookat(vec3 eye, vec3 target, vec3 up) {
    vec3 f = vec3_norm(vec3_sub(target, eye));
    vec3 s = vec3_norm(vec3_cross(f, up));
    vec3 u = vec3_cross(s, f);
    mat4 m = mat4_identity();
    m.m[0][0] = s.x; m.m[0][1] = s.y; m.m[0][2] = s.z;
    m.m[1][0] = u.x; m.m[1][1] = u.y; m.m[1][2] = u.z;
    m.m[2][0] = -f.x; m.m[2][1] = -f.y; m.m[2][2] = -f.z;
    m.m[0][3] = -vec3_dot(s, eye);
    m.m[1][3] = -vec3_dot(u, eye);
    m.m[2][3] = vec3_dot(f, eye);
    return m;
}

static inline mat4 mat4_translate(double x, double y, double z) {
    mat4 m = mat4_identity();
    m.m[0][3] = x; m.m[1][3] = y; m.m[2][3] = z;
    return m;
}

static inline mat4 mat4_rotate_x(double angle) {
    mat4 m = mat4_identity();
    double c = cos(angle), s = sin(angle);
    m.m[1][1] = c; m.m[1][2] = -s;
    m.m[2][1] = s; m.m[2][2] = c;
    return m;
}

static inline mat4 mat4_rotate_y(double angle) {
    mat4 m = mat4_identity();
    double c = cos(angle), s = sin(angle);
    m.m[0][0] = c; m.m[0][2] = s;
    m.m[2][0] = -s; m.m[2][2] = c;
    return m;
}

static inline mat4 mat4_rotate_z(double angle) {
    mat4 m = mat4_identity();
    double c = cos(angle), s = sin(angle);
    m.m[0][0] = c; m.m[0][1] = -s;
    m.m[1][0] = s; m.m[1][1] = c;
    return m;
}

static inline mat4 mat4_scale(double s) {
    mat4 m = {0};
    m.m[0][0] = s; m.m[1][1] = s; m.m[2][2] = s; m.m[3][3] = 1.0;
    return m;
}

static inline int project_to_screen(vec3 world_pos, mat4 vp, int w, int h, double* sx, double* sy, double* depth) {
    vec4 p = mat4_mul_v(vp, vec4_new(world_pos.x, world_pos.y, world_pos.z, 1.0));
    if (p.w <= 0) return 0;
    double ndc_x = p.x / p.w;
    double ndc_y = p.y / p.w;
    double ndc_z = p.z / p.w;
    if (fabs(ndc_x) > 1 || fabs(ndc_y) > 1 || ndc_z < -1 || ndc_z > 1) return 0;
    *sx = (ndc_x + 1.0) * 0.5 * w;
    *sy = (1.0 - ndc_y) * 0.5 * h;
    *depth = ndc_z;
    return 1;
}

#endif
