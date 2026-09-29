#include "mpu6050.h"
#include "i2c_bus.h"

#include <math.h>
#include <stdbool.h>

#define MPU6050_I2C_ADDRESS              (0x68U)
#define MPU6050_REG_SMPLRT_DIV           (0x19U)
#define MPU6050_REG_CONFIG               (0x1AU)
#define MPU6050_REG_GYRO_CONFIG          (0x1BU)
#define MPU6050_REG_ACCEL_CONFIG         (0x1CU)
#define MPU6050_REG_ACCEL_XOUT_H         (0x3BU)
#define MPU6050_REG_PWR_MGMT_1           (0x6BU)
#define MPU6050_REG_WHO_AM_I             (0x75U)
#define MPU6050_WHO_AM_I_MPU6050         (0x68U)
#define MPU6050_WHO_AM_I_MPU6500         (0x70U)
#define MPU6050_PWR_MGMT_1_CLKSEL_X_GYRO (0x01U)
#define MPU6050_PWR_MGMT_1_VERIFY_MASK   (0x47U)
#define MPU6050_SMPLRT_DIV_200_HZ        (0x04U)
#define MPU6050_CONFIG_DLPF_CFG_3        (0x03U)
#define MPU6050_GYRO_CONFIG_2000_DPS     (0x18U)
#define MPU6050_ACCEL_CONFIG_2_G         (0x00U)
#define MPU6050_STARTUP_DELAY_MS         (100U)
#define MPU6050_GYRO_SETTLING_DELAY_MS   (30U)
#define MPU6050_RAW_DATA_LENGTH          (14U)
#define MPU6050_ACCEL_LSB_PER_G           (16384.0F)
#define MPU6050_GYRO_LSB_PER_DPS          (16.4F)
#define MPU6050_GYRO_CALIBRATION_SAMPLES  (200U)
#define MPU6050_GYRO_CALIBRATION_DELAY_MS (5U)
#define MPU6050_TEMP_MPU6050_SCALE        (340.0F)
#define MPU6050_TEMP_MPU6050_OFFSET       (36.53F)
#define MPU6050_TEMP_MPU6500_SCALE        (333.87F)
#define MPU6050_TEMP_MPU6500_OFFSET       (21.0F)
#define MPU6050_RAD_TO_DEG                 (57.295779513F)
#define MPU6050_COMPLEMENTARY_ALPHA        (0.98F)

volatile uint8_t  g_mpu6050_status = MPU6050_STATUS_NOT_INITIALIZED;
volatile uint8_t  g_mpu6050_device = MPU6050_DEVICE_UNKNOWN;
volatile uint8_t  g_mpu6050_calibrated;
volatile uint8_t  g_mpu6050_who_am_i;
volatile uint8_t  g_mpu6050_pwr_mgmt_1;
volatile uint8_t  g_mpu6050_smplrt_div;
volatile uint8_t  g_mpu6050_config;
volatile uint8_t  g_mpu6050_gyro_config;
volatile uint8_t  g_mpu6050_accel_config;
volatile float    g_mpu6050_roll_deg;
volatile float    g_mpu6050_pitch_deg;
volatile uint32_t g_mpu6050_last_fsp_error = (uint32_t) FSP_SUCCESS;
volatile uint32_t g_mpu6050_read_count;
volatile uint32_t g_mpu6050_error_count;
volatile int16_t  g_mpu6050_accel_x;
volatile int16_t  g_mpu6050_accel_y;
volatile int16_t  g_mpu6050_accel_z;
volatile int16_t  g_mpu6050_temperature_raw;
volatile int16_t  g_mpu6050_gyro_x;
volatile int16_t  g_mpu6050_gyro_y;
volatile int16_t  g_mpu6050_gyro_z;
volatile mpu6050_data_t g_mpu6050_data;

static bool g_mpu6050_initialized;
static bool g_mpu6050_attitude_initialized;
static float g_mpu6050_gyro_offset_x;
static float g_mpu6050_gyro_offset_y;
static float g_mpu6050_gyro_offset_z;

static int16_t mpu6050_decode_int16 (uint8_t high_byte, uint8_t low_byte)
{
    return (int16_t) (((uint16_t) high_byte << 8U) | (uint16_t) low_byte);
}

