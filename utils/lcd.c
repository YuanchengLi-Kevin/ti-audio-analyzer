#include "lcd.h"

#include "ILI9341.h"
#include "buffer.h"

#include <stdbool.h>
#include <stdint.h>

#define LCD_BAR_COUNT 40U
#define LCD_BIN_START 1U
#define LCD_FRAME_SKIP 4U

#define LCD_PLOT_X 0U
#define LCD_PLOT_Y 8U
#define LCD_PLOT_WIDTH TFT_WIDTH
#define LCD_PLOT_HEIGHT 304U

#define LCD_BAR_GAP 1U
#define LCD_BAR_WIDTH ((LCD_PLOT_WIDTH / LCD_BAR_COUNT) - LCD_BAR_GAP)
#define LCD_BAR_STRIDE (LCD_BAR_WIDTH + LCD_BAR_GAP)
#define LCD_BASELINE_Y (LCD_PLOT_Y + LCD_PLOT_HEIGHT)

#define LCD_MIN_SCALE 256U
#define LCD_PEAK_DECAY_NUM 15U
#define LCD_PEAK_DECAY_DEN 16U

static uint16_t previous_heights[LCD_BAR_COUNT];
static uint16_t previous_colors[LCD_BAR_COUNT];
static uint16_t display_peak = LCD_MIN_SCALE;
static uint32_t last_drawn_sequence = 0;
static bool has_drawn_sequence = false;

static uint16_t max_u16(uint16_t a, uint16_t b)
{
  return (a > b) ? a : b;
}

static uint16_t spectrum_color_for_height(uint16_t height)
{
  uint32_t high_threshold = (LCD_PLOT_HEIGHT * 3U) / 4U;
  uint32_t mid_threshold = LCD_PLOT_HEIGHT / 2U;

  if (height >= high_threshold)
  {
    return COLOR_RED;
  }
  if (height >= mid_threshold)
  {
    return COLOR_YELLOW;
  }
  return COLOR_GREEN;
}

static uint16_t find_spectrum_peak(const uint16_t magnitudes[AUDIO_FFT_BIN_COUNT])
{
  uint16_t peak = LCD_MIN_SCALE;

  for (uint32_t i = LCD_BIN_START; i < AUDIO_FFT_BIN_COUNT; i++)
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
  const uint32_t usable_bins = AUDIO_FFT_BIN_COUNT - LCD_BIN_START;
  const uint32_t start =
      LCD_BIN_START + ((bar_index * usable_bins) / LCD_BAR_COUNT);
  uint32_t end =
      LCD_BIN_START + (((bar_index + 1U) * usable_bins) / LCD_BAR_COUNT);

  if (end <= start)
  {
    end = start + 1U;
  }

  uint32_t sum = 0;
  for (uint32_t i = start; i < end; i++)
  {
    sum += magnitudes[i];
  }

  return (uint16_t)(sum / (end - start));
}

static uint16_t magnitude_to_height(uint16_t magnitude)
{
  uint32_t height = ((uint32_t)magnitude * LCD_PLOT_HEIGHT) / display_peak;

  if (height > LCD_PLOT_HEIGHT)
  {
    height = LCD_PLOT_HEIGHT;
  }

  return (uint16_t)height;
}

static void draw_bar_delta(uint32_t bar_index, uint16_t new_height)
{
  uint16_t old_height = previous_heights[bar_index];
  uint16_t old_color = previous_colors[bar_index];
  uint16_t x = LCD_PLOT_X + (uint16_t)(bar_index * LCD_BAR_STRIDE);
  uint16_t color = spectrum_color_for_height(new_height);

  if ((new_height > 0U) && (new_height == old_height) && (color != old_color))
  {
    uint16_t y = LCD_BASELINE_Y - new_height;

    ILI9341_FillRect(x, y, LCD_BAR_WIDTH, new_height, color);
  }
  else if (new_height > old_height)
  {
    uint16_t y = LCD_BASELINE_Y - new_height;

    if ((old_height > 0U) && (color != old_color))
    {
      ILI9341_FillRect(x, y, LCD_BAR_WIDTH, new_height, color);
    }
    else
    {
      uint16_t grow = new_height - old_height;

      ILI9341_FillRect(x, y, LCD_BAR_WIDTH, grow, color);
    }
  }
  else if (old_height > new_height)
  {
    uint16_t shrink = old_height - new_height;
    uint16_t y = LCD_BASELINE_Y - old_height;

    ILI9341_FillRect(x, y, LCD_BAR_WIDTH, shrink, COLOR_BLACK);

    if (new_height > 0U)
    {
      y = LCD_BASELINE_Y - new_height;
      ILI9341_FillRect(x, y, LCD_BAR_WIDTH, new_height, color);
    }
  }
  else if (new_height > 0U)
  {
    uint16_t y = LCD_BASELINE_Y - new_height;
    ILI9341_FillRect(x, y, LCD_BAR_WIDTH, new_height, color);
  }

  previous_heights[bar_index] = new_height;
  previous_colors[bar_index] = color;
}

void lcd_init(void)
{
  ILI9341_Init();
  lcd_clear_spectrum();
}

void lcd_clear_spectrum(void)
{
  ILI9341_FillScreen(COLOR_BLACK);

  for (uint32_t i = 0; i < LCD_BAR_COUNT; i++)
  {
    previous_heights[i] = 0;
    previous_colors[i] = COLOR_BLACK;
  }

  display_peak = LCD_MIN_SCALE;
}

void lcd_update_spectrum(const uint16_t magnitudes[AUDIO_FFT_BIN_COUNT])
{
  uint16_t frame_peak = find_spectrum_peak(magnitudes);

  update_display_peak(frame_peak);

  for (uint32_t i = 0; i < LCD_BAR_COUNT; i++)
  {
    uint16_t magnitude = average_bin_group(magnitudes, i);
    uint16_t height = magnitude_to_height(magnitude);

    draw_bar_delta(i, height);
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
