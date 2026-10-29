#pragma once

#include <stddef.h>
#include <stdint.h>
#include <time.h>

static constexpr int SCREEN_ROTATION = 1;
static constexpr int SCREEN_W = 600;
static constexpr int SCREEN_H = 400;
static constexpr int HEADER_H = 36;
static constexpr int BEZEL_INSET = 4;

static constexpr int RADAR_CX = 160;
static constexpr int RADAR_CY = (HEADER_H + SCREEN_H - BEZEL_INSET) / 2;
static constexpr int RADAR_HALF = 148;
static constexpr int RADAR_CORNER_R = 41;

static_assert(RADAR_CY - RADAR_HALF > HEADER_H,
              "radar reaches into the header");
static_assert(RADAR_CY + RADAR_HALF < SCREEN_H - BEZEL_INSET,
              "radar does not fit below the header");
static_assert(RADAR_CORNER_R > 0 && RADAR_CORNER_R < RADAR_HALF,
              "radar corner radius exceeds half its side");

static constexpr int ROSE_STEP_DEG  = 10;
static constexpr int ROSE_MAJOR_DEG = 30;
static constexpr int ROSE_MINOR_LEN = 4;
static constexpr int ROSE_MAJOR_LEN = 9;

static constexpr int DART_NOSE  =  8;
static constexpr int DART_TAIL  = -6;
static constexpr int DART_HALF  =  5;
static constexpr int DART_NOTCH = -3;

static constexpr int MARK_CLEAR = DART_NOSE + 2;
static constexpr int LABEL_POSITIONS = 8;

static constexpr int VEL_MIN_PX  = 7;
static constexpr int VEL_MAX_PX  = 22;
static constexpr int VEL_FULL_KT = 500;

static constexpr int CARD_X = 316;
static constexpr int CARD_EDGE   = SCREEN_W - BEZEL_INSET - 4;
static constexpr int CARD_W      = CARD_EDGE - CARD_X;
static constexpr int CARD_PAD    = 8;
static constexpr int CARD_TEXT_W = CARD_W - 2 * CARD_PAD;
static constexpr int CARD_TEXT_X = CARD_X + CARD_PAD;
static constexpr int CARD_TEXT_R = CARD_TEXT_X + CARD_TEXT_W;

static constexpr int CARD_ICON      = 44;
static constexpr int CARD_PLATE_PAD = 4;
static constexpr int CARD_PLATE_H   = CARD_ICON + 2 * CARD_PLATE_PAD;

static constexpr int CARD_TITLE_X = CARD_X + CARD_PLATE_PAD + CARD_ICON + CARD_PAD;
static constexpr int CARD_TITLE_W = CARD_EDGE - CARD_PAD - CARD_TITLE_X;

static constexpr int CARD_VALUE_H   = 32;
static constexpr int CARD_VALUE_GAP = 8;
static constexpr int CARD_ROW_GAP   = 6;
static constexpr int CARD_LABEL_W   = 64;

static constexpr int CARD_QUOTE_LINES = 3;
static constexpr int CARD_RECORDS     = 4;
static constexpr int METAR_LINES      = 3;

static constexpr int AIRLINE_MAX = 41;

static constexpr int JB_PX_HEADER   = 16;
static constexpr int JB_PX_LABEL    = 14;
static constexpr int JB_PX_SUBLABEL = 11;
static constexpr int JB_PX_RING     = 11;
static constexpr int JB_PX_OFF      = 48;

static constexpr int JB_PX_CARD_PLATE  = 18;
static constexpr int JB_PX_CARD_VALUE  = 17;
static constexpr int JB_PX_CARD_LABEL  = 12;
static constexpr int JB_PX_CARD_QUOTE  = 15;
static constexpr int JB_PX_CARD_RECORD = 14;
static constexpr int JB_PX_METAR       = 13;

static constexpr int JB_ROW_H(int px) { return (1180 * px + 500) / 1000; }

static constexpr int CARD_BODY_TOP = HEADER_H + 8;
static constexpr int CARD_BODY_BOT = SCREEN_H - BEZEL_INSET;

static constexpr int CARD_RULE_GAP = CARD_ROW_GAP / 2;