static void mpu6050_update_physical_data (void)
{
    g_mpu6050_data.accel_x_g = (float) g_mpu6050_accel_x / MPU6050_ACCEL_LSB_PER_G;
    g_mpu6050_data.accel_y_g = (float) g_mpu6050_accel_y / MPU6050_ACCEL_LSB_PER_G;
    g_mpu6050_data.accel_z_g = (float) g_mpu6050_accel_z / MPU6050_ACCEL_LSB_PER_G;
    g_mpu6050_data.gyro_x_dps = ((float) g_mpu6050_gyro_x - g_mpu6050_gyro_offset_x) /
                                MPU6050_GYRO_LSB_PER_DPS;
    g_mpu6050_data.gyro_y_dps = ((float) g_mpu6050_gyro_y - g_mpu6050_gyro_offset_y) /
                                MPU6050_GYRO_LSB_PER_DPS;
    g_mpu6050_data.gyro_z_dps = ((float) g_mpu6050_gyro_z - g_mpu6050_gyro_offset_z) /
                                MPU6050_GYRO_LSB_PER_DPS;

    if (MPU6050_DEVICE_MPU6500 == g_mpu6050_device)
    {
        g_mpu6050_data.temperature_c = ((float) g_mpu6050_temperature_raw /
                                        MPU6050_TEMP_MPU6500_SCALE) +
                                       MPU6050_TEMP_MPU6500_OFFSET;
    }
    else
    {
        g_mpu6050_data.temperature_c = ((float) g_mpu6050_temperature_raw /
                                        MPU6050_TEMP_MPU6050_SCALE) +
                                       MPU6050_TEMP_MPU6050_OFFSET;
    }
}

static fsp_err_t mpu6050_read_registers (uint8_t register_address,
                                         uint8_t * p_data,
                                         uint32_t length)
{
    return I2C_Bus_WriteRead(MPU6050_I2C_ADDRESS,
                             &register_address,
                             1U,
                             p_data,
                             length);
}

static fsp_err_t mpu6050_write_register (uint8_t register_address, uint8_t value)
{
    uint8_t payload[2] = {register_address, value};

    return I2C_Bus_Write(MPU6050_I2C_ADDRESS, payload, sizeof(payload));
}

static fsp_err_t mpu6050_record_bus_error (fsp_err_t err)
{
    g_mpu6050_status = MPU6050_STATUS_BUS_ERROR;
    g_mpu6050_last_fsp_error = (uint32_t) err;
    g_mpu6050_error_count++;
    return err;
}

static fsp_err_t mpu6050_write_and_verify (uint8_t register_address,
                                            uint8_t value,
                                            uint8_t verify_mask,
                                            volatile uint8_t * p_readback)
{
    uint8_t readback = 0U;
    fsp_err_t err = mpu6050_write_register(register_address, value);

    if (FSP_SUCCESS != err)
    {
        return mpu6050_record_bus_error(err);
    }

    err = mpu6050_read_registers(register_address, &readback, 1U);
    if (FSP_SUCCESS != err)
    {
        return mpu6050_record_bus_error(err);
    }

    *p_readback = readback;
    if ((value & verify_mask) != (readback & verify_mask))
    {
        g_mpu6050_status = MPU6050_STATUS_CONFIG_VERIFY_FAILED;
        g_mpu6050_last_fsp_error = (uint32_t) FSP_ERR_INVALID_DATA;
        g_mpu6050_error_count++;
        return FSP_ERR_INVALID_DATA;
    }

    return FSP_SUCCESS;
}

