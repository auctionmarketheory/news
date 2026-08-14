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
#include <algorithm>

// ─── Global App State ─────────────────────────────────────
enum StateFlag { STATE_LOADING, STATE_DISPLAY, STATE_ERROR };
enum class FocusTarget { WEATHER, MARKET, TICKER };

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
    StateFlag   state;
    WeatherData weather;
    GoldData    gold;
    NewsItem    news[10];
    int         newsCount;
    Uint32      lastFetch;
    bool        quit;
    
    // UI State
    FocusTarget focus = FocusTarget::WEATHER;
    bool        modalOpen = false;
    float       modalAnimT = 0.0f;
} app;

inline SDL_Color toSDLColor(RGBA c) { return {c.r, c.g, c.b, c.a}; }

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

    void draw(SDL_Renderer* r, float x, float y, const std::string& txt, RGBA c) {
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

// ─── Data Fetching (Python via popen) ─────────────────────

void fetchWeather() {
    app.weather.valid = false;
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "python3 -c \"import urllib.request,json,sys;\n"
        "try:\n"
        " ctx=__import__('ssl')._create_unverified_context();"
        " req=urllib.request.Request('%s',headers={'User-Agent':'Mozilla/5.0'});"
        " r=urllib.request.urlopen(req,timeout=8,context=ctx);"
        " d=json.loads(r.read())['current'];"
        " wmap={0:'Clear',1:'Mostly Clear',2:'Partly Cloudy',3:'Overcast',45:'Fog',48:'Rime Fog',51:'Light Drizzle',53:'Drizzle',55:'Heavy Drizzle',61:'Light Rain',63:'Rain',65:'Heavy Rain',71:'Light Snow',73:'Snow',75:'Heavy Snow',95:'Thunderstorm'};"
        " wc=d.get('weather_code',0);"
        " vi=wmap.get(wc, 'Unknown');"
        " print(f'Hanoi|{int(d[\\\"temperature_2m\\\"])}|{int(d[\\\"relative_humidity_2m\\\"])}|{vi}|{int(d[\\\"wind_speed_10m\\\"])}');\n"
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
        " ctx=__import__('ssl')._create_unverified_context();"
        " req=urllib.request.Request('%s',headers={'User-Agent':'Mozilla/5.0'});"
        " r=urllib.request.urlopen(req,timeout=8,context=ctx).read().decode('utf-8');\n"
        " titles=re.findall(r'<title><\\!\\[CDATA\\[(.*?)\\]\\]></title>', r) or re.findall(r'<title>(.*?)</title>', r);\n"
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

// ─── Low-level draw helpers ─────────────────────────────────
inline void setColor(SDL_Renderer* r, RGBA c) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}

inline void fillRect(SDL_Renderer* r, SDL_Rect rect, RGBA c) {
    setColor(r, c);
    SDL_RenderFillRect(r, &rect);
}

inline void strokeRect(SDL_Renderer* r, SDL_Rect rect, RGBA c, int thickness = 1) {
    setColor(r, c);
    for (int i = 0; i < thickness; ++i) {
        SDL_Rect ring { rect.x - i, rect.y - i, rect.w + 2 * i, rect.h + 2 * i };
        SDL_RenderDrawRect(r, &ring);
    }
}

inline void drawGlow(SDL_Renderer* r, SDL_Rect base, RGBA color, int steps = 4) {
    for (int i = steps; i >= 1; --i) {
        RGBA layer = color;
        float falloff[4] = {1.0f, 0.45f, 0.25f, 0.10f};
        layer.a = static_cast<Uint8>(color.a * falloff[steps - i]);
        SDL_Rect ring { base.x - i, base.y - i, base.w + 2 * i, base.h + 2 * i };
        setColor(r, layer);
        SDL_RenderDrawRect(r, &ring);
    }
}

inline void drawCornerBracket(SDL_Renderer* r, int cx, int cy, int dx, int dy, RGBA color, int size = 6, int thickness = 2) {
    setColor(r, color);
    SDL_Rect horiz { cx, cy - (dy > 0 ? thickness - 1 : 0), size * dx > 0 ? size : -size, thickness };
    SDL_Rect vert  { cx - (dx > 0 ? thickness - 1 : 0), cy, thickness, size * dy > 0 ? size : -size };
    auto normalize = [](SDL_Rect rect) {
        if (rect.w < 0) { rect.x += rect.w; rect.w = -rect.w; }
        if (rect.h < 0) { rect.y += rect.h; rect.h = -rect.h; }
        return rect;
    };
    horiz = normalize(horiz);
    vert  = normalize(vert);
    SDL_RenderFillRect(r, &horiz);
    SDL_RenderFillRect(r, &vert);
}

inline void drawAllCornerBrackets(SDL_Renderer* r, SDL_Rect rect, RGBA color) {
    int off = 3;
    drawCornerBracket(r, rect.x - off, rect.y - off, 1, 1, color);
    drawCornerBracket(r, rect.x + rect.w + off, rect.y - off, -1, 1, color);
    drawCornerBracket(r, rect.x - off, rect.y + rect.h + off, 1, -1, color);
    drawCornerBracket(r, rect.x + rect.w + off, rect.y + rect.h + off, -1, -1, color);
}

// ─── Panel model ───────────────────────────────────────────
struct Panel {
    SDL_Rect bounds;
    std::string title;
    RGBA accent;
    bool focused = false;
    bool siblingDimmed = false;

    void draw(SDL_Renderer* r, CustomFont* titleFont) const {
        if (focused) drawGlow(r, bounds, accent);
        RGBA fill = focused ? Palette::PANEL_FILL_FOCUS : Palette::PANEL_FILL;
        fillRect(r, bounds, fill);

        RGBA borderColor = Palette::BORDER_DIM;
        int borderThickness = 1;
        if (focused) {
            borderColor = accent;
            borderThickness = 2;
        } else if (siblingDimmed) {
            borderColor.a = static_cast<Uint8>(Palette::BORDER_DIM.a * 0.6f);
        }
        strokeRect(r, bounds, borderColor, borderThickness);

        SDL_Rect divider { bounds.x, bounds.y + 24, bounds.w, 1 };
        fillRect(r, divider, Palette::BORDER_DIM);

        if (focused) drawAllCornerBrackets(r, bounds, accent);

        RGBA titleColor = focused ? RGBA{255,255,255,255} : siblingDimmed ? Palette::TEXT_SECONDARY : Palette::TEXT_PRIMARY;
        titleFont->draw(r, bounds.x + 12, bounds.y + 4, title, titleColor);
    }
};

// ─── Ticker Marquee ────────────────────────────────────────
class TickerMarquee {
public:
    SDL_Texture* texture = nullptr;
    int textureWidth = 0;
    int textureHeight = 0;
    float scrollX = 0.0f;

    void build(SDL_Renderer* renderer, CustomFont* font, const std::string& text, RGBA color) {
        if (texture) SDL_DestroyTexture(texture);
        textureWidth = font->getTextWidth(renderer, text);
        if (textureWidth == 0) return;
        textureHeight = 32; 

        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, textureWidth, textureHeight);
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        
        SDL_SetRenderTarget(renderer, texture);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);
        font->draw(renderer, 0, 4, text, color);
        SDL_SetRenderTarget(renderer, nullptr);
        
        scrollX = 0;
    }

    void update(float dtSeconds, float pixelsPerSecond) {
        scrollX += pixelsPerSecond * dtSeconds;
        if (textureWidth > 0 && scrollX >= textureWidth) {
            scrollX -= static_cast<float>(textureWidth);
        }
    }

    void draw(SDL_Renderer* r, SDL_Rect stripBounds) const {
        if (!texture) return;
        SDL_RenderSetClipRect(r, &stripBounds);
        int srcX = static_cast<int>(scrollX);
        SDL_Rect dst1 { stripBounds.x - srcX, stripBounds.y + (stripBounds.h - textureHeight) / 2, textureWidth, textureHeight };
        SDL_Rect dst2 { dst1.x + textureWidth, dst1.y, textureWidth, textureHeight };
        SDL_RenderCopy(r, texture, nullptr, &dst1);
        SDL_RenderCopy(r, texture, nullptr, &dst2);
        SDL_RenderSetClipRect(r, nullptr);
    }

    ~TickerMarquee() { if (texture) SDL_DestroyTexture(texture); }
};

