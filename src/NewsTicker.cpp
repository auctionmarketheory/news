#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vector>
#include <string>
#include <sstream>
#include <time.h>
#include <fstream>
#include "def_nt.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include <unordered_map>

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

class CustomFont {
public:
    stbtt_fontinfo info;
    std::vector<unsigned char> ttf_buffer;
    float scale;
    int ascent, descent, lineGap;
    float sz = 20.0f;
    
    struct Glyph {
        SDL_Texture* tex;
        int w, h, xoff, yoff, advance;
    };
    std::unordered_map<int, Glyph> cache;

    bool load(SDL_Renderer* r, const std::string& path, float s) {
        sz = s;
        std::ifstream f(path, std::ios::binary | std::ios::ate);
        if (!f.is_open()) return false;
        auto len = f.tellg(); f.seekg(0);
        ttf_buffer.resize(len);
        f.read((char*)ttf_buffer.data(), len);
        
        if (!stbtt_InitFont(&info, ttf_buffer.data(), 0)) return false;
        
        scale = stbtt_ScaleForPixelHeight(&info, s);
        stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
        return true;
    }

    Glyph getGlyph(SDL_Renderer* r, int cp) {
        if (cache.count(cp)) return cache[cp];
        Glyph g = {nullptr, 0, 0, 0, 0, 0};
        int adv, lsb;
        stbtt_GetCodepointHMetrics(&info, cp, &adv, &lsb);
        g.advance = adv * scale;
        
        int x0,y0,x1,y1;
        stbtt_GetCodepointBitmapBox(&info, cp, scale, scale, &x0, &y0, &x1, &y1);
        g.w = x1 - x0;
        g.h = y1 - y0;
        g.xoff = x0;
        g.yoff = y0;
        
        if (g.w > 0 && g.h > 0) {
            unsigned char* bitmap = stbtt_GetCodepointBitmap(&info, scale, scale, cp, &g.w, &g.h, &g.xoff, &g.yoff);
            std::vector<unsigned char> rgba(g.w * g.h * 4, 255);
            for (int i=0; i<g.w*g.h; i++) rgba[i*4+3] = bitmap[i];
            stbtt_FreeBitmap(bitmap, nullptr);
            SDL_Surface* sf = SDL_CreateRGBSurfaceFrom(rgba.data(), g.w, g.h, 32, g.w*4, 0xFF, 0xFF00, 0xFF0000, 0xFF000000);
            g.tex = SDL_CreateTextureFromSurface(r, sf);
            SDL_SetTextureBlendMode(g.tex, SDL_BLENDMODE_BLEND);
            SDL_FreeSurface(sf);
        }
        cache[cp] = g;
        return g;
    }

