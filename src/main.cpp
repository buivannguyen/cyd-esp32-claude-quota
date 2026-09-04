#include <Arduino.h>
#include <time.h>
#include "display/display.h"
#include "theme.h"
#include "assets/clawd_still.h"
#include "wifi_manager/wifi_manager.h"
#include "claude_quota/claude_quota.h"
#include "weather/weather.h"

// ---- Location config (change for your location) ----
#define TZ_OFFSET_SEC (7 * 3600)   // offset from UTC (VN = +7)
#define WEATHER_LAT   21.0285f     // Hanoi - change to your coordinates
#define WEATHER_LON   105.8542f

// ---- Layout (320x240 screen) ----
#define SCR_W       320
#define MARGIN      14
#define CARD_W      (SCR_W - 2 * MARGIN)
#define CARD_H      84
#define CARD_PAD_X  14
#define CARD_PAD_Y  10
#define CARD1_Y     46
#define CARD2_Y     (CARD1_Y + CARD_H + 6)
#define BAR_Y       32   // within the card, relative to content edge
#define RESET_Y     46

static lv_obj_t *g_root;
static lv_obj_t *lbl_clock, *lbl_weather, *lbl_status;
static lv_image_dsc_t mascot_dsc;

struct quota_row {
    lv_obj_t *bar;
    lv_obj_t *val;
    lv_obj_t *reset;
};
static quota_row row_5h, row_7d;

// Color thresholds matching Clawdmeter: >=80 red, >=50 amber, else green.
static lv_color_t color_for(int pct)
{
    if (pct >= 80) return CL_RED;
    if (pct >= 50) return CL_AMBER;
    return CL_GREEN;
}

static void fmt_reset(int m, char *out, size_t n)
{
    if (m < 0)          snprintf(out, n, "");
    else if (m < 60)    snprintf(out, n, "Resets in %dm", m);
    else if (m < 1440)  snprintf(out, n, "Resets in %dh %dm", m / 60, m % 60);
    else                snprintf(out, n, "Resets in %dd %dh", m / 1440, (m % 1440) / 60);
}

