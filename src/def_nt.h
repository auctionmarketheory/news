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
#define URL_WEATHER "https://wttr.in/Ho+Chi+Minh?format=j1"
#define URL_GOLD    "https://query1.finance.yahoo.com/v8/finance/chart/GC=F?interval=1m&range=1d"
#define URL_NEWS    "http://feeds.bbci.co.uk/news/world/rss.xml"

// ─── Joystick Buttons (R36S layout) ──────────────────────
#define BTN_A  0   // Confirm / Refresh
#define BTN_B  1   // Back / Exit
#define BTN_X  2
#define BTN_Y  3

#define BUTTON_A(e) (e.type==SDL_JOYBUTTONDOWN && e.jbutton.button==BTN_A)
#define BUTTON_B(e) (e.type==SDL_JOYBUTTONDOWN && e.jbutton.button==BTN_B)

// ─── Colors (R,G,B,A) ─────────────────────────────────────
// Background
#define C_BG       {13,  13, 26, 255}    // #0d0d1a
#define C_PANEL    {20,  20, 45, 255}    // #14142d
#define C_BORDER   { 0,  60, 80, 255}    // #003c50

// Neon
#define C_CYAN     { 0, 255,255, 255}    // #00ffff
#define C_YELLOW   {255,224,102, 255}    // #ffe066
#define C_MAGENTA  {255,  0,255, 255}    // #ff00ff
#define C_GREEN    { 0, 255,128, 255}    // #00ff80
#define C_RED      {255, 50, 50, 255}    // #ff3232
#define C_WHITE    {224,224,255, 255}    // #e0e0ff
#define C_DIM      {100,100,130, 255}    // dimmed
#define C_TICKER_BG {10, 10, 35, 255}   // ticker background
