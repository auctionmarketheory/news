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
// Firmware A/B: D-Pad = Hat
#define BTN_A  0   // Confirm / Refresh  
#define BTN_B  1   // Back / Exit
#define BTN_X  2
#define BTN_Y  3
// Firmware C: D-Pad = Buttons
#define BTN_DPAD_UP    8
#define BTN_DPAD_DOWN  9
#define BTN_DPAD_LEFT  10
#define BTN_DPAD_RIGHT 11

#define BUTTON_A(e) (e.type==SDL_JOYBUTTONDOWN && e.jbutton.button==BTN_A)
#define BUTTON_B(e) (e.type==SDL_JOYBUTTONDOWN && e.jbutton.button==BTN_B)

// ─── Colors (RGBA) ──────────────────────────────────────────
struct RGBA { Uint8 r, g, b, a; };

namespace Palette {
    // --- Cyberpunk Dark Theme ---
    constexpr RGBA BG_VOID          {0x02, 0x02, 0x08, 255};   // Black void
    constexpr RGBA BG_GRID          {0x00, 0xFF, 0xE0, 18};    // Cyan ghost grid ~7%
    constexpr RGBA PANEL_FILL       {0x08, 0x06, 0x18, 230};   // Deep purple-black
    constexpr RGBA PANEL_FILL_FOCUS {0x10, 0x06, 0x28, 245};   // Brighter deep purple
    constexpr RGBA BORDER_DIM       {0x28, 0x18, 0x55, 255};   // Purple dim border
    constexpr RGBA BORDER_HEADER    {0x00, 0xE5, 0xFF, 60};    // Faint cyan header bar
    // --- Neon Accents ---
    constexpr RGBA NEON_CYAN        {0x00, 0xFF, 0xE0, 255};   // Electric cyan
    constexpr RGBA NEON_AMBER       {0xFF, 0xC4, 0x00, 255};   // Pure gold
    constexpr RGBA NEON_MAGENTA     {0xFF, 0x00, 0xA8, 255};   // Hot pink
    constexpr RGBA NEON_GREEN       {0x00, 0xFF, 0x87, 255};   // Acid green
    constexpr RGBA ALERT_RED        {0xFF, 0x1A, 0x45, 255};   // Vivid red
    // --- Text ---
    constexpr RGBA TEXT_PRIMARY     {0xF0, 0xFF, 0xFC, 255};   // Near-white with cyan tint
    constexpr RGBA TEXT_SECONDARY   {0x00, 0xD4, 0xC8, 200};   // Dim cyan
    constexpr RGBA TEXT_DIM         {0x30, 0x28, 0x60, 255};   // Faded purple
    constexpr RGBA OVERLAY_SCRIM    {0x02, 0x00, 0x10, 210};   // ~82% deep purple-black
}
