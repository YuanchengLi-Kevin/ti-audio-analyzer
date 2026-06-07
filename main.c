#include "ILI9341.h"
#include "ti_msp_dl_config.h"
#include "utils/buffer.h"
#include <stdbool.h>
#include <stdint.h>

int main(void)
{
  SYSCFG_DL_init();

  DL_Timer_stopCounter(ADC_SAMPLE_TIMER_INST);
  DL_TimerG_stopCounter(OUTPUT_TIMER_INST);

  DL_ADC12_disableConversions(AUDIO_ADC_INST);

  // ILI9341_Init();

  // ILI9341_FillScreen(COLOR_BLUE);
  // ILI9341_FillRect(50, 50, 100, 100, COLOR_RED);

  // DL_GPIO_setPins(USER_LED_PORT, USER_LED_BLUE_PIN);

  DL_DMA_setSrcAddr(DMA, ADC_DMA_CHAN_ID, DL_ADC12_getFIFOAddress(AUDIO_ADC_INST));
  DL_DMA_setDestAddr(DMA, OUTPUT_DMA_CHAN_ID, (uint32_t)&DAC0->DATA0);

  buffer_init();

  DL_ADC12_clearInterruptStatus(AUDIO_ADC_INST, DL_ADC12_INTERRUPT_DMA_DONE);
  DL_ADC12_enableInterrupt(AUDIO_ADC_INST, DL_ADC12_INTERRUPT_DMA_DONE);

  NVIC_EnableIRQ(AUDIO_ADC_INST_INT_IRQN);
  NVIC_EnableIRQ(DMA_INT_IRQn);

  DL_ADC12_enableConversions(AUDIO_ADC_INST);
  DL_ADC12_startConversion(AUDIO_ADC_INST);

  DL_Timer_startCounter(ADC_SAMPLE_TIMER_INST);
  DL_TimerG_startCounter(OUTPUT_TIMER_INST);

  DL_GPIO_clearPins(AMP_CONTROL_PORT, AMP_CONTROL_MAIN_PIN);
  DL_GPIO_setPins(MICROPHONE_PORT, MICROPHONE_PWR_PIN);

  while (1)
  {
    process_ready();
  }
}

void AUDIO_ADC_INST_IRQHandler(void)
{
  handle_adc_interrupt();
}

void DMA_IRQHandler(void)
{
  handle_dma_interrupt();
}
