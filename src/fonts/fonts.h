#pragma once
#include <lvgl.h>

// Tiempos (Claude's serif) is only used for the clock - digits + ':' so plain
// ASCII is enough. Everything else uses LVGL's built-in Montserrat, which has
// '°', '•', and extended Latin.
#ifdef __cplusplus
extern "C" {
#endif

extern const lv_font_t font_tiempos_34;

#ifdef __cplusplus
}
#endif
