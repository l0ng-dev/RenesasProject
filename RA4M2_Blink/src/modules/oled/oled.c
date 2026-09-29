#include "oled.h"
#include "oled_font.h"
#include "i2c_bus.h"
#include <stddef.h>

#define OLED_I2C_ADDRESS            (0x3CU)
#define OLED_WIDTH                  (128U)
#define OLED_PAGE_COUNT             (8U)
#define OLED_CONTROL_COMMAND        (0x00U)
#define OLED_CONTROL_DATA           (0x40U)
#define OLED_MAX_PAYLOAD_SIZE       (OLED_WIDTH)

volatile uint8_t  g_oled_status = OLED_STATUS_NOT_INITIALIZED;
volatile uint32_t g_oled_last_fsp_error = (uint32_t) FSP_SUCCESS;
volatile uint32_t g_oled_transfer_count;
volatile uint32_t g_oled_abort_count;
volatile uint32_t g_oled_timeout_count;

static fsp_err_t oled_write (uint8_t control, uint8_t const * p_data, uint32_t length)
{
    uint8_t tx_buffer[OLED_MAX_PAYLOAD_SIZE + 1U];
    fsp_err_t err;

    if ((NULL == p_data) || (0U == length) || (length > OLED_MAX_PAYLOAD_SIZE))
    {
        g_oled_status = OLED_STATUS_TRANSFER_FAILED;
        g_oled_last_fsp_error = (uint32_t) FSP_ERR_INVALID_ARGUMENT;
        return FSP_ERR_INVALID_ARGUMENT;
    }

    tx_buffer[0] = control;
    for (uint32_t index = 0U; index < length; index++)
    {
        tx_buffer[index + 1U] = p_data[index];
    }

    err = I2C_Bus_Write(OLED_I2C_ADDRESS, tx_buffer, length + 1U);
    if (FSP_SUCCESS == err)
    {
        g_oled_transfer_count++;
        g_oled_status = OLED_STATUS_READY;
        g_oled_last_fsp_error = (uint32_t) FSP_SUCCESS;
        return FSP_SUCCESS;
    }

    if (FSP_ERR_ABORTED == err)
    {
        g_oled_abort_count++;
        g_oled_status = OLED_STATUS_TRANSFER_ABORTED;
    }
    else if (FSP_ERR_TIMEOUT == err)
    {
        g_oled_timeout_count++;
        g_oled_status = OLED_STATUS_TRANSFER_TIMEOUT;
    }
    else
    {
        g_oled_status = OLED_STATUS_TRANSFER_FAILED;
    }
    g_oled_last_fsp_error = (uint32_t) err;
    return err;
}

fsp_err_t OLED_WriteCommand (uint8_t command)
{
    return oled_write(OLED_CONTROL_COMMAND, &command, 1U);
}

fsp_err_t OLED_WriteData (uint8_t data)
{
    return oled_write(OLED_CONTROL_DATA, &data, 1U);
}

