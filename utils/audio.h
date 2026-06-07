#ifndef UTILS_AUDIO_H_
#define UTILS_AUDIO_H_

#include <stdint.h>

#define AUDIO_SAMPLE_MIDPOINT 2048U

uint16_t scale_audio_sample(uint16_t sample);

#endif