/* Madgwick AHRS filter implementation.
 *
 * Based on the open-source algorithm by Sebastian O.H. Madgwick,
 * published 2010.  This is the "Madgwick" fusion filter that combines
 * 9-DoF IMU + magnetometer data using gradient-descent correction.
 *
 * References:
 *   Madgwick, S.O.H. "An efficient orientation filter for inertial
 *   and inertial/magnetic sensor arrays." 2010.
 *
 * The filter uses a quaternion representation.  The accelerometer and
 * magnetometer provide absolute orientation references, while the
 * gyroscope provides relative orientation changes.  Gradient descent
 * corrects gyroscope drift.
 */

#include "madgwick.h"
#include <math.h>

/* -- Static state -- */
static float g_beta;         /* Algorithm gain */
static float g_sample_freq;  /* Nominal sample frequency (Hz) */
static float g_q0, g_q1, g_q2, g_q3; /* Quaternion elements */

/* Fast inverse square root approximation (optional, for speed).
 * Using standard 1/sqrtf() is fine for this application.
 */
static float inv_sqrt(float x) {
    return 1.0f / sqrtf(x);
}

void madgwick_init(float sample_freq_hz, float beta) {
    g_sample_freq = sample_freq_hz;
    g_beta = beta;

    /* Initial quaternion: identity (no rotation) */
    g_q0 = 1.0f;
    g_q1 = 0.0f;
    g_q2 = 0.0f;
    g_q3 = 0.0f;
}

void madgwick_update(float gx, float gy, float gz,
                     float ax, float ay, float az,
                     float mx, float my, float mz,
                     float dt) {
    float q0 = g_q0, q1 = g_q1, q2 = g_q2, q3 = g_q3;
    float recip_norm;
    float s0, s1, s2, s3;
    float q_dot0, q_dot1, q_dot2, q_dot3;
    float hx, hy, hz;
    float _2q0mx, _2q0my, _2q0mz, _2q1mx, _2bx, _2bz, _4bx, _4bz;
    float _2q0, _2q1, _2q2, _2q3, _2q0q2, _2q2q3;
    float q0q0, q0q1, q0q2, q0q3, q1q1, q1q2, q1q3, q2q2, q2q3, q3q3;

    /* Use nominal sample period if dt is not provided */
    if (dt <= 0.0f) dt = 1.0f / g_sample_freq;

    /* Normalize accelerometer measurement */
    recip_norm = inv_sqrt(ax * ax + ay * ay + az * az);
    if (recip_norm == 0.0f) goto gyro_only;
    ax *= recip_norm;
    ay *= recip_norm;
    az *= recip_norm;

    /* Normalize magnetometer measurement */
    recip_norm = inv_sqrt(mx * mx + my * my + mz * mz);
    if (recip_norm == 0.0f) goto gyro_only;
    mx *= recip_norm;
    my *= recip_norm;
    mz *= recip_norm;

    /* Auxiliary variables to avoid repeated calculations */
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
    q0q0 = q0 * q0;
    q0q1 = q0 * q1;
    q0q2 = q0 * q2;
    q0q3 = q0 * q3;
    q1q1 = q1 * q1;
    q1q2 = q1 * q2;
    q1q3 = q1 * q3;
    q2q2 = q2 * q2;
    q2q3 = q2 * q3;
    q3q3 = q3 * q3;

    /* Reference direction of Earth's magnetic field */
    hx = mx * q0q0 - _2q0my * q3 + _2q0mz * q2 + mx * q1q1
       + _2q1 * my * q2 + _2q1 * mz * q3 - mx * q2q2 - mx * q3q3;
    hy = _2q0mx * q3 + my * q0q0 - _2q0mz * q1 + _2q1mx * q2
       - my * q1q1 + my * q2q2 + _2q2 * mz * q3 - my * q3q3;

    _2bx = sqrtf(hx * hx + hy * hy);
    _2bz = -_2q0mx * q2 + _2q0my * q1 + mz * q0q0 + _2q1mx * q3
         - mz * q1q1 + _2q2 * my * q3 - mz * q2q2 + mz * q3q3;
    _4bx = 2.0f * _2bx;
    _4bz = 2.0f * _2bz;

    /* Gradient-descent corrective step */
    s0 = -_2q2 * (2.0f * q1q3 - _2q0q2 - ax)
       + _2q1 * (2.0f * q0q1 + _2q2q3 - ay)
       - _2bz * q2 * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx)
       + (-_2bx * q3 + _2bz * q1) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my)
       + _2bx * q2 * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);

    s1 = _2q3 * (2.0f * q1q3 - _2q0q2 - ax)
       + _2q0 * (2.0f * q0q1 + _2q2q3 - ay)
       - 4.0f * q1 * (1.0f - 2.0f * q1q1 - 2.0f * q2q2 - az)
       + _2bz * q3 * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx)
       + (_2bx * q2 + _2bz * q0) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my)
       + (_2bx * q3 - _4bz * q1) * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);

    s2 = -_2q0 * (2.0f * q1q3 - _2q0q2 - ax)
       + _2q3 * (2.0f * q0q1 + _2q2q3 - ay)
       - 4.0f * q2 * (1.0f - 2.0f * q1q1 - 2.0f * q2q2 - az)
       + (-_4bx * q2 - _2bz * q0) * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx)
       + (_2bx * q1 + _2bz * q3) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my)
       + (_2bx * q0 - _4bz * q2) * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);

    s3 = _2q1 * (2.0f * q1q3 - _2q0q2 - ax)
       + _2q2 * (2.0f * q0q1 + _2q2q3 - ay)
       + (-_4bx * q3 + _2bz * q1) * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx)
       + (-_2bx * q0 + _2bz * q2) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my)
       + _2bx * q1 * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);

    /* Normalize step magnitude */
    recip_norm = inv_sqrt(s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3);
    if (recip_norm == 0.0f) goto gyro_only;
    s0 *= recip_norm;
    s1 *= recip_norm;
    s2 *= recip_norm;
    s3 *= recip_norm;

    /* Compute rate of change of quaternion */
    q_dot0 = 0.5f * (-q1 * gx - q2 * gy - q3 * gz) - g_beta * s0;
    q_dot1 = 0.5f * ( q0 * gx + q2 * gz - q3 * gy) - g_beta * s1;
    q_dot2 = 0.5f * ( q0 * gy - q1 * gz + q3 * gx) - g_beta * s2;
    q_dot3 = 0.5f * ( q0 * gz + q1 * gy - q2 * gx) - g_beta * s3;
    goto integrate;

