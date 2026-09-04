#pragma once
#include <Arduino.h>

// Single source of truth for the Claude OAuth token: NVS namespace "claude",
// key "token". Entered only via the web config page (wifi_manager) - no
// hardcoded token in source.

bool claude_token_has();             // is a token stored
String claude_token_load();          // stored token, "" if none
void claude_token_save(const String &token);
