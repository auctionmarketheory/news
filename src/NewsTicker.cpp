#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vector>
#include <string>
#include <sstream>
#include <time.h>
#include "def_nt.h"

// ─── Global App State ─────────────────────────────────────
enum AppState { STATE_LOADING, STATE_DISPLAY, STATE_ERROR };

struct WeatherData {
    char city[64];
    int  temp_c;
    int  humidity;
    char condition[64];
    int  wind_kmh;
    bool valid;
};

struct GoldData {
    double price;
    double change;
    double changePct;
    double dayHigh;
    double dayLow;
    bool   valid;
};

struct NewsItem {
    char headline[256];
    char source[32];
};

struct App {
    AppState    state;
    WeatherData weather;
    GoldData    gold;
    NewsItem    news[10];
    int         newsCount;
    int         tickerX;
    Uint32      lastFetch;
    bool        quit;
} app;

SDL_Window*   window   = NULL;
SDL_Renderer* renderer = NULL;
TTF_Font*     fontLarge = NULL;
TTF_Font*     fontTitle = NULL;
TTF_Font*     fontNormal= NULL;
TTF_Font*     fontSmall = NULL;
TTF_Font*     fontTicker= NULL;

// ─── Helpers ──────────────────────────────────────────────

void DrawText(TTF_Font* font, const char* text, int x, int y, SDL_Color color) {
    if (!text || strlen(text) == 0) return;
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) return;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture) {
        SDL_Rect dst = { x, y, surface->w, surface->h };
        SDL_RenderCopy(renderer, texture, NULL, &dst);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}

void DrawRect(int x, int y, int w, int h, SDL_Color c) {
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
    SDL_Rect r = {x, y, w, h};
    SDL_RenderFillRect(renderer, &r);
}

// ─── Data Fetching (Python via popen) ─────────────────────

void fetchWeather() {
    app.weather.valid = false;
    const char* py_script = 
        "import urllib.request,json,sys;"
        "try:\n"
        " r=urllib.request.urlopen('" URL_WEATHER "',timeout=8);"
        " d=json.loads(r.read());"
        " cc=d['current_condition'][0];"
        " print(f\"Ho Chi Minh|{cc['temp_C']}|{cc['humidity']}|{cc['lang_vi'][0]['value'] if 'lang_vi' in cc else cc['weatherDesc'][0]['value']}|{cc['windspeedKmph']}\");"
        "except Exception as e: print('ERR', e)";
        
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "python3 -c \"%s\"", py_script);
    FILE* fp = popen(cmd, "r");
    if (fp) {
        char buffer[256];
        if (fgets(buffer, sizeof(buffer), fp) != NULL) {
            if (strncmp(buffer, "ERR", 3) != 0) {
                char* p = strtok(buffer, "|");
                if (p) strncpy(app.weather.city, p, sizeof(app.weather.city)-1);
                p = strtok(NULL, "|"); if (p) app.weather.temp_c = atoi(p);
                p = strtok(NULL, "|"); if (p) app.weather.humidity = atoi(p);
                p = strtok(NULL, "|"); if (p) { strncpy(app.weather.condition, p, sizeof(app.weather.condition)-1); app.weather.condition[strcspn(app.weather.condition, "\n")] = 0; }
                p = strtok(NULL, "|"); if (p) app.weather.wind_kmh = atoi(p);
                app.weather.valid = true;
            }
        }
        pclose(fp);
    }
}

void fetchGold() {
    app.gold.valid = false;
    const char* py_script = 
        "import urllib.request,json,sys;"
        "try:\n"
        " ctx=__import__('ssl')._create_unverified_context();"
        " r=urllib.request.urlopen(urllib.request.Request('" URL_GOLD "',headers={'User-Agent':'Mozilla/5.0'}),timeout=8,context=ctx);"
        " d=json.loads(r.read())['chart']['result'][0]['meta'];"
        " p=d['regularMarketPrice'];"
        " pc=d.get('regularMarketChangePercent',0);"
        " ph=d.get('regularMarketDayHigh',0);"
        " pl=d.get('regularMarketDayLow',0);"
        " ch=p-d.get('chartPreviousClose',p);"
        " print(f'{p}|{ch}|{pc}|{ph}|{pl}');"
        "except Exception as e: print('ERR', e)";

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "python3 -c \"%s\"", py_script);
    FILE* fp = popen(cmd, "r");
    if (fp) {
        char buffer[256];
        if (fgets(buffer, sizeof(buffer), fp) != NULL) {
            if (strncmp(buffer, "ERR", 3) != 0) {
                char* p = strtok(buffer, "|"); if(p) app.gold.price = atof(p);
                p = strtok(NULL, "|"); if(p) app.gold.change = atof(p);
                p = strtok(NULL, "|"); if(p) app.gold.changePct = atof(p);
                p = strtok(NULL, "|"); if(p) app.gold.dayHigh = atof(p);
                p = strtok(NULL, "|"); if(p) app.gold.dayLow = atof(p);
                app.gold.valid = true;
            }
        }
        pclose(fp);
    }
}

