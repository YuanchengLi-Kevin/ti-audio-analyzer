#ifndef UTILS_LCD_H_
#define UTILS_LCD_H_

#include "fft.h"

#include <stdint.h>

void lcd_init(void);
void lcd_clear_spectrum(void);
void lcd_update_spectrum(const uint16_t magnitudes[AUDIO_FFT_BIN_COUNT]);
void lcd_update_from_latest_spectrum(void);

#endif
