#ifndef MADGWICK_H
#define MADGWICK_H

#include <stdint.h>

/* Madgwick AHRS filter state.
 *
 * Implements the Madgwick orientation filter (Mahony + fusion of
 * gyroscope, accelerometer, and magnetometer measurements) using
 * a quaternion representation.  Equivalent to the open-source
 * MadgwickAHRS algorithm as published by Sebastian Madgwick (2010).
 *
 * Inputs (rad/s for gyro, m/s^2 for accel, uT for mag) are
 * assumed to use a standard **NED (North-East-Down)** sensor frame.
 * The resulting quaternion rotates from the NED earth frame to
 * the body frame.
 *
 * All math uses single-precision float.
 */

/* Quaternion: w + x*i + y*j + z*k */
typedef struct {
    float q0;  /* w */
    float q1;  /* x */
    float q2;  /* y */
    float q3;  /* z */
} quaternion_t;

/* Euler angles in degrees */
typedef struct {
    float yaw;     /* degrees, 0=North, 90=East */
    float pitch;
    float roll;
} euler_t;

/* Initialize the filter.
 * sample_freq_hz: expected update frequency, e.g. 125.0f.
 * beta: gyroscope measurement error gain (typically 0.041f for IMU-only,
 *       0.033f for IMU+magnetometer). Higher = more trust in accel/mag.
 */
void madgwick_init(float sample_freq_hz, float beta);

/* Update the filter with new sensor measurements.
 *
 * gx, gy, gz: gyroscope readings in rad/s
 * ax, ay, az: accelerometer readings in m/s^2
 * mx, my, mz: magnetometer readings in uT (micro-Tesla)
 *
 * dt: time delta in seconds since last update.
 *     Pass 0.0f to use the internally stored 1/sample_freq.
 */
void madgwick_update(float gx, float gy, float gz,
                     float ax, float ay, float az,
                     float mx, float my, float mz,
                     float dt);

/* Update with gyro + accelerometer only (no magnetometer) */
void madgwick_update_imu(float gx, float gy, float gz,
                         float ax, float ay, float az,
                         float dt);

/* Retrieve the current orientation quaternion */
quaternion_t madgwick_get_quaternion(void);

/* Convert quaternion to Euler angles (degrees) */
euler_t madgwick_get_euler(void);

/* Get heading/yaw in degrees (0 = magnetic North) */
float madgwick_get_heading(void);

/* Get pitch in degrees */
float madgwick_get_pitch(void);

/* Get roll in degrees */
float madgwick_get_roll(void);

#endif /* MADGWICK_H */
