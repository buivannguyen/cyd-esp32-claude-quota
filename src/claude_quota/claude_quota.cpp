#include "claude_quota.h"
#include "claude_token_store.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <time.h>

#define POLL_INTERVAL_MS 60000UL
#define API_URL "https://api.anthropic.com/v1/messages"
// Request body is kept minimal on purpose - we only need the response headers.
#define API_BODY "{\"model\":\"claude-haiku-4-5-20251001\",\"max_tokens\":1," \
                 "\"messages\":[{\"role\":\"user\",\"content\":\"hi\"}]}"

// Headers to collect (HTTPClient only keeps headers listed here).
static const char *HDR_KEYS[] = {
    "anthropic-ratelimit-unified-5h-utilization",
    "anthropic-ratelimit-unified-5h-reset",
    "anthropic-ratelimit-unified-5h-status",
    "anthropic-ratelimit-unified-7d-utilization",
    "anthropic-ratelimit-unified-7d-reset",
    "anthropic-ratelimit-unified-status",
    "anthropic-ratelimit-unified-overage-utilization",
    "anthropic-ratelimit-unified-overage-reset",
};

static claude_quota_t g_q;
static String g_token;
static claude_quota_cb_t g_cb = nullptr;
static uint32_t g_last_poll = 0;
static bool g_pending = true;

void claude_quota_begin()
{
    memset(&g_q, 0, sizeof(g_q));
    g_q.session_pct = g_q.session_reset_min = -1;
    g_q.week_pct = g_q.week_reset_min = -1;
    g_token = claude_token_load();
    g_q.token_missing = g_token.isEmpty();
}

void claude_quota_set_callback(claude_quota_cb_t cb) { g_cb = cb; }
const claude_quota_t &claude_quota_get() { return g_q; }
void claude_quota_poll_now() { g_pending = true; }

static int pct_from_util(const String &s)
{
    if (s.isEmpty()) return -1;
    int v = (int)lroundf(s.toFloat() * 100.0f); // header looks like "0.42"
    return v < 0 ? 0 : (v > 100 ? 100 : v);
}

static int mins_from_reset(const String &s)
{
    if (s.isEmpty()) return -1;
    double reset = atof(s.c_str()); // epoch seconds
    time_t now = time(nullptr);
    if (reset < 1e9 || now < 1000000000) return -1; // no NTP yet, or unexpected header
    double mins = (reset - (double)now) / 60.0;
    return mins < 0 ? 0 : (int)llround(mins);
}

static void do_poll()
{
    if (g_token.isEmpty()) {
        g_q.token_missing = true;
        g_q.valid = false;
        if (g_cb) g_cb(g_q);
        return;
    }
    if (WiFi.status() != WL_CONNECTED) return;

    Serial.printf("[quota] polling... (heap free %u)\n", (unsigned)ESP.getFreeHeap());

    WiFiClientSecure client;
    client.setInsecure();          // skip cert verification, keeps this simple
    client.setHandshakeTimeout(15); // seconds - avoid hanging long if TLS stalls

    HTTPClient https;
    https.setConnectTimeout(15000);
    https.setTimeout(15000);
    https.setReuse(false);
    if (!https.begin(client, API_URL)) {
        Serial.println("[quota] https.begin() that bai");
        g_q.valid = false;
        if (g_cb) g_cb(g_q);
        return;
    }

    https.collectHeaders(HDR_KEYS, sizeof(HDR_KEYS) / sizeof(HDR_KEYS[0]));
    https.addHeader("Content-Type", "application/json");
    https.addHeader("anthropic-version", "2023-06-01");
    https.addHeader("anthropic-beta", "oauth-2025-04-20"); // required for OAuth tokens
    https.addHeader("User-Agent", "claude-code/2.1.5");
    https.addHeader("Authorization", "Bearer " + g_token);

    int code = https.POST((uint8_t *)API_BODY, strlen(API_BODY));
    g_q.http_status = code;
    Serial.printf("[quota] POST -> %d\n", code);

    if (code == 401 || code == 403) {
        g_q.valid = false;
        g_q.token_missing = false;
        strncpy(g_q.status, "expired", sizeof(g_q.status));
        Serial.printf("[quota] HTTP %d - token het han/sai\n", code);
    } else if (code == 200) {
        String u5 = https.header("anthropic-ratelimit-unified-5h-utilization");
        String r5 = https.header("anthropic-ratelimit-unified-5h-reset");
        String u7 = https.header("anthropic-ratelimit-unified-7d-utilization");
        String r7 = https.header("anthropic-ratelimit-unified-7d-reset");
        String st = https.header("anthropic-ratelimit-unified-5h-status");

        if (u5.isEmpty()) { // Enterprise/overage account
            u5 = https.header("anthropic-ratelimit-unified-overage-utilization");
            r5 = https.header("anthropic-ratelimit-unified-overage-reset");
            st = https.header("anthropic-ratelimit-unified-status");
        }

        g_q.session_pct = pct_from_util(u5);
        g_q.session_reset_min = mins_from_reset(r5);
        g_q.week_pct = pct_from_util(u7);
        g_q.week_reset_min = mins_from_reset(r7);
        st.toCharArray(g_q.status, sizeof(g_q.status));
        g_q.valid = (g_q.session_pct >= 0);

        Serial.printf("[quota] 5h=%d%% (reset %dm)  7d=%d%% (reset %dm)  st=%s\n",
                      g_q.session_pct, g_q.session_reset_min,
                      g_q.week_pct, g_q.week_reset_min, g_q.status);
    } else {
        g_q.valid = false;
        Serial.printf("[quota] HTTP %d\n", code);
    }

    https.end();
    if (g_cb) g_cb(g_q);
}

void claude_quota_loop()
{
    if (WiFi.status() != WL_CONNECTED) return;

    uint32_t now = millis();
    if (g_pending || now - g_last_poll >= POLL_INTERVAL_MS) {
        g_pending = false;
        g_last_poll = now;
        do_poll();
    }
}
