#include "fft.h"

#include "third_party/CMSIS/DSP/Include/arm_const_structs.h"
#include "third_party/CMSIS/DSP/Include/arm_math.h"

#define FFT_IFFT_FLAG 0U
#define FFT_BIT_REVERSE 1U

static q15_t fft_buffer[AUDIO_FFT_SIZE * 2U];
static q15_t magnitude_buffer[AUDIO_FFT_BIN_COUNT];

void fft_prepare_adc_input(int16_t complex_samples[AUDIO_FFT_SIZE * 2U], const uint16_t samples[AUDIO_FFT_SIZE])
{
  for (uint32_t i = 0; i < AUDIO_FFT_SIZE; i++)
  {
    int32_t centered = (int32_t)(samples[i] & 0x0FFFU) - AUDIO_FFT_ADC_MIDPOINT;

    complex_samples[(i * 2U)] = (q15_t)(centered << 4);
    complex_samples[(i * 2U) + 1U] = 0;
  }
}

void fft_analyze_adc_block(uint16_t magnitudes[AUDIO_FFT_BIN_COUNT], const uint16_t samples[AUDIO_FFT_SIZE])
{
  fft_prepare_adc_input(fft_buffer, samples);

  arm_cfft_q15(&arm_cfft_sR_q15_len256, fft_buffer, FFT_IFFT_FLAG, FFT_BIT_REVERSE);
  arm_cmplx_mag_q15(fft_buffer, magnitude_buffer, AUDIO_FFT_BIN_COUNT);

  for (uint32_t i = 0; i < AUDIO_FFT_BIN_COUNT; i++)
  {
    magnitudes[i] = (uint16_t)magnitude_buffer[i];
  }
}
