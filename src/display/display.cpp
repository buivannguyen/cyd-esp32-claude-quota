#include "display.h"

/* LVGL vẽ vào buffer này, ~1/10 kích thước màn hình. Đơn vị: byte.
   Cấp phát trên heap (không để trong .bss) để dành DRAM tĩnh cho stack WiFi. */
#define DRAW_BUF_SIZE (TFT_HOR_RES * TFT_VER_RES / 10 * (LV_COLOR_DEPTH / 8))
static uint8_t *draw_buf = nullptr;

static uint8_t brightness = 255;

static uint32_t my_tick(void)
{
    return millis();
}

void led_init()
{
    // Arduino ESP32 core 3.x: ledcAttach picks the LEDC channel by pin
    ledcAttach(TFT_BL_PIN, 5000, 8);
}

void display_init()
{
    lv_init();
    lv_tick_set_cb(my_tick);

    draw_buf = (uint8_t *)malloc(DRAW_BUF_SIZE);

    lv_display_t *disp = lv_tft_espi_create(TFT_HOR_RES, TFT_VER_RES, draw_buf, DRAW_BUF_SIZE);
    lv_display_set_rotation(disp, TFT_ROTATION);
}

void turn_off_display()
{
    ledcWrite(TFT_BL_PIN, 0);
}

void turn_on_display()
{
    ledcWrite(TFT_BL_PIN, brightness);
}

void set_brightness(uint8_t new_brightness)
{
    brightness = new_brightness;
    turn_on_display();
}
