#ifndef SLANG_CLOCK_COLOR_CONFIG_H
#define SLANG_CLOCK_COLOR_CONFIG_H

#include <stddef.h>

#define SCREEN_ROTATION   0
#define EPD_WIDTH       400
#define EPD_HEIGHT      600

#define EPD_MODE_FASTEST 1

#define BTN_A_PIN        10
#define BTN_B_PIN         9
#define BTN_C_PIN         1

#define BTN_DEBOUNCE_MS      30
#define BTN_SETUP_HOLD_MS  2000
#define BTN_CANCEL_HOLD_MS 3000
#define BTN_POWER_HOLD_MS  PWR_HOLD_MS

#define LED_BRIGHTNESS   63

#define QUANT_MINUTES         5
#define NVS_FLUSH_EVERY      12

#define TZ_OFFSET_SEC  10800

#define AP_SSID          "SLANG-CLOCK"
#define AP_PASS          "slangclock"
#define SETUP_TIMEOUT_MS (10UL * 60UL * 1000UL)

#define WIFI_SSID        ""
#define WIFI_PASS        ""
#define NTP_SERVER_1     "time.google.com"
#define NTP_SERVER_2     "time.cloudflare.com"
#define NTP_SERVER_3     "ntp1.vniiftri.ru"
#define WIFI_TIMEOUT_MS  12000
#define NTP_PER_SERVER_MS 9000
#define NTP_VALID_EPOCH  1735689600UL
#define NTP_INTERVAL_SEC 86400UL

#define NVS_NAMESPACE    "slangclr"
#define STATE_MAGIC      0x534C4743UL
#define STATE_VERSION    2
#define PERSIST_MAGIC    0x534C5032UL
#define PERSIST_VERSION  1

#define SCREEN_W         EPD_WIDTH
#define SCREEN_H         EPD_HEIGHT

#define PANEL_BUSY_TIMEOUT_MS 20000

#define PM1_ADDR         0x6E
#define PM1_REG_BTN      0x48
#define PM1_REG_BTN_CFG1 0x49
#define PM1_REG_BTN_CFG2 0x4A
#define PM1_FREQ         100000
#define PWR_POLL_MS      20
#define PWR_DEBOUNCE_SAMPLES 2
#define PWR_HOLD_MS      1500

#define HERO_MAX_LINES       6
#define HERO_MIN_PX         26
#define HERO_MAX_PX         72
#define PUNCH_MAX_LINES      3
#define PUNCH_MAX_PX        26
#define PUNCH_MIN_PX        16
#define PUNCH_MAX_H        120

#define MARGIN_X            24
#define MARGIN_BOTTOM       32
#define HERO_TOP            32
#define CARD_GAP            20

#ifndef SLANG_USB_STAY_AWAKE
#define SLANG_USB_STAY_AWAKE 1
#endif

#endif