    uint32_t decodeUTF8(const std::string& str, size_t& i) {
        if (i >= str.length()) return 0;
        unsigned char c0 = str[i];
        if ((c0 & 0x80) == 0) { i += 1; return c0; }
        if ((c0 & 0xE0) == 0xC0) {
            if (i+1 >= str.length()) { i+=1; return 0; }
            unsigned char c1 = str[i+1];
            i += 2; return ((c0 & 0x1F) << 6) | (c1 & 0x3F);
        }
        if ((c0 & 0xF0) == 0xE0) {
            if (i+2 >= str.length()) { i+=1; return 0; }
            unsigned char c1 = str[i+1], c2 = str[i+2];
            i += 3; return ((c0 & 0x0F) << 12) | ((c1 & 0x3F) << 6) | (c2 & 0x3F);
        }
        if ((c0 & 0xF8) == 0xF0) {
            if (i+3 >= str.length()) { i+=1; return 0; }
            unsigned char c1 = str[i+1], c2 = str[i+2], c3 = str[i+3];
            i += 4; return ((c0 & 0x07) << 18) | ((c1 & 0x3F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
        }
        i += 1; return 0;
    }

    void draw(SDL_Renderer* r, float x, float y, const std::string& txt, SDL_Color c) {
        float cx = x;
        size_t i = 0;
        int base_y = y + ascent * scale;
        while(i < txt.length()) {
            uint32_t cp = decodeUTF8(txt, i);
            if (!cp) continue;
            Glyph g = getGlyph(r, cp);
            if (g.tex) {
                SDL_SetTextureColorMod(g.tex, c.r, c.g, c.b);
                SDL_SetTextureAlphaMod(g.tex, c.a);
                SDL_Rect d = { (int)(cx + g.xoff), (int)(base_y + g.yoff), g.w, g.h };
                SDL_RenderCopy(r, g.tex, nullptr, &d);
            }
            cx += g.advance;
        }
    }

    int getTextWidth(SDL_Renderer* r, const std::string& txt) {
        float cx = 0;
        size_t i = 0;
        while(i < txt.length()) {
            uint32_t cp = decodeUTF8(txt, i);
            if (!cp) continue;
            cx += getGlyph(r, cp).advance;
        }
        return (int)cx;
    }

    void freeCache() {
        for (auto& pair : cache) {
            if (pair.second.tex) SDL_DestroyTexture(pair.second.tex);
        }
        cache.clear();
    }
};

SDL_Window*   window   = NULL;
SDL_Renderer* renderer = NULL;
CustomFont*   fontLarge = NULL;
CustomFont*   fontTitle = NULL;
CustomFont*   fontNormal = NULL;
CustomFont*   fontSmall = NULL;
CustomFont*   fontTicker = NULL;

void freeFonts() {
    if (fontLarge) { fontLarge->freeCache(); delete fontLarge; }
    if (fontTitle) { fontTitle->freeCache(); delete fontTitle; }
    if (fontNormal) { fontNormal->freeCache(); delete fontNormal; }
    if (fontSmall) { fontSmall->freeCache(); delete fontSmall; }
    if (fontTicker) { fontTicker->freeCache(); delete fontTicker; }
}

// ─── Helpers ──────────────────────────────────────────────

void DrawText(CustomFont* font, const char* text, int x, int y, SDL_Color color) {
    if (!text || strlen(text) == 0 || !font) return;
    font->draw(renderer, x, y, text, color);
}

void DrawRect(int x, int y, int w, int h, SDL_Color c) {
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
    SDL_Rect r = {x, y, w, h};
    SDL_RenderFillRect(renderer, &r);
}

// ─── Data Fetching (Python via popen) ─────────────────────

void fetchWeather() {
    app.weather.valid = false;
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "python3 -c \"import urllib.request,json,sys;\n"
        "try:\n"
        " r=urllib.request.urlopen('%s',timeout=8);"
        " d=json.loads(r.read());"
        " cc=d['current_condition'][0];"
        " vi = cc['lang_vi'][0]['value'] if 'lang_vi' in cc else cc['weatherDesc'][0]['value'];"
        " print(f'Ho Chi Minh|{cc[\\\"temp_C\\\"]}|{cc[\\\"humidity\\\"]}|{vi}|{cc[\\\"windspeedKmph\\\"]}');\n"
        "except Exception as e: print('ERR', e)\"", URL_WEATHER);
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
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "python3 -c \"import urllib.request,json,sys;\n"
        "try:\n"
        " ctx=__import__('ssl')._create_unverified_context();"
        " r=urllib.request.urlopen(urllib.request.Request('%s',headers={'User-Agent':'Mozilla/5.0'}),timeout=8,context=ctx);"
        " d=json.loads(r.read())['chart']['result'][0]['meta'];"
        " p=d['regularMarketPrice'];"
        " pc=d.get('regularMarketChangePercent',0);"
        " ph=d.get('regularMarketDayHigh',0);"
        " pl=d.get('regularMarketDayLow',0);"
        " ch=p-d.get('chartPreviousClose',p);"
        " print(f'{p}|{ch}|{pc}|{ph}|{pl}');\n"
        "except Exception as e: print('ERR', e)\"", URL_GOLD);
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
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "python3 -c \"import urllib.request,re,sys;\n"
        "try:\n"
        " r=urllib.request.urlopen('%s',timeout=8).read().decode('utf-8');"
        " titles=re.findall(r'<title><!\\\\[CDATA\\\\[(.*?)\\\\]\\\\]></title>', r) or re.findall(r'<title>(.*?)</title>', r);"
        " for t in titles[1:6]: print(t);\n"
        "except Exception as e: print('ERR', e)\"", URL_NEWS);
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
    DrawText(fontTitle, "> CYBERPUNK DASHBOARD", 10, 8, C_CYAN);
    
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    char timeStr[64];
    sprintf(timeStr, "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
    DrawText(fontTitle, timeStr, SCREEN_WIDTH - 100, 8, C_CYAN);

    // Weather Panel
    DrawRect(0, MAIN_TOP, WEATHER_W, MAIN_H, C_BG);
    DrawText(fontNormal, "[ THỜI TIẾT ]", 10, MAIN_TOP + 10, C_CYAN);
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
    DrawText(fontNormal, "$ XAUUSD (GOLD)", GOLD_X + 10, MAIN_TOP + 10, C_YELLOW);
    if (app.gold.valid) {
        char priceStr[64]; sprintf(priceStr, "$ %.2f", app.gold.price);
        SDL_Color pColor = app.gold.change >= 0 ? SDL_Color C_GREEN : SDL_Color C_RED;
        DrawText(fontLarge, priceStr, GOLD_X + 10, MAIN_TOP + 60, pColor);
        
        char changeStr[64]; 
        sprintf(changeStr, "%s %+.2f (%+.2f%%)", app.gold.change >= 0 ? "+" : "-", app.gold.change, app.gold.changePct);
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
        int w = fontTicker->getTextWidth(renderer, tickerText);
        app.tickerX -= NEWS_SCROLL_PX;
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

    Uint32 flags = 0;
#if FULLSCREEN
    flags |= SDL_WINDOW_FULLSCREEN;
#endif

    window = SDL_CreateWindow(APP_NAME, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, flags);
    if (!window) return 1;

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) return 1;

    fontLarge  = new CustomFont(); fontLarge->load(renderer, FONT_NAME, FONT_LARGE);
    fontTitle  = new CustomFont(); fontTitle->load(renderer, FONT_NAME, FONT_TITLE);
    fontNormal = new CustomFont(); fontNormal->load(renderer, FONT_NAME, FONT_NORMAL);
    fontSmall  = new CustomFont(); fontSmall->load(renderer, FONT_NAME, FONT_SMALL);
    fontTicker = new CustomFont(); fontTicker->load(renderer, FONT_NAME, FONT_TICKER);

    if (!fontLarge->info.data || !fontTitle->info.data || !fontNormal->info.data || !fontSmall->info.data || !fontTicker->info.data) {
        printf("Error loading font\n");
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

    if (fontLarge) { fontLarge->freeCache(); delete fontLarge; }
    if (fontTitle) { fontTitle->freeCache(); delete fontTitle; }
    if (fontNormal) { fontNormal->freeCache(); delete fontNormal; }
    if (fontSmall) { fontSmall->freeCache(); delete fontSmall; }
    if (fontTicker) { fontTicker->freeCache(); delete fontTicker; }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
