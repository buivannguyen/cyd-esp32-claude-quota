#include "weather.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

#define POLL_INTERVAL_MS (15UL * 60UL * 1000UL) // 15 min

static weather_t g_w;
static weather_cb_t g_cb = nullptr;
static float g_lat = 0, g_lon = 0;
static uint32_t g_last_poll = 0;
static bool g_pending = true;

void weather_begin(float lat, float lon)
{
    memset(&g_w, 0, sizeof(g_w));
    g_lat = lat;
    g_lon = lon;
}

void weather_set_callback(weather_cb_t cb) { g_cb = cb; }
const weather_t &weather_get() { return g_w; }

// Reads the number right after "\"key\":" in Open-Meteo's flat JSON.
static bool json_number_after(const String &body, const char *key, double &out)
{
    int i = body.indexOf(key);
    if (i < 0) return false;
    i += strlen(key);
    while (i < (int)body.length() && (body[i] == ' ' || body[i] == ':')) i++;
    int j = i;
    while (j < (int)body.length() &&
           (isdigit(body[j]) || body[j] == '-' || body[j] == '+' || body[j] == '.' ||
            body[j] == 'e' || body[j] == 'E')) j++;
    if (j == i) return false;
    out = atof(body.substring(i, j).c_str());
    return true;
}

static void do_poll()
{
    if (WiFi.status() != WL_CONNECTED) return;

    char url[160];
    snprintf(url, sizeof(url),
             "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
             "&current=temperature_2m",
             g_lat, g_lon);

    WiFiClientSecure client;
    client.setInsecure();
    client.setHandshakeTimeout(15);

    HTTPClient https;
    https.setConnectTimeout(15000);
    https.setTimeout(15000);
    https.setReuse(false);
    if (!https.begin(client, url)) {
        Serial.println("[weather] https.begin() that bai");
        return;
    }

    int code = https.GET();
    if (code == 200) {
        String body = https.getString();
        // "temperature_2m" appears in both "current_units" (string) and
        // "current" (number) - anchor on "current" first, then read the number.
        int cur = body.indexOf("\"current\":");
        String scope = cur >= 0 ? body.substring(cur) : body;
        double temp;
        bool okt = json_number_after(scope, "\"temperature_2m\"", temp);
        if (okt) {
            g_w.temp_c = (float)temp;
            g_w.valid = true;
            Serial.printf("[weather] %.1fC\n", g_w.temp_c);
        }
    } else {
        Serial.printf("[weather] HTTP %d\n", code);
    }
    https.end();
    if (g_cb) g_cb(g_w);
}

void weather_loop()
{
    if (WiFi.status() != WL_CONNECTED) return;
    uint32_t now = millis();
    if (g_pending || now - g_last_poll >= POLL_INTERVAL_MS) {
        g_pending = false;
        g_last_poll = now;
        do_poll();
    }
}
