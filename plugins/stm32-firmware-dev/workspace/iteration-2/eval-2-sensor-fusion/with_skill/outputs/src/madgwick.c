/**
 * madgwick.c — Madgwick AHRS algorithm implementation
 *
 * Implements the gradient-descent orientation filter for 9-DOF IMU+MARG arrays.
 * Cortex-M3 has no FPU (uses software floating-point via -lm), so atan2 and
 * sqrt run in software emulation.
 */

#include "madgwick.h"
#include <math.h>

/* Quaternion */
static float q0 = 1.0f, q1 = 0.0f, q2 = 0.0f, q3 = 0.0f;

/* Sample period and filter gain */
static float sample_period = 0.01f;  /* default 100Hz */
static float beta = 0.1f;            /* filter gain */

void madgwick_init(float sample_freq_hz, float beta_gain) {
    sample_period = 1.0f / sample_freq_hz;
    beta = beta_gain;

    /* Initialize quaternion to identity */
    q0 = 1.0f;
    q1 = 0.0f;
    q2 = 0.0f;
    q3 = 0.0f;
}

/**
 * Fast inverse square root approximation (from Quake III Arena).
 * Reduces FPU load on Cortex-M3 (no hardware FPU).
 */
static float fast_inv_sqrt(float x) {
    float halfx = 0.5f * x;
    union { float f; int32_t i; } u;
    u.f = x;
    u.i = 0x5f3759df - (u.i >> 1);
    x = u.f;
    x = x * (1.5f - halfx * x * x);
    return x;
}

