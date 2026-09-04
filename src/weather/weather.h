#pragma once
#include <Arduino.h>

// Current temperature from Open-Meteo (free, no API key).
// GET https://api.open-meteo.com/v1/forecast?latitude=..&longitude=..&current=temperature_2m

struct weather_t {
    bool  valid;
    float temp_c;
};

typedef void (*weather_cb_t)(const weather_t &w);

void weather_begin(float lat, float lon);
void weather_loop();                       // call every loop(); polls every ~15 min
const weather_t &weather_get();
void weather_set_callback(weather_cb_t cb);
