#include "i2c_bus.h"

#include <stddef.h>

#define I2C_BUS_TRANSFER_TIMEOUT_MS    (100U)
#define I2C_BUS_MAX_7BIT_ADDRESS       (0x7FU)

volatile uint8_t  g_i2c_bus_status = I2C_BUS_STATUS_NOT_INITIALIZED;
volatile uint8_t  g_i2c_bus_active_address;
volatile uint32_t g_i2c_bus_last_fsp_error = (uint32_t) FSP_SUCCESS;
volatile uint32_t g_i2c_bus_last_event;
volatile uint32_t g_i2c_bus_transfer_count;
volatile uint32_t g_i2c_bus_abort_count;
volatile uint32_t g_i2c_bus_timeout_count;

static volatile i2c_master_event_t g_i2c_bus_event;
static bool g_i2c_bus_initialized;
static bool g_i2c_bus_in_use;

void i2c_bus_callback (i2c_master_callback_args_t * p_args)
{
    if (NULL != p_args)
    {
        g_i2c_bus_event = p_args->event;
        g_i2c_bus_last_event = (uint32_t) p_args->event;
    }
}

static void i2c_bus_release (void)
{
    g_i2c_bus_in_use = false;
    if (g_i2c_bus_initialized)
    {
        g_i2c_bus_status = I2C_BUS_STATUS_READY;
    }
}

static fsp_err_t i2c_bus_abort (fsp_err_t result, uint8_t status)
{
    (void) g_i2c_oled.p_api->abort(g_i2c_oled.p_ctrl);
    g_i2c_bus_status = status;
    g_i2c_bus_last_fsp_error = (uint32_t) result;
    if (I2C_BUS_STATUS_ABORTED == status)
    {
        g_i2c_bus_abort_count++;
    }
    else if (I2C_BUS_STATUS_TIMEOUT == status)
    {
        g_i2c_bus_timeout_count++;
    }
    g_i2c_bus_in_use = false;
    return result;
}

static fsp_err_t i2c_bus_wait (i2c_master_event_t expected_event)
{
    for (uint32_t elapsed = 0U; elapsed < I2C_BUS_TRANSFER_TIMEOUT_MS; elapsed++)
    {
        i2c_master_event_t const event = g_i2c_bus_event;

        if (expected_event == event)
        {
            return FSP_SUCCESS;
        }
        if (I2C_MASTER_EVENT_ABORTED == event)
        {
            g_i2c_bus_status = I2C_BUS_STATUS_ABORTED;
            g_i2c_bus_last_fsp_error = (uint32_t) FSP_ERR_ABORTED;
            g_i2c_bus_abort_count++;
            return FSP_ERR_ABORTED;
        }

        R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MILLISECONDS);
    }

    return i2c_bus_abort(FSP_ERR_TIMEOUT, I2C_BUS_STATUS_TIMEOUT);
}

static fsp_err_t i2c_bus_begin (uint8_t device_address)
{
    fsp_err_t err;

    if (!g_i2c_bus_initialized)
    {
        return FSP_ERR_NOT_OPEN;
    }
    if (device_address > I2C_BUS_MAX_7BIT_ADDRESS)
    {
        return FSP_ERR_INVALID_ARGUMENT;
    }
    if (g_i2c_bus_in_use)
    {
        return FSP_ERR_IN_USE;
    }

    g_i2c_bus_in_use = true;
    g_i2c_bus_status = I2C_BUS_STATUS_BUSY;
    g_i2c_bus_active_address = device_address;
    g_i2c_bus_last_fsp_error = (uint32_t) FSP_SUCCESS;
    g_i2c_bus_event = (i2c_master_event_t) 0;

    err = g_i2c_oled.p_api->slaveAddressSet(g_i2c_oled.p_ctrl,
                                             device_address,
                                             I2C_MASTER_ADDR_MODE_7BIT);
    if (FSP_SUCCESS != err)
    {
        g_i2c_bus_status = I2C_BUS_STATUS_ADDRESS_FAILED;
        g_i2c_bus_last_fsp_error = (uint32_t) err;
        g_i2c_bus_in_use = false;
    }

    return err;
}

