#ifndef OLED_H
#define OLED_H

#include "hal_data.h"
#include <stdint.h>

/* Driver state values kept visible in Keil Watch. */
#define OLED_STATUS_NOT_INITIALIZED    (0U)
#define OLED_STATUS_READY              (1U)
#define OLED_STATUS_OPEN_FAILED        (2U)
#define OLED_STATUS_TRANSFER_FAILED    (3U)
#define OLED_STATUS_TRANSFER_ABORTED   (4U)
#define OLED_STATUS_TRANSFER_TIMEOUT   (5U)

extern volatile uint8_t  g_oled_status;
extern volatile uint32_t g_oled_last_fsp_error;
extern volatile uint32_t g_oled_transfer_count;
extern volatile uint32_t g_oled_abort_count;
extern volatile uint32_t g_oled_timeout_count;

fsp_err_t OLED_Init(void);
fsp_err_t OLED_Clear(void);
fsp_err_t OLED_WriteCommand(uint8_t command);
fsp_err_t OLED_WriteData(uint8_t data);
fsp_err_t OLED_SetCursor(uint8_t y, uint8_t x);
fsp_err_t OLED_ShowChar(uint8_t line, uint8_t column, char character);
fsp_err_t OLED_ShowString(uint8_t line, uint8_t column, char const * p_string);
fsp_err_t OLED_ShowNum(uint8_t line, uint8_t column, uint32_t number, uint8_t length);
fsp_err_t OLED_ShowSignedNum(uint8_t line, uint8_t column, int32_t number, uint8_t length);
fsp_err_t OLED_ShowHexNum(uint8_t line, uint8_t column, uint32_t number, uint8_t length);
fsp_err_t OLED_ShowBinNum(uint8_t line, uint8_t column, uint32_t number, uint8_t length);
fsp_err_t OLED_ShowChinese(uint8_t x, uint8_t y, uint8_t const * p_font_data);

#endif
