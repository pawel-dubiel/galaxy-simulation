#ifndef MATH3D_H
#define MATH3D_H

#include <math.h>

typedef struct {
    float x, y, z;
} Vec3;

typedef struct {
    float m[3][3]; // Row-major 3x3 rotation matrix
} Mat3;

// Vector Operations
static inline Vec3 vec3_add(Vec3 a, Vec3 b) { return (Vec3){a.x+b.x, a.y+b.y, a.z+b.z}; }
static inline Vec3 vec3_sub(Vec3 a, Vec3 b) { return (Vec3){a.x-b.x, a.y-b.y, a.z-b.z}; }
static inline Vec3 vec3_mul(Vec3 a, float s) { return (Vec3){a.x*s, a.y*s, a.z*s}; }
static inline float vec3_dot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static inline float vec3_len(Vec3 a) { return sqrtf(vec3_dot(a, a)); }

static inline Vec3 vec3_norm(Vec3 a) {
    float l = vec3_len(a);
    if (l < 1e-6) return (Vec3){0,0,0};
    return vec3_mul(a, 1.0f/l);
}

static inline Vec3 vec3_cross(Vec3 a, Vec3 b) {
    return (Vec3){
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

// Matrix Operations (Rotation)
static inline Vec3 mat3_mul_vec(Mat3 m, Vec3 v) {
    return (Vec3){
        m.m[0][0]*v.x + m.m[0][1]*v.y + m.m[0][2]*v.z,
        m.m[1][0]*v.x + m.m[1][1]*v.y + m.m[1][2]*v.z,
        m.m[2][0]*v.x + m.m[2][1]*v.y + m.m[2][2]*v.z
    };
}

// Rotation Matrices
static inline Mat3 mat3_rot_x(float a) {
    float c=cosf(a), s=sinf(a);
    return (Mat3){{{1,0,0},{0,c,-s},{0,s,c}}};
}
static inline Mat3 mat3_rot_y(float a) {
    float c=cosf(a), s=sinf(a);
    return (Mat3){{{c,0,s},{0,1,0},{-s,0,c}}};
}
static inline Mat3 mat3_rot_z(float a) {
    float c=cosf(a), s=sinf(a);
    return (Mat3){{{c,-s,0},{s,c,0},{0,0,1}}};
}

// Combine rotations (Yaw * Pitch * Roll order usually)
static inline Mat3 mat3_mul(Mat3 a, Mat3 b) {
    Mat3 r;
    for(int i=0; i<3; i++) {
        for(int j=0; j<3; j++) {
            r.m[i][j] = a.m[i][0]*b.m[0][j] + a.m[i][1]*b.m[1][j] + a.m[i][2]*b.m[2][j];
        }
    }
    return r;
}

#endif