static lv_obj_t *make_card(lv_coord_t y)
{
    lv_obj_t *c = lv_obj_create(g_root);
    lv_obj_remove_style_all(c);
    lv_obj_set_pos(c, MARGIN, y);
    lv_obj_set_size(c, CARD_W, CARD_H);
    lv_obj_set_style_bg_color(c, CL_CARD, 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(c, 12, 0);
    lv_obj_set_style_pad_left(c, CARD_PAD_X, 0);
    lv_obj_set_style_pad_right(c, CARD_PAD_X, 0);
    lv_obj_set_style_pad_top(c, CARD_PAD_Y, 0);
    lv_obj_set_style_pad_bottom(c, CARD_PAD_Y, 0);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    return c;
}

static lv_obj_t *make_pill(lv_obj_t *parent, const char *text)
{
    lv_obj_t *p = lv_label_create(parent);
    lv_label_set_text(p, text);
    lv_obj_set_style_text_font(p, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(p, CL_TEXT, 0);
    lv_obj_set_style_bg_color(p, CL_PILL, 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(p, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_hor(p, 14, 0);
    lv_obj_set_style_pad_ver(p, 4, 0);
    lv_obj_align(p, LV_ALIGN_TOP_RIGHT, 0, 0);
    return p;
}

static void make_row(quota_row &r, lv_coord_t card_y, const char *pill_text)
{
    lv_obj_t *card = make_card(card_y);

    r.val = lv_label_create(card);
    lv_label_set_text(r.val, "--%");
    lv_obj_set_style_text_font(r.val, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(r.val, CL_TEXT, 0);
    lv_obj_set_pos(r.val, 0, 0);

    make_pill(card, pill_text);

    r.bar = lv_bar_create(card);
    lv_obj_set_pos(r.bar, 0, BAR_Y);
    lv_obj_set_size(r.bar, CARD_W - 2 * CARD_PAD_X, 12);
    lv_bar_set_range(r.bar, 0, 100);
    lv_bar_set_value(r.bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(r.bar, CL_TRACK, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(r.bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(r.bar, 3, LV_PART_MAIN);
    lv_obj_set_style_bg_color(r.bar, CL_GREEN, LV_PART_INDICATOR);
    lv_obj_set_style_radius(r.bar, 3, LV_PART_INDICATOR);

    r.reset = lv_label_create(card);
    lv_label_set_text(r.reset, "");
    lv_obj_set_style_text_font(r.reset, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(r.reset, CL_DIM, 0);
    lv_obj_set_pos(r.reset, 0, RESET_Y);
}

static void set_row(quota_row &r, int pct, int reset_min)
{
    if (pct < 0) return;
    lv_bar_set_value(r.bar, pct, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(r.bar, color_for(pct), LV_PART_INDICATOR);
    char b[8];
    snprintf(b, sizeof(b), "%d%%", pct);
    lv_label_set_text(r.val, b);
    char rs[28];
    fmt_reset(reset_min, rs, sizeof(rs));
    lv_label_set_text(r.reset, rs);
}

static void set_status(const char *msg)
{
    char b[48];
    snprintf(b, sizeof(b), "\xE2\x80\xA2 %s", msg); // "• msg"
    lv_label_set_text(lbl_status, b);
}

static void on_quota(const claude_quota_t &q)
{
    if (q.token_missing) { set_status("set token at clawd.local"); return; }
    if (!q.valid) {
        set_status((q.http_status == 401 || q.http_status == 403) ? "token expired"
                                                                 : "API error");
        return;
    }
    set_row(row_5h, q.session_pct, q.session_reset_min);
    set_row(row_7d, q.week_pct, q.week_reset_min);
    lv_label_set_text(lbl_status, ""); // clear it when things are fine
}

static void on_weather(const weather_t &w)
{
    if (!w.valid) return;
    char b[16];
    snprintf(b, sizeof(b), "%.0f\xC2\xB0""C", w.temp_c); // 27°C
    lv_label_set_text(lbl_weather, b);
}

static void on_wifi_status(const char *msg)
{
    if (!lbl_status) return;
    set_status(msg);
    lv_timer_handler();
}

static void update_clock()
{
    time_t now = time(nullptr);
    if (now < 1000000000) return;
    now += TZ_OFFSET_SEC;
    struct tm tm;
    gmtime_r(&now, &tm);

    static int last_min = -1;
    if (tm.tm_min == last_min) return;
    last_min = tm.tm_min;

    char b[8];
    snprintf(b, sizeof(b), "%02d:%02d", tm.tm_hour, tm.tm_min);
    lv_label_set_text(lbl_clock, b);
}

void setup()
{
    Serial.begin(115200);

    led_init();
    display_init();

    g_root = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(g_root);
    lv_obj_set_size(g_root, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(g_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(g_root, CL_BG, 0);
    lv_obj_set_style_bg_opa(g_root, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(g_root, CL_TEXT, 0);
    lv_obj_set_style_text_font(g_root, &lv_font_montserrat_16, 0);

    // Clawd mascot (top-left)
    mascot_dsc.header.cf = LV_COLOR_FORMAT_RGB565A8;
    mascot_dsc.header.w = CLAWD_STILL_W;
    mascot_dsc.header.h = CLAWD_STILL_H;
    mascot_dsc.header.stride = CLAWD_STILL_W * 2;
    mascot_dsc.data = clawd_still_data;
    mascot_dsc.data_size = CLAWD_STILL_W * CLAWD_STILL_H * 3;
    lv_obj_t *mascot = lv_image_create(g_root);
    lv_image_set_src(mascot, &mascot_dsc);
    lv_image_set_scale(mascot, 140); // ~55% -> ~40x26
    lv_obj_align(mascot, LV_ALIGN_TOP_LEFT, MARGIN, 4);

    // Clock - Tiempos serif, top-centered
    lbl_clock = lv_label_create(g_root);
    lv_label_set_text(lbl_clock, "--:--");
    lv_obj_set_style_text_font(lbl_clock, &font_tiempos_34, 0);
    lv_obj_set_style_text_color(lbl_clock, CL_TEXT, 0);
    lv_obj_align(lbl_clock, LV_ALIGN_TOP_MID, 0, 3);

    // Temperature - top-right, dimmed
    lbl_weather = lv_label_create(g_root);
    lv_label_set_text(lbl_weather, "");
    lv_obj_set_style_text_color(lbl_weather, CL_DIM, 0);
    lv_obj_align(lbl_weather, LV_ALIGN_TOP_RIGHT, -MARGIN, 10);

    make_row(row_5h, CARD1_Y, "Current");
    make_row(row_7d, CARD2_Y, "Weekly");

    lbl_status = lv_label_create(g_root);
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_status, CL_ACCENT, 0);
    lv_obj_align(lbl_status, LV_ALIGN_BOTTOM_MID, 0, -3);
    set_status("starting");

    lv_timer_handler();
    turn_on_display();

    wifi_manager_set_status_callback(on_wifi_status);
    wifi_manager_begin("ESP32-CYD-Setup");

    claude_quota_set_callback(on_quota);
    claude_quota_begin();

    weather_set_callback(on_weather);
    weather_begin(WEATHER_LAT, WEATHER_LON);
}

void loop()
{
    wifi_manager_loop();
    claude_quota_loop();
    weather_loop();
    update_clock();
    lv_timer_handler();
    delay(5);
}