void madgwick_update(float gx, float gy, float gz,
                     float ax, float ay, float az,
                     float mx, float my, float mz) {
    float recip_norm;
    float s0, s1, s2, s3;
    float q_dot0, q_dot1, q_dot2, q_dot3;
    float hx, hy;
    float _2q0mx, _2q0my, _2q0mz, _2q1mx, _2bx, _2bz;
    float _4bx, _4bz;
    float _2q0, _2q1, _2q2, _2q3;
    float _2q0q2, _2q2q3;
    float q0q0, q0q1, q0q2, q0q3, q1q1, q1q2, q1q3, q2q2, q2q3, q3q3;

    /* Use IMU-only update if no magnetometer data */
    if (mx == 0.0f && my == 0.0f && mz == 0.0f) {
        madgwick_update_imu(gx, gy, gz, ax, ay, az);
        return;
    }

    /* Normalize accelerometer */
    recip_norm = fast_inv_sqrt(ax * ax + ay * ay + az * az);
    ax *= recip_norm;
    ay *= recip_norm;
    az *= recip_norm;

    /* Normalize magnetometer */
    recip_norm = fast_inv_sqrt(mx * mx + my * my + mz * mz);
    mx *= recip_norm;
    my *= recip_norm;
    mz *= recip_norm;

    /* Auxiliary variables */
    _2q0mx = 2.0f * q0 * mx;
    _2q0my = 2.0f * q0 * my;
    _2q0mz = 2.0f * q0 * mz;
    _2q1mx = 2.0f * q1 * mx;
    _2q0 = 2.0f * q0;
    _2q1 = 2.0f * q1;
    _2q2 = 2.0f * q2;
    _2q3 = 2.0f * q3;
    _2q0q2 = 2.0f * q0 * q2;
    _2q2q3 = 2.0f * q2 * q3;
    q0q0 = q0 * q0; q0q1 = q0 * q1; q0q2 = q0 * q2; q0q3 = q0 * q3;
    q1q1 = q1 * q1; q1q2 = q1 * q2; q1q3 = q1 * q3;
    q2q2 = q2 * q2; q2q3 = q2 * q3; q3q3 = q3 * q3;

    /* Reference direction of Earth's magnetic field */
    hx = mx * q0q0 - _2q0my * q3 + _2q0mz * q2
       + mx * q1q1 + _2q1 * my * q2 + _2q1 * mz * q3
       - mx * q2q2 - mx * q3q3;
    hy = _2q0mx * q3 + my * q0q0 - _2q0mz * q1
       + _2q1mx * q2 - my * q1q1 + my * q2q2
       + _2q2 * mz * q3 - my * q3q3;

    _2bx = sqrtf(hx * hx + hy * hy);
    _2bz = -_2q0mx * q2 + _2q0my * q1 + mz * q0q0
          + _2q1mx * q3 - mz * q1q1 + _2q2 * my * q3
          - mz * q2q2 + mz * q3q3;
    _4bx = 2.0f * _2bx;
    _4bz = 2.0f * _2bz;

    /* Gradient descent corrective step */
    s0 = -_2q2 * (2.0f * q1q3 - _2q0q2 - ax)
         + _2q1 * (2.0f * q0q1 + _2q2q3 - ay)
         - _2bz * q2 * (_2bx * (0.5f - q2q2 - q3q3)
         + _2bz * (q1q3 - q0q2) - mx)
         + (-_2bx * q3 + _2bz * q1) * (_2bx * (q1q2 - q0q3)
         + _2bz * (q0q1 + q2q3) - my)
         + _2bx * q2 * (_2bx * (q0q2 + q1q3)
         + _2bz * (0.5f - q1q1 - q2q2) - mz);
    s1 = _2q3 * (2.0f * q1q3 - _2q0q2 - ax)
         + _2q0 * (2.0f * q0q1 + _2q2q3 - ay)
         - 4.0f * q1 * (1.0f - 2.0f * q1q1 - 2.0f * q2q2 - az)
         + _2bz * q3 * (_2bx * (0.5f - q2q2 - q3q3)
         + _2bz * (q1q3 - q0q2) - mx)
         + (_2bx * q2 + _2bz * q0) * (_2bx * (q1q2 - q0q3)
         + _2bz * (q0q1 + q2q3) - my)
         + (_2bx * q3 - _4bz * q1) * (_2bx * (q0q2 + q1q3)
         + _2bz * (0.5f - q1q1 - q2q2) - mz);
    s2 = -_2q0 * (2.0f * q1q3 - _2q0q2 - ax)
         + _2q3 * (2.0f * q0q1 + _2q2q3 - ay)
         - 4.0f * q2 * (1.0f - 2.0f * q1q1 - 2.0f * q2q2 - az)
         + (-_4bx * q2 - _2bz * q0) * (_2bx * (0.5f - q2q2 - q3q3)
         + _2bz * (q1q3 - q0q2) - mx)
         + (_2bx * q1 + _2bz * q3) * (_2bx * (q1q2 - q0q3)
         + _2bz * (q0q1 + q2q3) - my)
         + (_2bx * q0 - _4bz * q2) * (_2bx * (q0q2 + q1q3)
         + _2bz * (0.5f - q1q1 - q2q2) - mz);
    s3 = _2q1 * (2.0f * q1q3 - _2q0q2 - ax)
         + _2q2 * (2.0f * q0q1 + _2q2q3 - ay)
         + (-_4bx * q3 + _2bz * q1) * (_2bx * (0.5f - q2q2 - q3q3)
         + _2bz * (q1q3 - q0q2) - mx)
         + (-_2bx * q0 + _2bz * q2) * (_2bx * (q1q2 - q0q3)
         + _2bz * (q0q1 + q2q3) - my)
         + _2bx * q1 * (_2bx * (q0q2 + q1q3)
         + _2bz * (0.5f - q1q1 - q2q2) - mz);

    /* Normalize step magnitude */
    recip_norm = fast_inv_sqrt(s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3);
    s0 *= recip_norm;
    s1 *= recip_norm;
    s2 *= recip_norm;
    s3 *= recip_norm;

    /* Compute rate of change of quaternion */
    q_dot0 = 0.5f * (-q1 * gx - q2 * gy - q3 * gz) - beta * s0;
    q_dot1 = 0.5f * ( q0 * gx + q2 * gz - q3 * gy) - beta * s1;
    q_dot2 = 0.5f * ( q0 * gy - q1 * gz + q3 * gx) - beta * s2;
    q_dot3 = 0.5f * ( q0 * gz + q1 * gy - q2 * gx) - beta * s3;

    /* Integrate */
    q0 += q_dot0 * sample_period;
    q1 += q_dot1 * sample_period;
    q2 += q_dot2 * sample_period;
    q3 += q_dot3 * sample_period;

    /* Normalize quaternion */
    recip_norm = fast_inv_sqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
    q0 *= recip_norm;
    q1 *= recip_norm;
    q2 *= recip_norm;
    q3 *= recip_norm;
}

