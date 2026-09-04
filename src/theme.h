#pragma once
#include <lvgl.h>
#include "fonts/fonts.h"

// Palette matches Clawdmeter's "Usage" UI (Anthropic dark + terracotta).
#define CL_BG     lv_color_hex(0x0B0B0C) // background outside cards
#define CL_CARD   lv_color_hex(0x2A2633) // card fill (dark purple-gray)
#define CL_TRACK  lv_color_hex(0x453F55) // bar track
#define CL_PILL   lv_color_hex(0x3A3547) // pill background
#define CL_TEXT   lv_color_hex(0xFBFAF7) // primary text
#define CL_DIM    lv_color_hex(0xD9D6DF) // secondary text
#define CL_FAINT  lv_color_hex(0x8B8794) // very faint text
#define CL_GREEN  lv_color_hex(0x8CC152) // low usage (<50%)
#define CL_AMBER  lv_color_hex(0xD97757) // medium usage (50-80%)
#define CL_RED    lv_color_hex(0xC0392B) // high usage (>=80%)
#define CL_ACCENT lv_color_hex(0xD97757) // status line