fsp_err_t MPU6050_Init (void)
{
    fsp_err_t err;

    g_mpu6050_initialized = false;
    g_mpu6050_status = MPU6050_STATUS_NOT_INITIALIZED;
    g_mpu6050_device = MPU6050_DEVICE_UNKNOWN;
    g_mpu6050_calibrated = 0U;
    g_mpu6050_who_am_i = 0U;
    g_mpu6050_pwr_mgmt_1 = 0U;
    g_mpu6050_smplrt_div = 0U;
    g_mpu6050_config = 0U;
    g_mpu6050_gyro_config = 0U;
    g_mpu6050_accel_config = 0U;
    g_mpu6050_roll_deg = 0.0F;
    g_mpu6050_pitch_deg = 0.0F;
    g_mpu6050_last_fsp_error = (uint32_t) FSP_SUCCESS;
    g_mpu6050_read_count = 0U;
    g_mpu6050_error_count = 0U;
    g_mpu6050_accel_x = 0;
    g_mpu6050_accel_y = 0;
    g_mpu6050_accel_z = 0;
    g_mpu6050_temperature_raw = 0;
    g_mpu6050_gyro_x = 0;
    g_mpu6050_gyro_y = 0;
    g_mpu6050_gyro_z = 0;
    g_mpu6050_data.accel_x_g = 0.0F;
    g_mpu6050_data.accel_y_g = 0.0F;
    g_mpu6050_data.accel_z_g = 0.0F;
    g_mpu6050_data.temperature_c = 0.0F;
    g_mpu6050_data.gyro_x_dps = 0.0F;
    g_mpu6050_data.gyro_y_dps = 0.0F;
    g_mpu6050_data.gyro_z_dps = 0.0F;
    g_mpu6050_gyro_offset_x = 0.0F;
    g_mpu6050_gyro_offset_y = 0.0F;
    g_mpu6050_gyro_offset_z = 0.0F;
    g_mpu6050_attitude_initialized = false;

    R_BSP_SoftwareDelay(MPU6050_STARTUP_DELAY_MS, BSP_DELAY_UNITS_MILLISECONDS);

    err = mpu6050_read_registers(MPU6050_REG_WHO_AM_I,
                                 (uint8_t *) &g_mpu6050_who_am_i,
                                 1U);
    if (FSP_SUCCESS != err)
    {
        return mpu6050_record_bus_error(err);
    }
    if (MPU6050_WHO_AM_I_MPU6050 == g_mpu6050_who_am_i)
    {
        g_mpu6050_device = MPU6050_DEVICE_MPU6050;
    }
    else if (MPU6050_WHO_AM_I_MPU6500 == g_mpu6050_who_am_i)
    {
        g_mpu6050_device = MPU6050_DEVICE_MPU6500;
    }
    else
    {
        g_mpu6050_status = MPU6050_STATUS_ID_MISMATCH;
        g_mpu6050_last_fsp_error = (uint32_t) FSP_ERR_INVALID_DATA;
        g_mpu6050_error_count++;
        return FSP_ERR_INVALID_DATA;
    }

    err = mpu6050_write_and_verify(MPU6050_REG_PWR_MGMT_1,
                                   MPU6050_PWR_MGMT_1_CLKSEL_X_GYRO,
                                   MPU6050_PWR_MGMT_1_VERIFY_MASK,
                                   &g_mpu6050_pwr_mgmt_1);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    R_BSP_SoftwareDelay(MPU6050_GYRO_SETTLING_DELAY_MS, BSP_DELAY_UNITS_MILLISECONDS);

    err = mpu6050_write_and_verify(MPU6050_REG_SMPLRT_DIV,
                                   MPU6050_SMPLRT_DIV_200_HZ,
                                   0xFFU,
                                   &g_mpu6050_smplrt_div);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    err = mpu6050_write_and_verify(MPU6050_REG_CONFIG,
                                   MPU6050_CONFIG_DLPF_CFG_3,
                                   0x07U,
                                   &g_mpu6050_config);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    err = mpu6050_write_and_verify(MPU6050_REG_GYRO_CONFIG,
                                   MPU6050_GYRO_CONFIG_2000_DPS,
                                   0x18U,
                                   &g_mpu6050_gyro_config);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    err = mpu6050_write_and_verify(MPU6050_REG_ACCEL_CONFIG,
                                   MPU6050_ACCEL_CONFIG_2_G,
                                   0x18U,
                                   &g_mpu6050_accel_config);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    g_mpu6050_initialized = true;
    g_mpu6050_status = MPU6050_STATUS_READY;
    g_mpu6050_last_fsp_error = (uint32_t) FSP_SUCCESS;
    return FSP_SUCCESS;
}

