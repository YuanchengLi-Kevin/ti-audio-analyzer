#include "ti_msp_dl_config.h"
#include <stdbool.h>
#include <stdint.h>

#define FFT_SIZE 1024
#define ADC_DAC_MIDPOINT 2048
#define MONITOR_GAIN_NUM 1
#define MONITOR_GAIN_DEN 8

// Ping-pong buffers reserved for future FFT processing.
uint16_t adc_buffer_A[FFT_SIZE];
uint16_t adc_buffer_B[FFT_SIZE];

volatile bool buffer_A_ready = false;
volatile bool buffer_B_ready = false;

static uint16_t scale_audio_sample(uint16_t sample) {
  int32_t centered = (int32_t)sample - ADC_DAC_MIDPOINT;
  int32_t scaled =
      ADC_DAC_MIDPOINT + ((centered * MONITOR_GAIN_NUM) / MONITOR_GAIN_DEN);

  if (scaled < 0) {
    return 0;
  }
  if (scaled > 4095) {
    return 4095;
  }
  return (uint16_t)scaled;
}

int main(void) {
  SYSCFG_DL_init();

  NVIC_EnableIRQ(AUDIO_ADC_INST_INT_IRQN);

  DL_ADC12_startConversion(AUDIO_ADC_INST);

  DL_Timer_startCounter(ADC_SAMPLE_TIMER_INST);

  DL_GPIO_clearPins(AMP_CONTROL_PORT, AMP_CONTROL_MAIN_PIN);
  DL_GPIO_setPins(MICROPHONE_PORT, MICROPHONE_PWR_PIN);

  while (1) {
    if (buffer_A_ready) {
      buffer_A_ready = false;

      // Future FFT processing for adc_buffer_A.
    }

    if (buffer_B_ready) {
      buffer_B_ready = false;

      // Future FFT processing for adc_buffer_B.
    }
  }
}

void AUDIO_ADC_INST_IRQHandler(void) {
  static uint16_t sample_index = 0;
  static bool filling_buffer_A = true;

  switch (DL_ADC12_getPendingInterrupt(AUDIO_ADC_INST)) {
  case DL_ADC12_IIDX_MEM0_RESULT_LOADED: {
    uint16_t mic_voltage =
        DL_ADC12_getMemResult(AUDIO_ADC_INST, DL_ADC12_MEM_IDX_0);

    // Reduce monitor gain to limit mic/speaker feedback.
    DL_DAC12_output12(DAC0, scale_audio_sample(mic_voltage));

    if (filling_buffer_A) {
      adc_buffer_A[sample_index] = mic_voltage;
    } else {
      adc_buffer_B[sample_index] = mic_voltage;
    }
    sample_index++;

    if (sample_index >= FFT_SIZE) {
      sample_index = 0;

      if (filling_buffer_A) {
        buffer_A_ready = true;
        filling_buffer_A = false;
      } else {
        buffer_B_ready = true;
        filling_buffer_A = true;
      }
    }
    break;
  }
  default:
    break;
  }

  DL_ADC12_clearInterruptStatus(AUDIO_ADC_INST,
                                DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED);
  DL_ADC12_enableConversions(AUDIO_ADC_INST);
}
