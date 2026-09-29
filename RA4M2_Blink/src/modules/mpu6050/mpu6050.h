#ifndef MPU6050_H
#define MPU6050_H

#include "hal_data.h"
#include <stdint.h>

#define MPU6050_STATUS_NOT_INITIALIZED       (0U)
#define MPU6050_STATUS_READY                 (1U)
#define MPU6050_STATUS_READ_OK               (2U)
#define MPU6050_STATUS_BUS_ERROR             (3U)
#define MPU6050_STATUS_ID_MISMATCH           (4U)
#define MPU6050_STATUS_CONFIG_VERIFY_FAILED  (5U)

#define MPU6050_DEVICE_UNKNOWN                (0U)
#define MPU6050_DEVICE_MPU6050                (1U)
#define MPU6050_DEVICE_MPU6500                (2U)

typedef struct st_mpu6050_data
{
    float accel_x_g;
    float accel_y_g;
    float accel_z_g;
    float temperature_c;
    float gyro_x_dps;
    float gyro_y_dps;
    float gyro_z_dps;
} mpu6050_data_t;

extern volatile uint8_t  g_mpu6050_status;
extern volatile uint8_t  g_mpu6050_device;
extern volatile uint8_t  g_mpu6050_calibrated;
extern volatile uint8_t  g_mpu6050_who_am_i;
extern volatile uint8_t  g_mpu6050_pwr_mgmt_1;
extern volatile uint8_t  g_mpu6050_smplrt_div;
extern volatile uint8_t  g_mpu6050_config;
extern volatile uint8_t  g_mpu6050_gyro_config;
extern volatile uint8_t  g_mpu6050_accel_config;
extern volatile float    g_mpu6050_roll_deg;
extern volatile float    g_mpu6050_pitch_deg;
extern volatile uint32_t g_mpu6050_last_fsp_error;
extern volatile uint32_t g_mpu6050_read_count;
extern volatile uint32_t g_mpu6050_error_count;
extern volatile int16_t  g_mpu6050_accel_x;
extern volatile int16_t  g_mpu6050_accel_y;
extern volatile int16_t  g_mpu6050_accel_z;
extern volatile int16_t  g_mpu6050_temperature_raw;
extern volatile int16_t  g_mpu6050_gyro_x;
extern volatile int16_t  g_mpu6050_gyro_y;
extern volatile int16_t  g_mpu6050_gyro_z;
extern volatile mpu6050_data_t g_mpu6050_data;

fsp_err_t MPU6050_Init (void);
fsp_err_t MPU6050_ReadRaw (void);
fsp_err_t MPU6050_CalibrateGyro (void);
fsp_err_t MPU6050_UpdateAttitude (float delta_time_s);

#endif
