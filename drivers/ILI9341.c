#include "ILI9341.h"
#include "ti_msp_dl_config.h"

/* SysConfig signal names for the ILI9341 SPI and control pins. */
#define TFT_SPI_INST SPI_LED_INST
#define TFT_DC_PORT DATA_COMMAND_PORT
#define TFT_DC_PIN DATA_COMMAND_PIN_0_PIN

#define TFT_CS_PORT TFT_SELECT_PORT
#define TFT_CS_PIN TFT_SELECT_CS_PIN

#define CS_LOW() DL_GPIO_clearPins(TFT_CS_PORT, TFT_CS_PIN)
#define CS_HIGH() DL_GPIO_setPins(TFT_CS_PORT, TFT_CS_PIN)
#define DC_DATA() DL_GPIO_setPins(TFT_DC_PORT, TFT_DC_PIN)
#define DC_CMD() DL_GPIO_clearPins(TFT_DC_PORT, TFT_DC_PIN)

static void SPI_Write8(uint8_t data) {
  DL_SPI_transmitDataBlocking8(TFT_SPI_INST, data);
}

static void WriteCommand(uint8_t cmd) {
  DC_CMD();
  CS_LOW();
  SPI_Write8(cmd);
  CS_HIGH();
}

static void WriteCommandData(uint8_t cmd, const uint8_t *data,
                             uint32_t length) {
  DC_CMD();
  CS_LOW();
  SPI_Write8(cmd);

  DC_DATA();
  for (uint32_t i = 0; i < length; i++) {
    SPI_Write8(data[i]);
  }
  CS_HIGH();
}

/* Selects the rectangle that subsequent pixel data will fill. */
static void ILI9341_SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1,
                                     uint16_t y1) {
  uint8_t column_data[] = {x0 >> 8, x0 & 0xFF, x1 >> 8, x1 & 0xFF};
  uint8_t page_data[] = {y0 >> 8, y0 & 0xFF, y1 >> 8, y1 & 0xFF};

  WriteCommandData(0x2A, column_data, sizeof(column_data));
  WriteCommandData(0x2B, page_data, sizeof(page_data));

  WriteCommand(0x2C);
}

void ILI9341_DrawPixel(uint16_t x, uint16_t y, uint16_t color) {
  if ((x >= TFT_WIDTH) || (y >= TFT_HEIGHT))
    return;
  ILI9341_SetAddressWindow(x, y, x, y);
  DC_DATA();
  CS_LOW();
  SPI_Write8(color >> 8);
  SPI_Write8(color & 0xFF);
  CS_HIGH();
}

void ILI9341_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                      uint16_t color) {
  if ((x >= TFT_WIDTH) || (y >= TFT_HEIGHT))
    return;
  if ((x + w - 1) >= TFT_WIDTH)
    w = TFT_WIDTH - x;
  if ((y + h - 1) >= TFT_HEIGHT)
    h = TFT_HEIGHT - y;

  ILI9341_SetAddressWindow(x, y, x + w - 1, y + h - 1);

  DC_DATA();
  CS_LOW();
  for (uint32_t i = 0; i < (w * h); i++) {
    SPI_Write8(color >> 8);
    SPI_Write8(color & 0xFF);
  }
  CS_HIGH();
}

void ILI9341_FillScreen(uint16_t color) {
  ILI9341_FillRect(0, 0, TFT_WIDTH, TFT_HEIGHT, color);
}

void ILI9341_Init(void) {
  const uint8_t ili9341_enable_3byte[] = {0x03, 0x80, 0x02};
  const uint8_t power_control_b[] = {0x00, 0xC1, 0x30};
  const uint8_t power_on_sequence[] = {0x64, 0x03, 0x12, 0x81};
  const uint8_t driver_timing_a[] = {0x85, 0x00, 0x78};
  const uint8_t power_control_a[] = {0x39, 0x2C, 0x00, 0x34, 0x02};
  const uint8_t pump_ratio_control[] = {0x20};
  const uint8_t driver_timing_b[] = {0x00, 0x00};
  const uint8_t power_control_1[] = {0x23};
  const uint8_t power_control_2[] = {0x10};
  const uint8_t vcom_control_1[] = {0x3E, 0x28};
  const uint8_t vcom_control_2[] = {0x86};
  const uint8_t memory_access_control[] = {0x48};
  const uint8_t pixel_format[] = {0x55};
  const uint8_t frame_rate_control[] = {0x00, 0x18};
  const uint8_t display_function_control[] = {0x08, 0x82, 0x27};
  const uint8_t enable_3g[] = {0x00};
  const uint8_t gamma_set[] = {0x01};
  const uint8_t positive_gamma[] = {0x0F, 0x31, 0x2B, 0x0C, 0x0E,
                                    0x08, 0x4E, 0xF1, 0x37, 0x07,
                                    0x10, 0x03, 0x0E, 0x09, 0x00};
  const uint8_t negative_gamma[] = {0x00, 0x0E, 0x14, 0x03, 0x11,
                                    0x07, 0x31, 0xC1, 0x48, 0x08,
                                    0x0F, 0x0C, 0x31, 0x36, 0x0F};

  CS_HIGH();
  DC_DATA();

  /* RST is hardwired high; wait for power-on reset to settle. */
  DL_Common_delayCycles(3200000);

  /* Standard ILI9341 startup sequence for RGB565 pixel writes. */
  WriteCommand(0x01);
  DL_Common_delayCycles(3200000);

  WriteCommandData(0xEF, ili9341_enable_3byte, sizeof(ili9341_enable_3byte));
  WriteCommandData(0xCF, power_control_b, sizeof(power_control_b));
  WriteCommandData(0xED, power_on_sequence, sizeof(power_on_sequence));
  WriteCommandData(0xE8, driver_timing_a, sizeof(driver_timing_a));
  WriteCommandData(0xCB, power_control_a, sizeof(power_control_a));
  WriteCommandData(0xF7, pump_ratio_control, sizeof(pump_ratio_control));
  WriteCommandData(0xEA, driver_timing_b, sizeof(driver_timing_b));
  WriteCommandData(0xC0, power_control_1, sizeof(power_control_1));
  WriteCommandData(0xC1, power_control_2, sizeof(power_control_2));
  WriteCommandData(0xC5, vcom_control_1, sizeof(vcom_control_1));
  WriteCommandData(0xC7, vcom_control_2, sizeof(vcom_control_2));
  WriteCommandData(0x36, memory_access_control, sizeof(memory_access_control));
  WriteCommandData(0x3A, pixel_format, sizeof(pixel_format));
  WriteCommandData(0xB1, frame_rate_control, sizeof(frame_rate_control));
  WriteCommandData(0xB6, display_function_control,
                   sizeof(display_function_control));
  WriteCommandData(0xF2, enable_3g, sizeof(enable_3g));
  WriteCommandData(0x26, gamma_set, sizeof(gamma_set));
  WriteCommandData(0xE0, positive_gamma, sizeof(positive_gamma));
  WriteCommandData(0xE1, negative_gamma, sizeof(negative_gamma));

  WriteCommand(0x11);
  DL_Common_delayCycles(3200000);

  WriteCommand(0x29);
  DL_Common_delayCycles(3200000);
}
