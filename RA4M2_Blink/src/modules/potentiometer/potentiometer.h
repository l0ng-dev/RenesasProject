#ifndef POTENTIOMETER_H
#define POTENTIOMETER_H

#include "hal_data.h"
#include <stdint.h>

extern volatile uint8_t  g_pot_adc_result;
extern volatile uint32_t g_pot_last_fsp_error;
extern volatile uint16_t g_pot_raw;
extern volatile uint16_t g_pot_min;
extern volatile uint16_t g_pot_max;
extern volatile uint16_t g_pot_percent_x10;
extern volatile uint32_t g_pot_sample_count;

void Potentiometer_Init (void);
void Potentiometer_Sample (void);

#endif