static constexpr int CARD_ROWS_TOP   = CARD_BODY_TOP + CARD_PLATE_H + CARD_ROW_GAP;
static constexpr int CARD_VALUES_TOP = CARD_ROWS_TOP + 2 * JB_ROW_H(JB_PX_CARD_VALUE)
                                                     + CARD_ROW_GAP;
static constexpr int CARD_QUOTE_TOP  = CARD_VALUES_TOP + CARD_VALUE_H + CARD_ROW_GAP;
static constexpr int CARD_BLOCK_H    = CARD_QUOTE_TOP
                                     + CARD_QUOTE_LINES * JB_ROW_H(JB_PX_CARD_QUOTE)
                                     - CARD_BODY_TOP;

static constexpr int RECORDS_BLOCK_H = CARD_RULE_GAP
                                     + CARD_RECORDS * JB_ROW_H(JB_PX_CARD_RECORD);

static constexpr int METAR_BLOCK_H = 2 * CARD_PAD + METAR_LINES * JB_ROW_H(JB_PX_METAR);

static constexpr int METAR_TOP        = CARD_BODY_BOT - METAR_BLOCK_H;
static constexpr int CARD_RECORDS_TOP = METAR_TOP - CARD_ROW_GAP - RECORDS_BLOCK_H;

static_assert(CARD_BODY_TOP + CARD_BLOCK_H + CARD_ROW_GAP <= CARD_RECORDS_TOP,
              "right column does not fit the body band");
static_assert(CARD_LABEL_W < CARD_TEXT_W,
              "label column wider than the text column");

static constexpr int CARD_ADV_UP(int px) { return (600 * px + 999) / 1000; }

static constexpr int CARD_PLATE_INNER = (CARD_W - CARD_VALUE_GAP) / 2 - 2 * CARD_PAD;

static_assert(7 * CARD_ADV_UP(JB_PX_CARD_VALUE) <= CARD_PLATE_INNER,
              "value '13100 M' does not fit the ALT plate");
static_assert(9 * CARD_ADV_UP(JB_PX_CARD_VALUE) <= CARD_PLATE_INNER,
              "value '1200 KM/H' does not fit the SPD plate");
static_assert(15 * CARD_ADV_UP(JB_PX_CARD_PLATE) <= CARD_TITLE_W,
              "title 'CELEBRITY ALERT' does not fit the card plate");
static_assert(9 * CARD_ADV_UP(JB_PX_CARD_PLATE) <= CARD_TITLE_W,
              "title 'EMERGENCY' does not fit the card plate");

static constexpr int STATUS_BATT_W = 24;
static constexpr int STATUS_BATT_H = 12;
static constexpr int STATUS_BATT_NUB_W = 3;
static constexpr int STATUS_BATT_GAP = 5;
static constexpr int STATUS_GAP = 14;
static constexpr int STATUS_ARROW_W = 11;
static constexpr int STATUS_ARROW_H = 9;
static constexpr int STATUS_ARROW_GAP = 5;

static constexpr int VBUS_PRESENT_MV = 3000;

static constexpr int      BATT_TREND_MIN_MV = 10;
static constexpr uint32_t BATT_TREND_MIN_MS = 60000;

static constexpr uint32_t VBUS_POLL_MS = 2000;
static constexpr int      VBUS_CONFIRM_SAMPLES = 2;

static constexpr int MAX_TARGETS = 64;

static constexpr time_t TIME_VALID_EPOCH = 1577836800;

static constexpr uint32_t SHUTDOWN_HOLD_MS = 1500;

static constexpr uint32_t PANEL_BUSY_TIMEOUT_MS  = 20000;
static constexpr uint32_t PANEL_FRAME_SUSPECT_MS = 3 * PANEL_BUSY_TIMEOUT_MS + 1000;

static constexpr uint32_t LOOP_WDT_MS = 180000;

static_assert(LOOP_WDT_MS > PANEL_FRAME_SUSPECT_MS,
              "the watchdog would fire on a legitimate frame");

static constexpr uint32_t BTN_LONG_MS  = 800;
static constexpr uint32_t CHORD_MS     = 1500;
static constexpr uint32_t INPUT_PERIOD_MS = 20;
static constexpr int      INPUT_DEBOUNCE_SAMPLES = 2;
static constexpr int      INPUT_QUEUE_LEN = 16;

