#include "ti_msp_dl_config.h"
#include <stdbool.h>
#include <stdint.h>

// --- 1. GLOBAL VARIABLES ---
#define FFT_SIZE 1024

// The ping-pong buckets to hold audio for the FFT
uint16_t adc_buffer_A[FFT_SIZE];
uint16_t adc_buffer_B[FFT_SIZE];

// Flags to tell the main loop when a bucket is completely full
volatile bool buffer_A_ready = false;
volatile bool buffer_B_ready = false;

// --- 2. THE MAIN LOOP (The "Waiting Room") ---
int main(void) {
  // Initialize all the SysConfig hardware
  SYSCFG_DL_init();

  // Tell the CPU to listen to the ADC's "I have a number" interrupt
  NVIC_EnableIRQ(AUDIO_ADC_INST_INT_IRQN);

  DL_ADC12_startConversion(AUDIO_ADC_INST);

  DL_Timer_startCounter(ADC_SAMPLE_TIMER_INST);

  DL_GPIO_clearPins(AMP_CONTROL_PORT, AMP_CONTROL_MAIN_PIN);
  DL_GPIO_setPins(MICROPHONE_PORT, MICROPHONE_PWR_PIN);

  // The CPU sits here forever, waiting for a bucket to fill up
  while (1) {
    if (buffer_A_ready) {
      buffer_A_ready = false; // Acknowledge the flag

      // [FUTURE FFT CODE GOES HERE]
      // We will run the math on adc_buffer_A and update the LCD
    }

    if (buffer_B_ready) {
      buffer_B_ready = false; // Acknowledge the flag

      // [FUTURE FFT CODE GOES HERE]
      // We will run the math on adc_buffer_B and update the LCD
    }
  }
}

// --- 3. THE HARDWARE INTERRUPT (The "Traffic Cop") ---
// This function runs automatically 40,000 times a second.
void AUDIO_ADC_INST_IRQHandler(void) {
  DL_GPIO_togglePins(USER_LED_PORT, USER_LED_BLUE_PIN);
  static uint16_t sample_index = 0;
  static bool filling_buffer_A = true;

  // A. Grab the live microphone reading (12-bit number)
  uint16_t mic_voltage =
      DL_ADC12_getMemResult(AUDIO_ADC_INST, DL_ADC12_MEM_IDX_0);

  // B. TRUE ANALOG PASSTHROUGH: Instantly push that number out to the speaker
  DL_DAC12_output12(DAC0, mic_voltage);

  // C. Save a copy of the number into the active bucket for the FFT later
  if (filling_buffer_A) {
    adc_buffer_A[sample_index] = mic_voltage;
  } else {
    adc_buffer_B[sample_index] = mic_voltage;
  }
  sample_index++;

  // D. When the bucket reaches 1024, swap buckets and alert the main loop
  if (sample_index >= FFT_SIZE) {
    sample_index = 0; // Reset the counter

    if (filling_buffer_A) {
      buffer_A_ready = true;    // Alert the main loop
      filling_buffer_A = false; // Switch to Buffer B
    } else {
      buffer_B_ready = true;   // Alert the main loop
      filling_buffer_A = true; // Switch to Buffer A
    }
  }
}