gyro_only:
    /* Fallback: gyro-only integration */
    q_dot0 = 0.5f * (-q1 * gx - q2 * gy - q3 * gz);
    q_dot1 = 0.5f * ( q0 * gx + q2 * gz - q3 * gy);
    q_dot2 = 0.5f * ( q0 * gy - q1 * gz + q3 * gx);
    q_dot3 = 0.5f * ( q0 * gz + q1 * gy - q2 * gx);

integrate:
    /* Integrate rate of change to yield quaternion */
    q0 += q_dot0 * dt;
    q1 += q_dot1 * dt;
    q2 += q_dot2 * dt;
    q3 += q_dot3 * dt;

    /* Normalize quaternion */
    recip_norm = inv_sqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
    if (recip_norm == 0.0f) {
        /* Reset to identity if degenerate */
        q0 = 1.0f; q1 = 0.0f; q2 = 0.0f; q3 = 0.0f;
    } else {
        q0 *= recip_norm;
        q1 *= recip_norm;
        q2 *= recip_norm;
        q3 *= recip_norm;
    }

    g_q0 = q0; g_q1 = q1; g_q2 = q2; g_q3 = q3;
}

void madgwick_update_imu(float gx, float gy, float gz,
                         float ax, float ay, float az,
                         float dt) {
    float q0 = g_q0, q1 = g_q1, q2 = g_q2, q3 = g_q3;
    float recip_norm;
    float s0, s1, s2, s3;
    float q_dot0, q_dot1, q_dot2, q_dot3;
    float _2q0, _2q1, _2q2, _2q3, _4q0, _4q1, _4q2;
    float _8q1, _8q2, q0q0, q1q1, q2q2, q3q3;

    if (dt <= 0.0f) dt = 1.0f / g_sample_freq;

    /* Normalize accelerometer */
    recip_norm = inv_sqrt(ax * ax + ay * ay + az * az);
    if (recip_norm == 0.0f) goto gyro_only_imu;
    ax *= recip_norm;
    ay *= recip_norm;
    az *= recip_norm;

    /* Auxiliary variables */
    _2q0 = 2.0f * q0;
    _2q1 = 2.0f * q1;
    _2q2 = 2.0f * q2;
    _2q3 = 2.0f * q3;
    _4q0 = 4.0f * q0;
    _4q1 = 4.0f * q1;
    _4q2 = 4.0f * q2;
    _8q1 = 8.0f * q1;
    _8q2 = 8.0f * q2;
    q0q0 = q0 * q0;
    q1q1 = q1 * q1;
    q2q2 = q2 * q2;
    q3q3 = q3 * q3;

    /* Gradient-descent corrective step (accel only) */
    s0 = _4q0 * q2q2 + _2q2 * ax + _4q0 * q1q1 - _2q1 * ay;
    s1 = _4q1 * q3q3 - _2q3 * ax + 4.0f * q0q0 * q1 - _2q0 * ay
       - _4q1 + _8q1 * q1q1 + _8q1 * q2q2 + _4q1 * az;
    s2 = 4.0f * q0q0 * q2 + _2q0 * ax + _4q2 * q3q3 - _2q3 * ay
       - _4q2 + _8q2 * q1q1 + _8q2 * q2q2 + _4q2 * az;
    s3 = 4.0f * q1q1 * q3 - _2q1 * ax + 4.0f * q2q2 * q3 - _2q2 * ay;

    recip_norm = inv_sqrt(s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3);
    if (recip_norm == 0.0f) goto gyro_only_imu;
    s0 *= recip_norm;
    s1 *= recip_norm;
    s2 *= recip_norm;
    s3 *= recip_norm;

    q_dot0 = 0.5f * (-q1 * gx - q2 * gy - q3 * gz) - g_beta * s0;
    q_dot1 = 0.5f * ( q0 * gx + q2 * gz - q3 * gy) - g_beta * s1;
    q_dot2 = 0.5f * ( q0 * gy - q1 * gz + q3 * gx) - g_beta * s2;
    q_dot3 = 0.5f * ( q0 * gz + q1 * gy - q2 * gx) - g_beta * s3;
    goto integrate_imu;

gyro_only_imu:
    q_dot0 = 0.5f * (-q1 * gx - q2 * gy - q3 * gz);
    q_dot1 = 0.5f * ( q0 * gx + q2 * gz - q3 * gy);
    q_dot2 = 0.5f * ( q0 * gy - q1 * gz + q3 * gx);
    q_dot3 = 0.5f * ( q0 * gz + q1 * gy - q2 * gx);

integrate_imu:
    q0 += q_dot0 * dt;
    q1 += q_dot1 * dt;
    q2 += q_dot2 * dt;
    q3 += q_dot3 * dt;

    recip_norm = inv_sqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
    if (recip_norm == 0.0f) {
        q0 = 1.0f; q1 = 0.0f; q2 = 0.0f; q3 = 0.0f;
    } else {
        q0 *= recip_norm;
        q1 *= recip_norm;
        q2 *= recip_norm;
        q3 *= recip_norm;
    }

    g_q0 = q0; g_q1 = q1; g_q2 = q2; g_q3 = q3;
}