TickerMarquee globalTicker;

// ─── Modal ───────────────────────────────────────────────
void drawModal(SDL_Renderer* r) {
    if (!app.modalOpen || app.modalAnimT <= 0.0f) return;

    SDL_Rect full { 0, 0, 640, 480 };
    RGBA scrim = Palette::OVERLAY_SCRIM;
    scrim.a = static_cast<Uint8>(scrim.a * app.modalAnimT);
    fillRect(r, full, scrim);

    SDL_Rect bounds {60, 60, 520, 360};
    float scale = 0.92f + 0.08f * app.modalAnimT;
    SDL_Rect scaled {
        static_cast<int>(bounds.x + bounds.w * (1 - scale) / 2),
        static_cast<int>(bounds.y + bounds.h * (1 - scale) / 2),
        static_cast<int>(bounds.w * scale),
        static_cast<int>(bounds.h * scale)
    };

    SDL_Rect shadow { scaled.x + 4, scaled.y + 4, scaled.w, scaled.h };
    fillRect(r, shadow, RGBA{0, 0, 0, static_cast<Uint8>(89 * app.modalAnimT)});

    RGBA accent = Palette::NEON_MAGENTA;
    drawGlow(r, scaled, accent);
    RGBA fill = Palette::PANEL_FILL_FOCUS;
    fill.a = static_cast<Uint8>(fill.a * app.modalAnimT);
    fillRect(r, scaled, fill);
    strokeRect(r, scaled, accent, 2);

    SDL_Rect divider { scaled.x, scaled.y + 34, scaled.w, 1 };
    fillRect(r, divider, Palette::BORDER_DIM);

    fontTitle->draw(r, scaled.x + 16, scaled.y + 8, "LATEST NEWS (BBC)", Palette::TEXT_PRIMARY);

    int yOffset = scaled.y + 50;
    for (int i = 0; i < app.newsCount; i++) {
        fontNormal->draw(r, scaled.x + 16, yOffset, std::string("> ") + app.news[i].headline, Palette::TEXT_PRIMARY);
        yOffset += 45;
        if (yOffset > scaled.y + scaled.h - 40) break;
    }
}