void fetchNews() {
    app.newsCount = 0;
    const char* py_script = 
        "import urllib.request,re,sys;"
        "try:\n"
        " r=urllib.request.urlopen('" URL_NEWS "',timeout=8).read().decode('utf-8');"
        " titles=re.findall(r'<title><!\\[CDATA\\[(.*?)\\]\\]></title>', r) or re.findall(r'<title>(.*?)</title>', r);"
        " for t in titles[1:6]: print(t);" // Skip channel title
        "except Exception as e: print('ERR', e)";

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "python3 -c \"%s\"", py_script);
    FILE* fp = popen(cmd, "r");
    if (fp) {
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), fp) != NULL && app.newsCount < 5) {
            if (strncmp(buffer, "ERR", 3) == 0) break;
            buffer[strcspn(buffer, "\n")] = 0;
            strncpy(app.news[app.newsCount].headline, buffer, sizeof(app.news[0].headline)-1);
            strncpy(app.news[app.newsCount].source, "BBC", sizeof(app.news[0].source)-1);
            app.newsCount++;
        }
        pclose(fp);
    }
}

void fetchAllData() {
    app.state = STATE_LOADING;
    // Xóa bộ nhớ
    DrawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, C_BG);
    DrawText(fontTitle, "Đang tải dữ liệu...", SCREEN_WIDTH/2 - 80, SCREEN_HEIGHT/2, C_CYAN);
    SDL_RenderPresent(renderer);
    
    fetchWeather();
    fetchGold();
    fetchNews();
    
    app.tickerX = SCREEN_WIDTH;
    app.lastFetch = SDL_GetTicks();
    app.state = STATE_DISPLAY;
}

// ─── Rendering ────────────────────────────────────────────

