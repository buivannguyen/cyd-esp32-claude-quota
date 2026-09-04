# esp32-cyd-claude-quota

[**Flash firmware from your browser**](https://buivannguyen.github.io/cyd-esp32-claude-quota/) · [**Buy the board**](https://s.shopee.vn/8KpFFpuR4g)

![ESP32 CYD running the quota dashboard](https://buivannguyen.github.io/cyd-esp32-claude-quota/esp32-claude-quota.png)

Shows **Claude quota usage** (5h / 7d windows), **clock**, and **temperature** on an **ESP32 CYD** board (ESP32-2432S028R, 240×320 ILI9341). PlatformIO + Arduino + LVGL 9.

The board connects to WiFi, calls `api.anthropic.com` directly with an OAuth token, reads the rate-limit headers, and draws two % bars. Clock comes from NTP, temperature from Open-Meteo (no API key needed).

Screen: `HH:MM` clock (top-left), temperature (top-right), two quota bars `5h` / `7d` (green < 70% → amber < 90% → red) with time-to-reset, status line at the bottom.

## Usage flow

1. Flash the firmware, power on. On first boot the board broadcasts an open WiFi network **`ESP32-CYD-Setup`**.
2. Connect your phone/laptop to it → the config page opens automatically (or go to `http://192.168.4.1`).
3. Enter your **home WiFi name + password** and **Claude token**. Save → the board restarts and connects.
4. It then shows 5h / 7d usage %, refreshed every 60 seconds.
5. To change the token later: visit `http://clawd.local` (or the board's LAN IP).

Generate a token with `claude setup-token` on a machine with Claude Code installed (token looks like `sk-ant-oat01-...`, valid ~1 year). Running that command again **revokes the previous token**.

Hold the **BOOT button (GPIO0)** while powering on to wipe the WiFi config and return to step 1.

## Components

| Component | Role |
|---|---|
| `platformio.ini` | Board `esp32dev`, TFT_eSPI pin flags + LVGL |
| `partitions.csv` | Single app (~3.9 MB) + NVS. No OTA, no filesystem |
| `include/lv_conf.h` | LVGL 9.4 config (`LV_USE_TFT_ESPI`, 16-bit color, Montserrat 28 enabled) |
| `src/display/` | LVGL + TFT_eSPI init + backlight (PWM on GPIO 21) |
| `src/wifi_manager/` | WiFi connect + captive portal + config page (WiFi & token), NTP, mDNS `clawd.local` |
| `src/claude_quota/` | Calls `POST api.anthropic.com/v1/messages`, reads `anthropic-ratelimit-unified-*` headers |
| `src/weather/` | Open-Meteo (no API key), polls current temperature every 15 min |
| `src/main.cpp` | Wires everything together + LVGL UI (clock, temperature, 2 quota bars). Edit `TZ_OFFSET_SEC`, `WEATHER_LAT`, `WEATHER_LON` at the top for your location |

## How the quota API works

```
POST https://api.anthropic.com/v1/messages
  Authorization: Bearer <oat01 token>
  anthropic-version: 2023-06-01
  anthropic-beta: oauth-2025-04-20      # required to use an OAuth token
  Content-Type: application/json
  User-Agent: claude-code/2.1.5
  body: {"model":"claude-haiku-4-5-20251001","max_tokens":1,"messages":[{"role":"user","content":"hi"}]}
```

The numbers live **in the response headers**, not the body:

| Header | Meaning |
|---|---|
| `anthropic-ratelimit-unified-5h-utilization` | 0.0–1.0 → ×100 = % used in the 5h window |
| `anthropic-ratelimit-unified-5h-reset` | epoch seconds → time until reset |
| `anthropic-ratelimit-unified-7d-utilization` / `-7d-reset` | 7-day window |
| `anthropic-ratelimit-unified-overage-*` | Enterprise/overage accounts |

`401` = token expired or revoked. Idea borrowed from [Clawdmeter](https://github.com/HermannBjorgvin/Clawdmeter) (the original runs as a PC daemon and sends data over BLE; here the board calls the API itself).

## Hardware (hardcoded in `src/display/display.h`)

Board: ESP32 CYD (ESP32-2432S028R)

![ESP32 CYD board](https://down-vn.img.susercontent.com/file/cn-11134207-7r98o-lzavtfbcpm029c.webp)

- 240×320, rotated `LV_DISPLAY_ROTATION_270` (landscape)
- Display SPI: MISO=12, MOSI=13, SCLK=14, CS=15, DC=2, RST=-1
- Driver `ILI9341_2_DRIVER`, SPI 55 MHz, backlight on GPIO 21
- Touch not wired up yet (XPT2046: CLK=25, MOSI=32, MISO=39, CS=33, IRQ=36)

## Build

```bash
pio run                # build
pio run -t upload      # flash
pio device monitor     # serial 115200
```