fsp_err_t I2C_Bus_Init (void)
{
    fsp_err_t err;

    if (g_i2c_bus_initialized)
    {
        return FSP_SUCCESS;
    }

    g_i2c_bus_status = I2C_BUS_STATUS_NOT_INITIALIZED;
    g_i2c_bus_active_address = 0U;
    g_i2c_bus_last_fsp_error = (uint32_t) FSP_SUCCESS;
    g_i2c_bus_last_event = 0U;
    g_i2c_bus_transfer_count = 0U;
    g_i2c_bus_abort_count = 0U;
    g_i2c_bus_timeout_count = 0U;
    g_i2c_bus_event = (i2c_master_event_t) 0;
    g_i2c_bus_in_use = false;

    err = g_i2c_oled.p_api->open(g_i2c_oled.p_ctrl, g_i2c_oled.p_cfg);
    if (FSP_SUCCESS != err)
    {
        g_i2c_bus_last_fsp_error = (uint32_t) err;
        return err;
    }

    g_i2c_bus_initialized = true;
    g_i2c_bus_status = I2C_BUS_STATUS_READY;
    return FSP_SUCCESS;
}

bool I2C_Bus_IsReady (void)
{
    return g_i2c_bus_initialized && !g_i2c_bus_in_use;
}

fsp_err_t I2C_Bus_Write (uint8_t device_address, uint8_t const * p_data, uint32_t length)
{
    fsp_err_t err;

    if ((NULL == p_data) || (0U == length))
    {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    err = i2c_bus_begin(device_address);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    err = g_i2c_oled.p_api->write(g_i2c_oled.p_ctrl, (uint8_t *) p_data, length, false);
    if (FSP_SUCCESS != err)
    {
        g_i2c_bus_status = I2C_BUS_STATUS_TRANSFER_FAILED;
        g_i2c_bus_last_fsp_error = (uint32_t) err;
        g_i2c_bus_in_use = false;
        return err;
    }

    err = i2c_bus_wait(I2C_MASTER_EVENT_TX_COMPLETE);
    if (FSP_SUCCESS == err)
    {
        g_i2c_bus_transfer_count++;
        i2c_bus_release();
    }
    else if (FSP_ERR_TIMEOUT != err)
    {
        g_i2c_bus_in_use = false;
    }

    return err;
}

fsp_err_t I2C_Bus_WriteRead (uint8_t device_address,
                             uint8_t const * p_write_data,
                             uint32_t write_length,
                             uint8_t * p_read_data,
                             uint32_t read_length)
{
    fsp_err_t err;

    if ((NULL == p_write_data) || (0U == write_length) ||
        (NULL == p_read_data) || (0U == read_length))
    {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    err = i2c_bus_begin(device_address);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    err = g_i2c_oled.p_api->write(g_i2c_oled.p_ctrl,
                                   (uint8_t *) p_write_data,
                                   write_length,
                                   true);
    if (FSP_SUCCESS != err)
    {
        g_i2c_bus_status = I2C_BUS_STATUS_TRANSFER_FAILED;
        g_i2c_bus_last_fsp_error = (uint32_t) err;
        g_i2c_bus_in_use = false;
        return err;
    }

    err = i2c_bus_wait(I2C_MASTER_EVENT_TX_COMPLETE);
    if (FSP_SUCCESS != err)
    {
        if (FSP_ERR_TIMEOUT != err)
        {
            g_i2c_bus_in_use = false;
        }
        return err;
    }

    g_i2c_bus_event = (i2c_master_event_t) 0;
    err = g_i2c_oled.p_api->read(g_i2c_oled.p_ctrl, p_read_data, read_length, false);
    if (FSP_SUCCESS != err)
    {
        return i2c_bus_abort(err, I2C_BUS_STATUS_TRANSFER_FAILED);
    }

    err = i2c_bus_wait(I2C_MASTER_EVENT_RX_COMPLETE);
    if (FSP_SUCCESS == err)
    {
        g_i2c_bus_transfer_count++;
        i2c_bus_release();
    }
    else if (FSP_ERR_TIMEOUT != err)
    {
        g_i2c_bus_in_use = false;
    }

    return err;
}
