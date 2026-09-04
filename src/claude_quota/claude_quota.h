#pragma once
#include <Arduino.h>

// Fetches Claude quota by POSTing to https://api.anthropic.com/v1/messages
// with an OAuth token (sk-ant-oat01-..., created via `claude setup-token`).
// The quota numbers are in the RESPONSE HEADERS "anthropic-ratelimit-unified-*",
// not the body.
//
// Token comes from the WiFi config page, read back via claude_token_store.

struct claude_quota_t {
    bool valid;             // last poll returned usable data
    bool token_missing;     // no token stored
    int  http_status;       // last HTTP status code

    int  session_pct;       // % used in the 5h window (0-100), -1 if unknown
    int  session_reset_min; // minutes until the 5h window resets, -1 if unknown
    int  week_pct;          // % used in the 7d window
    int  week_reset_min;

    char status[16];        // "allowed" / "expired" / ...
};

typedef void (*claude_quota_cb_t)(const claude_quota_t &q);

void claude_quota_begin();                            // loads the token, no network call yet
void claude_quota_loop();                             // call every loop(); polls every 60s
void claude_quota_poll_now();                         // force a poll on the next loop()
const claude_quota_t &claude_quota_get();
void claude_quota_set_callback(claude_quota_cb_t cb); // called after each poll
