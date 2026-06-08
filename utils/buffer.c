#include "buffer.h"
#include "audio.h"
#include "fft.h"
#include "ti_msp_dl_config.h"
#include <stdbool.h>
#include <stdint.h>

#define FFT_SIZE AUDIO_FFT_SIZE

typedef enum
{
  SAMPLE_BUFFER_EMPTY,
  SAMPLE_BUFFER_FILLING,
  SAMPLE_BUFFER_READY,
  SAMPLE_BUFFER_PROCESSING
} sample_buffer_state_t;

typedef enum
{
  SAMPLE_BUFFER_A,
  SAMPLE_BUFFER_B
} sample_buffer_id_t;

static volatile uint16_t adc_buffer_A[FFT_SIZE];
static volatile uint16_t adc_buffer_B[FFT_SIZE];

static uint16_t spectrum_magnitudes[AUDIO_FFT_BIN_COUNT];
static volatile uint32_t spectrum_sequence = 0;
static volatile uint32_t sample_buffer_overruns = 0;

static volatile sample_buffer_state_t buffer_A_state = SAMPLE_BUFFER_FILLING;
static volatile sample_buffer_state_t buffer_B_state = SAMPLE_BUFFER_EMPTY;
static volatile sample_buffer_id_t filling_buffer = SAMPLE_BUFFER_A;
static volatile uint32_t filling_index = 0;

static volatile uint16_t *get_buffer(sample_buffer_id_t id)
{
  return (id == SAMPLE_BUFFER_A) ? adc_buffer_A : adc_buffer_B;
}

static volatile sample_buffer_state_t *get_buffer_state(sample_buffer_id_t id)
{
  return (id == SAMPLE_BUFFER_A) ? &buffer_A_state : &buffer_B_state;
}

static sample_buffer_id_t other_buffer(sample_buffer_id_t id)
{
  return (id == SAMPLE_BUFFER_A) ? SAMPLE_BUFFER_B : SAMPLE_BUFFER_A;
}

static void update_spectrum(const uint16_t *samples)
{
  fft_analyze_adc_block(spectrum_magnitudes, samples);
  spectrum_sequence++;
}

static bool claim_ready_buffer(sample_buffer_id_t id)
{
  volatile sample_buffer_state_t *state = get_buffer_state(id);
  bool claimed = false;

  __disable_irq();
  if (*state == SAMPLE_BUFFER_READY)
  {
    *state = SAMPLE_BUFFER_PROCESSING;
    claimed = true;
  }
  __enable_irq();

  return claimed;
}

static void release_processing_buffer(sample_buffer_id_t id)
{
  __disable_irq();
  *get_buffer_state(id) = SAMPLE_BUFFER_EMPTY;
  __enable_irq();
}

static void handle_sample(uint16_t sample)
{
  volatile uint16_t *buffer = get_buffer(filling_buffer);

  DL_DAC12_output12(DAC0, sample);

  buffer[filling_index] = sample;
  filling_index++;

  if (filling_index < FFT_SIZE)
  {
    return;
  }

  *get_buffer_state(filling_buffer) = SAMPLE_BUFFER_READY;

  sample_buffer_id_t next_buffer = other_buffer(filling_buffer);
  volatile sample_buffer_state_t *next_state = get_buffer_state(next_buffer);

  if (*next_state == SAMPLE_BUFFER_EMPTY)
  {
    *next_state = SAMPLE_BUFFER_FILLING;
    filling_buffer = next_buffer;
  }
  else
  {
    sample_buffer_overruns++;
    *get_buffer_state(filling_buffer) = SAMPLE_BUFFER_FILLING;
  }

  filling_index = 0;
}

void handle_adc_interrupt(void)
{
  DL_ADC12_IIDX adc_last_iidx = DL_ADC12_getPendingInterrupt(AUDIO_ADC_INST);

  switch (adc_last_iidx)
  {
  case DL_ADC12_IIDX_MEM0_RESULT_LOADED:
    uint16_t scaled_sample = scale_audio_sample((uint16_t)(DL_ADC12_getMemResult(AUDIO_ADC_INST, AUDIO_ADC_ADCMEM_ADC_MEM0) & 0x0FFFU));
    handle_sample(scaled_sample);
    break;

  default:
    break;
  }
}

void buffer_init(void)
{
  __disable_irq();
  buffer_A_state = SAMPLE_BUFFER_FILLING;
  buffer_B_state = SAMPLE_BUFFER_EMPTY;
  filling_buffer = SAMPLE_BUFFER_A;
  filling_index = 0;
  sample_buffer_overruns = 0;
  spectrum_sequence = 0;
  __enable_irq();

  DL_ADC12_disableDMA(AUDIO_ADC_INST);
  DL_ADC12_disableDMATrigger(AUDIO_ADC_INST,
                             DL_ADC12_DMA_MEM0_RESULT_LOADED);
  DL_ADC12_disableDMATrigger(AUDIO_ADC_INST,
                             DL_ADC12_DMA_MEM10_RESULT_LOADED);
  DL_ADC12_disableFIFO(AUDIO_ADC_INST);
  DL_ADC12_disableInterrupt(AUDIO_ADC_INST, DL_ADC12_INTERRUPT_DMA_DONE);

  DL_DAC12_disableDMATrigger(DAC0);
  DL_DAC12_disableFIFO(DAC0);
  DL_DAC12_disableSampleTimeGenerator(DAC0);
  DL_DAC12_disableInterrupt(DAC0, DL_DAC12_INTERRUPT_DMA_DONE);
  DL_DAC12_output12(DAC0, AUDIO_SAMPLE_MIDPOINT);
}

void process_ready(void)
{
  if (claim_ready_buffer(SAMPLE_BUFFER_A))
  {
    update_spectrum((const uint16_t *)adc_buffer_A);
    release_processing_buffer(SAMPLE_BUFFER_A);
  }

  if (claim_ready_buffer(SAMPLE_BUFFER_B))
  {
    update_spectrum((const uint16_t *)adc_buffer_B);
    release_processing_buffer(SAMPLE_BUFFER_B);
  }
}

bool buffer_copy_latest_spectrum(uint16_t magnitudes[AUDIO_FFT_BIN_COUNT],
                                 uint32_t *sequence)
{
  uint32_t start_sequence = spectrum_sequence;

  for (uint32_t i = 0; i < AUDIO_FFT_BIN_COUNT; i++)
  {
    magnitudes[i] = spectrum_magnitudes[i];
  }

  if (sequence != NULL)
  {
    *sequence = start_sequence;
  }

  return start_sequence == spectrum_sequence;
}
