#ifndef UTILS_BUFFER_H
#define UTILS_BUFFER_H

void handle_adc_capture_done(void);
void handle_dac_playback_done(void);

void handle_dma_interrupt(void);
void handle_adc_interrupt(void);

void buffer_init(void);

void process_ready(void);

#endif
