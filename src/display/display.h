#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "lvgl.h"

// ---- CYD display specs (ESP32-2432S028R, ILI9341) ----
#define TFT_HOR_RES 240
#define TFT_VER_RES 320
#define TFT_ROTATION LV_DISPLAY_ROTATION_270 // landscape

// Backlight control pin - GPIO 21 on the CYD board
#define TFT_BL_PIN 21

void led_init();
void display_init();
void turn_off_display();
void turn_on_display();
void set_brightness(uint8_t new_brightness);