static constexpr uint32_t REFRESH_INTERVAL_MS = 120000;
static constexpr uint32_t WIFI_TIMEOUT_MS = 15000;
static constexpr uint32_t HTTP_TIMEOUT_MS = 15000;
static constexpr uint32_t NTP_WAIT_MS     = 8000;
static constexpr uint32_t METAR_TIMEOUT_MS  = 12000;
static constexpr uint32_t METAR_REFRESH_MS  = 20 * 60 * 1000;
static constexpr uint32_t METAR_STALE_MS    = 90 * 60 * 1000;

static constexpr uint32_t NET_STACK_BYTES = 20480;
static constexpr int      NET_QUEUE_LEN   = 4;
static constexpr int      NET_TASK_PRIO   = 2;
static constexpr int      NET_TASK_CORE   = 0;

static constexpr uint32_t TERRAIN_TILE_TIMEOUT_MS  = 15000;
static constexpr uint32_t TERRAIN_BUILD_TIMEOUT_MS = 60000;
static constexpr uint32_t TERRAIN_RETRY_MS         = 10 * 60 * 1000;
static constexpr float    TERRAIN_SUN_AZ_DEG       = 315.0f;
static constexpr float    TERRAIN_SUN_ALT_DEG      = 45.0f;
static constexpr float    TERRAIN_REF_SLOPE        = 0.20f;
static constexpr float    TERRAIN_GAMMA            = 1.45f;
static constexpr int      TERRAIN_MAX_DENSITY      = 32;
static constexpr int      TERRAIN_COAST            = 64;
static constexpr int      TERRAIN_STRETCH_PERMILLE = 995;

static constexpr uint32_t NET_REQUEST_TIMEOUT_MS =
    WIFI_TIMEOUT_MS + NTP_WAIT_MS + HTTP_TIMEOUT_MS + METAR_TIMEOUT_MS +
    TERRAIN_BUILD_TIMEOUT_MS + 7000;

static_assert(NET_REQUEST_TIMEOUT_MS < LOOP_WDT_MS,
              "network wait longer than the watchdog timeout");
static_assert(NET_REQUEST_TIMEOUT_MS + PANEL_FRAME_SUSPECT_MS < LOOP_WDT_MS,
              "the longest legitimate tick no longer fits the watchdog");
static_assert(TERRAIN_COAST > TERRAIN_MAX_DENSITY,
              "coast must stay outside the dither density range");
static_assert(TERRAIN_MAX_DENSITY < 64,
              "density above 64 is unrepresentable by an 8x8 matrix");

static constexpr uint8_t  LED_BRIGHT = 221;
static constexpr uint32_t LED_TICK_MS        = 20;
static constexpr uint32_t LED_BEAT_PERIOD_MS = 1000;
static constexpr uint32_t LED_BEAT_RISE_MS   = 70;
static constexpr uint32_t LED_BEAT_FALL_MS   = 520;
static constexpr uint8_t  LED_BEAT_R = 0;
static constexpr uint8_t  LED_BEAT_G = 120;
static constexpr uint8_t  LED_BEAT_B = 255;
static constexpr uint32_t LED_STOP_SHOW_MS = 250;
static_assert(LED_BEAT_RISE_MS + LED_BEAT_FALL_MS < LED_BEAT_PERIOD_MS,
              "the beat never goes dark between pulses");

static constexpr uint16_t WEB_PORT = 80;
static constexpr const char* AP_PREFIX = "SarcasticRadar-";
static constexpr const char* AP_IP = "192.168.4.1";
static constexpr int AP_PASS_LEN = 8;
static constexpr int QR_SIZE_PX  = 232;

static constexpr float RADIUS_KM_NEAR = 50.0f;
static constexpr float RADIUS_KM_FAR  = 100.0f;
static constexpr float RADIUS_KM_DEF  = RADIUS_KM_NEAR;
static constexpr int   UTC_OFFSET_MIN = -12 * 60;
static constexpr int   UTC_OFFSET_MAX = 14 * 60;
static constexpr int   METAR_ID_LEN   = 4;

static_assert(RADIUS_KM_FAR > RADIUS_KM_NEAR, "radius choices out of order");
static_assert(QR_SIZE_PX < SCREEN_H - HEADER_H, "QR code taller than the free space on screen");
