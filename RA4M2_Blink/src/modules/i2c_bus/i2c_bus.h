#ifndef I2C_BUS_H
#define I2C_BUS_H

#include "hal_data.h"

#include <stdbool.h>
#include <stdint.h>

#define I2C_BUS_STATUS_NOT_INITIALIZED    (0U)
#define I2C_BUS_STATUS_READY              (1U)
#define I2C_BUS_STATUS_BUSY               (2U)
#define I2C_BUS_STATUS_ADDRESS_FAILED     (3U)
#define I2C_BUS_STATUS_TRANSFER_FAILED    (4U)
#define I2C_BUS_STATUS_ABORTED             (5U)
#define I2C_BUS_STATUS_TIMEOUT             (6U)

extern volatile uint8_t  g_i2c_bus_status;
extern volatile uint8_t  g_i2c_bus_active_address;
extern volatile uint32_t g_i2c_bus_last_fsp_error;
extern volatile uint32_t g_i2c_bus_last_event;
extern volatile uint32_t g_i2c_bus_transfer_count;
extern volatile uint32_t g_i2c_bus_abort_count;
extern volatile uint32_t g_i2c_bus_timeout_count;

fsp_err_t I2C_Bus_Init(void);
bool I2C_Bus_IsReady(void);
fsp_err_t I2C_Bus_Write(uint8_t device_address, uint8_t const * p_data, uint32_t length);
fsp_err_t I2C_Bus_WriteRead(uint8_t device_address,
                            uint8_t const * p_write_data,
                            uint32_t write_length,
                            uint8_t * p_read_data,
                            uint32_t read_length);

#endif
