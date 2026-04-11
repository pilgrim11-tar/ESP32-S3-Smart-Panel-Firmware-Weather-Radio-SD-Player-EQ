// Main sketch entry point for ESP32-S3 4848S040 (LVGL + audio + weather UI).
#include "doMain.h"
#include "serial_upload_port.h"
#include <Audio.h>
#include "radio_vendor.h"
#include "weather_sd_assets.h"

#include <Arduino_GFX_Library.h>

#define GFX_BL 38

Arduino_ESP32RGBPanel *bus = new Arduino_ESP32RGBPanel(
    39 /* CS */, 48 /* SCK */, 47 /* SDA */,
    18 /* DE */, 17 /* VSYNC */, 16 /* HSYNC */, 21 /* PCLK */,
    11 /* R0 */, 12 /* R1 */, 13 /* R2 */, 14 /* R3 */, 0 /* R4 */,
    8 /* G0 */, 20 /* G1 */, 3 /* G2 */, 46 /* G3 */, 9 /* G4 */, 10 /* G5 */,
    4 /* B0 */, 5 /* B1 */, 6 /* B2 */, 7 /* B3 */, 15 /* B4 */
);
Arduino_ST7701_RGBPanel *gfx = new Arduino_ST7701_RGBPanel(
    bus, GFX_NOT_DEFINED /* RST */, 0 /* rotation */,
    true /* IPS */, 480 /* width */, 480 /* height */,
    st7701_type1_init_operations, sizeof(st7701_type1_init_operations), true /* BGR */,
    10 /* hsync_front_porch */, 8 /* hsync_pulse_width */, 50 /* hsync_back_porch */,
    10 /* vsync_front_porch */, 8 /* vsync_pulse_width */, 20 /* vsync_back_porch */);

#include "touch.h"

#include <esp_heap_caps.h>

static uint32_t screenWidth;
static uint32_t screenHeight;
static lv_disp_draw_buf_t draw_buf;
static lv_disp_drv_t disp_drv;

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

#if (LV_COLOR_16_SWAP != 0)
  // gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#else
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#endif

  lv_disp_flush_ready(disp);
}

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
  if (touch_has_signal())
  {
    if (touch_touched())
    {
      data->state = LV_INDEV_STATE_PR;

      data->point.x = touch_last_x;
      data->point.y = touch_last_y;
    }
    else if (touch_released())
    {
      data->state = LV_INDEV_STATE_REL;
    }
  }
  else
  {
    data->state = LV_INDEV_STATE_REL;
  }
}

HardwareSerial UploadSerial(0);

void setup()
{
  Serial.setRxBufferSize(16384);
  Serial.setTimeout(20);
  Serial.begin(115200);
  UploadSerial.setRxBufferSize(16384);
  UploadSerial.setTimeout(20);
  UploadSerial.begin(115200);

  Serial.printf("SRAM free size: %d\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  Serial.printf("PSRAM free size: %d\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

  Serial.println("Switch86 boot WXFIX6");
  Serial.println("[wxfix] ino build active");

  touch_init();

  gfx->begin(11000000);
  gfx->fillScreen(BLACK);

#ifdef GFX_BL
  ledcSetup(0, 600, 8);
  ledcAttachPin(GFX_BL, 0);
  ledcWrite(0, 150);
#endif

  lv_init();

  screenWidth = gfx->width();
  screenHeight = gfx->height();

  lv_color_t *buf_3_1 = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * screenWidth * 480, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

  if (!buf_3_1)
  {
    Serial.println("LVGL disp_draw_buf allocate failed!");
  }
  else
  {
    lv_disp_draw_buf_init(&draw_buf, buf_3_1, NULL, screenWidth * 480);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    ui_main();
    Serial.println("Setup done");

    Serial.printf("SRAM free size: %d\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    Serial.printf("PSRAM free size: %d\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    lv_timer_handler();
  }
}

void loop()
{
  weather_sd_assets_poll_upload();
  lv_timer_handler();
  ui_sync_runtime();
  radio_vendor_loop();
  delay(1);
}
