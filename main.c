#include "ILI9341.h"
#include "ti_msp_dl_config.h"
#include <stdbool.h>
#include <stdint.h>

#define FFT_SIZE 256
#define ADC_FIFO_WORDS FFT_SIZE

#if OUTPUT_DMA_CHAN_ID == 0
#define OUTPUT_DMA_INTERRUPT DL_DMA_INTERRUPT_CHANNEL0
#define OUTPUT_DMA_IIDX DL_DMA_EVENT_IIDX_DMACH0
#else
#error "Add output DMA interrupt mapping for this OUTPUT_DMA_CHAN_ID"
#endif

// In this single-sample FIFO mode, each DMA word advances one ADC sample.
volatile uint32_t adc_fifo_buffer_A[ADC_FIFO_WORDS];
volatile uint32_t adc_fifo_buffer_B[ADC_FIFO_WORDS];
uint16_t adc_buffer_A[FFT_SIZE];
uint16_t adc_buffer_B[FFT_SIZE];
uint16_t dac_buffer_A[FFT_SIZE];
uint16_t dac_buffer_B[FFT_SIZE];

volatile bool buffer_A_ready = false;
volatile bool buffer_B_ready = false;
volatile bool adc_fifo_buffer_A_ready = false;
volatile bool adc_fifo_buffer_B_ready = false;
volatile DL_ADC12_IIDX adc_last_iidx = (DL_ADC12_IIDX)0;
volatile DL_DMA_EVENT_IIDX dma_last_iidx = DL_DMA_EVENT_IIDX_NO_INTR;

#define ADC_DAC_MIDPOINT 2048
#define MONITOR_GAIN_NUM 1
#define MONITOR_GAIN_DEN 8

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

static void configure_adc_capture_buffer(volatile uint32_t *buffer) {
  DL_DMA_disableChannel(DMA, ADC_DMA_CHAN_ID);
  DL_DMA_setDestAddr(DMA, ADC_DMA_CHAN_ID, (uint32_t)&buffer[0]);
  DL_DMA_setTransferSize(DMA, ADC_DMA_CHAN_ID, ADC_FIFO_WORDS);
  DL_DMA_enableChannel(DMA, ADC_DMA_CHAN_ID);

  // ADC DMA mode is cleared after a completed transfer, so re-arm it each time.
  DL_ADC12_setDMASamplesCnt(AUDIO_ADC_INST, 1);
  DL_ADC12_enableDMA(AUDIO_ADC_INST);
}

static void configure_dac_playback_buffer(uint16_t *buffer) {
  DL_DMA_disableChannel(DMA, OUTPUT_DMA_CHAN_ID);
  DL_DMA_setSrcAddr(DMA, OUTPUT_DMA_CHAN_ID, (uint32_t)&buffer[0]);
  DL_DMA_setTransferSize(DMA, OUTPUT_DMA_CHAN_ID, FFT_SIZE);
  DL_DMA_enableChannel(DMA, OUTPUT_DMA_CHAN_ID);
}

static void prepare_dac_playback_buffer(uint16_t *dest, const uint16_t *src) {
  for (uint32_t i = 0; i < FFT_SIZE; i++) {
    dest[i] = scale_audio_sample(src[i]);
  }
}

static void fill_dac_midpoint_buffer(uint16_t *buffer) {
  for (uint32_t i = 0; i < FFT_SIZE; i++) {
    buffer[i] = ADC_DAC_MIDPOINT;
  }
}

static void unpack_adc_fifo_buffer(uint16_t *dest,
                                   const volatile uint32_t *src) {
  for (uint32_t i = 0; i < ADC_FIFO_WORDS; i++) {
    dest[i] = (uint16_t)(src[i] & 0x0FFFU);
  }
}

static void handle_adc_capture_done(void) {
  static bool filling_buffer_A = true;

  if (filling_buffer_A) {
    configure_adc_capture_buffer(adc_fifo_buffer_B);
    adc_fifo_buffer_A_ready = true;
    filling_buffer_A = false;
  } else {
    configure_adc_capture_buffer(adc_fifo_buffer_A);
    adc_fifo_buffer_B_ready = true;
    filling_buffer_A = true;
  }
}