fsp_err_t MPU6050_ReadRaw (void)
{
    uint8_t raw_data[MPU6050_RAW_DATA_LENGTH];
    fsp_err_t err;

    if (!g_mpu6050_initialized)
    {
        return FSP_ERR_NOT_OPEN;
    }

    err = mpu6050_read_registers(MPU6050_REG_ACCEL_XOUT_H,
                                 raw_data,
                                 sizeof(raw_data));
    if (FSP_SUCCESS != err)
    {
        return mpu6050_record_bus_error(err);
    }

    g_mpu6050_accel_x = mpu6050_decode_int16(raw_data[0], raw_data[1]);
    g_mpu6050_accel_y = mpu6050_decode_int16(raw_data[2], raw_data[3]);
    g_mpu6050_accel_z = mpu6050_decode_int16(raw_data[4], raw_data[5]);
    g_mpu6050_temperature_raw = mpu6050_decode_int16(raw_data[6], raw_data[7]);
    g_mpu6050_gyro_x = mpu6050_decode_int16(raw_data[8], raw_data[9]);
    g_mpu6050_gyro_y = mpu6050_decode_int16(raw_data[10], raw_data[11]);
    g_mpu6050_gyro_z = mpu6050_decode_int16(raw_data[12], raw_data[13]);
    mpu6050_update_physical_data();
    g_mpu6050_read_count++;
    g_mpu6050_status = MPU6050_STATUS_READ_OK;
    g_mpu6050_last_fsp_error = (uint32_t) FSP_SUCCESS;
    return FSP_SUCCESS;
}

fsp_err_t MPU6050_CalibrateGyro (void)
{
    int32_t sum_x = 0;
    int32_t sum_y = 0;
    int32_t sum_z = 0;
    fsp_err_t err;

    if (!g_mpu6050_initialized)
    {
        return FSP_ERR_NOT_OPEN;
    }

    g_mpu6050_calibrated = 0U;
    g_mpu6050_gyro_offset_x = 0.0F;
    g_mpu6050_gyro_offset_y = 0.0F;
    g_mpu6050_gyro_offset_z = 0.0F;

    for (uint32_t sample = 0U; sample < MPU6050_GYRO_CALIBRATION_SAMPLES; sample++)
    {
        err = MPU6050_ReadRaw();
        if (FSP_SUCCESS != err)
        {
            return err;
        }

        sum_x += g_mpu6050_gyro_x;
        sum_y += g_mpu6050_gyro_y;
        sum_z += g_mpu6050_gyro_z;
        R_BSP_SoftwareDelay(MPU6050_GYRO_CALIBRATION_DELAY_MS, BSP_DELAY_UNITS_MILLISECONDS);
    }

    g_mpu6050_gyro_offset_x = (float) sum_x / (float) MPU6050_GYRO_CALIBRATION_SAMPLES;
    g_mpu6050_gyro_offset_y = (float) sum_y / (float) MPU6050_GYRO_CALIBRATION_SAMPLES;
    g_mpu6050_gyro_offset_z = (float) sum_z / (float) MPU6050_GYRO_CALIBRATION_SAMPLES;
    g_mpu6050_calibrated = 1U;

    return MPU6050_ReadRaw();
}

fsp_err_t MPU6050_UpdateAttitude (float delta_time_s)
{
    float accel_roll_deg;
    float accel_pitch_deg;
    float accel_y_g;
    float accel_z_g;
    fsp_err_t err;

    if (delta_time_s <= 0.0F)
    {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    err = MPU6050_ReadRaw();
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    accel_y_g = g_mpu6050_data.accel_y_g;
    accel_z_g = g_mpu6050_data.accel_z_g;
    accel_roll_deg = atan2f(accel_y_g, accel_z_g) * MPU6050_RAD_TO_DEG;
    accel_pitch_deg = atan2f(-g_mpu6050_data.accel_x_g,
                            sqrtf((accel_y_g * accel_y_g) +
                                  (accel_z_g * accel_z_g))) * MPU6050_RAD_TO_DEG;

    if (!g_mpu6050_attitude_initialized)
    {
        g_mpu6050_roll_deg = accel_roll_deg;
        g_mpu6050_pitch_deg = accel_pitch_deg;
        g_mpu6050_attitude_initialized = true;
    }
    else
    {
        g_mpu6050_roll_deg = MPU6050_COMPLEMENTARY_ALPHA *
                             (g_mpu6050_roll_deg +
                              (g_mpu6050_data.gyro_x_dps * delta_time_s)) +
                             ((1.0F - MPU6050_COMPLEMENTARY_ALPHA) * accel_roll_deg);
        g_mpu6050_pitch_deg = MPU6050_COMPLEMENTARY_ALPHA *
                              (g_mpu6050_pitch_deg +
                               (g_mpu6050_data.gyro_y_dps * delta_time_s)) +
                              ((1.0F - MPU6050_COMPLEMENTARY_ALPHA) * accel_pitch_deg);
    }

    return FSP_SUCCESS;
}
