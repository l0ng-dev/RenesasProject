#ifndef DA16200_H
#define DA16200_H

#include "hal_data.h"
#include <stdbool.h>
#include <stdint.h>

bool DA16200_Connect (void);
bool DA16200_IsReady (void);
void DA16200_ServiceDelay (uint32_t delay_ms);

void user_uart_callback (uart_callback_args_t * p_args);

#endif
