#include "wifi_manager.h"
#include "../claude_quota/claude_token_store.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <time.h>

// Hold this pin LOW at boot -> wipes the WiFi config and opens the config page.
// GPIO0 = BOOT button on the CYD board. Set to -1 to disable this feature.
#define WIFI_RESET_PIN 0

// Max time to wait when trying the saved WiFi (ms).
#define WIFI_CONNECT_TIMEOUT_MS 15000

#define MDNS_HOST "clawd" // reachable at http://clawd.local

static const char *NVS_WIFI = "wifi";

static Preferences prefs;
static WebServer server(80);
static DNSServer dns_server;

static bool server_running = false; // web server is up (AP or STA)
static bool portal_active = false;  // in AP + DNS captive-portal mode
static wifi_status_cb_t status_cb = nullptr;

static bool save_pending = false;
static uint32_t save_at = 0;

// Note: status strings stay plain ASCII (no diacritics) since the default
// LVGL font has no Vietnamese glyphs. The web page uses full Vietnamese.
static void notify(const char *msg)
{
    Serial.printf("[wifi] %s\n", msg);
    if (status_cb) status_cb(msg);
}

void wifi_manager_set_status_callback(wifi_status_cb_t cb) { status_cb = cb; }

bool wifi_manager_connected() { return WiFi.status() == WL_CONNECTED; }

void wifi_manager_reset()
{
    prefs.begin(NVS_WIFI, false);
    prefs.clear();
    prefs.end();
    notify("Da xoa cau hinh WiFi");
}

// ----- Config web page -----

static String html_form(const String &datalist_options)
{
    bool sta = wifi_manager_connected();

    String p = F(
        "<!DOCTYPE html><html lang='vi'><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>C\xE1\xBA\xA5u h\xC3\xACnh Clawdmeter</title><style>"
        "body{font-family:system-ui,-apple-system,sans-serif;background:#111;color:#eee;"
        "margin:0;padding:24px;display:flex;justify-content:center}"
        "form{width:100%;max-width:380px}h1{font-size:20px}"
        "p.info{color:#9ca3af;font-size:13px;line-height:1.5}"
        "label{display:block;margin:14px 0 4px}"
        "input{width:100%;padding:10px;border-radius:8px;border:1px solid #555;"
        "background:#1c1c1c;color:#eee;box-sizing:border-box}"
        "button{margin-top:20px;width:100%;padding:12px;border:0;border-radius:8px;"
        "background:#3b82f6;color:#fff;font-size:16px}"
        "</style></head><body><form method='POST' action='/save'><h1>Clawdmeter</h1>");

    if (sta) {
        p += F("<p class='info'>\xC4\x90\xC3\xA3 k\xE1\xBA\xBFt n\xE1\xBB\x91i WiFi: <b>");
        p += WiFi.SSID();
        p += F("</b> (");
        p += WiFi.localIP().toString();
        p += F(").<br>\xC4\x90\xE1\xBB\x83 tr\xE1\xBB\x91ng \xC3\xB4 kh\xC3\xB4ng mu\xE1\xBB\x91n \xC4\x91\xE1\xBB\x95i.</p>");
    }

    p += F(
        "<label>T\xC3\xAAn WiFi (SSID)</label>"
        "<input list='nw' name='ssid' autocomplete='off'>"
        "<datalist id='nw'>");
    p += datalist_options;
    p += F(
        "</datalist>"
        "<label>M\xE1\xBA\xADt kh\xE1\xBA\xA9u WiFi</label>"
        "<input type='password' name='pass'>"
        "<label>Claude token (sk-ant-oat01-...)</label>"
        "<input name='token' autocomplete='off' placeholder='");
    p += claude_token_has() ? F("\xC4\x91\xC3\xA3 l\xC6\xB0u - nh\xE1\xBA\xADp \xC4\x91\xE1\xBB\x83 thay")
                            : F("t\xE1\xBA\xA1o b\xE1\xBA\xB1ng: claude setup-token");
    p += F(
        "'>"
        "<button type='submit'>L\xC6\xB0u &amp; kh\xE1\xBB\x9Fi \xC4\x91\xE1\xBB\x99ng l\xE1\xBA\xA1i</button>"
        "</form></body></html>");
    return p;
}

static void handle_root()
{
    int n = WiFi.scanNetworks();
    String opts;
    for (int i = 0; i < n; i++) {
        String s = WiFi.SSID(i);
        s.replace("'", "");
        if (s.length()) opts += "<option value='" + s + "'>";
    }
    WiFi.scanDelete();
    server.send(200, "text/html; charset=utf-8", html_form(opts));
}