static void handle_dac_playback_done(void) {
  static bool play_buffer_A_next = true;

  if (play_buffer_A_next) {
    configure_dac_playback_buffer(dac_buffer_A);
    play_buffer_A_next = false;
  } else {
    configure_dac_playback_buffer(dac_buffer_B);
    play_buffer_A_next = true;
  }
}

int main(void) {
  SYSCFG_DL_init();

  DL_Timer_stopCounter(ADC_SAMPLE_TIMER_INST);
  DL_TimerG_stopCounter(OUTPUT_TIMER_INST);

  DL_ADC12_disableConversions(AUDIO_ADC_INST);

  // ILI9341_Init();

  // ILI9341_FillScreen(COLOR_BLUE);
  // ILI9341_FillRect(50, 50, 100, 100, COLOR_RED);

  // DL_GPIO_setPins(USER_LED_PORT, USER_LED_BLUE_PIN);

  DL_DMA_setSrcAddr(DMA, ADC_DMA_CHAN_ID,
                    DL_ADC12_getFIFOAddress(AUDIO_ADC_INST));
  DL_DMA_setDestAddr(DMA, OUTPUT_DMA_CHAN_ID, (uint32_t)&DAC0->DATA0);

  fill_dac_midpoint_buffer(dac_buffer_A);
  fill_dac_midpoint_buffer(dac_buffer_B);

  configure_adc_capture_buffer(adc_fifo_buffer_A);
  configure_dac_playback_buffer(dac_buffer_B);

  DL_ADC12_clearInterruptStatus(AUDIO_ADC_INST, DL_ADC12_INTERRUPT_DMA_DONE);
  DL_ADC12_enableInterrupt(AUDIO_ADC_INST, DL_ADC12_INTERRUPT_DMA_DONE);
  DL_DMA_clearInterruptStatus(DMA, OUTPUT_DMA_INTERRUPT);
  DL_DMA_enableInterrupt(DMA, OUTPUT_DMA_INTERRUPT);
  NVIC_EnableIRQ(AUDIO_ADC_INST_INT_IRQN);
  NVIC_EnableIRQ(DMA_INT_IRQn);

  DL_ADC12_enableConversions(AUDIO_ADC_INST);
  DL_ADC12_startConversion(AUDIO_ADC_INST);

  DL_Timer_startCounter(ADC_SAMPLE_TIMER_INST);
  DL_TimerG_startCounter(OUTPUT_TIMER_INST);

  DL_GPIO_clearPins(AMP_CONTROL_PORT, AMP_CONTROL_MAIN_PIN);
  DL_GPIO_setPins(MICROPHONE_PORT, MICROPHONE_PWR_PIN);

  while (1) {
    if (adc_fifo_buffer_A_ready) {
      adc_fifo_buffer_A_ready = false;
      unpack_adc_fifo_buffer(adc_buffer_A, adc_fifo_buffer_A);
      buffer_A_ready = true;
    }

    if (adc_fifo_buffer_B_ready) {
      adc_fifo_buffer_B_ready = false;
      unpack_adc_fifo_buffer(adc_buffer_B, adc_fifo_buffer_B);
      buffer_B_ready = true;
    }

    if (buffer_A_ready) {
      buffer_A_ready = false;

      // Process adc_A and write the results into dac_B
      prepare_dac_playback_buffer(dac_buffer_B, adc_buffer_A);

      // TODO: Future FFT processing for adc_buffer_A goes here!
    }

    if (buffer_B_ready) {
      buffer_B_ready = false;

      // Process adc_B and write the results into dac_A
      prepare_dac_playback_buffer(dac_buffer_A, adc_buffer_B);

      // TODO: Future FFT processing for adc_buffer_B goes here!
    }
  }
}

void AUDIO_ADC_INST_IRQHandler(void) {
  adc_last_iidx = DL_ADC12_getPendingInterrupt(AUDIO_ADC_INST);

  switch (adc_last_iidx) {
  case DL_ADC12_IIDX_DMA_DONE:

    handle_adc_capture_done();
    break;

  default:
    break;
  }
}

void DMA_IRQHandler(void) {
  dma_last_iidx = DL_DMA_getPendingInterrupt(DMA);

  switch (dma_last_iidx) {
  case OUTPUT_DMA_IIDX:
    handle_dac_playback_done();
    break;

  default:
    break;
  }
}
