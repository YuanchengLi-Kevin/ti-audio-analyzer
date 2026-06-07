#include "audio.h"

#define MONITOR_GAIN_NUM 1
#define MONITOR_GAIN_DEN 8

uint16_t scale_audio_sample(uint16_t sample) {
  int32_t centered = (int32_t)sample - AUDIO_SAMPLE_MIDPOINT;
  int32_t scaled =
      AUDIO_SAMPLE_MIDPOINT + ((centered * MONITOR_GAIN_NUM) / MONITOR_GAIN_DEN);

  if (scaled < 0) {
    return 0;
  }
  if (scaled > 4095) {
    return 4095;
  }
  return (uint16_t)scaled;
}

