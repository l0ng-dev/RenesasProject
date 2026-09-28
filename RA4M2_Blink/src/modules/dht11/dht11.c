#include "dht11.h"

#define DHT11_START_LOW_MS              (20U)
#define DHT11_HOST_RELEASE_US           (13U)
#define DHT11_RESPONSE_TIMEOUT_US       (120U)
#define DHT11_BIT_TIMEOUT_US            (100U)
#define DHT11_BIT_SAMPLE_US             (40U)

volatile uint8_t  g_dht11_status;
volatile uint32_t g_dht11_last_fsp_error;
volatile uint8_t  g_dht11_humidity_integer;
volatile uint8_t  g_dht11_humidity_decimal;
volatile uint8_t  g_dht11_temperature_integer;
volatile uint8_t  g_dht11_temperature_decimal;
volatile uint8_t  g_dht11_checksum;
volatile uint8_t  g_dht11_raw_data[5];
volatile uint8_t  g_dht11_stage;
volatile uint8_t  g_dht11_idle_level;
volatile uint8_t  g_dht11_release_level;
volatile uint8_t  g_dht11_last_level;
volatile uint32_t g_dht11_read_count;
volatile uint32_t g_dht11_timeout_count;
volatile uint32_t g_dht11_checksum_error_count;

static fsp_err_t dht11_wait_for_level (bsp_io_level_t target_level, uint32_t timeout_us)
{
    bsp_io_level_t level = BSP_IO_LEVEL_LOW;

    while (timeout_us-- > 0U)
    {
        fsp_err_t err = R_IOPORT_PinRead(&g_ioport_ctrl, DHT11_DATA, &level);
        if (FSP_SUCCESS != err)
        {
            return err;
        }
        g_dht11_last_level = (uint8_t) level;
        if (target_level == level)
        {
            return FSP_SUCCESS;
        }

        R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MICROSECONDS);
    }

    return FSP_ERR_TIMEOUT;
}

static fsp_err_t dht11_release_bus (void)
{
    return R_IOPORT_PinCfg(&g_ioport_ctrl,
                           DHT11_DATA,
                           (uint32_t) IOPORT_CFG_PORT_DIRECTION_INPUT);
}

fsp_err_t DHT11_Init (void)
{
    fsp_err_t err;

    g_dht11_status = DHT11_STATUS_NOT_INITIALIZED;
    g_dht11_last_fsp_error = (uint32_t) FSP_SUCCESS;
    g_dht11_humidity_integer = 0U;
    g_dht11_humidity_decimal = 0U;
    g_dht11_temperature_integer = 0U;
    g_dht11_temperature_decimal = 0U;
    g_dht11_checksum = 0U;
    g_dht11_stage = 0U;
    g_dht11_idle_level = (uint8_t) BSP_IO_LEVEL_LOW;
    g_dht11_release_level = (uint8_t) BSP_IO_LEVEL_LOW;
    g_dht11_last_level = (uint8_t) BSP_IO_LEVEL_HIGH;
    g_dht11_read_count = 0U;
    g_dht11_timeout_count = 0U;
    g_dht11_checksum_error_count = 0U;
    for (uint8_t index = 0U; index < 5U; index++)
    {
        g_dht11_raw_data[index] = 0U;
    }

    err = dht11_release_bus();
    if (FSP_SUCCESS != err)
    {
        g_dht11_status = DHT11_STATUS_START_FAILED;
        g_dht11_last_fsp_error = (uint32_t) err;
        return err;
    }

    g_dht11_status = DHT11_STATUS_READY;
    return FSP_SUCCESS;
}

