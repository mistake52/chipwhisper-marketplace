/**
 * madgwick.h — Madgwick AHRS algorithm for sensor fusion
 *
 * Fuses 9-DOF IMU data (accel + gyro + magnetometer) into a quaternion
 * orientation estimate. Computes yaw (azimuth / compass heading) from quaternion.
 *
 * Reference: S. Madgwick, "An efficient orientation filter for IMU and MARG arrays"
 */

#ifndef MADGWICK_H
#define MADGWICK_H

#include <stdint.h>

/* Quaternion */
typedef struct {
    float q0, q1, q2, q3;  /* w, x, y, z */
} quaternion_t;

/* Euler angles in degrees */
typedef struct {
    float roll;
    float pitch;
    float yaw;  /* compass heading, 0 = North, 90 = East */
} euler_t;

void madgwick_init(float sample_freq_hz, float beta_gain);
void madgwick_update(float gx, float gy, float gz,
                     float ax, float ay, float az,
                     float mx, float my, float mz);
void madgwick_update_imu(float gx, float gy, float gz,
                         float ax, float ay, float az);
void madgwick_get_quaternion(quaternion_t *q);
void madgwick_get_euler(euler_t *e);

#endif /* MADGWICK_H */