static void handle_save()
{
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");
    String token = server.arg("token");
    ssid.trim();
    token.trim();
    token.replace(" ", ""); // terminals sometimes wrap long tokens with a stray space

    bool changed = false;

    if (ssid.length()) {
        prefs.begin(NVS_WIFI, false);
        prefs.putString("ssid", ssid);
        prefs.putString("pass", pass);
        prefs.end();
        changed = true;
    } else if (!wifi_manager_connected()) {
        server.send(400, "text/html; charset=utf-8",
                    "<meta charset='utf-8'>Ch\xC6\xB0" "a k\xE1\xBA\xBFt n\xE1\xBB\x91i WiFi n\xC3\xAAn c\xE1\xBA\xA7n "
                    "nh\xE1\xBA\xADp t\xC3\xAAn WiFi. <a href='/'>Quay l\xE1\xBA\xA1i</a>");
        return;
    }

    if (token.length()) {
        claude_token_save(token);
        changed = true;
    }

    if (!changed) {
        server.send(200, "text/html; charset=utf-8",
                    "<meta charset='utf-8'>Kh\xC3\xB4ng c\xC3\xB3 g\xC3\xAC thay \xC4\x91\xE1\xBB\x95i. <a href='/'>Quay l\xE1\xBA\xA1i</a>");
        return;
    }

    server.send(200, "text/html; charset=utf-8", F(
        "<!DOCTYPE html><html><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'></head>"
        "<body style='font-family:system-ui;background:#111;color:#eee;padding:24px'>"
        "<h2>\xC4\x90\xC3\xA3 l\xC6\xB0u.</h2><p>Board s\xE1\xBA\xBD kh\xE1\xBB\x9Fi \xC4\x91\xE1\xBB\x99ng l\xE1\xBA\xA1i.</p></body></html>"));

    save_pending = true;
    save_at = millis();
}

static void handle_not_found()
{
    // Captive portal: redirect every request to the config page.
    server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
    server.send(302, "text/plain", "");
}

static void start_web_server(bool captive)
{
    server.on("/", HTTP_GET, handle_root);
    server.on("/save", HTTP_POST, handle_save);
    if (captive) server.onNotFound(handle_not_found);
    server.begin();
    server_running = true;
}

// ----- Connect / portal -----

static bool try_connect(const String &ssid, const String &pass, uint32_t timeout_ms)
{
    if (ssid.isEmpty()) return false;

    notify("Dang ket noi WiFi da luu...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeout_ms) {
        delay(200);
    }
    return WiFi.status() == WL_CONNECTED;
}

static void start_portal(const char *ap_ssid, const char *ap_pass)
{
    WiFi.mode(WIFI_AP_STA);
    bool secured = ap_pass && strlen(ap_pass) >= 8;
    WiFi.softAP(ap_ssid, secured ? ap_pass : nullptr);
    delay(100);

    IPAddress ip = WiFi.softAPIP(); // usually 192.168.4.1

    dns_server.setErrorReplyCode(DNSReplyCode::NoError);
    dns_server.start(53, "*", ip);
    start_web_server(true);
    portal_active = true;

    char buf[110];
    snprintf(buf, sizeof(buf), "Vao WiFi \"%s\" roi mo http://%s",
             ap_ssid, ip.toString().c_str());
    notify(buf);
}

void wifi_manager_begin(const char *ap_ssid, const char *ap_pass)
{
    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);

#if WIFI_RESET_PIN >= 0
    pinMode(WIFI_RESET_PIN, INPUT_PULLUP);
    delay(10);
    bool force_portal = (digitalRead(WIFI_RESET_PIN) == LOW);
    if (force_portal) {
        notify("Nut BOOT dang giu -> xoa cau hinh");
        wifi_manager_reset();
    }
#else
    bool force_portal = false;
#endif

    String ssid, pass;
    if (!force_portal) {
        // RW so the namespace gets created on first run (avoids a NOT_FOUND log).
        prefs.begin(NVS_WIFI, false);
        ssid = prefs.getString("ssid", "");
        pass = prefs.getString("pass", "");
        prefs.end();
    }

    if (!force_portal && try_connect(ssid, pass, WIFI_CONNECT_TIMEOUT_MS)) {
        configTime(0, 0, "pool.ntp.org", "time.google.com"); // UTC, for reset-time math
        if (MDNS.begin(MDNS_HOST)) MDNS.addService("http", "tcp", 80);
        start_web_server(false); // keep the config page reachable on LAN to change the token later

        char buf[80];
        snprintf(buf, sizeof(buf), "Da ket noi: %s", WiFi.localIP().toString().c_str());
        notify(buf);
        return;
    }

    notify("Chua co WiFi -> bat che do cau hinh");
    start_portal(ap_ssid, ap_pass);
}

void wifi_manager_loop()
{
    if (portal_active) dns_server.processNextRequest();
    if (server_running) server.handleClient();

    // Restart once the "saved" page has been served to the browser.
    if (save_pending && millis() - save_at > 1500) {
        save_pending = false;
        notify("Khoi dong lai...");
        delay(200);
        ESP.restart();
    }
}
