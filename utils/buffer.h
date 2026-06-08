#ifndef UTILS_BUFFER_H
#define UTILS_BUFFER_H

#include "fft.h"

#include <stdbool.h>
#include <stdint.h>

void handle_adc_interrupt(void);

void buffer_init(void);

void process_ready(void);

bool buffer_copy_latest_spectrum(uint16_t magnitudes[AUDIO_FFT_BIN_COUNT],
                                 uint32_t *sequence);

#endif
