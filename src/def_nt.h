#pragma once

// ─── Screen ───────────────────────────────────────────────
#define SCREEN_WIDTH     640
#define SCREEN_HEIGHT    480
#define APP_NAME         "NewsTicker"
#define FULLSCREEN       true
#define MS_PER_FRAME     16       // ~60 FPS

// ─── Layout zones (y-axis) ────────────────────────────────
#define HEADER_H         42
#define FOOTER_H         48
#define TICKER_H         44
#define MAIN_TOP         HEADER_H
#define TICKER_TOP       (SCREEN_HEIGHT - FOOTER_H - TICKER_H)
#define FOOTER_TOP       (SCREEN_HEIGHT - FOOTER_H)
#define MAIN_H           (TICKER_TOP - MAIN_TOP)
#define WEATHER_W        205
#define DIVIDER_X        WEATHER_W
#define GOLD_X           (WEATHER_W + 2)
#define GOLD_W           (SCREEN_WIDTH - GOLD_X)

// ─── Font sizes ───────────────────────────────────────────
#define FONT_TITLE       20
#define FONT_LARGE       38
#define FONT_NORMAL      16
#define FONT_SMALL       13
#define FONT_TICKER      14
#define FONT_NAME        "res/NotoSans-Regular.ttf"

// ─── Timing ───────────────────────────────────────────────
#define REFRESH_MS       60000     // 60 giây tự động refresh
#define NEWS_SCROLL_PX   1         // px/frame ticker cuộn

// ─── API URLs ─────────────────────────────────────────────
#define URL_WEATHER "https://api.open-meteo.com/v1/forecast?latitude=21.0285&longitude=105.8542&current=temperature_2m,relative_humidity_2m,wind_speed_10m,weather_code"
#define URL_GOLD    "https://query1.finance.yahoo.com/v8/finance/chart/GC=F?interval=1m&range=1d"
#define URL_NEWS    "http://feeds.bbci.co.uk/news/world/rss.xml"

// ─── Joystick Buttons (R36S layout) ──────────────────────
#define BTN_A  0   // Confirm / Refresh
#define BTN_B  1   // Back / Exit
#define BTN_X  2
#define BTN_Y  3

#define BUTTON_A(e) (e.type==SDL_JOYBUTTONDOWN && e.jbutton.button==BTN_A)
#define BUTTON_B(e) (e.type==SDL_JOYBUTTONDOWN && e.jbutton.button==BTN_B)

// ─── Colors (RGBA) ──────────────────────────────────────────
struct RGBA { Uint8 r, g, b, a; };

namespace Palette {
    constexpr RGBA BG_VOID          {0x0A, 0x0E, 0x17, 255};
    constexpr RGBA BG_GRID          {0x12, 0x1A, 0x2C, 102};   // ~40%
    constexpr RGBA PANEL_FILL       {0x0F, 0x15, 0x24, 217};   // ~85%
    constexpr RGBA PANEL_FILL_FOCUS {0x14, 0x1C, 0x33, 235};   // ~92%
    constexpr RGBA BORDER_DIM       {0x2A, 0x35, 0x50, 255};
    constexpr RGBA BORDER_HEADER    {0x1C, 0x25, 0x40, 255};
    constexpr RGBA NEON_CYAN        {0x00, 0xF0, 0xFF, 255};
    constexpr RGBA NEON_AMBER       {0xFF, 0xB0, 0x00, 255};
    constexpr RGBA NEON_MAGENTA     {0xFF, 0x2F, 0xD0, 255};
    constexpr RGBA NEON_GREEN       {0x39, 0xFF, 0x88, 255};
    constexpr RGBA ALERT_RED        {0xFF, 0x3B, 0x5C, 255};
    constexpr RGBA TEXT_PRIMARY     {0xE8, 0xF1, 0xFF, 255};
    constexpr RGBA TEXT_SECONDARY   {0x7C, 0x8D, 0xA6, 255};
    constexpr RGBA TEXT_DIM         {0x46, 0x52, 0x72, 255};
    constexpr RGBA OVERLAY_SCRIM    {0x00, 0x00, 0x00, 178};   // ~70%
}
