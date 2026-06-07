#ifndef UTILS_FFT_H_
#define UTILS_FFT_H_

#include <stdint.h>

#define AUDIO_FFT_SIZE 256U
#define AUDIO_FFT_BIN_COUNT (AUDIO_FFT_SIZE / 2U)
#define AUDIO_FFT_ADC_MIDPOINT 2048

void fft_prepare_adc_input(int16_t complex_samples[AUDIO_FFT_SIZE * 2U],
                           const uint16_t samples[AUDIO_FFT_SIZE]);

void fft_analyze_adc_block(uint16_t magnitudes[AUDIO_FFT_BIN_COUNT],
                           const uint16_t samples[AUDIO_FFT_SIZE]);

#endif