void renderDisplay() {
    // Background
    DrawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, C_BG);
    
    // Header
    DrawRect(0, 0, SCREEN_WIDTH, HEADER_H, C_PANEL);
    DrawText(fontTitle, "◈ CYBERPUNK DASHBOARD", 10, 8, C_CYAN);
    
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    char timeStr[64];
    sprintf(timeStr, "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
    DrawText(fontTitle, timeStr, SCREEN_WIDTH - 100, 8, C_CYAN);

    // Weather Panel
    DrawRect(0, MAIN_TOP, WEATHER_W, MAIN_H, C_BG);
    DrawText(fontNormal, "⛅ THỜI TIẾT", 10, MAIN_TOP + 10, C_CYAN);
    if (app.weather.valid) {
        DrawText(fontTitle, app.weather.city, 10, MAIN_TOP + 40, C_WHITE);
        char tempStr[32]; sprintf(tempStr, "%d°C", app.weather.temp_c);
        DrawText(fontLarge, tempStr, 10, MAIN_TOP + 70, C_YELLOW);
        DrawText(fontNormal, app.weather.condition, 10, MAIN_TOP + 120, C_WHITE);
        char humStr[32]; sprintf(humStr, "Độ ẩm: %d%%", app.weather.humidity);
        DrawText(fontSmall, humStr, 10, MAIN_TOP + 150, C_DIM);
        char windStr[32]; sprintf(windStr, "Gió: %d km/h", app.weather.wind_kmh);
        DrawText(fontSmall, windStr, 10, MAIN_TOP + 170, C_DIM);
    } else {
        DrawText(fontNormal, "Lỗi tải thời tiết", 10, MAIN_TOP + 50, C_RED);
    }
    
    // Divider
    DrawRect(DIVIDER_X, MAIN_TOP, 2, MAIN_H, C_BORDER);

    // Gold Panel
    DrawRect(GOLD_X, MAIN_TOP, GOLD_W, MAIN_H, C_BG);
    DrawText(fontNormal, "💰 XAUUSD (GOLD)", GOLD_X + 10, MAIN_TOP + 10, C_YELLOW);
    if (app.gold.valid) {
        char priceStr[64]; sprintf(priceStr, "$ %.2f", app.gold.price);
        SDL_Color pColor = app.gold.change >= 0 ? SDL_Color C_GREEN : SDL_Color C_RED;
        DrawText(fontLarge, priceStr, GOLD_X + 10, MAIN_TOP + 60, pColor);
        
        char changeStr[64]; 
        sprintf(changeStr, "%s %+.2f (%+.2f%%)", app.gold.change >= 0 ? "▲" : "▼", app.gold.change, app.gold.changePct);
        DrawText(fontNormal, changeStr, GOLD_X + 10, MAIN_TOP + 110, pColor);
        
        char hlStr[64]; sprintf(hlStr, "H: %.2f   L: %.2f", app.gold.dayHigh, app.gold.dayLow);
        DrawText(fontSmall, hlStr, GOLD_X + 10, MAIN_TOP + 140, C_WHITE);
        
        // Simple sparkline bar
        DrawRect(GOLD_X + 10, MAIN_TOP + 170, GOLD_W - 20, 4, C_DIM);
        if (app.gold.dayHigh > app.gold.dayLow) {
            double ratio = (app.gold.price - app.gold.dayLow) / (app.gold.dayHigh - app.gold.dayLow);
            int barW = (int)((GOLD_W - 20) * ratio);
            DrawRect(GOLD_X + 10, MAIN_TOP + 170, barW, 4, pColor);
        }
    } else {
        DrawText(fontNormal, "Lỗi tải giá vàng", GOLD_X + 10, MAIN_TOP + 50, C_RED);
    }

    // Ticker
    DrawRect(0, TICKER_TOP, SCREEN_WIDTH, TICKER_H, C_TICKER_BG);
    if (app.newsCount > 0) {
        std::string tickerText = "";
        for (int i=0; i<app.newsCount; i++) {
            tickerText += "▶ " + std::string(app.news[i].source) + ": " + std::string(app.news[i].headline) + "   ◈   ";
        }
        DrawText(fontTicker, tickerText.c_str(), app.tickerX, TICKER_TOP + 12, C_WHITE);
        
        // Tạm tính độ rộng chuỗi (rất thô, có thể cải thiện nếu cần thiết)
        int w, h;
        TTF_SizeUTF8(fontTicker, tickerText.c_str(), &w, &h);
        app.tickerX -= NEWS_SCROLL_SPEED;
        if (app.tickerX < -w) {
            app.tickerX = SCREEN_WIDTH;
        }
    } else {
        DrawText(fontTicker, "Không có tin tức...", 10, TICKER_TOP + 12, C_DIM);
    }
    
    // Footer
    DrawRect(0, FOOTER_TOP, SCREEN_WIDTH, FOOTER_H, C_PANEL);
    DrawText(fontSmall, "[A] Làm mới    [B] Thoát", 10, FOOTER_TOP + 15, C_WHITE);
}

// ─── Main ─────────────────────────────────────────────────

int main(int argc, char* args[]) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) return 1;
    if (TTF_Init() == -1) return 1;

    Uint32 flags = 0;
#if FULLSCREEN
    flags |= SDL_WINDOW_FULLSCREEN;
#endif

    window = SDL_CreateWindow(APP_NAME, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, flags);
    if (!window) return 1;

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) return 1;

    fontLarge  = TTF_OpenFont(FONT_NAME, FONT_LARGE);
    fontTitle  = TTF_OpenFont(FONT_NAME, FONT_TITLE);
    fontNormal = TTF_OpenFont(FONT_NAME, FONT_NORMAL);
    fontSmall  = TTF_OpenFont(FONT_NAME, FONT_SMALL);
    fontTicker = TTF_OpenFont(FONT_NAME, FONT_TICKER);

    if (!fontLarge || !fontTitle || !fontNormal || !fontSmall || !fontTicker) {
        printf("Error loading font: %s\n", TTF_GetError());
        return 1;
    }

    if (SDL_NumJoysticks() > 0) {
        SDL_JoystickOpen(0);
    }

    fetchAllData();

    SDL_Event e;
    while (!app.quit) {
        Uint32 frameStart = SDL_GetTicks();

        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) app.quit = true;
            if (BUTTON_B(e) || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)) {
                app.quit = true;
            }
            if (BUTTON_A(e) || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_RETURN)) {
                fetchAllData();
            }
        }

        if (SDL_GetTicks() - app.lastFetch > REFRESH_MS) {
            fetchAllData();
        }

        renderDisplay();
        SDL_RenderPresent(renderer);

        Uint32 frameTime = SDL_GetTicks() - frameStart;
        if (frameTime < MS_PER_FRAME) {
            SDL_Delay(MS_PER_FRAME - frameTime);
        }
    }

    TTF_CloseFont(fontLarge);
    TTF_CloseFont(fontTitle);
    TTF_CloseFont(fontNormal);
    TTF_CloseFont(fontSmall);
    TTF_CloseFont(fontTicker);
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