void madgwick_update_imu(float gx, float gy, float gz,
                         float ax, float ay, float az) {
    float recip_norm;
    float s0, s1, s2, s3;
    float q_dot0, q_dot1, q_dot2, q_dot3;
    float _2q0, _2q1, _2q2, _2q3;
    float _4q0, _4q1, _4q2;
    float _8q1, _8q2;

    /* Normalize accelerometer */
    recip_norm = fast_inv_sqrt(ax * ax + ay * ay + az * az);
    ax *= recip_norm;
    ay *= recip_norm;
    az *= recip_norm;

    _2q0 = 2.0f * q0; _2q1 = 2.0f * q1;
    _2q2 = 2.0f * q2; _2q3 = 2.0f * q3;
    _4q0 = 4.0f * q0; _4q1 = 4.0f * q1;
    _4q2 = 4.0f * q2; _8q1 = 8.0f * q1;
    _8q2 = 8.0f * q2;

    /* Gradient descent corrective step for IMU-only */
    s0 = _4q0 * q2 * q2 + _2q2 * ax + _4q0 * q1 * q1 - _2q1 * ay;
    s1 = _4q1 * q3 * q3 - _2q3 * ax + 4.0f * q0 * q0 * q1
         - _2q0 * ay - _4q1 + _8q1 * q1 * q1
         + _8q1 * q2 * q2 + _4q1 * az;
    s2 = 4.0f * q0 * q0 * q2 + _2q0 * ax + _4q2 * q3 * q3
         - _2q3 * ay - _4q2 + _8q2 * q1 * q1
         + _8q2 * q2 * q2 + _4q2 * az;
    s3 = 4.0f * q1 * q1 * q3 - _2q1 * ax + 4.0f * q2 * q2 * q3
         - _2q2 * ay;

    /* Normalize step magnitude */
    recip_norm = fast_inv_sqrt(s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3);
    s0 *= recip_norm;
    s1 *= recip_norm;
    s2 *= recip_norm;
    s3 *= recip_norm;

    /* Compute rate of change */
    q_dot0 = 0.5f * (-q1 * gx - q2 * gy - q3 * gz) - beta * s0;
    q_dot1 = 0.5f * ( q0 * gx + q2 * gz - q3 * gy) - beta * s1;
    q_dot2 = 0.5f * ( q0 * gy - q1 * gz + q3 * gx) - beta * s2;
    q_dot3 = 0.5f * ( q0 * gz + q1 * gy - q2 * gx) - beta * s3;

    /* Integrate */
    q0 += q_dot0 * sample_period;
    q1 += q_dot1 * sample_period;
    q2 += q_dot2 * sample_period;
    q3 += q_dot3 * sample_period;

    /* Normalize quaternion */
    recip_norm = fast_inv_sqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
    q0 *= recip_norm;
    q1 *= recip_norm;
    q2 *= recip_norm;
    q3 *= recip_norm;
}

void madgwick_get_quaternion(quaternion_t *q) {
    q->q0 = q0;
    q->q1 = q1;
    q->q2 = q2;
    q->q3 = q3;
}

void madgwick_get_euler(euler_t *e) {
    /* Roll (x-axis rotation) */
    e->roll = atan2f(2.0f * (q0 * q1 + q2 * q3),
                     1.0f - 2.0f * (q1 * q1 + q2 * q2));
    e->roll *= 180.0f / (float)M_PI;

    /* Pitch (y-axis rotation) */
    float sinp = 2.0f * (q0 * q2 - q3 * q1);
    if (fabsf(sinp) >= 1.0f) {
        e->pitch = copysignf(90.0f, sinp);
    } else {
        e->pitch = asinf(sinp) * 180.0f / (float)M_PI;
    }

    /* Yaw (z-axis rotation) — compass heading, 0=North, clockwise */
    e->yaw = atan2f(2.0f * (q0 * q3 + q1 * q2),
                    1.0f - 2.0f * (q2 * q2 + q3 * q3));
    e->yaw *= 180.0f / (float)M_PI;

    /* Convert to 0-360 compass heading */
    if (e->yaw < 0.0f) {
        e->yaw += 360.0f;
    }
}
