#include "lcd.h"

#include "ILI9341.h"
#include "buffer.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#define LCD_BAR_COUNT 24U
#define LCD_BIN_START 1U
#define LCD_FRAME_SKIP 4U
#define LCD_SAMPLE_RATE_HZ 40000U
#define LCD_MAX_FREQ_HZ 1000U
#define LCD_LOG_CURVE 1.0f
#define LCD_MAX_BIN_EXCLUSIVE \
  (((LCD_MAX_FREQ_HZ * AUDIO_FFT_SIZE) / LCD_SAMPLE_RATE_HZ) + 1U)
#define LCD_DISPLAY_BIN_COUNT (LCD_MAX_BIN_EXCLUSIVE - LCD_BIN_START)

#if LCD_MAX_BIN_EXCLUSIVE > AUDIO_FFT_BIN_COUNT
#error "LCD_MAX_FREQ_HZ exceeds the usable FFT output range."
#endif

#if LCD_BAR_COUNT > LCD_DISPLAY_BIN_COUNT
#error "LCD_BAR_COUNT must not exceed the number of displayed FFT bins."
#endif

#define LCD_PLOT_X 0U
#define LCD_PLOT_Y 8U
#define LCD_PLOT_WIDTH TFT_WIDTH
#define LCD_PLOT_HEIGHT (TFT_HEIGHT - LCD_PLOT_Y)

#define LCD_BAR_GAP 1U
#define LCD_BAR_HEIGHT ((LCD_PLOT_HEIGHT / LCD_BAR_COUNT) - LCD_BAR_GAP)
#define LCD_BAR_STRIDE (LCD_BAR_HEIGHT + LCD_BAR_GAP)

#define LCD_MIN_SCALE 256U
#define LCD_PEAK_DECAY_NUM 15U
#define LCD_PEAK_DECAY_DEN 16U
#define LCD_DB_RANGE 30.0f
#define LCD_MIN_MAGNITUDE 1.0f
#define LCD_NOISE_FLOOR_MAGNITUDE 1U

static uint16_t previous_widths[LCD_BAR_COUNT];
static uint16_t previous_colors[LCD_BAR_COUNT];
static uint16_t log_bin_edges[LCD_BAR_COUNT + 1U];
static uint16_t display_peak = LCD_MIN_SCALE;
static uint32_t last_drawn_sequence = 0;
static bool has_drawn_sequence = false;
static bool has_calculated_edges = false;

static uint16_t max_u16(uint16_t a, uint16_t b)
{
  return (a > b) ? a : b;
}

static void calculate_log_bin_edges(void)
{
  float start = (float)LCD_BIN_START;
  float end = (float)LCD_MAX_BIN_EXCLUSIVE;
  float ratio = end / start;

  log_bin_edges[0] = LCD_BIN_START;
  log_bin_edges[LCD_BAR_COUNT] = LCD_MAX_BIN_EXCLUSIVE;

  for (uint32_t i = 1U; i < LCD_BAR_COUNT; i++)
  {
    float t = (float)i / (float)LCD_BAR_COUNT;
    float curved_t = powf(t, LCD_LOG_CURVE);
    uint32_t edge =
        (uint32_t)((start * powf(ratio, curved_t)) + 0.5f);
    uint32_t min_edge = (uint32_t)log_bin_edges[i - 1U] + 1U;
    uint32_t max_edge = LCD_MAX_BIN_EXCLUSIVE - (LCD_BAR_COUNT - i);

    if (edge < min_edge)
    {
      edge = min_edge;
    }
    if (edge > max_edge)
    {
      edge = max_edge;
    }

    log_bin_edges[i] = (uint16_t)edge;
  }

  has_calculated_edges = true;
}

static uint16_t spectrum_color_for_width(uint16_t width)
{
  uint32_t high_threshold = (LCD_PLOT_WIDTH * 3U) / 4U;
  uint32_t mid_threshold = LCD_PLOT_WIDTH / 2U;

  if (width >= high_threshold)
  {
    return COLOR_RED;
  }
  if (width >= mid_threshold)
  {
    return COLOR_YELLOW;
  }
  return COLOR_GREEN;
}

static uint16_t find_spectrum_peak(const uint16_t magnitudes[AUDIO_FFT_BIN_COUNT])
{
  uint16_t peak = LCD_MIN_SCALE;
  uint32_t end = log_bin_edges[LCD_BAR_COUNT];

  for (uint32_t i = LCD_BIN_START; i < end; i++)
  {
    peak = max_u16(peak, magnitudes[i]);
  }

  return peak;
}

static void update_display_peak(uint16_t frame_peak)
{
  if (frame_peak > display_peak)
  {
    display_peak = frame_peak;
    return;
  }

  uint32_t decayed =
      ((uint32_t)display_peak * LCD_PEAK_DECAY_NUM + frame_peak) /
      LCD_PEAK_DECAY_DEN;

  display_peak = max_u16((uint16_t)decayed, LCD_MIN_SCALE);
}