fsp_err_t OLED_SetCursor (uint8_t y, uint8_t x)
{
    uint8_t commands[3];

    if ((y >= OLED_PAGE_COUNT) || (x >= OLED_WIDTH))
    {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    commands[0] = (uint8_t) (0xB0U | y);
    commands[1] = (uint8_t) (0x10U | ((x & 0xF0U) >> 4U));
    commands[2] = (uint8_t) (x & 0x0FU);
    return oled_write(OLED_CONTROL_COMMAND, commands, sizeof(commands));
}

fsp_err_t OLED_Clear (void)
{
    uint8_t clear_data[OLED_WIDTH] = {0};

    for (uint8_t page = 0U; page < OLED_PAGE_COUNT; page++)
    {
        fsp_err_t err = OLED_SetCursor(page, 0U);
        if (FSP_SUCCESS != err)
        {
            return err;
        }

        err = oled_write(OLED_CONTROL_DATA, clear_data, sizeof(clear_data));
        if (FSP_SUCCESS != err)
        {
            return err;
        }
    }

    return FSP_SUCCESS;
}

fsp_err_t OLED_Init (void)
{
    static uint8_t const init_commands[] =
    {
        0xAEU,
        0xD5U, 0x80U,
        0xA8U, 0x3FU,
        0xD3U, 0x00U,
        0x40U,
        0xA1U,
        0xC8U,
        0xDAU, 0x12U,
        0x81U, 0xCFU,
        0xD9U, 0xF1U,
        0xDBU, 0x30U,
        0xA4U,
        0xA6U,
        0x8DU, 0x14U,
        0xAFU
    };
    fsp_err_t err;

    g_oled_status = OLED_STATUS_NOT_INITIALIZED;
    g_oled_last_fsp_error = (uint32_t) FSP_SUCCESS;
    g_oled_transfer_count = 0U;
    g_oled_abort_count = 0U;
    g_oled_timeout_count = 0U;
    if (!I2C_Bus_IsReady())
    {
        g_oled_status = OLED_STATUS_OPEN_FAILED;
        g_oled_last_fsp_error = (uint32_t) FSP_ERR_NOT_OPEN;
        return FSP_ERR_NOT_OPEN;
    }

    R_BSP_SoftwareDelay(100U, BSP_DELAY_UNITS_MILLISECONDS);

    err = oled_write(OLED_CONTROL_COMMAND, init_commands, sizeof(init_commands));
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    return OLED_Clear();
}

fsp_err_t OLED_ShowChar (uint8_t line, uint8_t column, char character)
{
    uint8_t font_index;
    fsp_err_t err;

    if ((line < 1U) || (line > 4U) || (column < 1U) || (column > 16U))
    {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    if ((character < ' ') || (character > '~'))
    {
        character = '?';
    }
    font_index = (uint8_t) (character - ' ');

    err = OLED_SetCursor((uint8_t) ((line - 1U) * 2U), (uint8_t) ((column - 1U) * 8U));
    if (FSP_SUCCESS != err)
    {
        return err;
    }
    err = oled_write(OLED_CONTROL_DATA, &OLED_F8x16[(uint32_t) font_index * 16U], 8U);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    err = OLED_SetCursor((uint8_t) (((line - 1U) * 2U) + 1U), (uint8_t) ((column - 1U) * 8U));
    if (FSP_SUCCESS != err)
    {
        return err;
    }
    return oled_write(OLED_CONTROL_DATA, &OLED_F8x16[((uint32_t) font_index * 16U) + 8U], 8U);
}

fsp_err_t OLED_ShowString (uint8_t line, uint8_t column, char const * p_string)
{
    uint8_t pixel_data[OLED_WIDTH];
    uint8_t character_count = 0U;
    uint8_t max_characters;
    fsp_err_t err;

    if ((NULL == p_string) || (line < 1U) || (line > 4U) ||
        (column < 1U) || (column > 16U))
    {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    max_characters = (uint8_t) (17U - column);
    while (('\0' != p_string[character_count]) && (character_count < max_characters))
    {
        character_count++;
    }

    if (0U == character_count)
    {
        return FSP_SUCCESS;
    }

    for (uint8_t index = 0U; index < character_count; index++)
    {
        char character = p_string[index];
        uint8_t font_index;

        if ((character < ' ') || (character > '~'))
        {
            character = '?';
        }
        font_index = (uint8_t) (character - ' ');
        for (uint8_t byte = 0U; byte < 8U; byte++)
        {
            pixel_data[((uint32_t) index * 8U) + byte] =
                OLED_F8x16[((uint32_t) font_index * 16U) + byte];
        }
    }

    err = OLED_SetCursor((uint8_t) ((line - 1U) * 2U), (uint8_t) ((column - 1U) * 8U));
    if (FSP_SUCCESS != err)
    {
        return err;
    }
    err = oled_write(OLED_CONTROL_DATA, pixel_data, (uint32_t) character_count * 8U);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    for (uint8_t index = 0U; index < character_count; index++)
    {
        char character = p_string[index];
        uint8_t font_index;

        if ((character < ' ') || (character > '~'))
        {
            character = '?';
        }
        font_index = (uint8_t) (character - ' ');
        for (uint8_t byte = 0U; byte < 8U; byte++)
        {
            pixel_data[((uint32_t) index * 8U) + byte] =
                OLED_F8x16[((uint32_t) font_index * 16U) + 8U + byte];
        }
    }

    err = OLED_SetCursor((uint8_t) (((line - 1U) * 2U) + 1U), (uint8_t) ((column - 1U) * 8U));
    if (FSP_SUCCESS != err)
    {
        return err;
    }
    return oled_write(OLED_CONTROL_DATA, pixel_data, (uint32_t) character_count * 8U);
}

static uint32_t oled_pow (uint32_t base, uint32_t exponent)
{
    uint32_t result = 1U;

    while (exponent > 0U)
    {
        result *= base;
        exponent--;
    }
    return result;
}

fsp_err_t OLED_ShowNum (uint8_t line, uint8_t column, uint32_t number, uint8_t length)
{
    for (uint8_t index = 0U; index < length; index++)
    {
        char digit = (char) ((number / oled_pow(10U, (uint32_t) (length - index - 1U))) % 10U) + '0';
        fsp_err_t err = OLED_ShowChar(line, (uint8_t) (column + index), digit);
        if (FSP_SUCCESS != err)
        {
            return err;
        }
    }
    return FSP_SUCCESS;
}

fsp_err_t OLED_ShowSignedNum (uint8_t line, uint8_t column, int32_t number, uint8_t length)
{
    uint32_t magnitude;
    fsp_err_t err;

    if (number >= 0)
    {
        err = OLED_ShowChar(line, column, '+');
        magnitude = (uint32_t) number;
    }
    else
    {
        err = OLED_ShowChar(line, column, '-');
        magnitude = (uint32_t) (-(number + 1)) + 1U;
    }

    if (FSP_SUCCESS != err)
    {
        return err;
    }
    return OLED_ShowNum(line, (uint8_t) (column + 1U), magnitude, length);
}

fsp_err_t OLED_ShowHexNum (uint8_t line, uint8_t column, uint32_t number, uint8_t length)
{
    for (uint8_t index = 0U; index < length; index++)
    {
        uint8_t value = (uint8_t) ((number / oled_pow(16U, (uint32_t) (length - index - 1U))) % 16U);
        char character = (value < 10U) ? (char) (value + '0') : (char) (value - 10U + 'A');
        fsp_err_t err = OLED_ShowChar(line, (uint8_t) (column + index), character);
        if (FSP_SUCCESS != err)
        {
            return err;
        }
    }
    return FSP_SUCCESS;
}

fsp_err_t OLED_ShowBinNum (uint8_t line, uint8_t column, uint32_t number, uint8_t length)
{
    for (uint8_t index = 0U; index < length; index++)
    {
        char digit = (char) ((number / oled_pow(2U, (uint32_t) (length - index - 1U))) % 2U) + '0';
        fsp_err_t err = OLED_ShowChar(line, (uint8_t) (column + index), digit);
        if (FSP_SUCCESS != err)
        {
            return err;
        }
    }
    return FSP_SUCCESS;
}

fsp_err_t OLED_ShowChinese (uint8_t x, uint8_t y, uint8_t const * p_font_data)
{
    fsp_err_t err;

    if ((NULL == p_font_data) || (x > (OLED_WIDTH - 16U)) || (y > (OLED_PAGE_COUNT - 2U)))
    {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    err = OLED_SetCursor(y, x);
    if (FSP_SUCCESS != err)
    {
        return err;
    }
    err = oled_write(OLED_CONTROL_DATA, p_font_data, 16U);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    err = OLED_SetCursor((uint8_t) (y + 1U), x);
    if (FSP_SUCCESS != err)
    {
        return err;
    }
    return oled_write(OLED_CONTROL_DATA, &p_font_data[16], 16U);
}