quaternion_t madgwick_get_quaternion(void) {
    quaternion_t q;
    q.q0 = g_q0;
    q.q1 = g_q1;
    q.q2 = g_q2;
    q.q3 = g_q3;
    return q;
}

euler_t madgwick_get_euler(void) {
    euler_t e;
    float q0 = g_q0, q1 = g_q1, q2 = g_q2, q3 = g_q3;

    /* Yaw (heading): rotation around Z axis */
    e.yaw = atan2f(2.0f * q1 * q2 + 2.0f * q0 * q3,
                   2.0f * q0 * q0 + 2.0f * q1 * q1 - 1.0f);

    /* Pitch: rotation around Y axis */
    float sinp = 2.0f * q0 * q2 - 2.0f * q3 * q1;
    if (sinp >= 1.0f) {
        e.pitch = 1.5707963f; /* 90 degrees */
    } else if (sinp <= -1.0f) {
        e.pitch = -1.5707963f; /* -90 degrees */
    } else {
        e.pitch = asinf(sinp);
    }

    /* Roll: rotation around X axis */
    e.roll = atan2f(2.0f * q0 * q1 + 2.0f * q2 * q3,
                    1.0f - 2.0f * q1 * q1 - 2.0f * q2 * q2);

    /* Convert radians to degrees */
    e.yaw   *= 57.29578f;
    e.pitch *= 57.29578f;
    e.roll  *= 57.29578f;

    /* Normalize yaw to [0, 360) */
    if (e.yaw < 0.0f) e.yaw += 360.0f;

    return e;
}

float madgwick_get_heading(void) {
    return madgwick_get_euler().yaw;
}

float madgwick_get_pitch(void) {
    return madgwick_get_euler().pitch;
}

float madgwick_get_roll(void) {
    return madgwick_get_euler().roll;
}