fsp_err_t DHT11_Read (void)
{
    uint8_t data[5] = {0U};
    bsp_io_level_t level = BSP_IO_LEVEL_LOW;
    fsp_err_t err;

    if (DHT11_STATUS_NOT_INITIALIZED == g_dht11_status)
    {
        g_dht11_last_fsp_error = (uint32_t) FSP_ERR_NOT_INITIALIZED;
        return FSP_ERR_NOT_INITIALIZED;
    }

    err = R_IOPORT_PinRead(&g_ioport_ctrl, DHT11_DATA, &level);
    if (FSP_SUCCESS != err)
    {
        g_dht11_status = DHT11_STATUS_START_FAILED;
        g_dht11_last_fsp_error = (uint32_t) err;
        return err;
    }
    g_dht11_idle_level = (uint8_t) level;
    g_dht11_last_level = (uint8_t) level;
    if (BSP_IO_LEVEL_HIGH != level)
    {
        g_dht11_status = DHT11_STATUS_START_FAILED;
        g_dht11_last_fsp_error = (uint32_t) FSP_ERR_INVALID_HW_CONDITION;
        return FSP_ERR_INVALID_HW_CONDITION;
    }

    err = R_IOPORT_PinCfg(&g_ioport_ctrl,
                          DHT11_DATA,
                          (uint32_t) IOPORT_CFG_PORT_DIRECTION_OUTPUT |
                          (uint32_t) IOPORT_CFG_PORT_OUTPUT_LOW);
    if (FSP_SUCCESS != err)
    {
        g_dht11_status = DHT11_STATUS_START_FAILED;
        g_dht11_last_fsp_error = (uint32_t) err;
        return err;
    }
    g_dht11_stage = 1U;

    /* The product manual requires the host start pulse to remain low for at least 18 ms. */
    R_BSP_SoftwareDelay(DHT11_START_LOW_MS, BSP_DELAY_UNITS_MILLISECONDS);

    err = dht11_release_bus();
    if (FSP_SUCCESS != err)
    {
        g_dht11_status = DHT11_STATUS_START_FAILED;
        g_dht11_last_fsp_error = (uint32_t) err;
        return err;
    }
    g_dht11_stage = 2U;

    /* The V1.3 product manual specifies a 10-20 us host release interval
     * (13 us typical) before the sensor's 81-85 us response-low pulse. */
    R_BSP_SoftwareDelay(DHT11_HOST_RELEASE_US, BSP_DELAY_UNITS_MICROSECONDS);
    err = R_IOPORT_PinRead(&g_ioport_ctrl, DHT11_DATA, &level);
    if (FSP_SUCCESS != err)
    {
        g_dht11_status = DHT11_STATUS_START_FAILED;
        g_dht11_last_fsp_error = (uint32_t) err;
        return err;
    }
    g_dht11_release_level = (uint8_t) level;
    g_dht11_last_level = (uint8_t) level;
    g_dht11_stage = 3U;

    err = dht11_wait_for_level(BSP_IO_LEVEL_LOW, DHT11_RESPONSE_TIMEOUT_US);
    if (FSP_SUCCESS != err)
    {
        g_dht11_status = DHT11_STATUS_RESPONSE_TIMEOUT;
        g_dht11_last_fsp_error = (uint32_t) err;
        g_dht11_timeout_count++;
        return err;
    }
    g_dht11_stage = 4U;

    err = dht11_wait_for_level(BSP_IO_LEVEL_HIGH, DHT11_RESPONSE_TIMEOUT_US);
    if (FSP_SUCCESS != err)
    {
        g_dht11_status = DHT11_STATUS_RESPONSE_TIMEOUT;
        g_dht11_last_fsp_error = (uint32_t) err;
        g_dht11_timeout_count++;
        return err;
    }
    g_dht11_stage = 5U;

    err = dht11_wait_for_level(BSP_IO_LEVEL_LOW, DHT11_RESPONSE_TIMEOUT_US);
    if (FSP_SUCCESS != err)
    {
        g_dht11_status = DHT11_STATUS_RESPONSE_TIMEOUT;
        g_dht11_last_fsp_error = (uint32_t) err;
        g_dht11_timeout_count++;
        return err;
    }
    g_dht11_stage = 6U;

    for (uint8_t bit_index = 0U; bit_index < 40U; bit_index++)
    {
        bsp_io_level_t sampled_level = BSP_IO_LEVEL_LOW;

        g_dht11_stage = 7U;
        err = dht11_wait_for_level(BSP_IO_LEVEL_HIGH, DHT11_BIT_TIMEOUT_US);
        if (FSP_SUCCESS != err)
        {
            g_dht11_status = DHT11_STATUS_DATA_TIMEOUT;
            g_dht11_last_fsp_error = (uint32_t) err;
            g_dht11_timeout_count++;
            return err;
        }

        /* A zero pulse has ended and a one pulse is still high at this point. */
        R_BSP_SoftwareDelay(DHT11_BIT_SAMPLE_US, BSP_DELAY_UNITS_MICROSECONDS);
        err = R_IOPORT_PinRead(&g_ioport_ctrl, DHT11_DATA, &sampled_level);
        if (FSP_SUCCESS != err)
        {
            g_dht11_status = DHT11_STATUS_DATA_TIMEOUT;
            g_dht11_last_fsp_error = (uint32_t) err;
            return err;
        }
        g_dht11_last_level = (uint8_t) sampled_level;

        data[bit_index / 8U] <<= 1U;
        if (BSP_IO_LEVEL_HIGH == sampled_level)
        {
            data[bit_index / 8U] |= 1U;
            err = dht11_wait_for_level(BSP_IO_LEVEL_LOW, DHT11_BIT_TIMEOUT_US);
            if (FSP_SUCCESS != err)
            {
                g_dht11_status = DHT11_STATUS_DATA_TIMEOUT;
                g_dht11_last_fsp_error = (uint32_t) err;
                g_dht11_timeout_count++;
                return err;
            }
        }
    }

    for (uint8_t index = 0U; index < 5U; index++)
    {
        g_dht11_raw_data[index] = data[index];
    }

    if ((uint8_t) (data[0] + data[1] + data[2] + data[3]) != data[4])
    {
        g_dht11_status = DHT11_STATUS_CHECKSUM_FAILED;
        g_dht11_last_fsp_error = (uint32_t) FSP_ERR_INVALID_DATA;
        g_dht11_checksum_error_count++;
        return FSP_ERR_INVALID_DATA;
    }

    g_dht11_humidity_integer = data[0];
    g_dht11_humidity_decimal = data[1];
    g_dht11_temperature_integer = data[2];
    g_dht11_temperature_decimal = data[3];
    g_dht11_checksum = data[4];
    g_dht11_read_count++;
    g_dht11_stage = 8U;
    g_dht11_status = DHT11_STATUS_READ_OK;
    g_dht11_last_fsp_error = (uint32_t) FSP_SUCCESS;
    return FSP_SUCCESS;
}
