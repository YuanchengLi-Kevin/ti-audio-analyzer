#include "ti_msp_dl_config.h"
#include "utils/buffer.h"
#include "utils/lcd.h"
#include <stdint.h>

int main(void)
{
  SYSCFG_DL_init();

  DL_Timer_stopCounter(ADC_SAMPLE_TIMER_INST);

  DL_ADC12_disableConversions(AUDIO_ADC_INST);

  lcd_init();

  buffer_init();

  DL_ADC12_clearInterruptStatus(AUDIO_ADC_INST,
                                DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED);
  DL_ADC12_enableInterrupt(AUDIO_ADC_INST,
                           DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED);

  NVIC_EnableIRQ(AUDIO_ADC_INST_INT_IRQN);

  DL_ADC12_enableConversions(AUDIO_ADC_INST);
  DL_ADC12_startConversion(AUDIO_ADC_INST);

  DL_Timer_startCounter(ADC_SAMPLE_TIMER_INST);

  DL_GPIO_clearPins(AMP_CONTROL_PORT, AMP_CONTROL_MAIN_PIN);
  DL_GPIO_setPins(MICROPHONE_PORT, MICROPHONE_PWR_PIN);

  while (1)
  {
    process_ready();
    lcd_update_from_latest_spectrum();
  }
}

void AUDIO_ADC_INST_IRQHandler(void)
{
  // Sampling timer triggers ADC sampling. MEM0 interrupt handles each sample.
  handle_adc_interrupt();
}