static uint16_t average_bin_group(const uint16_t magnitudes[AUDIO_FFT_BIN_COUNT],
                                  uint32_t bar_index)
{
  const uint32_t start = log_bin_edges[bar_index];
  const uint32_t end = log_bin_edges[bar_index + 1U];

  uint32_t sum = 0;
  for (uint32_t i = start; i < end; i++)
  {
    sum += magnitudes[i];
  }

  return (uint16_t)(sum / (end - start));
}

static uint16_t magnitude_to_width(uint16_t magnitude)
{
  if (magnitude < LCD_NOISE_FLOOR_MAGNITUDE)
  {
    return 0U;
  }

  float mag = (float)magnitude;
  float peak = (float)display_peak;

  if (mag < LCD_MIN_MAGNITUDE)
  {
    mag = LCD_MIN_MAGNITUDE;
  }
  if (peak < LCD_MIN_MAGNITUDE)
  {
    peak = LCD_MIN_MAGNITUDE;
  }

  float db = 20.0f * log10f(mag / peak);

  if (db <= -LCD_DB_RANGE)
  {
    return 0U;
  }
  if (db > 0.0f)
  {
    db = 0.0f;
  }

  float width = ((db + LCD_DB_RANGE) * (float)LCD_PLOT_WIDTH) / LCD_DB_RANGE;

  return (uint16_t)width;
}

static void draw_bar_delta(uint32_t bar_index, uint16_t new_width)
{
  uint16_t old_width = previous_widths[bar_index];
  uint16_t old_color = previous_colors[bar_index];
  uint16_t y = LCD_PLOT_Y + (uint16_t)(bar_index * LCD_BAR_STRIDE);
  uint16_t color = spectrum_color_for_width(new_width);

  if ((new_width > 0U) && (new_width == old_width) && (color != old_color))
  {
    ILI9341_FillRect(LCD_PLOT_X, y, new_width, LCD_BAR_HEIGHT, color);
  }
  else if (new_width > old_width)
  {
    if ((old_width > 0U) && (color != old_color))
    {
      ILI9341_FillRect(LCD_PLOT_X, y, new_width, LCD_BAR_HEIGHT, color);
    }
    else
    {
      uint16_t grow = new_width - old_width;

      ILI9341_FillRect(LCD_PLOT_X + old_width, y, grow, LCD_BAR_HEIGHT, color);
    }
  }
  else if (old_width > new_width)
  {
    uint16_t shrink = old_width - new_width;

    ILI9341_FillRect(LCD_PLOT_X + new_width, y, shrink, LCD_BAR_HEIGHT,
                     COLOR_BLACK);

    if (new_width > 0U)
    {
      ILI9341_FillRect(LCD_PLOT_X, y, new_width, LCD_BAR_HEIGHT, color);
    }
  }
  else if (new_width > 0U)
  {
    ILI9341_FillRect(LCD_PLOT_X, y, new_width, LCD_BAR_HEIGHT, color);
  }

  previous_widths[bar_index] = new_width;
  previous_colors[bar_index] = color;
}

void lcd_init(void)
{
  ILI9341_Init();
  calculate_log_bin_edges();
  lcd_clear_spectrum();
}

void lcd_clear_spectrum(void)
{
  ILI9341_FillScreen(COLOR_BLACK);

  for (uint32_t i = 0; i < LCD_BAR_COUNT; i++)
  {
    previous_widths[i] = 0;
    previous_colors[i] = COLOR_BLACK;
  }

  display_peak = LCD_MIN_SCALE;
}

void lcd_update_spectrum(const uint16_t magnitudes[AUDIO_FFT_BIN_COUNT])
{
  if (!has_calculated_edges)
  {
    calculate_log_bin_edges();
  }

  uint16_t frame_peak = find_spectrum_peak(magnitudes);

  update_display_peak(frame_peak);

  for (uint32_t i = 0; i < LCD_BAR_COUNT; i++)
  {
    uint16_t magnitude = average_bin_group(magnitudes, i);
    uint16_t width = magnitude_to_width(magnitude);

    draw_bar_delta(i, width);
  }
}

void lcd_update_from_latest_spectrum(void)
{
  static uint16_t magnitudes[AUDIO_FFT_BIN_COUNT];
  uint32_t sequence = 0;

  if (!buffer_copy_latest_spectrum(magnitudes, &sequence))
  {
    return;
  }

  if (has_drawn_sequence &&
      ((sequence - last_drawn_sequence) < LCD_FRAME_SKIP))
  {
    return;
  }

  last_drawn_sequence = sequence;
  has_drawn_sequence = true;

  lcd_update_spectrum(magnitudes);
}