// ─── Rendering Assembly ──────────────────────────────────
void fetchAllData() {
    app.state = STATE_LOADING;
    fillRect(renderer, SDL_Rect{0, 0, SCREEN_WIDTH, SCREEN_HEIGHT}, Palette::BG_VOID);
    fontTitle->draw(renderer, SCREEN_WIDTH/2 - 80, SCREEN_HEIGHT/2, "Fetching Intel...", Palette::NEON_CYAN);
    SDL_RenderPresent(renderer);
    
    fetchWeather();
    fetchGold();
    fetchNews();
    
    std::string tickerText = "";
    if (app.newsCount > 0) {
        for (int i=0; i<app.newsCount; i++) {
            tickerText += "[ " + std::string(app.news[i].source) + " ] " + std::string(app.news[i].headline) + "   ///   ";
        }
    } else {
        tickerText = "[ NO SIGNAL DETECTED ]";
    }
    globalTicker.build(renderer, fontTicker, tickerText, Palette::TEXT_PRIMARY);
    
    app.lastFetch = SDL_GetTicks();
    app.state = STATE_DISPLAY;
}

void renderDisplay() {
    // 1. Grid Background
    fillRect(renderer, SDL_Rect{0, 0, 640, 480}, Palette::BG_VOID);
    setColor(renderer, Palette::BG_GRID);
    for (int x = 0; x < 640; x += 24) SDL_RenderDrawLine(renderer, x, 0, x, 480);
    for (int y = 0; y < 480; y += 24) SDL_RenderDrawLine(renderer, 0, y, 640, y);
    
    // 2. Header
    fillRect(renderer, SDL_Rect{0, 0, SCREEN_WIDTH, 28}, Palette::BORDER_HEADER);
    SDL_RenderDrawLine(renderer, 0, 28, SCREEN_WIDTH, 28);
    fontNormal->draw(renderer, 12, 4, "[SYS] DASHBOARD_V2", Palette::NEON_CYAN);
    
    time_t t = time(NULL); struct tm tm = *localtime(&t); char timeStr[64];
    sprintf(timeStr, "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
    fontNormal->draw(renderer, SCREEN_WIDTH - 90, 4, timeStr, Palette::TEXT_PRIMARY);
    
    // 3. Setup Panels
    Panel weatherP = { {12, 44, 300, 360}, "[ENVIR_MONITOR] WEATHER", Palette::NEON_CYAN, false, false };
    Panel marketP  = { {328, 44, 300, 360}, "[MKT_TRACKER] GC=F", Palette::NEON_AMBER, false, false };
    Panel tickerP  = { {12, 416, 616, 32}, "[STREAM] GLOBAL NEWS", Palette::NEON_MAGENTA, false, false };
    
    weatherP.focused = (app.focus == FocusTarget::WEATHER);
    weatherP.siblingDimmed = (app.focus == FocusTarget::MARKET);
    marketP.focused = (app.focus == FocusTarget::MARKET);
    marketP.siblingDimmed = (app.focus == FocusTarget::WEATHER);
    tickerP.focused = (app.focus == FocusTarget::TICKER);
    
    // 4. Draw Panels & Content
    weatherP.draw(renderer, fontTitle);
    if (app.weather.valid) {
        fontLarge->draw(renderer, weatherP.bounds.x + 16, weatherP.bounds.y + 40, std::to_string(app.weather.temp_c) + " C", Palette::TEXT_PRIMARY);
        fontTitle->draw(renderer, weatherP.bounds.x + 16, weatherP.bounds.y + 90, app.weather.condition, Palette::NEON_CYAN);
        fontNormal->draw(renderer, weatherP.bounds.x + 16, weatherP.bounds.y + 130, "HUMIDITY: " + std::to_string(app.weather.humidity) + "%", Palette::TEXT_SECONDARY);
        fontNormal->draw(renderer, weatherP.bounds.x + 16, weatherP.bounds.y + 160, "WIND: " + std::to_string(app.weather.wind_kmh) + " KM/H", Palette::TEXT_SECONDARY);
        fontTitle->draw(renderer, weatherP.bounds.x + 16, weatherP.bounds.y + 220, "LOC: " + std::string(app.weather.city), Palette::TEXT_PRIMARY);
    }
    
    marketP.draw(renderer, fontTitle);
    if (app.gold.valid) {
        char priceStr[64]; sprintf(priceStr, "$%.2f", app.gold.price);
        RGBA pColor = app.gold.change >= 0 ? Palette::NEON_GREEN : Palette::ALERT_RED;
        fontLarge->draw(renderer, marketP.bounds.x + 16, marketP.bounds.y + 40, priceStr, pColor);
        
        char changeStr[64]; sprintf(changeStr, "%s %+.2f (%+.2f%%)", app.gold.change >= 0 ? "+" : "", app.gold.change, app.gold.changePct);
        fontTitle->draw(renderer, marketP.bounds.x + 16, marketP.bounds.y + 90, changeStr, pColor);
        
        char hlStr[64]; sprintf(hlStr, "HIGH: %.2f  LOW: %.2f", app.gold.dayHigh, app.gold.dayLow);
        fontNormal->draw(renderer, marketP.bounds.x + 16, marketP.bounds.y + 140, hlStr, Palette::TEXT_SECONDARY);
        
        // Sparkline
        fillRect(renderer, SDL_Rect{marketP.bounds.x + 16, marketP.bounds.y + 200, 268, 2}, Palette::BORDER_DIM);
        if (app.gold.dayHigh > app.gold.dayLow) {
            double ratio = (app.gold.price - app.gold.dayLow) / (app.gold.dayHigh - app.gold.dayLow);
            int barW = (int)(268 * ratio);
            fillRect(renderer, SDL_Rect{marketP.bounds.x + 16, marketP.bounds.y + 199, barW, 4}, pColor);
        }
    }
    
    tickerP.draw(renderer, fontNormal); // Has title inside the render skeleton? Actually Ticker has no inner title in spec, just bounds. 
    // We'll draw bounds and then clip content
    globalTicker.draw(renderer, SDL_Rect{tickerP.bounds.x + 4, tickerP.bounds.y + 2, tickerP.bounds.w - 8, tickerP.bounds.h - 4});
    
    // Footer
    fillRect(renderer, SDL_Rect{0, 456, 640, 24}, Palette::BORDER_HEADER);
    SDL_RenderDrawLine(renderer, 0, 456, 640, 456);
    std::string prompt = (app.focus == FocusTarget::TICKER) ? "[A] READ ARTICLE  [B] EXIT" : "[A] REFRESH  [B] EXIT";
    fontNormal->draw(renderer, 12, 460, prompt, Palette::TEXT_SECONDARY);
    
    // Modal
    drawModal(renderer);
}

void handleDpad(SDL_Keycode key) {
    switch (app.focus) {
        case FocusTarget::WEATHER:
            if (key == SDLK_RIGHT) app.focus = FocusTarget::MARKET;
            else if (key == SDLK_DOWN) app.focus = FocusTarget::TICKER;
            break;
        case FocusTarget::MARKET:
            if (key == SDLK_LEFT) app.focus = FocusTarget::WEATHER;
            else if (key == SDLK_DOWN) app.focus = FocusTarget::TICKER;
            break;
        case FocusTarget::TICKER:
            if (key == SDLK_UP) app.focus = FocusTarget::WEATHER;
            else if (key == SDLK_a || key == SDLK_RETURN) app.modalOpen = true;
            break;
    }
    if (app.modalOpen && (key == SDLK_b || key == SDLK_ESCAPE)) {
        app.modalOpen = false;
        app.modalAnimT = 0.0f;
    }
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

    if (SDL_NumJoysticks() > 0) SDL_JoystickOpen(0);

    fetchAllData();

    SDL_Event e;
    Uint32 lastTime = SDL_GetTicks();
    
    while (!app.quit) {
        Uint32 frameStart = SDL_GetTicks();
        float dt = (frameStart - lastTime) / 1000.0f;
        lastTime = frameStart;

        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) app.quit = true;
            
            if (e.type == SDL_KEYDOWN) {
                if (!app.modalOpen && e.key.keysym.sym == SDLK_RETURN) fetchAllData();
                else handleDpad(e.key.keysym.sym);
            }
            if (e.type == SDL_JOYBUTTONDOWN) {
                if (e.jbutton.button == BTN_B) {
                    if (app.modalOpen) { app.modalOpen = false; app.modalAnimT = 0.0f; }
                    else app.quit = true;
                }
                if (e.jbutton.button == BTN_A) {
                    if (app.focus == FocusTarget::TICKER) app.modalOpen = true;
                    else fetchAllData();
                }
            }
            if (e.type == SDL_JOYHATMOTION) {
                if (e.jhat.value == SDL_HAT_LEFT) handleDpad(SDLK_LEFT);
                else if (e.jhat.value == SDL_HAT_RIGHT) handleDpad(SDLK_RIGHT);
                else if (e.jhat.value == SDL_HAT_UP) handleDpad(SDLK_UP);
                else if (e.jhat.value == SDL_HAT_DOWN) handleDpad(SDLK_DOWN);
            }
        }

        if (SDL_GetTicks() - app.lastFetch > REFRESH_MS && !app.modalOpen) {
            fetchAllData();
        }

        if (app.modalOpen) {
            app.modalAnimT = std::min(1.0f, app.modalAnimT + dt / 0.12f);
        } else {
            app.modalAnimT = std::max(0.0f, app.modalAnimT - dt / 0.12f);
        }

        // Ticker speed 60px/s when unfocused, 20px/s when focused
        globalTicker.update(dt, (app.focus == FocusTarget::TICKER && !app.modalOpen) ? 20.0f : 60.0f);

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
