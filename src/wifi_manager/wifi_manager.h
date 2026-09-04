#pragma once
#include <Arduino.h>

// WiFi connection + config page (captive portal).
//
// Flow:
//   1. On boot, read the saved SSID/password from NVS and try to connect (STA).
//   2. If none saved or connection fails -> board starts its own WiFi (AP) +
//      web server. User joins the board's WiFi, opens its IP, enters the
//      home WiFi name + password -> board saves to NVS and restarts.
//   3. Next boot connects directly.
//
// Hold the BOOT button (GPIO0) while powering on to wipe the config and
// go back to the config page.

typedef void (*wifi_status_cb_t)(const char *msg);

// Call once in setup(). ap_ssid: WiFi name the board broadcasts when it needs
// configuring. ap_pass: >= 8 chars gives the AP a password, otherwise it's open.
// This function CAN block for up to ~15s while trying the saved WiFi.
void wifi_manager_begin(const char *ap_ssid, const char *ap_pass = nullptr);

// Call every loop() to serve the web server + DNS while in config mode.
void wifi_manager_loop();

// true once connected to WiFi (STA).
bool wifi_manager_connected();

// true if a WiFi SSID is saved in NVS, regardless of current connection state.
bool wifi_manager_has_credentials();

// Registers a callback for status updates (shown on screen).
void wifi_manager_set_status_callback(wifi_status_cb_t cb);

// Clears the saved SSID/password.
void wifi_manager_reset();
