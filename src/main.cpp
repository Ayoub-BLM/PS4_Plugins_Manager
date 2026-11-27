// GoldHEN Plugin Manager - PS4 (OpenOrbis)
// - الفحص يتم في خيط خلفي مع نتائج تدريجية، ومهلة قصوى، وإمكانية الخروج في أي وقت.
// - الأيقونات والنصوص تُحمَّل عند الحاجة فقط (للصفوف الظاهرة).
// - الواجهة مرتّبة بمناطق ثابتة لمنع تداخل الكلمات.
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <orbis/Sysmodule.h>
#include <orbis/Net.h>
#include <orbis/Pad.h>
#include <orbis/UserService.h>
#include <orbis/SysUtil.h>      // يحتاج -lSceSysUtil في LIBS
#include "PluginStore.h"
#include "PluginIniRepair.h"   // إصلاح plugins.ini (منطق نقي، مختبر على الحاسوب)
#ifndef ORBIS_PAD_BUTTON_L2
#define ORBIS_PAD_BUTTON_L2 0x00000100   // قيمة L2 في قناع أزرار يد PS4 (تُستعمل إن لم يعرّفها الرأس)
#endif
#include <arpa/inet.h>
#include "ArabicText.h"
#include "QrCodeGen.h"
#include <atomic>
#include <unordered_map>
#include "RootAccess.h"   // local QR generator (single header, include from this file only)

// Hides the system launch splash (sce_sys/pic1.png). Needs -lSceSystemService in LIBS (a missing lib is now a link error on purpose).
extern "C" int sceSystemServiceHideSplashScreen(void);
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_SIMD
#include "stb_image.h"
#include <algorithm>
#include <deque>
#include <map>
#include <set>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <iterator>
#include <map>
#include <netinet/in.h>
#include <set>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <tuple>
#include <unistd.h>
#include <vector>

static const int SW = 1920, SH = 1080;
// Troubleshooting switches (1 = on, 0 = off):
// STORE_CHECK_ON_OPEN : check the internet connection when the Store is first opened (offline screen). Later opens reuse the loaded catalog (no new check) unless the last check failed.
// STORE_CONFIRM_REPLACE: ask before replacing a plugin that is already installed (0 = replace silently).
// SHOW_FPS            : show a frames-per-second counter in the top-right corner (for performance testing).
// USE_SHAPE_CACHE     : draw rounded corners as cached images (1) or directly every frame (0). Try both and compare the FPS.
#define STORE_CHECK_ON_OPEN 0   // 0 = الفحص بالخلفية (المتجر يفتح فوراً بدون تجميد الشاشة)
#define STORE_CONFIRM_REPLACE 1
#define USE_SHAPE_CACHE 0
static const char* PLUGIN_DIR = "/data/GoldHEN/plugins";
static const char* CONFIG_PATH = "/data/GoldHEN/plugins.ini";
static const char* CONFIG_BAK_PATH = "/data/GoldHEN/plugins.ini.bak";
static const char* APP_DATA_DIR = "/data/PluginManager";

#include "Language.h"


// ---------- ضبط التحكم (عدّلها بحرية) ----------
static const int BTN_UP = 13, BTN_DOWN = 14;               // أسهم أعلى/أسفل (كما كانت)
static const int BTN_PAGE_PREV = 4, BTN_PAGE_NEXT = 5;     // L1 / R1: صفحة كاملة (تحقق من الرقمين على جهازك)
static const int BTN_LEFT = 15, BTN_RIGHT = 16;
static const int BTN_TOUCHPAD = -1;   // زر لوحة اللمس: -1 = غير مفعّل حتى نعرف رقمه (يظهر عند الضغط داخل المتجر)            // يسار / يمين: صفحة كاملة (تحقق من الرقمين على جهازك)
static const Uint32 NAV_FIRST_DELAY_MS = 360;              // الانتظار قبل بدء التكرار عند الضغط المستمر
static const int STICK_DEADZONE = 11000;                   // المنطقة الميتة للعصا (من 32767)
static const int RAW_STICK_DEADZONE = 40;                  // المنطقة الميتة للعصا عند القراءة المباشرة (من 127)
static const int STICK_STALE_MS = 160;                     // قيمة العصا التي لا تصلها أحداث جديدة أطول من هذا تُعتبر قديمة
static const int STICK_HOLD_STRONG = 24000;                // الميل القوي يُعتبر ضغطاً متعمداً حتى لو لم تصل أحداث جديدة

// ---------- أيقونات أزرار اليد: ترسم برمجياً مرة واحدة بحواف ناعمة (لا تحتاج ملفات صور) ----------
enum { BI_CROSS = 0, BI_CIRCLE, BI_SQUARE, BI_TRIANGLE, BI_DPAD, BI_L2, BI_TOUCH, BI_COUNT };
struct HintItem { int icon; const char* label; };
static float segDist(float px, float py, float ax, float ay, float bx, float by) {
    float vx = bx - ax, vy = by - ay, wx = px - ax, wy = py - ay;
    float l2 = vx * vx + vy * vy, t = l2 > 0 ? (wx * vx + wy * vy) / l2 : 0;
    t = t < 0 ? 0 : (t > 1 ? 1 : t);
    float dx = px - (ax + t * vx), dy = py - (ay + t * vy);
    return std::sqrt(dx * dx + dy * dy);
}
// شكل لوحة اللمس الحقيقي: مستطيل عريض بزوايا دائرية، سطح منقّط داكن، حافة فاتحة، وخط أزرق لامع في الأعلى
static SDL_Texture* makeTouchpadIcon(SDL_Renderer* rd, int D) {
    SDL_Surface* sf = SDL_CreateRGBSurfaceWithFormat(0, D, D, 32, SDL_PIXELFORMAT_RGBA32);
    if (!sf) return nullptr;
    SDL_LockSurface(sf);
    Uint8* base = (Uint8*)sf->pixels;
    const int pitch = sf->pitch;
    const float cx = D / 2.0f, cy = D / 2.0f, hw = D * 0.49f, hh = D * 0.29f, rr = D * 0.12f;
    for (int y = 0; y < D; y++) for (int x = 0; x < D; x++) {
        const float px = x + 0.5f, py = y + 0.5f;
        const float qx = std::fabs(px - cx) - (hw - rr), qy = std::fabs(py - cy) - (hh - rr);
        const float ox = std::max(qx, 0.0f), oy = std::max(qy, 0.0f);
        const float sd = std::sqrt(ox * ox + oy * oy) + std::min(std::max(qx, qy), 0.0f) - rr;
        const float cover = std::min(1.0f, std::max(0.0f, 0.5f - sd));
        Uint8* o = base + y * pitch + x * 4;
        if (cover <= 0.0f) { o[0] = o[1] = o[2] = o[3] = 0; continue; }
        int R = 46, G = 52, B = 70;                                   // السطح الداكن
        if ((x % 4) == 2 && (y % 4) == 2) { R = 98; G = 106; B = 128; }   // النقاط
        if (std::fabs(py - (cy - hh * 0.62f)) < 1.1f && std::fabs(px - cx) < hw * 0.62f) { R = 70; G = 150; B = 255; }   // الخط الأزرق اللامع
        if (sd > -1.5f) { R = 150; G = 162; B = 190; }                // الحافة الفاتحة
        o[0] = (Uint8)R; o[1] = (Uint8)G; o[2] = (Uint8)B; o[3] = (Uint8)(cover * 255.0f);
    }
    SDL_UnlockSurface(sf);
    SDL_Texture* t = SDL_CreateTextureFromSurface(rd, sf);
    SDL_FreeSurface(sf);
    return t;
}
static SDL_Texture* makeButtonIcon(SDL_Renderer* rd, int kind, int D) {
    SDL_Surface* sf = SDL_CreateRGBSurfaceWithFormat(0, D, D, 32, SDL_PIXELFORMAT_RGBA32);
    if (!sf) return nullptr;
    static const float SYM[BI_COUNT][3] = {
        {110, 168, 255},   // ✕ أزرق
        {255, 107, 107},   // ○ أحمر
        {255, 143, 208},   // □ وردي
        { 92, 214, 160},   // △ أخضر
        {232, 238, 250},   // الأسهم: أبيض مائل للأزرق
        {255, 190, 108},   // L2: لون التحذير
        {232, 238, 250}    // لوحة اللمس
    };
    const float BG[3] = {36, 48, 74}, EDGE[3] = {88, 106, 146};
    const float c = D / 2.0f, R = D / 2.0f - 0.5f, half = D * (kind == BI_DPAD ? 0.075f : 0.062f), k = D * 0.20f;
    for (int y = 0; y < D; y++) for (int x = 0; x < D; x++) {
        float px = x + 0.5f, py = y + 0.5f, dd = std::sqrt((px - c) * (px - c) + (py - c) * (py - c));
        float disk = std::min(1.0f, std::max(0.0f, R - dd + 0.5f)), d = 1e9f;
        switch (kind) {
            case BI_CROSS: d = std::min(segDist(px, py, c - k, c - k, c + k, c + k), segDist(px, py, c + k, c - k, c - k, c + k)); break;
            case BI_CIRCLE: d = std::fabs(dd - D * 0.215f); break;
            case BI_SQUARE: { float s = D * 0.2f;
                d = std::min(std::min(segDist(px, py, c - s, c - s, c + s, c - s), segDist(px, py, c + s, c - s, c + s, c + s)),
                             std::min(segDist(px, py, c + s, c + s, c - s, c + s), segDist(px, py, c - s, c + s, c - s, c - s))); break; }
            case BI_TRIANGLE: { float tx = c, ty = c - D * 0.25f, rx = c + D * 0.235f, ry = c + D * 0.17f, lx = c - D * 0.235f;
                d = std::min(std::min(segDist(px, py, tx, ty, rx, ry), segDist(px, py, rx, ry, lx, ry)), segDist(px, py, lx, ry, tx, ty)); break; }
            case BI_TOUCH: { float hw = D * 0.31f, hh = D * 0.19f;   // مستطيل لوحة اللمس
                d = std::min(std::min(segDist(px, py, c - hw, c - hh, c + hw, c - hh), segDist(px, py, c + hw, c - hh, c + hw, c + hh)),
                             std::min(segDist(px, py, c + hw, c + hh, c - hw, c + hh), segDist(px, py, c - hw, c + hh, c - hw, c - hh))); break; }
            case BI_DPAD: { float a = D * 0.25f;
                d = std::min(segDist(px, py, c - a, c, c + a, c), segDist(px, py, c, c - a, c, c + a)); break; }
            case BI_L2: { float x0 = c - 0.18f * D, x1 = c + 0.04f * D, w = 0.14f * D;
                float y0 = c - 0.20f * D, y1 = c + 0.20f * D, ym = c;
                d = std::min(std::min(std::min(segDist(px, py, x0, y0, x0, y1), segDist(px, py, x0, y1, x0 + w, y1)),
                                      std::min(segDist(px, py, x1, y0, x1 + w, y0), segDist(px, py, x1 + w, y0, x1 + w, ym))),
                             std::min(segDist(px, py, x1 + w, ym, x1, y1), segDist(px, py, x1, y1, x1 + w, y1))); break; }
        }
        float sym = std::min(1.0f, std::max(0.0f, half - d + 0.5f));
        float edge = std::min(1.0f, std::max(0.0f, dd - (R - 2.0f))) * 0.65f;
        Uint8* p = (Uint8*)sf->pixels + (size_t)y * sf->pitch + (size_t)x * 4;
        for (int ch = 0; ch < 3; ch++) {
            float col = BG[ch] + (EDGE[ch] - BG[ch]) * edge;
            col = col + (SYM[kind][ch] - col) * sym;
            p[ch] = (Uint8)(col + 0.5f);
        }
        p[3] = (Uint8)(disk * 255.0f + 0.5f);
    }
    SDL_Texture* t = SDL_CreateTextureFromSurface(rd, sf);
    SDL_FreeSurface(sf);
    if (t) SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    return t;
}

// إشعار نظام PS4 (يظهر أعلى الشاشة). الترويسة تعلن الدالة بلا معاملات فنحوّلها إلى (int, const char*)
// ---- إشعار بأيقونة التطبيق نفسه (بنية الطلب كما في مشروع orbisRPC) ----
#pragma pack(push, 1)
struct NotifyReq {
    int      Type;             // 0 = رسالة
    uint32_t ReqId;
    uint32_t Priority;
    uint32_t MsgId;
    int      TargetId;         // -1 = للجميع
    uint32_t UserId, DeviceId, AddressingUserId, AppId, ErrorNumber, Attribute;
    uint8_t  HasIcon;
    union {
        struct { char Message[0x400]; char IconImageUri[0x800]; } t;
        char buffer[0xC03];     // يثبت الحجم
    };
};
#pragma pack(pop)
static_assert(sizeof(NotifyReq) == 0xC30, "NotifyReq must match the console layout");
extern "C" int sceKernelSendNotificationRequest(int api, char* buffer, size_t size, int blocking);
static const char* NOTIFY_ICON_DST = "/data/PluginManager/notify_icon_v25.png";   // اسم جديد لكل إصدار: النظام يخزّن الأيقونة مؤقتاً حسب المسار
static const char* NOTIFY_ICON_URI = "file://user/data/PluginManager/notify_icon_v25.png";
// ينسخ أيقونة التطبيق مرة واحدة إلى مجلد البيانات (الإشعار يقرأها بالمسار)، ويجرب أكثر من مصدر
static bool ensureNotifyIcon() {   // تُنسخ في كل تشغيل (الملف صغير) حتى لا تبقى نسخة قديمة
    // أولاً ملف الإشعار من مجلد assets (يُقرأ فعلاً من التطبيق)، ثم أيقونة النظام، ثم الديفولت
    static const char* sources[] = {"/app0/assets/images/notify.png", "/app0/sce_sys/icon0.png", "/app0/assets/images/default.png"};
    for (const char* src : sources) {
        FILE* in = fopen(src, "rb");
        if (!in) continue;
        FILE* out = fopen(NOTIFY_ICON_DST, "wb");
        if (!out) { fclose(in); return false; }
        char b[4096]; size_t n = 0, total = 0;
        while ((n = fread(b, 1, sizeof b, in)) > 0) { fwrite(b, 1, n, out); total += n; }
        fclose(in); fclose(out);
        if (total > 0) return true;
    }
    return false;
}
static bool gNotifyIconDrop = false;     // تُحذف نسخة الأيقونة بعد أن يعرض النظام الإشعار
static Uint32 gNotifyIconDropAt = 0;
static void sysNotify(const char* msg) {
    static const bool iconReady = ensureNotifyIcon();
    if (iconReady) {
        static NotifyReq req;   // static: الحجم كبير على المكدس
        std::memset(&req, 0, sizeof req);
        req.Type = 0; req.TargetId = -1; req.HasIcon = 1;
        std::snprintf(req.t.Message, sizeof req.t.Message, "%s", msg);
        std::snprintf(req.t.IconImageUri, sizeof req.t.IconImageUri, "%s", NOTIFY_ICON_URI);
        if (sceKernelSendNotificationRequest(0, reinterpret_cast<char*>(&req), sizeof req, 0) >= 0) {
            gNotifyIconDrop = true; gNotifyIconDropAt = SDL_GetTicks() + 10000;
            return;
        }
    }
    // احتياط: إشعار النظام العادي إن تعذّر الإشعار المخصص
    typedef int (*NotifyFn)(int, const char*);
    NotifyFn fn = reinterpret_cast<NotifyFn>(&sceSysUtilSendSystemNotificationWithText);
    fn(222, msg);
}

// ---------- مهل الجلب (بالميلي ثانية) — زِدها إذا كان عندك ألعاب كثيرة ----------
static const Uint32 LIST_TIMEOUT_MS = 20000;     // قراءة قائمة الألعاب عبر FTP
static const Uint32 FETCH_TIMEOUT_MS = 180000;   // جلب الأيقونات والأسماء (3 دقائق)
static const Uint32 SCAN_WATCHDOG_MS = 240000;   // الحد الأقصى لكامل الفحص (4 دقائق)

// ---------- Layout ----------
static const int ROWS_G = 8;      // عدد صفوف الألعاب الظاهرة
static const int STRIDE_G = 86;   // المسافة بين صفوف الألعاب
static const int ROWS_P = 9;      // عدد صفوف الإضافات الظاهرة
static const int STRIDE_P = 72;
static const int LIST_Y = 214;
static const int LX = 1090, LW = 740;   // قائمة الألعاب (يمين)
static const int DX = 90, DW = 970;     // لوحة التفاصيل (يسار)

// ---------- Colors ----------
static const SDL_Color C_BG = {6, 9, 18, 255}, C_BAR = {17, 23, 39, 255}, C_LINE = {45, 57, 81, 255};
// مفاتيح حركات بطاقة المتجر: اطفِ أيٍّ منها لو سبب مشكلة
static const bool kAnimBob = false;       // المثلث يطفو لأعلى ولأسفل
static const bool kAnimRipple = false;    // موجة تتوسع حول الدائرة
static const bool kAnimSweep = false;    // لمعة تمر فوق زر "افتح المتجر"
// خلفية متدرجة + توهج وردي: مطفأة (الخلفية سوداء عادية)
static const bool kBgGlow = true;   // خلفية متدرجة مع توهج وردي (تُرسم مرة واحدة وتُنسخ كل إطار)
static const SDL_Color C_PANEL = {17, 24, 41, 255}, C_CARD = {21, 29, 47, 255};
static const SDL_Color C_ROW = {28, 38, 59, 255}, C_ROWSEL = {52, 67, 104, 255}, C_ACCENT = {240, 68, 122, 255};
static const SDL_Color C_TEXT = {235, 240, 250, 255}, C_DIM = {138, 155, 186, 255}, C_WHITE = {255, 255, 255, 255};
static const SDL_Color C_ON = {117, 235, 183, 255}, C_ONBG = {24, 80, 65, 255}, C_OFF = {164, 175, 196, 255}, C_OFFBG = {47, 54, 72, 255};
static const SDL_Color C_WARN = {255, 190, 108, 255}, C_PH = {44, 58, 86, 255};

struct Plugin { std::string name, path; };
// أيقونة الكرة الأرضية بجانب "اللغة" في قائمة الإعدادات
static SDL_Texture* makeGlobeIcon(SDL_Renderer* rd, int D) {
    SDL_Surface* sf = SDL_CreateRGBSurfaceWithFormat(0, D, D, 32, SDL_PIXELFORMAT_RGBA32);
    if (!sf) return nullptr;
    SDL_LockSurface(sf);
    Uint8* base = (Uint8*)sf->pixels;
    const int pitch = sf->pitch;
    const float c = D / 2.0f, R = D / 2.0f - 2.0f, a = R * 0.45f, w = 1.5f;
    for (int y = 0; y < D; y++) for (int x = 0; x < D; x++) {
        const float px = x + 0.5f - c, py = y + 0.5f - c;
        const float dd = std::sqrt(px * px + py * py);
        float d = std::fabs(dd - R);                                                     // الحافة
        d = std::min<float>(d, std::fabs(std::sqrt((px / a) * (px / a) + (py / R) * (py / R)) - 1.0f) * a); // خط الطول
        if (dd <= R) {
            d = std::min<float>(d, std::fabs(py));                                              // خط الاستواء
            d = std::min<float>(d, std::fabs(std::fabs(py) - R * 0.5f));                        // خطوط العرض
        }
        const float cover = std::min<float>(1.0f, std::max<float>(0.0f, w - d + 0.5f));
        Uint8* o = base + y * pitch + x * 4;
        if (cover <= 0.0f) { o[0] = o[1] = o[2] = o[3] = 0; continue; }
        o[0] = 110; o[1] = 168; o[2] = 255; o[3] = (Uint8)(cover * 255.0f);
    }
    SDL_UnlockSurface(sf);
    SDL_Texture* t = SDL_CreateTextureFromSurface(rd, sf);
    SDL_FreeSurface(sf);
    return t;
}
struct Game { std::string id, title, iconPath; };
struct Section { std::string name; std::vector<std::string> lines; };
struct Config { std::vector<Section> sections; };
static bool configEqual(const Config& a, const Config& b) {
    if (a.sections.size() != b.sections.size()) return false;
    for (size_t i = 0; i < a.sections.size(); ++i)
        if (a.sections[i].name != b.sections[i].name || a.sections[i].lines != b.sections[i].lines) return false;
    return true;
}

// ---------- Small helpers ----------
// سطح ARGB أبيض بزوايا مدوّرة (الشفافية = مقدار التغطية، مع حافة ناعمة). يُبنى مرة لكل شكل.
static SDL_Surface* makeRoundMask(int ww, int hh, int rad) {
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, ww, hh, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!s) return nullptr;
    SDL_LockSurface(s);
    Uint32* px = (Uint32*)s->pixels;
    const int pitch = s->pitch / 4;
    auto put = [&](int x, int y, Uint8 a) { if (x >= 0 && x < ww && y >= 0 && y < hh) px[y * pitch + x] = ((Uint32)a << 24) | 0x00FFFFFFu; };
    for (int y = 0; y < hh; y++) {
        const int i = y < rad ? y : (y >= hh - rad ? hh - 1 - y : -1);   // -1 = الجزء المستقيم في الوسط
        int full = 0;
        float frac = 0.0f;
        if (i >= 0) {
            float dy = rad - i - 0.5f;
            float inset = rad - std::sqrt((float)rad * rad - dy * dy);
            full = (int)std::ceil(inset);
            frac = full - inset;
        }
        for (int x = full; x < ww - full; x++) put(x, y, 255);
        if (i >= 0 && full > 0 && frac > 0.03f) {
            const Uint8 pa = (Uint8)(255 * frac);
            put(full - 1, y, pa);
            put(ww - full, y, pa);
        }
    }
    SDL_UnlockSurface(s);
    return s;
}
static std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) ++a;
    while (b > a && std::isspace((unsigned char)s[b - 1])) --b;
    return s.substr(a, b - a);
}
static std::string lower(std::string s) { for (char& c : s) c = (char)std::tolower((unsigned char)c); return s; }
static bool endsWith(const std::string& s, const char* tail) {
    size_t n = std::strlen(tail);
    return s.size() >= n && s.compare(s.size() - n, n, tail) == 0;
}

// ---------- token file (store: "Edit token") ----------
static const size_t TOKEN_MAX = 256;
static bool readTokenTxt(const std::string& path, std::string& out);
static bool writeTokenTxt(const std::string& path, const std::string& token);
static bool isJsonTokenPath(const std::string& p) { return p.size() > 5 && p.compare(p.size() - 5, 5, ".json") == 0; }
static bool isIniTokenPath(const std::string& p) { return p.size() > 4 && p.compare(p.size() - 4, 4, ".ini") == 0; }
// .json -> key path, .ini -> "key" or "Section.key", anything else -> the whole file is the token
static bool readTokenFile(const std::string& path, const std::string& key, std::string& out) {
    if (isJsonTokenPath(path)) return PluginStore::readTokenJson(path, key, out);
    if (isIniTokenPath(path)) return PluginStore::readTokenIni(path, key, out);
    return readTokenTxt(path, out);
}
static bool writeTokenFile(const std::string& path, const std::string& key, const std::string& token) {
    if (isJsonTokenPath(path)) return PluginStore::writeTokenJson(path, key, token);
    if (isIniTokenPath(path)) return PluginStore::writeTokenIni(path, key, token);
    return writeTokenTxt(path, token);
}
static bool readTokenTxt(const std::string& path, std::string& out) {
    out.clear();
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    char buf[512]; size_t n;
    while (out.size() < 1024 && (n = std::fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    std::fclose(f);
    out = trim(out);
    if (out.size() > TOKEN_MAX) out.resize(TOKEN_MAX);
    return true;
}
static bool writeTokenTxt(const std::string& path, const std::string& token) {
    for (size_t i = 1; i < path.size(); ++i) if (path[i] == '/') mkdir(path.substr(0, i).c_str(), 0777);   // create parent folders (errors ignored: they may already exist)
    const std::string tmp = path + ".tmp";
    FILE* f = std::fopen(tmp.c_str(), "wb");
    if (!f) return false;
    bool ok = std::fwrite(token.data(), 1, token.size(), f) == token.size();
    ok = (std::fclose(f) == 0) && ok;
    if (!ok) { unlink(tmp.c_str()); return false; }
    if (std::rename(tmp.c_str(), path.c_str()) != 0) { unlink(tmp.c_str()); return false; }
    return true;
}
// ---------- native PS4 keyboard (system IME dialog, libSceImeDialog) ----------
// Declared here with the same layout as the SDK's SceImeDialogParam, so no extra header is needed. Add -lSceImeDialog to LIBS.
struct ImeDialogParam {
    int32_t userId; uint32_t type; uint64_t supportedLanguages; uint32_t enterLabel; uint32_t inputMethod;
    void* filter; uint32_t option; uint32_t maxTextLength; char16_t* inputTextBuffer;
    float posx; float posy; uint32_t horizontalAlignment; uint32_t verticalAlignment;
    const char16_t* placeholder; const char16_t* title; int8_t reserved[16];
};
struct ImeDialogResult { int32_t endstatus; int8_t reserved[60]; };   // endstatus: 0 = OK, 1 = canceled by the user, 2 = aborted
extern "C" {
int sceImeDialogInit(const ImeDialogParam* param, const void* extended);
int sceImeDialogGetStatus(void);   // 0 = none, 1 = running, 2 = finished
int sceImeDialogGetResult(ImeDialogResult* result);
int sceImeDialogTerm(void);
}
static void utf8ToUtf16(const std::string& in, char16_t* out, size_t cap) {
    size_t n = 0;
    for (size_t i = 0; i < in.size() && n + 1 < cap;) {
        const unsigned char c = (unsigned char)in[i]; uint32_t cp = '?'; size_t len = 1;
        if (c < 0x80) cp = c;
        else if ((c >> 5) == 6 && i + 1 < in.size()) { cp = ((c & 0x1Fu) << 6) | ((unsigned char)in[i + 1] & 0x3Fu); len = 2; }
        else if ((c >> 4) == 14 && i + 2 < in.size()) { cp = ((c & 0x0Fu) << 12) | (((unsigned char)in[i + 1] & 0x3Fu) << 6) | ((unsigned char)in[i + 2] & 0x3Fu); len = 3; }
        i += len; out[n++] = (char16_t)cp;
    }
    out[n] = 0;
}
static std::string utf16ToUtf8(const char16_t* in, size_t cap) {
    std::string o;
    for (size_t i = 0; i < cap && in[i]; ++i) {
        const uint32_t c = in[i];
        if (c < 0x80) o += (char)c;
        else if (c < 0x800) { o += (char)(0xC0 | (c >> 6)); o += (char)(0x80 | (c & 0x3F)); }
        else { o += (char)(0xE0 | (c >> 12)); o += (char)(0x80 | ((c >> 6) & 0x3F)); o += (char)(0x80 | (c & 0x3F)); }
    }
    return o;
}

// ---------- plugins.ini ----------
static bool pluginLine(const std::string& raw, std::string& path, bool& enabled) {
    std::string s = trim(raw);
    if (s.empty() || s[0] == ';' || s[0] == '#' || s[0] == '[') return false;
    size_t eq = s.find('=');
    std::string tail;
    if (eq != std::string::npos) { tail = lower(trim(s.substr(eq + 1))); s = trim(s.substr(0, eq)); }
    std::string l = lower(s);
    if (!endsWith(l, ".prx") && !endsWith(l, ".sprx")) return false;
    path = s;
    enabled = tail != "false";
    return true;
}
static Section* section(Config& c, const std::string& name, bool create) {
    for (auto& s : c.sections) if (s.name == name) return &s;
    if (!create) return nullptr;
    c.sections.push_back({name, {}});
    return &c.sections.back();
}
static Config readConfig() {
    Config c;
    std::ifstream in(CONFIG_PATH);
    std::string line;
    Section* cur = nullptr;
    while (std::getline(in, line)) {
        std::string s = trim(line);
        if (s.size() > 2 && s.front() == '[' && s.back() == ']') {
            c.sections.push_back({trim(s.substr(1, s.size() - 2)), {}});
            cur = &c.sections.back();
        } else {
            if (!cur) { c.sections.push_back({"", {}}); cur = &c.sections.back(); }
            if (!s.empty()) cur->lines.push_back(line);
        }
    }
    if (!section(c, "default", false)) c.sections.push_back({"default", {}});
    return c;
}
static int explicitState(const Config& c, const std::string& sec, const std::string& path) {
    for (const auto& s : c.sections) {
        if (s.name != sec) continue;
        for (const auto& line : s.lines) {
            std::string p; bool en;
            if (pluginLine(line, p, en) && p == path) return en ? 1 : 0;
        }
    }
    return -1;
}
static bool defaultState(const Config& c, const std::string& path) { return explicitState(c, "default", path) == 1; }
static bool effectiveState(const Config& c, const std::string& sec, const std::string& path) {
    int st = explicitState(c, sec, path);
    return st >= 0 ? st == 1 : (sec != "default" && defaultState(c, path));
}
static void setState(Config& c, const std::string& sec, const std::string& path, bool enabled) {
    Section* s = section(c, sec, true);
    size_t first = s->lines.size();
    for (size_t i = 0; i < s->lines.size();) {
        std::string p; bool old;
        if (pluginLine(s->lines[i], p, old) && p == path) {
            if (first == s->lines.size()) first = i;
            s->lines.erase(s->lines.begin() + i);
        } else ++i;
    }
    if (sec == "default") {
        if (enabled) s->lines.insert(s->lines.begin() + first, path + "=true");
    } else {
        bool inherited = defaultState(c, path);
        if (enabled != inherited) s->lines.insert(s->lines.begin() + first, path + (enabled ? "=true" : "=false"));
    }
}
static bool saveConfig(const Config& c) {
    std::string temp = "/data/GoldHEN/plugins.ini.tmp", backup = "/data/GoldHEN/plugins.ini.bak";
    struct stat st{};
    if (stat(CONFIG_PATH, &st) == 0) {
        // نقرأ الملف كاملاً ثم نكتبه: operator<< مع rdbuf يفشل على الملف الفاضي ويمنع الحفظ
        std::string original;
        {
            std::ifstream src(CONFIG_PATH, std::ios::binary);
            original.assign(std::istreambuf_iterator<char>(src), std::istreambuf_iterator<char>());
        }
        std::ofstream dst(backup, std::ios::binary | std::ios::trunc);
        if (!dst) return false;
        dst.write(original.data(), (std::streamsize)original.size());
        dst.flush();
        if (!dst.good()) return false;
    }
    std::ofstream out(temp, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    for (size_t i = 0; i < c.sections.size(); ++i) {
        const auto& s = c.sections[i];
        if (!s.name.empty()) out << '[' << s.name << "]\n";
        for (const auto& line : s.lines) if (!trim(line).empty()) out << line << '\n';
        if (i + 1 < c.sections.size()) out << '\n';
    }
    out.flush();
    if (!out.good()) return false;
    out.close();
    return std::rename(temp.c_str(), CONFIG_PATH) == 0;
}

// كتابة نص plugins.ini المُصلَح، مع نسخة .bak من الملف الأصلي (بايتاً ببايت) قبل أي شيء
static bool saveConfigText(const std::string& original, const std::string& text) {
    const std::string temp = "/data/GoldHEN/plugins.ini.tmp", backup = "/data/GoldHEN/plugins.ini.bak";
    {
        std::ofstream dst(backup, std::ios::binary | std::ios::trunc);
        if (!dst) return false;
        dst.write(original.data(), (std::streamsize)original.size());
        dst.flush();
        if (!dst.good()) return false;
    }
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out.write(text.data(), (std::streamsize)text.size());
        out.flush();
        if (!out.good()) return false;
    }
    return std::rename(temp.c_str(), CONFIG_PATH) == 0;
}

// حذف كل أسطر إضافة معينة من كل الأقسام
static void removePluginFromConfig(Config& c, const std::string& path) {
    for (auto& sec : c.sections) {
        for (size_t i = 0; i < sec.lines.size();) {
            std::string p; bool en;
            if (pluginLine(sec.lines[i], p, en) && p == path) sec.lines.erase(sec.lines.begin() + i);
            else ++i;
        }
    }
}

// ---------- Title IDs / SFO ----------
static uint16_t le16(const unsigned char* p) { return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }
static uint32_t le32(const unsigned char* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static bool validTitleId(const std::string& s) {
    if (s.size() != 9) return false;
    for (int i = 0; i < 4; i++) if (!std::isalnum((unsigned char)s[i])) return false;
    for (int i = 4; i < 9; i++) if (!std::isdigit((unsigned char)s[i])) return false;
    return true;
}
static std::string titleIdFromEntry(const std::string& s) {
    if (s.size() < 9) return {};
    std::string id = s.substr(0, 9);
    if (!validTitleId(id)) return {};
    if (s.size() > 9 && s[9] != '-' && s[9] != '_' && s[9] != '.') return {};
    return id;
}
static std::string sfoValue(const std::vector<unsigned char>& b, const char* want) {
    if (b.size() < 20 || std::memcmp(b.data(), "\0PSF", 4) != 0) return {};
    size_t key = le32(&b[8]), data = le32(&b[12]);
    uint32_t n = le32(&b[16]);
    if (n > 2048 || key >= b.size() || data >= b.size()) return {};
    for (uint32_t i = 0; i < n; i++) {
        size_t q = 20 + (size_t)i * 16;
        if (q > b.size() || b.size() - q < 16) break;
        size_t ko = le16(&b[q]);
        uint32_t len = le32(&b[q + 4]), off = le32(&b[q + 12]);
        if (ko >= b.size() - key) continue;
        size_t kp = key + ko, ke = kp;
        while (ke < b.size() && b[ke]) ++ke;
        if (ke == b.size()) continue;
        std::string name((const char*)&b[kp], ke - kp);
        if (name == want && off <= b.size() - data && len <= b.size() - data - off) {
            std::string v((const char*)&b[data + off], len);
            while (!v.empty() && v.back() == 0) v.pop_back();
            return v;
        }
    }
    return {};
}
static bool readFileCapped(const std::string& p, size_t cap, std::vector<unsigned char>& out) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    out.resize(cap);
    f.read((char*)out.data(), (std::streamsize)cap);
    out.resize((size_t)f.gcount());
    return !out.empty();
}
// يفك ترميز PNG بـ stb_image ويصغّره إلى 256 بكسل كحد أقصى (أسرع بكثير في الرسم البرمجي)
static std::string gIconErr;
// يعبّي مستطيلاً بزوايا منحنية (يُرسم بخطوط أفقية، بدون تنعيم)
static void fillRoundRect(SDL_Renderer* r, int x, int y, int w, int h, int rad, SDL_Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    if (rad > w / 2) rad = w / 2;
    if (rad > h / 2) rad = h / 2;
    if (rad <= 0) { SDL_Rect q{x, y, w, h}; SDL_RenderFillRect(r, &q); return; }
    for (int dy = 0; dy < h; dy++) {
        float t = -1.f;
        if (dy < rad)         t = rad - (dy + 0.5f);
        else if (dy >= h - rad) t = (dy + 0.5f) - (h - rad);
        int inset = 0;
        if (t >= 0.f) {
            float under = (float)rad * rad - t * t;
            if (under < 0.f) under = 0.f;
            inset = (int)std::ceil(rad - std::sqrt(under));
        }
        SDL_Rect row{x + inset, y + dy, w - 2 * inset, 1};
        SDL_RenderFillRect(r, &row);
    }
}
// يجعل الزوايا منحنية بتنعيم (anti-aliasing): البكسلات خارج الدائرة الركنية تصير شفافة تدريجياً
static void roundCorners(std::vector<unsigned char>& px, int w, int h, float rad) {
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float cx, cy;
            if (x < rad && y < rad)            { cx = rad;         cy = rad; }
            else if (x >= w - rad && y < rad)  { cx = w - rad;     cy = rad; }
            else if (x < rad && y >= h - rad)  { cx = rad;         cy = h - rad; }
            else if (x >= w - rad && y >= h - rad) { cx = w - rad; cy = h - rad; }
            else continue;
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            float d = std::sqrt(dx * dx + dy * dy);
            float cov = rad - d + 0.5f;
            if (cov < 0.f) cov = 0.f;
            if (cov > 1.f) cov = 1.f;
            size_t i = ((size_t)y * w + x) * 4 + 3;
            px[i] = (unsigned char)(px[i] * cov);
        }
    }
}
// فك الصورة: عمل ثقيل، يتم في خيط الخلفية (وليس في الخيط الرئيسي)
static bool decodeIconPixels(const std::string& path, std::vector<unsigned char>& cur, int& cw, int& ch, std::string& err) {
    std::vector<unsigned char> bytes;
    if (!readFileCapped(path, 4 * 1024 * 1024, bytes)) { err = "read"; return false; }
    int w = 0, h = 0, n = 0;
    unsigned char* px = stbi_load_from_memory(bytes.data(), (int)bytes.size(), &w, &h, &n, 4);
    if (!px) { const char* why = stbi_failure_reason(); err = why ? why : "decode"; return false; }
    cur.assign(px, px + (size_t)w * h * 4);
    stbi_image_free(px);
    cw = w; ch = h;
    while (cw > 256 && ch > 256 && cw % 2 == 0 && ch % 2 == 0) {
        int nw = cw / 2, nh = ch / 2;
        std::vector<unsigned char> nx((size_t)nw * nh * 4);
        for (int y = 0; y < nh; y++)
            for (int x = 0; x < nw; x++)
                for (int c = 0; c < 4; c++) {
                    int sum = cur[((size_t)(2 * y) * cw + 2 * x) * 4 + c] + cur[((size_t)(2 * y) * cw + 2 * x + 1) * 4 + c]
                            + cur[((size_t)(2 * y + 1) * cw + 2 * x) * 4 + c] + cur[((size_t)(2 * y + 1) * cw + 2 * x + 1) * 4 + c];
                    nx[((size_t)y * nw + x) * 4 + c] = (unsigned char)(sum / 4);
                }
        cur.swap(nx);
        cw = nw; ch = nh;
    }
    roundCorners(cur, cw, ch, cw * 0.14f);   // نصف القطر حوالي 14% من عرض الأيقونة
    return true;
}
// إنشاء التكستشر من بيانات مفكوكة جاهزة (سريع): يتم في الخيط الرئيسي فقط
static SDL_Texture* textureFromPixels(SDL_Renderer* r, std::vector<unsigned char>& cur, int cw, int ch) {
    SDL_Surface* sf = SDL_CreateRGBSurfaceWithFormatFrom(cur.data(), cw, ch, 32, cw * 4, SDL_PIXELFORMAT_RGBA32);
    if (!sf) { gIconErr = "surface"; return nullptr; }
    SDL_Texture* t = SDL_CreateTextureFromSurface(r, sf);
    SDL_FreeSurface(sf);
    if (t) SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND); else gIconErr = "texture";
    return t;
}

// ---- خيط فك الأيقونات في الخلفية ----
struct IconDecoded { std::vector<unsigned char> px; int w = 0, h = 0; bool ok = false; std::string err; };
static SDL_mutex* gIconMx = nullptr;
static SDL_sem* gIconSem = nullptr;
static SDL_Thread* gIconThread = nullptr;
static volatile int gIconRun = 0;
static std::deque<std::string> gIconQueue;             // مسارات بانتظار الفك
static std::map<std::string, IconDecoded> gIconDone;   // نتائج جاهزة
static std::set<std::string> gIconWanted;              // طُلبت مرة فلا تُطلب ثانية

static int iconWorkerMain(void*) {
    for (;;) {
        SDL_SemWait(gIconSem);
        if (!gIconRun) break;
        std::string path;
        SDL_LockMutex(gIconMx);
        if (!gIconQueue.empty()) { path = gIconQueue.front(); gIconQueue.pop_front(); }
        SDL_UnlockMutex(gIconMx);
        if (path.empty()) continue;
        IconDecoded d;
        d.ok = decodeIconPixels(path, d.px, d.w, d.h, d.err);
        SDL_LockMutex(gIconMx);
        gIconDone[path] = std::move(d);
        SDL_UnlockMutex(gIconMx);
    }
    return 0;
}
static void startIconWorker() {
    if (gIconThread) return;
    gIconMx = SDL_CreateMutex();
    gIconSem = SDL_CreateSemaphore(0);
    if (!gIconMx || !gIconSem) return;
    gIconRun = 1;
    gIconThread = SDL_CreateThread(iconWorkerMain, "icon-decode", nullptr);
}
static void stopIconWorker() {
    if (!gIconThread) return;
    gIconRun = 0;
    SDL_SemPost(gIconSem);
    SDL_WaitThread(gIconThread, nullptr);
    gIconThread = nullptr;
}
static void requestIcon(const std::string& path) {
    if (!gIconMx) return;
    SDL_LockMutex(gIconMx);
    if (!gIconWanted.count(path)) {
        gIconWanted.insert(path);
        gIconQueue.push_back(path);
        SDL_SemPost(gIconSem);
    }
    SDL_UnlockMutex(gIconMx);
}
static bool takeIcon(const std::string& path, IconDecoded& out) {
    if (!gIconMx) return false;
    bool got = false;
    SDL_LockMutex(gIconMx);
    auto it = gIconDone.find(path);
    if (it != gIconDone.end()) { out = std::move(it->second); gIconDone.erase(it); got = true; }
    SDL_UnlockMutex(gIconMx);
    return got;
}
// صورة QR: تُركَّب على خلفية بيضاء وتُصغَّر (أو تُكبَّر بمضاعف صحيح) إلى حجم ثابت تماماً، فتظهر بلا ضبابية وبنفس الحجم دائماً.
static const int QR_PX = 190;
static const int STORE_ROWS = 7;   // visible rows in the Store list (left panel)
// معالجة الصورة فقط (فك + تصغير) بدون أي عمل على SDL: تعمل في خيط خلفي
static bool qrPixelsFromBytes(const std::vector<unsigned char>& bytes, int S, std::vector<unsigned char>& out) {
    const unsigned char* data = bytes.data();
    int w = 0, h = 0, n = 0;
    unsigned char* px = stbi_load_from_memory(data, (int)bytes.size(), &w, &h, &n, 4);
    if (!px) { const char* why = stbi_failure_reason(); gIconErr = why ? why : "decode"; return false; }
    if (w < 8 || h < 8 || w > 4096 || h > 4096) { stbi_image_free(px); gIconErr = "size"; return false; }
    // 1) تسطيح الشفافية على الأبيض
    std::vector<float> lum((size_t)w * h);
    for (size_t i = 0; i < (size_t)w * h; i++) {
        const unsigned char* q = px + i * 4;
        float a = q[3] / 255.0f;
        lum[i] = (0.299f * q[0] + 0.587f * q[1] + 0.114f * q[2]) * a + 255.0f * (1.0f - a);
    }
    stbi_image_free(px);
    out.assign((size_t)S * S * 4, 255);   // أبيض
    const int side = std::max(w, h);
    if (side >= S) {
        // تصغير بمتوسط المساحة إلى S×S (الصورة تُوضع في المنتصف إن لم تكن مربعة)
        const float sc = (float)side / S;
        const int ow = std::max(1, (int)(w / sc + 0.5f)), oh = std::max(1, (int)(h / sc + 0.5f));
        const int ox = (S - ow) / 2, oy = (S - oh) / 2;
        for (int y = 0; y < oh; y++) {
            int y0 = (int)(y * sc), y1 = std::min(h, std::max(y0 + 1, (int)std::ceil((y + 1) * sc)));
            for (int x = 0; x < ow; x++) {
                int x0 = (int)(x * sc), x1 = std::min(w, std::max(x0 + 1, (int)std::ceil((x + 1) * sc)));
                float sum = 0; int cnt = 0;
                for (int yy = y0; yy < y1; yy++) for (int xx = x0; xx < x1; xx++) { sum += lum[(size_t)yy * w + xx]; cnt++; }
                unsigned char v = (unsigned char)(sum / std::max(cnt, 1) + 0.5f);
                unsigned char* o = &out[((size_t)(oy + y) * S + (ox + x)) * 4];
                o[0] = o[1] = o[2] = v; o[3] = 255;
            }
        }
    } else {
        // تكبير بمضاعف صحيح (أقرب جار) كي تبقى الوحدات حادة
        const int k = std::max(1, S / side);
        const int ow = w * k, oh = h * k, ox = (S - ow) / 2, oy = (S - oh) / 2;
        for (int y = 0; y < oh; y++) for (int x = 0; x < ow; x++) {
            unsigned char v = (unsigned char)(lum[(size_t)(y / k) * w + (x / k)] + 0.5f);
            unsigned char* o = &out[((size_t)(oy + y) * S + (ox + x)) * 4];
            o[0] = o[1] = o[2] = v; o[3] = 255;
        }
    }
    return true;
}

// تحضير QR في خيط خلفي: الحالة 0 = جارٍ، 1 = نجح، 2 = فشل
struct QrPrep { std::vector<unsigned char> in, out; std::string url; std::atomic<int> state{0}; };
static QrPrep* gQrPrep = nullptr;
static int qrPrepThread(void* p) {
    QrPrep* q = (QrPrep*)p;
    q->state = qrPixelsFromBytes(q->in, QR_PX, q->out) ? 1 : 2;
    return 0;
}

static SDL_Texture* loadQrTextureFromBytes(SDL_Renderer* r, const std::vector<unsigned char>& bytes, int S) {
    int w = 0, h = 0, n = 0;
    unsigned char* px = stbi_load_from_memory(bytes.data(), (int)bytes.size(), &w, &h, &n, 4);
    if (!px) { const char* why = stbi_failure_reason(); gIconErr = why ? why : "decode"; return nullptr; }
    if (w < 8 || h < 8 || w > 4096 || h > 4096) { stbi_image_free(px); gIconErr = "size"; return nullptr; }
    // 1) تسطيح الشفافية على الأبيض
    std::vector<float> lum((size_t)w * h);
    for (size_t i = 0; i < (size_t)w * h; i++) {
        const unsigned char* q = px + i * 4;
        float a = q[3] / 255.0f;
        lum[i] = (0.299f * q[0] + 0.587f * q[1] + 0.114f * q[2]) * a + 255.0f * (1.0f - a);
    }
    stbi_image_free(px);
    std::vector<unsigned char> out((size_t)S * S * 4, 255);   // أبيض
    const int side = std::max(w, h);
    if (side >= S) {
        // تصغير بمتوسط المساحة إلى S×S (الصورة تُوضع في المنتصف إن لم تكن مربعة)
        const float sc = (float)side / S;
        const int ow = std::max(1, (int)(w / sc + 0.5f)), oh = std::max(1, (int)(h / sc + 0.5f));
        const int ox = (S - ow) / 2, oy = (S - oh) / 2;
        for (int y = 0; y < oh; y++) {
            int y0 = (int)(y * sc), y1 = std::min(h, std::max(y0 + 1, (int)std::ceil((y + 1) * sc)));
            for (int x = 0; x < ow; x++) {
                int x0 = (int)(x * sc), x1 = std::min(w, std::max(x0 + 1, (int)std::ceil((x + 1) * sc)));
                float sum = 0; int cnt = 0;
                for (int yy = y0; yy < y1; yy++) for (int xx = x0; xx < x1; xx++) { sum += lum[(size_t)yy * w + xx]; cnt++; }
                unsigned char v = (unsigned char)(sum / std::max(cnt, 1) + 0.5f);
                unsigned char* o = &out[((size_t)(oy + y) * S + (ox + x)) * 4];
                o[0] = o[1] = o[2] = v; o[3] = 255;
            }
        }
    } else {
        // تكبير بمضاعف صحيح (أقرب جار) كي تبقى الوحدات حادة
        const int k = std::max(1, S / side);
        const int ow = w * k, oh = h * k, ox = (S - ow) / 2, oy = (S - oh) / 2;
        for (int y = 0; y < oh; y++) for (int x = 0; x < ow; x++) {
            unsigned char v = (unsigned char)(lum[(size_t)(y / k) * w + (x / k)] + 0.5f);
            unsigned char* o = &out[((size_t)(oy + y) * S + (ox + x)) * 4];
            o[0] = o[1] = o[2] = v; o[3] = 255;
        }
    }
    SDL_Surface* sf = SDL_CreateRGBSurfaceWithFormatFrom(out.data(), S, S, 32, S * 4, SDL_PIXELFORMAT_RGBA32);
    if (!sf) { gIconErr = "surface"; return nullptr; }
    SDL_Texture* t = SDL_CreateTextureFromSurface(r, sf);
    SDL_FreeSurface(sf);
    if (!t) gIconErr = "texture";
    return t;
}
// supportQR can be an image link (.png/.jpg/...: downloaded as before) or a plain link such as a Discord invite:
// in that case the QR code is generated here on the console, instantly, with nothing to download.
static bool isQrImageUrl(const std::string& u) {
    size_t end = u.find_first_of("?#"); if (end == std::string::npos) end = u.size();
    std::string path = u.substr(0, end);
    for (auto& c : path) c = (char)std::tolower((unsigned char)c);
    for (const char* ext : {".png", ".jpg", ".jpeg", ".bmp", ".gif"}) {
        const size_t n = std::strlen(ext);
        if (path.size() >= n && path.compare(path.size() - n, n, ext) == 0) return true;
    }
    return false;
}
static SDL_Texture* makeQrTextureFromText(SDL_Renderer* r, const std::string& text, int S) {
    if (text.empty() || text.size() > 500) return nullptr;
    const qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(text.c_str(), qrcodegen::QrCode::Ecc::MEDIUM);
    const int n = qr.getSize();
    const int Q = 2, N = n + 2 * Q;                          // quiet zone of 2 modules (enough for phone cameras), no extra white margin
    std::vector<unsigned char> out((size_t)S * S * 4, 255);   // white
    std::vector<int> mod((size_t)S);                          // pixel -> module index (module sizes differ by at most 1 px)
    for (int i = 0; i < S; i++) mod[(size_t)i] = (i * N) / S - Q;
    for (int py = 0; py < S; py++) for (int px = 0; px < S; px++) {
        const int mx = mod[(size_t)px], my = mod[(size_t)py];
        if (mx < 0 || my < 0 || mx >= n || my >= n || !qr.getModule(mx, my)) continue;
        unsigned char* o = &out[((size_t)py * S + px) * 4];
        o[0] = o[1] = o[2] = 0; o[3] = 255;
    }
    SDL_Surface* sf = SDL_CreateRGBSurfaceWithFormatFrom(out.data(), S, S, 32, S * 4, SDL_PIXELFORMAT_RGBA32);
    if (!sf) return nullptr;
    SDL_Texture* t = SDL_CreateTextureFromSurface(r, sf);
    SDL_FreeSurface(sf);
    return t;
}
static std::string locateIcon(const std::string& id) {
    const std::string icons[] = {
        "/data/PluginManager/icons/" + id + ".png",
        "/user/appmeta/" + id + "/icon0.png", "/system_data/priv/appmeta/" + id + "/icon0.png",
        "/user/app/" + id + "/sce_sys/icon0.png", "/user/app/" + id + "/icon0.png",
        "/mnt/ext0/user/app/" + id + "/sce_sys/icon0.png"};
    for (const auto& p : icons) {
        struct stat st{};
        if (stat(p.c_str(), &st) == 0 && S_ISREG(st.st_mode)) return p;
    }
    return {};
}
static std::vector<Plugin> scanPlugins() {
    std::vector<Plugin> v;
    DIR* d = opendir(PLUGIN_DIR);
    if (!d) return v;
    struct dirent* e;
    while ((e = readdir(d)) != nullptr) {
        std::string n = e->d_name, l = lower(n);
        if (!endsWith(l, ".prx") && !endsWith(l, ".sprx")) continue;
        std::string p = std::string(PLUGIN_DIR) + "/" + n;
        struct stat st{};
        if (stat(p.c_str(), &st) == 0 && S_ISREG(st.st_mode)) v.push_back({n, p});
    }
    closedir(d);
    std::sort(v.begin(), v.end(), [](const Plugin& a, const Plugin& b) { return lower(a.name) < lower(b.name); });
    return v;
}

// ---------- Shared scan state (scan thread <-> UI thread) ----------
struct Shared {
    SDL_mutex* mx = nullptr;
    std::vector<Game> games;
    std::vector<Plugin> plugins;
    Config config;
    bool pluginsReady = false;
    int readableRoots = 0, entriesSeen = 0;
    bool ftpConnected = false;
    int ftpBytes = 0;
    std::string ftpNote1, ftpNote2, ftpNote3;
    std::string rootNote;   // رسالة حالة الروت (لا تكتب عليها رسائل FTP)
    int ftpState = 0, ftpTried = 0, ftpGot = 0;   // 0 لم يُجرَّب، 1 فشل الاتصال، 2 فشل الترحيب، 3 فشل الدخول، 4 متصل
    std::string stage;
};
static Shared gs;
static SDL_atomic_t gDone, gVersion, gAbort;
static Uint32 gFtpDeadline = 0;

struct Lock { Lock() { SDL_LockMutex(gs.mx); } ~Lock() { SDL_UnlockMutex(gs.mx); } };
static void bump() { SDL_AtomicAdd(&gVersion, 1); }
static bool aborted() { return SDL_AtomicGet(&gAbort) != 0; }
static void setStage(const char* s) { Lock l; gs.stage = s; bump(); }
static void setFtpState(int st) { Lock l; gs.ftpState = st; if (st == 4) gs.ftpConnected = true; bump(); }
static void addFtpStats(int tried, int got) { Lock l; gs.ftpTried += tried; gs.ftpGot += got; bump(); }
static void sortGames(std::vector<Game>& g) {
    std::sort(g.begin(), g.end(), [](const Game& a, const Game& b) { return lower(a.title) < lower(b.title); });
}
static void publishGames(const std::vector<Game>& g, int entries = -1) {
    std::vector<Game> copy = g;
    sortGames(copy);
    Lock l;
    gs.games = copy;
    if (entries >= 0) gs.entriesSeen = entries;
    bump();
}

static RootAccess gRoot;
static bool gRootWorked = false;   // true إذا الروت جاب الألعاب بنجاح (بدون FTP)
static int gRootState = 0; // 0 لم يُجرَّب، 1 يعمل، 2 فشل
static bool rootReady();
// هل التطبيق المختار مثبت بالفعل؟ (إخفاء زر التثبيت حتى لا يتداخل مع رسالة الحالة)
static bool rpiAlreadyInstalled() { std::string m; return PluginStore::Rpi::state(m) == PluginStore::Rpi::State::AlreadyInstalled; }
static bool fetchMetaRoot(std::vector<Game>& games);

// ---------- Local scan ----------
// تطبيقات النظام المؤكدة حسب بادئة المعرّف. كل ما عدا ذلك يظهر.
static bool isSystemApp(const std::string& id) {
    static const char* prefixes[] = {"NPXS", "PKGI"};   // مؤكدة من السجل فقط
    for (const char* p : prefixes)
        if (id.rfind(p, 0) == 0) return true;
    return false;
}
static std::vector<Game> scanLocal() {
    std::vector<Game> g;
    std::set<std::string> seen;
    // لازم نفعّل الروت قبل أي قراءة، وإلا القراءة تفشل داخل الـ sandbox
    {
        bool ok = rootReady();
        Lock l;
        if (ok) gs.rootNote = "الوصول الجذري مفعّل";
        else if (gRoot.ProbeOk()) gs.rootNote = "اختبار الكيرنل ناجح - الهروب متوقف (مرحلة اختبار)";
        else gs.rootNote = "اختبار الكيرنل فشل - تحقق من GoldHEN والتفعيل";
        bump();
    }
    const char* roots[] = {"/user/app", "/mnt/ext0/user/app", "/system_data/priv/appmeta", "/user/appmeta"};
    int entries = 0, noSfo = 0;
    std::string catLog, noSfoLog, idCat;   // تشخيص: الفئات والمعرّفات والمجلدات بدون param.sfo
    for (const char* root : roots) {
        if (aborted()) break;
        DIR* d = opendir(root);
        if (!d) continue;
        { Lock l; gs.readableRoots++; bump(); }
        struct dirent* e;
        while (!aborted() && (e = readdir(d)) != nullptr) {
            ++entries;
            std::string id = titleIdFromEntry(e->d_name);
            if (id.empty() || seen.count(id)) continue;
            seen.insert(id);
            std::string base = std::string(root) + "/" + e->d_name;
            const std::string paths[] = {
                base + "/sce_sys/param.sfo", base + "/param.sfo",
                "/system_data/priv/appmeta/" + id + "/param.sfo",
                "/user/appmeta/" + id + "/param.sfo"
            };
            std::vector<unsigned char> b;
            bool haveSfo = false;
            for (const auto& p : paths) {
                if (readFileCapped(p, 131072, b)) { haveSfo = true; break; }
            }
            if (!haveSfo) {                               // بدون param.sfo لا نعرف نوع التطبيق
                noSfo++;
                if (noSfoLog.size() < 1500) noSfoLog += std::string(e->d_name) + " ";
                continue;
            }
            std::string cat = sfoValue(b, "CATEGORY");
            if (catLog.size() < 1500) catLog += "[" + cat + "]";
            if (idCat.size() < 2500) idCat += id + "=" + cat + " ";
            if (isSystemApp(id)) continue;   // تطبيقات النظام فقط تُستبعد
            std::string title = sfoValue(b, "TITLE");
            Game game{id, title.empty() ? id : title, locateIcon(id)};
            g.push_back(game);
            if (g.size() % 8 == 0) publishGames(g, entries);
            if (g.size() >= 512) break;
        }
        closedir(d);
        if (g.size() >= 512) break;
    }
    if (rootReady()) fetchMetaRoot(g);   // صور وأسماء الألعاب الناقصة عبر الروت
    gRootWorked = !g.empty();
    publishGames(g, entries);
    return g;
}

// معرّفات الألعاب المثبتة، من نفس المجلدات التي يقرأها الفحص. false = لم تُقرأ أي قائمة أو القائمة فارغة
static bool collectInstalledTitleIds(std::set<std::string>& out) {
    const char* roots[] = {"/user/app", "/mnt/ext0/user/app", "/system_data/priv/appmeta", "/user/appmeta"};
    int readable = 0;
    for (const char* root : roots) {
        DIR* d = opendir(root);
        if (!d) continue;
        ++readable;
        struct dirent* e;
        while ((e = readdir(d)) != nullptr) {
            std::string id = titleIdFromEntry(e->d_name);
            if (id.empty()) continue;
            for (char& c : id) c = (char)std::toupper((unsigned char)c);
            out.insert(id);
        }
        closedir(d);
    }
    return readable > 0 && !out.empty();
}

// ---------- GoldHEN FTP fallback (with hard deadline) ----------
static bool ftpExpired() { return aborted() || (int)(SDL_GetTicks() - gFtpDeadline) > 0; }

class GoldHenFtp {
    int control = -1;
public:
    int failStage = 0;
    std::string lastReply;
private:
    static void timeouts(int fd, int sec) {
        timeval tv{sec, 0};
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    }
    static bool sendAll(int fd, const std::string& s) {
        size_t at = 0;
        while (at < s.size()) {
            ssize_t n = send(fd, s.data() + at, s.size() - at, 0);
            if (n <= 0) return false;
            at += (size_t)n;
        }
        return true;
    }
    bool line(std::string& out) {
        out.clear();
        char c;
        while (out.size() < 2048 && !ftpExpired()) {
            ssize_t n = recv(control, &c, 1, 0);
            if (n <= 0) return false;
            if (c == '\n') return true;
            if (c != '\r') out.push_back(c);
        }
        return false;
    }
    bool reply(int& code, std::string& first) {
        if (!line(first) || first.size() < 3) return false;
        code = std::atoi(first.substr(0, 3).c_str());
        if (first.size() > 3 && first[3] == '-') {
            std::string l;
            while (line(l)) if (l.size() >= 4 && std::atoi(l.substr(0, 3).c_str()) == code && l[3] == ' ') break;
        }
        return true;
    }
    bool command(const std::string& cmd, int& code, std::string& resp) { return sendAll(control, cmd + "\r\n") && reply(code, resp); }
    int dataSocket() {
        int code; std::string resp;
        if (!command("PASV", code, resp) || code != 227) return -1;
        size_t a = resp.find('('), b = resp.find(')', a == std::string::npos ? 0 : a);
        if (a == std::string::npos || b == std::string::npos) return -1;
        int h1, h2, h3, h4, p1, p2;
        if (std::sscanf(resp.substr(a + 1, b - a - 1).c_str(), "%d,%d,%d,%d,%d,%d", &h1, &h2, &h3, &h4, &p1, &p2) != 6) return -1;
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) return -1;
        timeouts(fd, 2);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons((uint16_t)(p1 * 256 + p2));
        if ((h1 | h2 | h3 | h4) == 0) addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        else addr.sin_addr.s_addr = htonl(((uint32_t)h1 << 24) | ((uint32_t)h2 << 16) | ((uint32_t)h3 << 8) | (uint32_t)h4);
        if (connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0) { close(fd); return -1; }
        return fd;
    }
public:
    bool open() {
        static bool netReady = false;
        if (!netReady) {
            sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NET);
            sceNetInit();
            sceNetPoolCreate("pluginmgr-net", 64 * 1024, 0);
            netReady = true;
        }
        control = socket(AF_INET, SOCK_STREAM, 0);
        if (control < 0) { failStage = 1; return false; }
        timeouts(control, 2);
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_port = htons(2121);
        a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (connect(control, (sockaddr*)&a, sizeof(a)) < 0) { failStage = 1; return false; }
        int code; std::string r;
        if (!reply(code, r) || code != 220) { failStage = 2; return false; }
        if (!command("USER anonymous", code, r)) { failStage = 3; return false; }
        if (code == 331 && !command("PASS anonymous", code, r)) { failStage = 3; return false; }
        if (code == 230 || code == 202) return true;
        failStage = 3;
        return false;
    }
    std::vector<unsigned char> transfer(const std::string& cmd) {
        std::vector<unsigned char> out;
        if (ftpExpired()) return out;
        int data = dataSocket();
        if (data < 0) { lastReply = "PASV failed"; return out; }
        int code = 0; std::string r;
        if (!command(cmd, code, r) || (code != 125 && code != 150)) { lastReply = r.empty() ? std::string("no reply") : r; close(data); return out; }
        unsigned char buf[8192];
        while (out.size() < 2 * 1024 * 1024 && !ftpExpired()) {
            ssize_t n = recv(data, buf, sizeof(buf), 0);
            if (n <= 0) break;
            out.insert(out.end(), buf, buf + n);
        }
        close(data);
        reply(code, r);
        return out;
    }
    std::vector<std::string> list(const std::string& path) {
        auto bytes = transfer("NLST " + path);
        std::string text(bytes.begin(), bytes.end());
        std::vector<std::string> out;
        size_t at = 0;
        while (at < text.size()) {
            size_t end = text.find('\n', at);
            if (end == std::string::npos) end = text.size();
            std::string n = trim(text.substr(at, end - at));
            size_t slash = n.find_last_of('/');
            if (slash != std::string::npos) n = n.substr(slash + 1);
            if (!n.empty()) out.push_back(n);
            at = end + 1;
        }
        return out;
    }
    std::vector<unsigned char> get(const std::string& path) { return transfer("RETR " + path); }
    void binary() { int c; std::string r; command("TYPE I", c, r); }
    void closeSession() {
        if (control >= 0) {
            int c; std::string r;
            if (!ftpExpired()) command("QUIT", c, r);
            close(control);
            control = -1;
        }
    }
    ~GoldHenFtp() { if (control >= 0) close(control); }
};

static bool gCacheFailed = false;
static std::string cacheIcon(const std::string& id, const std::vector<unsigned char>& bytes) {
    if (bytes.size() < 8 || std::memcmp(bytes.data(), "\x89PNG\r\n\x1a\n", 8) != 0) return {};
    mkdir(APP_DATA_DIR, 0777);
    mkdir("/data/PluginManager/icons", 0777);
    std::string path = "/data/PluginManager/icons/" + id + ".png";
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) { gCacheFailed = true; return {}; }
    f.write((const char*)bytes.data(), (std::streamsize)bytes.size());
    if (!f.good()) { gCacheFailed = true; return {}; }
    return path;
}

static std::string joinNames(const std::vector<std::string>& v, size_t maxN) {
    std::string o;
    for (size_t i = 0; i < v.size() && i < maxN; i++) { if (i) o += ", "; o += v[i]; }
    return o;
}


// ---------- Root access fast path (RootAccess) ----------
// يقرأ الأيقونات وملفات param.sfo مباشرة من نظام الملفات بعد تفعيل الجذر.
// إذا فشل أو لم يكتمل، يكمل fetchMeta عبر FTP كما كان.
// يرجّع الصلاحيات الأصلية بعد الفحص، حتى لا تبقى مرفوعة طول عمر التطبيق
static void restoreRootAfterScan() {
    gRoot.Terminate();
    gRootState = 0;
}
static bool gRegisterRestoreHook = (PluginStore::Rpi::setRestoreHook(restoreRootAfterScan), true);
static bool rootReady() {
    if (gRootState == 0) gRootState = gRoot.Initialize() ? 1 : 2;
    return gRootState == 1;
}
// تسجيل خطاف الهروب لمسار تثبيت BGFT (يُنفَّذ قبل تهيئة BGFT لأن الهروب يُرجَّع بعد فحص الأيقونات)
static bool gRegisterEscapeHook = (PluginStore::Rpi::setEscapeHook(rootReady), true);
static bool rootReadFile(const std::string& path, std::vector<unsigned char>& out) {
    out.clear();
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    unsigned char buf[8192];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0 && out.size() < 2 * 1024 * 1024)
        out.insert(out.end(), buf, buf + n);
    std::fclose(f);
    return !out.empty();
}
// يرجع true إذا اكتملت كل الألعاب (أيقونة + اسم) بدون FTP.
static bool fetchMetaRoot(std::vector<Game>& games) {
    if (!rootReady()) return false;
    {
        Lock l;
        gs.ftpNote1 = "الوصول الجذري مفعّل (libjbc)";
        gs.ftpNote2 = "جلب الصور والأسماء مباشرة من الجهاز";
        bump();
    }
    int got = 0;
    std::vector<unsigned char> bytes;
    for (auto& game : games) {
        bool cached = game.iconPath.rfind("/data/PluginManager/icons/", 0) == 0;
        if (!cached) {
            const std::string ico[] = {"/user/appmeta/" + game.id + "/icon0.png", "/system_data/priv/appmeta/" + game.id + "/icon0.png", "/user/app/" + game.id + "/sce_sys/icon0.png", "/user/app/" + game.id + "/icon0.png"};
            for (const auto& p : ico) {
                if (!rootReadFile(p, bytes)) continue;
                std::string path = cacheIcon(game.id, bytes);
                if (!path.empty()) { game.iconPath = path; break; }
            }
        }
        if (game.title == game.id) {
            const std::string sfo[] = {"/system_data/priv/appmeta/" + game.id + "/param.sfo", "/user/appmeta/" + game.id + "/param.sfo", "/user/app/" + game.id + "/sce_sys/param.sfo"};
            for (const auto& p : sfo) {
                if (!rootReadFile(p, bytes)) continue;
                std::string t = sfoValue(bytes, "TITLE");
                if (!t.empty()) { game.title = t; break; }
            }
        }
        if (!game.iconPath.empty() && game.title != game.id) ++got;
    }
    publishGames(games);
    {
        Lock l;
        gs.ftpNote3 = "الجذر: مكتمل=" + std::to_string(got) + " من " + std::to_string((int)games.size());
        bump();
    }
    return got == (int)games.size();
}

// يجلب الأيقونة والاسم لكل لعبة عبر FTP (مع إحصاءات وتشخيص)
static void fetchMeta(GoldHenFtp& ftp, std::vector<Game>& games) {
    gFtpDeadline = SDL_GetTicks() + FETCH_TIMEOUT_MS;
    if (fetchMetaRoot(games)) { addFtpStats(0, 0); return; }   // الجذر أولاً؛ إن لم يكتمل نكمل FTP
    ftp.binary();
    if (!games.empty()) {   // فحص تشخيصي للعبة الأولى: ماذا يوجد في مجلداتها؟
        const std::string id = games[0].id;
        auto a = ftp.list("/user/appmeta/" + id);
        auto b = ftp.list("/system_data/priv/appmeta/" + id);
        auto c = ftp.list("/user/app/" + id);
        Lock l;
        gs.ftpNote1 = "مجلدات " + id + ": appmeta=" + std::to_string((int)a.size()) + " priv=" + std::to_string((int)b.size()) + " app=" + std::to_string((int)c.size());
        gs.ftpNote2 = "appmeta: " + joinNames(a, 5) + "  |  app: " + joinNames(c, 5);
        bump();
    }
    int tried = 0, got = 0, bytesOk = 0, n = 0;
    for (auto& game : games) {
        if (ftpExpired()) break;
        if (game.iconPath.empty()) {
            ++tried;
            const std::string ico[] = {"/user/appmeta/" + game.id + "/icon0.png", "/system_data/priv/appmeta/" + game.id + "/icon0.png", "/user/app/" + game.id + "/sce_sys/icon0.png", "/user/app/" + game.id + "/icon0.png"};
            for (const auto& p : ico) {
                if (ftpExpired()) break;
                auto bytes = ftp.get(p);
                if (bytes.empty()) continue;
                ++bytesOk;
                std::string path = cacheIcon(game.id, bytes);
                if (!path.empty()) { game.iconPath = path; ++got; break; }
            }
        }
        if (game.title == game.id && !ftpExpired()) {
            const std::string sfo[] = {"/system_data/priv/appmeta/" + game.id + "/param.sfo", "/user/appmeta/" + game.id + "/param.sfo", "/user/app/" + game.id + "/sce_sys/param.sfo"};
            for (const auto& p : sfo) {
                if (ftpExpired()) break;
                auto bytes = ftp.get(p);
                if (bytes.empty()) continue;
                ++bytesOk;
                std::string t = sfoValue(bytes, "TITLE");
                if (!t.empty()) { game.title = t; break; }
            }
        }
        if (++n % 6 == 0) { publishGames(games); }
        if (n == 3) {   // بعد أول ثلاث ألعاب: سجّل أول رد فشل من الخادم
            Lock l;
            gs.ftpNote3 = got == 0 && bytesOk == 0 ? "آخر رد من الخادم: " + ftp.lastReply
                         : (gCacheFailed ? std::string("فشل حفظ الأيقونة في /data/PluginManager") : std::string());
            bump();
        }
    }
    { Lock l; gs.ftpBytes += bytesOk; if (gs.ftpNote3.empty() && got == 0) gs.ftpNote3 = (gCacheFailed ? std::string("فشل حفظ الأيقونة في /data/PluginManager") : "آخر رد من الخادم: " + ftp.lastReply); bump(); }
    addFtpStats(tried, got);
    publishGames(games);
}

static std::vector<Game> scanViaFtp() {
    std::vector<Game> games;
    gFtpDeadline = SDL_GetTicks() + LIST_TIMEOUT_MS;
    GoldHenFtp ftp;
    if (!ftp.open()) { setFtpState(ftp.failStage); return games; }
    setFtpState(4);
    std::set<std::string> seen;
    for (const auto& entry : ftp.list("/user/app")) {
        if (ftpExpired() || games.size() >= 256) break;
        std::string id = titleIdFromEntry(entry);
        if (id.empty() || seen.count(id)) continue;
        seen.insert(id);
        games.push_back({id, id, locateIcon(id)});   // يستعمل الأيقونة المخزّنة إن وُجدت
    }
    publishGames(games);
    setStage("icons");
    fetchMeta(ftp, games);
    ftp.closeSession();
    return games;
}

static void enrichViaFtp(std::vector<Game>& games) {
    bool need = false;
    for (const auto& g : games) if (g.iconPath.empty() || g.title == g.id) { need = true; break; }
    if (!need || aborted()) return;
    gFtpDeadline = SDL_GetTicks() + LIST_TIMEOUT_MS;
    GoldHenFtp ftp;
    if (!ftp.open()) { setFtpState(ftp.failStage); return; }
    setFtpState(4);
    fetchMeta(ftp, games);
    ftp.closeSession();
}

static int scanThreadMain(void*) {
    setStage("plugins");
    {
        std::vector<Plugin> p = scanPlugins();
        Config c = readConfig();
        Lock l;
        gs.plugins = p;
        gs.config = c;
        gs.pluginsReady = true;
        bump();
    }
    setStage("games");
    std::vector<Game> found = scanLocal();
    restoreRootAfterScan();
    if (found.empty() && !aborted()) {
        setStage("ftp");
        found = scanViaFtp();
    } else if (!aborted() && !gRootWorked) {
        setStage("icons");
        enrichViaFtp(found);
    }
    // ألعاب مذكورة في plugins.ini ولم تُكتشف
    // الألعاب تُعرض فقط إذا وُجدت فعلياً على الجهاز (بدون إضافة ما هو مذكور في plugins.ini فقط)
    publishGames(found);
    setStage("done");
    SDL_AtomicSet(&gDone, 1);
    return 0;
}

// =====================================================================
int main() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) != 0) return 1;
    SDL_AudioDeviceID moveAudio = 0;
    SDL_AudioSpec moveAudioSpec = {};
    Uint8* moveAudioData = nullptr;
    Uint32 moveAudioLength = 0;
    Uint8* clickAudioData = nullptr;
    Uint32 clickAudioLength = 0;
    bool clickAudioConverted = false;
    Uint8* successAudioData = nullptr;
    Uint32 successAudioLength = 0;
    bool successAudioConverted = false;
    Uint8* errorAudioData = nullptr;
    Uint32 errorAudioLength = 0;
    bool errorAudioConverted = false;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0 &&
        SDL_LoadWAV("/app0/assets/audio/move.wav", &moveAudioSpec, &moveAudioData, &moveAudioLength)) {
        moveAudioSpec.callback = nullptr;
        moveAudio = SDL_OpenAudioDevice(nullptr, 0, &moveAudioSpec, nullptr, 0);
        if (moveAudio) SDL_PauseAudioDevice(moveAudio, 0);
        else { SDL_FreeWAV(moveAudioData); moveAudioData = nullptr; moveAudioLength = 0; }
    }
    if (moveAudio) {
        SDL_AudioSpec clickSpec = {};
        Uint8* source = nullptr;
        Uint32 sourceLength = 0;
        if (SDL_LoadWAV("/app0/assets/audio/click.wav", &clickSpec, &source, &sourceLength)) {
            SDL_AudioCVT cvt = {};
            int conversion = SDL_BuildAudioCVT(&cvt, clickSpec.format, clickSpec.channels, clickSpec.freq,
                                               moveAudioSpec.format, moveAudioSpec.channels, moveAudioSpec.freq);
            if (conversion == 0) {
                clickAudioData = source;
                clickAudioLength = sourceLength;
            } else if (conversion > 0) {
                cvt.len = (int)sourceLength;
                clickAudioData = (Uint8*)std::malloc((size_t)sourceLength * (size_t)cvt.len_mult);
                if (clickAudioData) {
                    std::memcpy(clickAudioData, source, sourceLength);
                    cvt.buf = clickAudioData;
                    if (SDL_ConvertAudio(&cvt) == 0) {
                        clickAudioLength = (Uint32)cvt.len_cvt;
                        clickAudioConverted = true;
                    } else { std::free(clickAudioData); clickAudioData = nullptr; }
                }
                SDL_FreeWAV(source);
            } else SDL_FreeWAV(source);
        }
    }
    // تحميل أصوات النتائج وتحويلها إلى صيغة جهاز الصوت المفتوح نفسه.
    auto loadResultSound = [&](const char* path, Uint8** outData, Uint32* outLength, bool* converted) {
        if (!moveAudio) return;
        SDL_AudioSpec sourceSpec = {};
        Uint8* source = nullptr;
        Uint32 sourceLength = 0;
        if (!SDL_LoadWAV(path, &sourceSpec, &source, &sourceLength)) return;
        SDL_AudioCVT cvt = {};
        int conversion = SDL_BuildAudioCVT(&cvt, sourceSpec.format, sourceSpec.channels, sourceSpec.freq,
                                           moveAudioSpec.format, moveAudioSpec.channels, moveAudioSpec.freq);
        if (conversion == 0) {
            *outData = source;
            *outLength = sourceLength;
        } else if (conversion > 0) {
            cvt.len = (int)sourceLength;
            *outData = (Uint8*)std::malloc((size_t)sourceLength * (size_t)cvt.len_mult);
            if (*outData) {
                std::memcpy(*outData, source, sourceLength);
                cvt.buf = *outData;
                if (SDL_ConvertAudio(&cvt) == 0) {
                    *outLength = (Uint32)cvt.len_cvt;
                    *converted = true;
                } else { std::free(*outData); *outData = nullptr; }
            }
            SDL_FreeWAV(source);
        } else SDL_FreeWAV(source);
    };
    loadResultSound("/app0/assets/audio/success.wav", &successAudioData, &successAudioLength, &successAudioConverted);
    loadResultSound("/app0/assets/audio/error.wav", &errorAudioData, &errorAudioLength, &errorAudioConverted);
    IMG_Init(IMG_INIT_PNG);
    SDL_JoystickEventState(SDL_ENABLE);
    SDL_Joystick* pad = SDL_NumJoysticks() > 0 ? SDL_JoystickOpen(0) : nullptr;
    // قراءة العصا والأسهم مباشرة من نظام PS4 (حالة حقيقية لا تتأخر ولا تعلق) بدل الاعتماد على أحداث SDL
    int32_t rawPad = -1;
    {
        int32_t uid = 0;
        if (sceUserServiceGetInitialUser(&uid) < 0) {
            OrbisUserServiceInitializeParams up; up.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST;
            sceUserServiceInitialize(&up);
            sceUserServiceGetInitialUser(&uid);
        }
        rawPad = scePadGetHandle(uid, 0, 0);
    }
    loadLanguagePreference(); // الإنجليزية هي الافتراضية عند عدم وجود ملف تفضيل
    bool rawStickOn = false;
    SDL_Window* w = SDL_CreateWindow("GoldHEN Plugin Manager", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SW, SH, 0);
    if (!w) { if (pad) SDL_JoystickClose(pad); SDL_Quit(); return 2; }
    SDL_Surface* ws = SDL_GetWindowSurface(w);
    if (!ws) { SDL_DestroyWindow(w); if (pad) SDL_JoystickClose(pad); SDL_Quit(); return 3; }
    SDL_Renderer* r = SDL_CreateSoftwareRenderer(ws);
    if (!r) { SDL_DestroyWindow(w); if (pad) SDL_JoystickClose(pad); SDL_Quit(); return 4; }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    gs.mx = SDL_CreateMutex();

    // شاشة أولية فورية
    SDL_SetRenderDrawColor(r, C_BG.r, C_BG.g, C_BG.b, 255);
    SDL_RenderClear(r);
    SDL_RenderPresent(r);
    SDL_UpdateWindowSurface(w);
    sceSystemServiceHideSplashScreen();   // first frame is on screen: drop the pic1 splash

    // ---- Fonts: S=20, L=30, XL=40 ----
    const char* AR = "/app0/assets/fonts/NotoSansArabic-Regular.ttf";
    const char* FB = "/app0/assets/fonts/Gontserrat-Regular.ttf";
    bool ft = sceSysmoduleLoadModule(ORBIS_SYSMODULE_FREETYPE_OL) >= 0;
    ArabicText fonts[5];
    const int sizes[5] = {24, 32, 42, 72, 36};   // index 4 (34 px): plugin names in the store
    bool hasFont = ft;
    for (int i = 0; i < 5 && hasFont; i++) hasFont = fonts[i].initialize(AR, sizes[i], FB);


    // ---- Text cache (نصوص تُبنى عند الحاجة فقط) ----
    struct TextEnt { SDL_Texture* t; Uint32 used; };
    std::map<std::string, TextEnt> textCache;
    Uint32 frameNo = 0;
    // ---- كاش الأشكال المدوّرة: الشكل (عرض × ارتفاع × نصف القطر) يُرسم مرة كصورة قناع أبيض، ثم يُنسخ بلونه وشفافيته ----
    struct ShapeEnt { SDL_Texture* t; Uint32 used; int hits; };   // t = null حتى يتكرر الشكل مرتين
    std::map<std::tuple<int, int, int>, ShapeEnt> shapeCache;
    std::map<int, SDL_Texture*> sliceCache;   // قناع صغير لكل نصف قطر، يُمدّ للأشكال الكبيرة
    int shapeNew = 0;   // عدد الصور الجديدة المبنية في هذا الإطار (حد أقصى حتى لا يتثقّل الإطار)
    auto playMoveSound = [&]() {
        if (!moveAudio || !moveAudioData || !moveAudioLength) return;
        SDL_ClearQueuedAudio(moveAudio);
        SDL_QueueAudio(moveAudio, moveAudioData, moveAudioLength);
    };
    auto playClickSound = [&]() {
        if (!moveAudio || !clickAudioData || !clickAudioLength) return;
        SDL_ClearQueuedAudio(moveAudio);
        SDL_QueueAudio(moveAudio, clickAudioData, clickAudioLength);
    };
    auto playSuccessSound = [&]() {
        if (!moveAudio || !successAudioData || !successAudioLength) return;
        SDL_ClearQueuedAudio(moveAudio);
        SDL_QueueAudio(moveAudio, successAudioData, successAudioLength);
    };
    auto playErrorSound = [&]() {
        if (!moveAudio || !errorAudioData || !errorAudioLength) return;
        SDL_ClearQueuedAudio(moveAudio);
        SDL_QueueAudio(moveAudio, errorAudioData, errorAudioLength);
    };
    auto getText = [&](const std::string& s, SDL_Color c, int size) -> SDL_Texture* {
        if (!hasFont || s.empty()) return nullptr;
        std::string key;
        key.push_back((char)('0' + size));
        key.push_back((char)c.r); key.push_back((char)c.g); key.push_back((char)c.b);
        key += s;
        auto it = textCache.find(key);
        if (it != textCache.end()) { it->second.used = frameNo; return it->second.t; }
        SDL_Texture* t = fonts[size].texture(r, s, c);
        
        textCache[key] = TextEnt{t, frameNo};
        return t;
    };
    // يرسم نصاً داخل صندوق ثابت: يُصغَّر إن كان أكبر منه ولا يتجاوزه أبداً.
    // align: 0 يسار، 1 يمين، 2 وسط
    auto textBox = [&](const std::string& s, SDL_Color c, int size, int bx, int by, int bw, int bh, int align) {
        SDL_Texture* t = getText(tr(s), c, size);
        if (!t) return;
        int tw = 0, th = 0;
        SDL_QueryTexture(t, nullptr, nullptr, &tw, &th);
        if (tw <= 0 || th <= 0) return;
        float z = std::min(1.0f, std::min(bw / (float)tw, bh / (float)th));
        int dw = (int)(tw * z), dh = (int)(th * z);
        int dx = align == 0 ? bx : align == 1 ? bx + bw - dw : bx + (bw - dw) / 2;
        SDL_Rect d{dx, by + (bh - dh) / 2, dw, dh};
        SDL_RenderCopy(r, t, nullptr, &d);
    };
    // نص بحجمه الطبيعي (لا يُصغَّر رأسياً): كل الأسطر من نفس الحجم تماماً، ويُصغَّر فقط إن تجاوز العرض. cy = منتصف السطر.
    auto textNat = [&](const std::string& s, SDL_Color c, int size, int bx, int cy, int bw, int align) {
        SDL_Texture* t = getText(tr(s), c, size);
        if (!t) return;
        int tw = 0, th = 0;
        SDL_QueryTexture(t, nullptr, nullptr, &tw, &th);
        if (tw <= 0 || th <= 0) return;
        float z = std::min(1.0f, bw / (float)tw);
        int dw = (int)(tw * z), dh = (int)(th * z);
        int dx = align == 0 ? bx : align == 1 ? bx + bw - dw : bx + (bw - dw) / 2;
        SDL_Rect d{dx, cy - dh / 2, dw, dh};
        SDL_RenderCopy(r, t, nullptr, &d);
    };
    // تقطيع نص إلى أسطر بعرض أقصى ثابت (بحجم الخط الطبيعي) مع "..." عند القص؛ النتيجة تُخزَّن فلا تُعاد كل إطار.
    auto wrapText = [&](const std::string& text, int size, int maxW, size_t maxLines) -> std::vector<std::string> {
        static std::map<std::string, std::vector<std::string>> wrapCache;
        const std::string key = std::to_string(size) + "|" + std::to_string(maxW) + "|" + std::to_string(maxLines) + "|" + std::to_string(gLanguage) + "|" + text;
        auto hit = wrapCache.find(key);
        if (hit != wrapCache.end()) return hit->second;
        auto widthOf = [&](const std::string& s) { SDL_Texture* t = getText(tr(s), C_TEXT, size); int tw = 0, th = 0; if (t) SDL_QueryTexture(t, nullptr, nullptr, &tw, &th); return tw; };
        std::vector<std::string> lines; std::string line, word;
        auto flushWord = [&]() {
            if (word.empty()) return;
            std::string cand = line.empty() ? word : line + " " + word;
            if (!line.empty() && widthOf(cand) > maxW) { lines.push_back(line); line = word; } else line = cand;
            word.clear();
        };
        for (unsigned char ch : text) {
            if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') { flushWord(); if (ch == '\n' && !line.empty()) { lines.push_back(line); line.clear(); } }
            else word.push_back((char)ch);
        }
        flushWord(); if (!line.empty()) lines.push_back(line);
        auto ellipsize = [&](std::string& l) {      // قص آمن لـ UTF-8 ثم "..."
            while (!l.empty() && widthOf(l + "...") > maxW) { size_t n = l.size() - 1; while (n > 0 && (l[n] & 0xC0) == 0x80) --n; l.resize(n); }
            l += "...";
        };
        const bool overflow = lines.size() > maxLines;
        if (overflow) lines.resize(maxLines);
        if (overflow && !lines.empty()) ellipsize(lines.back());
        for (size_t li = 0; li < lines.size(); li++)     // كلمة واحدة أطول من العرض (اسم ملف مثلاً): تُقصّ بدل أن تُصغَّر
            if (!(overflow && li + 1 == lines.size()) && widthOf(lines[li]) > maxW) ellipsize(lines[li]);
        if (wrapCache.size() > 64) wrapCache.clear();
        wrapCache[key] = lines;
        return lines;
    };
    // نص كامل بأسطر متعددة داخل صندوق: يلتف على العرض، ويُصغَّر الكتلة كلها إن لم تكفي الارتفاع (لا قص ولا نقاط)
    auto textFit = [&](const std::string& text, SDL_Color c, int bx, int by, int bw, int bh, int align) {
        if (!hasFont || text.empty() || bw <= 0 || bh <= 0) return;
        // نجرب الحجم العادي أولاً، ثم الأصغر؛ ونختار الأكبر حجماً الذي يتسع ارتفاعه (أو الأقل تصغيراً)
        std::vector<SDL_Texture*> tex;
        std::vector<std::pair<int, int>> dims;
        float bestZ = -1.0f;
        int bestTotal = 0;
        for (int size : {1, 0}) {
            std::vector<SDL_Texture*> tt;
            std::vector<std::pair<int, int>> dd;
            int total = 0;
            for (const std::string& ln : wrapText(tr(text), size, bw, 1000)) {
                SDL_Texture* t = getText(ln, c, size);
                if (!t) continue;
                int tw = 0, th = 0;
                SDL_QueryTexture(t, nullptr, nullptr, &tw, &th);
                tt.push_back(t); dd.push_back({tw, th});
                total += th;
            }
            if (tt.empty() || total <= 0) continue;
            const float zz = std::min(1.0f, bh / (float)total);
            if (zz > bestZ + 0.001f) { bestZ = zz; tex = tt; dims = dd; bestTotal = total; }
        }
        if (tex.empty() || bestTotal <= 0) return;
        const float z = bestZ;
        const int totalH = bestTotal;
        float y = by + (bh - totalH * z) / 2.0f;
        for (size_t i = 0; i < tex.size(); i++) {
            const int dw = (int)(dims[i].first * z), dh = (int)(dims[i].second * z);
            const int dx = align == 0 ? bx : align == 1 ? bx + bw - dw : bx + (bw - dw) / 2;
            SDL_Rect d{dx, (int)y, dw, dh};
            SDL_RenderCopy(r, tex[i], nullptr, &d);
            y += dh;
        }
    };
    auto rect = [&](int x, int y, int ww, int hh, SDL_Color c) {
        SDL_Rect q{x, y, ww, hh};
        SDL_SetRenderDrawBlendMode(r, c.a == 255 ? SDL_BLENDMODE_NONE : SDL_BLENDMODE_BLEND);   // التعبئة المعتمة أسرع
        SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
        SDL_RenderFillRect(r, &q);
    };
    // مستطيل بزوايا مدوّرة مع حافة ناعمة (anti-aliasing) — عدد قليل من الاستدعاءات لكل شكل
    // الرسم المباشر (يُستعمل للأشكال الكبيرة، ولأول مرة يظهر فيها الشكل، وعند امتلاء حد البناء في الإطار)
    auto roundRectDirect = [&](int x, int y, int ww, int hh, int rad, SDL_Color c) {
        
        if (rad < 1 || ww < 2 * rad || hh < 2 * rad) { rect(x, y, ww, hh, c); return; }
        rect(x, y + rad, ww, hh - 2 * rad, c);
        for (int i = 0; i < rad; i++) {
            float dy = rad - i - 0.5f;
            float inset = rad - std::sqrt((float)rad * rad - dy * dy);
            int full = (int)std::ceil(inset);
            float frac = full - inset;
            rect(x + full, y + i, ww - 2 * full, 1, c);
            rect(x + full, y + hh - 1 - i, ww - 2 * full, 1, c);
            if (full > 0 && frac > 0.03f) {
                SDL_Color p = c; p.a = (Uint8)(c.a * frac);
                rect(x + full - 1, y + i, 1, 1, p);            rect(x + ww - full, y + i, 1, 1, p);
                rect(x + full - 1, y + hh - 1 - i, 1, 1, p);   rect(x + ww - full, y + hh - 1 - i, 1, 1, p);
            }
        }
    };
    // مستطيل بزوايا مدوّرة مع حافة ناعمة. الشكل المتكرر يُبنى مرة كصورة (على المعالج، بدون تبديل الهدف) ثم يُنسخ بلونه
    auto roundRectSliced = [&](int x, int y, int ww, int hh, int rad, SDL_Color c) -> bool {
        const int S = 2 * rad + 2;   // قناع صغير: الأركان rad والمركز مفتوح (وسط ثابت)
        SDL_Texture*& t = sliceCache[rad];
        if (!t) {
            SDL_Surface* sf = makeRoundMask(S, S, rad);
            if (!sf) return false;
            t = SDL_CreateTextureFromSurface(r, sf);
            SDL_FreeSurface(sf);
            if (!t) return false;
            SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
        }
        SDL_SetTextureColorMod(t, c.r, c.g, c.b);
        SDL_SetTextureAlphaMod(t, c.a);
        const int sxs[3] = {0, rad, rad + 2}, sws[3] = {rad, 2, rad};
        const int dxs[3] = {x, x + rad, x + ww - rad}, dws[3] = {rad, ww - 2 * rad, rad};
        const int sys[3] = {0, rad, rad + 2}, sys_h[3] = {rad, 2, rad};
        const int dys[3] = {y, y + rad, y + hh - rad}, dys_h[3] = {rad, hh - 2 * rad, rad};
        for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) {
            if (dws[i] <= 0 || dys_h[j] <= 0) continue;
            SDL_Rect src{sxs[i], sys[j], sws[i], sys_h[j]};
            SDL_Rect dst{dxs[i], dys[j], dws[i], dys_h[j]};
            SDL_RenderCopy(r, t, &src, &dst);
        }
        return true;
    };
    auto roundRect = [&](int x, int y, int ww, int hh, int rad, SDL_Color c) {
        if (!USE_SHAPE_CACHE || rad < 1 || ww < 2 * rad || hh < 2 * rad || ww * hh > 360000) { roundRectDirect(x, y, ww, hh, rad, c); return; }
        const auto key = std::make_tuple(ww, hh, rad);
        auto it = shapeCache.find(key);
        if (it == shapeCache.end()) { shapeCache.emplace(key, ShapeEnt{nullptr, frameNo, 1}); roundRectDirect(x, y, ww, hh, rad, c); return; }
        ShapeEnt& se = it->second;
        se.used = frameNo;
        if (!se.t) {
            se.hits++;
            if (se.hits < 2 || shapeNew >= 12) { roundRectDirect(x, y, ww, hh, rad, c); return; }
            ++shapeNew;
            SDL_Surface* sf = makeRoundMask(ww, hh, rad);   // قناع أبيض، والشفافية = مقدار التغطية
            if (sf) { se.t = SDL_CreateTextureFromSurface(r, sf); SDL_FreeSurface(sf); }
            if (!se.t) { roundRectDirect(x, y, ww, hh, rad, c); return; }
            SDL_SetTextureBlendMode(se.t, SDL_BLENDMODE_BLEND);
        }
        SDL_SetTextureColorMod(se.t, c.r, c.g, c.b);
        SDL_SetTextureAlphaMod(se.t, c.a);
        SDL_Rect d{x, y, ww, hh};
        SDL_RenderCopy(r, se.t, nullptr, &d);
    };
    auto spinner = [&](int cx, int cy, int radius) {
        int phase = (int)((SDL_GetTicks() / 80) % 12);
        for (int k = 0; k < 12; k++) {
            double ang = k * 3.14159265 / 6.0;
            int age = ((phase - k) % 12 + 12) % 12;
            Uint8 a = (Uint8)(255 - age * 18);
            rect(cx + (int)(std::cos(ang) * radius) - 7, cy + (int)(std::sin(ang) * radius) - 7, 14, 14, SDL_Color{255, 72, 125, a});
        }
    };

    // ---- State ----
    std::vector<Game> games = {{"default", "كل الألعاب", "/app0/assets/images/default.png"}};
    std::vector<SDL_Texture*> iconTex(1, nullptr);
    std::vector<char> iconTried(1, 0);
    std::vector<Plugin> plugins;
    Config cfg, savedCfg;   // savedCfg = what plugins.ini currently holds, so "unsaved changes" means a real difference
    cfg.sections.push_back({"default", {}});
    bool tookPlugins = false, scanDone = false;
    int seenVersion = -1, roots = 0, entries = 0;
    bool ftpConnected = false;
    int ftpState = 0, ftpTried = 0, ftpGot = 0, ftpBytes = 0;
    std::string note1, note2, note3;
    std::string stage;
    SDL_Thread* scanThread = nullptr;
    Uint32 scanStart = 0;

    int selectedGame = 0, selectedPlugin = 0, gameScroll = 0, pluginScroll = 0;
bool storeInfoReady = false;   // يصبح true بعد استقرار الاختيار؛ قبلها تظهر رسالة جلب معلومات الإضافة
float plugVisScroll = 0.0f, plugVisSel = 0.0f;   // مواضع العرض المتحركة لقائمة الإضافات
float storeVisScroll = 0.0f, storeVisSel = 0.0f;   // مواضع العرض المتحركة لقائمة المتجر
float gameVisScroll = 0.0f, gameVisSel = 0.0f;   // مواضع العرض المتحركة لقائمة الألعاب
    bool inPlugins = false, inStore = false, running = true, dirty = false;
    PluginStore::Client store; store.initialize();
    int storeSelected = 0, storeScroll = 0; PluginStore::Phase lastStorePhase = PluginStore::Idle;
    PluginStore::Snapshot storeState;
    SDL_Texture* supportQrTexture = nullptr; std::string supportQrUrl;   // QR currently shown (not owned)
    std::map<std::string, SDL_Texture*> qrMem;                          // QR textures kept in memory only; QR images are never written to the console
    bool languageMenu = false, optionsWasDown = false, r3WasDown = false;
    bool confirmIniReset = false; int iniResetSel = 0; bool iniDlgRestore = false;   // نافذة plugins.ini: إعادة ضبط (false) أو استعادة النسخة (true)   // نافذة إعادة ضبط plugins.ini (0 = لا، 1 = موافق)
    static const int INI_ROWS = 26;   // عدد أسطر الصفحة الظاهرة في عارض plugins.ini
    bool iniView = false; int iniScroll = 0; std::vector<std::string> iniLines; std::string iniStatus;   // عارض plugins.ini (يعرض الملف كما هو)
    bool settingsMenu = false; int settingsSel = 0;   // قائمة الإعدادات (زر OPTIONS): 0 اللغة، 1 إصلاح plugins.ini، 2 عن التطبيق
    bool aboutPage = false; SDL_Texture* aboutQr = nullptr;   // "About" page, opened/closed with R3
    bool languageRepeat = false;   // true only for auto-repeat steps while a direction is held (no wrap-around then)
    bool searchEdit = false; char16_t searchBuf[65] = {0}; Uint32 searchOpenedAt = 0; std::string storeQuery;   // لوحة البحث في المتجر
    bool tokenEdit = false;   // true while the system keyboard (IME dialog) is open for the token
    // ---- تخزين الشاشة الثابتة: نرسم الشاشة كاملة فقط عند تغيّر شي، والبانر المتحرك يُرسم فوقها كل إطار ----
    bool sceneDirty = true;
    Uint32 sceneAt = 0;
    bool lerpBusy = false;   // true إذا لم تكتمل حركة التمرير/التحديد للشاشة المعروضة في الإطار الأخير
    size_t lastIconCount = 0;
    struct DynBanner { bool on; bool rtl; int icX, icY, IC, pX, pY, PW, PH, pi; };
    DynBanner dynBanner{false, false, 0, 0, 0, 0, 0, 0, 0, 0};
    char16_t imeBuf[TOKEN_MAX + 1] = {0}; Uint32 imeOpenedAt = 0;
    std::string tokenBuf, tokenPathCur, tokenKeyCur, tokenName;
    int languageCursor = gLanguage;
    auto moveLanguage = [&](int d) {
        int previous = languageCursor;
        int target = languageCursor + d;
        if (!languageRepeat) {   // a fresh press wraps around: Up on the first language goes to the last one and vice versa
            if (target < 0) target = 5;
            else if (target > 5) target = 0;
        }
        languageCursor = std::max(0, std::min(5, target));
        if (languageCursor != previous) playMoveSound();
    };
    bool confirmReplace = false; int replaceSel = 0;   // replaceSel: 0 = cancel, 1 = install .prx only (no ZIP), 2 = replace everything
    //   // store: plugin already installed -> ask before replacing it
    bool confirmDel = false; int confirmSel = 0;   // نافذة تأكيد الحذف: 0 = إلغاء (الافتراضي)، 1 = حذف
    std::string notice = "اختر لعبة ثم عدّل الإضافات";
    Uint32 noticeAt = SDL_GetTicks(), lastAxis = 0;
    auto setNotice = [&](const std::string& s) { notice = s; noticeAt = SDL_GetTicks(); };
    std::string shownRoot;

    // ---- حالة إصلاح plugins.ini المعروضة على الشاشة: تُحسب مرة بعد كل فحص (لا في كل إطار) ----
    int repairIssues = 0;      // عدد التغييرات التي سيجريها الإصلاح على الملف الحالي
    bool repairStale = true;   // true = يلزم إعادة الحساب
    auto startScan = [&]() {
        {
            Lock l;
            gs.games.clear(); gs.plugins.clear(); gs.config = Config();
            gs.pluginsReady = false; gs.readableRoots = 0; gs.entriesSeen = 0; gs.ftpConnected = false; gs.ftpState = 0; gs.ftpTried = 0; gs.ftpGot = 0; gs.ftpBytes = 0; gs.ftpNote1.clear(); gs.ftpNote2.clear(); gs.ftpNote3.clear(); gs.rootNote.clear();
            gs.stage = "plugins";
        }
        SDL_AtomicSet(&gDone, 0);
        SDL_AtomicSet(&gAbort, 0);
        scanDone = false;
        tookPlugins = false;
        repairStale = true;
        scanStart = SDL_GetTicks();
        scanThread = SDL_CreateThread(scanThreadMain, "startup-scan", nullptr);
        if (!scanThread) { scanDone = true; setNotice("تعذر بدء الفحص"); }
    };

    // نسخ نتائج الفحص إلى الواجهة (يحافظ على اللعبة المحددة والأيقونات المحمّلة)
    auto syncScan = [&]() {
        std::vector<Game> nv;
        nv.push_back({"default", "كل الألعاب", "/app0/assets/images/default.png"});
        {
            Lock l;
            nv.insert(nv.end(), gs.games.begin(), gs.games.end());
            if (gs.pluginsReady && !tookPlugins) { plugins = gs.plugins; cfg = gs.config; savedCfg = cfg; tookPlugins = true; }
            if (gs.rootNote != shownRoot && !gs.rootNote.empty()) { shownRoot = gs.rootNote; setNotice(shownRoot); }
            roots = gs.readableRoots; entries = gs.entriesSeen; ftpConnected = gs.ftpConnected; ftpState = gs.ftpState; ftpTried = gs.ftpTried; ftpGot = gs.ftpGot; ftpBytes = gs.ftpBytes; note1 = gs.ftpNote1; note2 = gs.ftpNote2; note3 = gs.ftpNote3; stage = gs.stage;
        }
        std::string selId = games[std::min(selectedGame, (int)games.size() - 1)].id;
        std::map<std::string, size_t> oldIdx;
        for (size_t i = 0; i < games.size(); i++) oldIdx[games[i].id] = i;
        std::vector<SDL_Texture*> ni(nv.size(), nullptr);
        std::vector<char> nt(nv.size(), 0);
        for (size_t i = 0; i < nv.size(); i++) {
            auto it = oldIdx.find(nv[i].id);
            if (it != oldIdx.end() && games[it->second].iconPath == nv[i].iconPath) {
                ni[i] = iconTex[it->second]; nt[i] = iconTried[it->second];
                iconTex[it->second] = nullptr;
            }
        }
        for (auto* t : iconTex) if (t) SDL_DestroyTexture(t);
        games.swap(nv); iconTex.swap(ni); iconTried.swap(nt);
        selectedGame = 0;
        for (size_t i = 0; i < games.size(); i++) if (games[i].id == selId) { selectedGame = (int)i; break; }
        if (selectedGame < gameScroll) gameScroll = selectedGame;
        if (selectedGame >= gameScroll + ROWS_G) gameScroll = selectedGame - ROWS_G + 1;
        gameScroll = std::max(0, std::min(gameScroll, std::max(0, (int)games.size() - ROWS_G)));
        if (selectedPlugin >= (int)plugins.size()) selectedPlugin = std::max(0, (int)plugins.size() - 1);
    };

    auto sectionName = [&]() { return games[selectedGame].id; };
    auto currentEnabled = [&](int i) { return effectiveState(cfg, sectionName(), plugins[i].path); };
    auto countEnabled = [&](const std::string& sec) {
        int n = 0;
        for (const auto& p : plugins) if (effectiveState(cfg, sec, p.path)) ++n;
        return n;
    };
    auto moveGame = [&](int d) {
        int previous = selectedGame;
        selectedGame = std::max(0, std::min((int)games.size() - 1, selectedGame + d));
        if (selectedGame != previous) playMoveSound();
        if (selectedGame < gameScroll) gameScroll = selectedGame;
        if (selectedGame >= gameScroll + ROWS_G) gameScroll = selectedGame - ROWS_G + 1;
    };
    auto movePlugin = [&](int d) {
        if (plugins.empty()) return;
        int previous = selectedPlugin;
        selectedPlugin = std::max(0, std::min((int)plugins.size() - 1, selectedPlugin + d));
        if (selectedPlugin != previous) playMoveSound();
        if (selectedPlugin < pluginScroll) pluginScroll = selectedPlugin;
        if (selectedPlugin >= pluginScroll + ROWS_P) pluginScroll = selectedPlugin - ROWS_P + 1;
    };
    auto moveStore = [&](int d) {
        if (storeState.entries.empty()) return;
        int old = storeSelected;
        storeSelected = std::max(0, std::min((int)storeState.entries.size()-1, storeSelected+d));
        if (old != storeSelected) playMoveSound();
        if (storeSelected < storeScroll) storeScroll=storeSelected;
        if (storeSelected >= storeScroll+STORE_ROWS) storeScroll=storeSelected-(STORE_ROWS-1);
    };
    auto openStore = [&]() {
        if (!scanDone || !tookPlugins) { setNotice("Wait for the initial scan to finish"); return; }
        if (dirty) { setNotice("Save changes first"); return; }
        inStore=true; inPlugins=false; storeSelected=storeScroll=0; playClickSound();
        storeState=store.snapshot();
        // First time the store is opened (or after a failed check): check the internet connection before showing the store UI.
#if STORE_CHECK_ON_OPEN
        if (!storeState.catalogLoaded && storeState.phase!=PluginStore::Downloading && storeState.phase!=PluginStore::Verifying && storeState.phase!=PluginStore::LoadingCatalog && storeState.phase!=PluginStore::LoadingQr) {
            store.refreshCatalog(true); storeState=store.snapshot();
        }
#endif
        if (!storeState.catalogLoaded && storeState.phase!=PluginStore::LoadingCatalog) store.refreshCatalog();
    };
    auto storePaths = [&](const PluginStore::Entry& e) {
        const std::string dir = PLUGIN_DIR;
        std::vector<std::string> paths{dir + "/" + e.filename};
        if (!e.url2download.empty()) paths.push_back(dir + "/" + e.filename2);
        return paths;
    };
    auto installStoreEntry = [&]() {
        if (dirty) { setNotice("Save changes first"); return; }
        if (storeSelected<0 || storeSelected >= (int)storeState.entries.size()) return;
        if (PluginStore::Rpi::progress().active) { playErrorSound(); setNotice(languageText(kWaitAnAppIsBeing)); return; }   // لا تنزيل إضافة أثناء تثبيت تطبيق
        const auto& e=storeState.entries[storeSelected];
        if (mkdir(PLUGIN_DIR, 0777) != 0 && errno != EEXIST) { playErrorSound(); setNotice("Could not create plugins directory"); return; }
        const bool alreadyThere = PluginStore::Client::exists(e, PLUGIN_DIR);
#if STORE_CONFIRM_REPLACE
        if (alreadyThere) { confirmReplace = true; replaceSel = 0; playClickSound(); return; }   // already installed: ask first
#endif
        if(!store.download(e,PLUGIN_DIR,alreadyThere)){playErrorSound();setNotice("Could not start download");return;}
        setNotice(e.packageUrl.empty()?"Download started; plugin will be disabled":"Downloading ZIP package; it will be installed disabled");
    };
    auto reload = [&]() {
        if (dirty) { setNotice("احفظ التغييرات أولاً؛ إعادة الفحص ستلغي التعديلات"); return; }
        if (!scanDone) { setNotice("الفحص جارٍ بالفعل"); return; }
        setNotice("بدأت إعادة الفحص");
        startScan();
    };
    auto doDelete = [&]() {
        if (selectedPlugin < 0 || selectedPlugin >= (int)plugins.size()) return;
        Plugin p = plugins[selectedPlugin];
        if (unlink(p.path.c_str()) != 0) { playErrorSound(); setNotice("Could not delete plugin: " + p.name); return; }
        removePluginFromConfig(cfg, p.path); plugins.erase(plugins.begin() + selectedPlugin);
        if (selectedPlugin >= (int)plugins.size()) selectedPlugin = std::max(0, (int)plugins.size() - 1);
        pluginScroll = std::max(0, std::min(pluginScroll, std::max(0, (int)plugins.size() - ROWS_P)));
        if (pluginScroll > selectedPlugin) pluginScroll = selectedPlugin;
        if (!dirty) {
            if (saveConfig(cfg)) { savedCfg = cfg; playSuccessSound(); setNotice("Plugin deleted and plugins.ini updated"); }
            else { playErrorSound(); dirty = true; setNotice("Plugin deleted; save plugins.ini"); }
        } else { playSuccessSound(); setNotice("Plugin deleted; save changes"); }
    };
    // ---- إصلاح plugins.ini: زر L2 من شاشة الألعاب ثم نافذة (موافق / لا) ----
    bool confirmRepair = false; int repairSel = 0;   // repairSel: 0 = لا (الافتراضي)، 1 = موافق
    bool pkgConfirm = false; int pkgSel = 0; bool pkgPending = false;   // نافذة تثبيت PKG عبر Remote Package Installer (pkgSel: 0 = لا، 1 = موافق)
    PluginIniRepair::Report repairPreview;            // ما سيتغير، يُعرض في النافذة قبل التأكيد
    bool l2WasDown = false;
    // يقرأ plugins.ini ويحسب الإصلاح دون كتابة أي شيء. false = تعذّرت القراءة
    auto buildRepair = [&](std::string& original, std::string& fixed, PluginIniRepair::Report& rep) -> bool {
        std::vector<unsigned char> bytes;
        if (!readFileCapped(CONFIG_PATH, 4u * 1024u * 1024u, bytes)) return false;
        original.assign(bytes.begin(), bytes.end());
        PluginIniRepair::Options o;
        o.pluginDir = PLUGIN_DIR;
        for (const auto& p : scanPlugins()) o.pluginFiles.push_back(p.name);
        o.installedKnown = collectInstalledTitleIds(o.installedIds);
        fixed = PluginIniRepair::repair(original, o, rep);
        return true;
    };
    // إرسال رابط PKG إلى Remote Package Installer بعد تأكيد المستخدم فقط (التطبيق لا ينزّل الـ PKG)
    auto sendPkgNow = [&]() {
        if (storeSelected < 0 || storeSelected >= (int)storeState.entries.size()) return;
        const std::string url = storeState.entries[storeSelected].pkgUrl;
        if (url.empty()) return;
        if (storeState.phase == PluginStore::Downloading || storeState.phase == PluginStore::Verifying) { playErrorSound(); setNotice(languageText(kWaitAPluginIsDownloading)); return; }   // لا تثبيت تطبيق أثناء تنزيل إضافة
        if (PluginStore::Rpi::sendPkg(url)) {
            pkgPending = true;
            setNotice(languageText(kStartingInstallation));
        } else { playErrorSound(); setNotice(languageText(kAnInstallIsAlreadyBeing)); }
    };
    auto openIniView = [&]() {
        iniLines.clear(); iniScroll = 0; iniStatus.clear();
        FILE* f = std::fopen(CONFIG_PATH, "rb");
        if (!f) { iniStatus = languageText(kCouldNotReadPluginsIni); }
        else {
            std::string data; char buf[4096]; size_t n = 0;
            while ((n = std::fread(buf, 1, sizeof buf, f)) > 0 && data.size() < 262144) data.append(buf, n);
            std::fclose(f);
            std::string cur;
            for (char c : data) { if (c == '\r') continue; if (c == '\n') { iniLines.push_back(cur); cur.clear(); } else cur.push_back(c); }
            if (!cur.empty()) iniLines.push_back(cur);
            if (iniLines.empty()) iniStatus = languageText(kPluginsIniIsEmpty);
        }
        iniView = true; playClickSound(); sceneDirty = true;
    };
    auto openIniReset = [&]() {
        confirmIniReset = true; iniDlgRestore = false; iniResetSel = 0; playClickSound();
    };
    auto openIniRestore = [&]() {
        FILE* f = std::fopen(CONFIG_BAK_PATH, "rb");
        if (!f) { playErrorSound(); setNotice(languageText(kNoBackupFound)); return; }
        std::fclose(f);
        confirmIniReset = true; iniDlgRestore = true; iniResetSel = 0; playClickSound();
    };
    auto applyIniRestore = [&]() {
        std::string bak, current;
        FILE* f = std::fopen(CONFIG_BAK_PATH, "rb");
        if (f) { char buf[4096]; size_t n = 0; while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) bak.append(buf, n); std::fclose(f); }
        FILE* g = std::fopen(CONFIG_PATH, "rb");
        if (g) { char buf[4096]; size_t n = 0; while ((n = std::fread(buf, 1, sizeof buf, g)) > 0) current.append(buf, n); std::fclose(g); }
        // الملف الحالي يصبح النسخة .bak الجديدة، ثم يُكتب محتوى النسخة القديمة في plugins.ini
        if (!saveConfigText(current, bak)) { playErrorSound(); setNotice(languageText(kCouldNotRestoreBackup)); return; }
        cfg = readConfig(); savedCfg = cfg; dirty = false; repairStale = true;
        playSuccessSound();
        setNotice(languageText(kPluginsIniRestoredFromBackup));
    };
    auto applyIniReset = [&]() {
        std::string original;
        FILE* f = std::fopen(CONFIG_PATH, "rb");
        if (f) { char buf[4096]; size_t n = 0; while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) original.append(buf, n); std::fclose(f); }
        if (!saveConfigText(original, "")) { playErrorSound(); setNotice(languageText(kCouldNotResetPluginsIni)); return; }
        cfg = readConfig(); savedCfg = cfg; dirty = false; repairStale = true;
        playSuccessSound();
        setNotice(languageText(kPluginsIniResetBackupBak));
    };
    auto openRepair = [&]() {
        if (!scanDone || !tookPlugins) { setNotice("انتظر حتى تُقرأ الإضافات"); return; }
        if (dirty) { setNotice("احفظ التغييرات أولاً قبل إصلاح plugins.ini"); return; }
        std::string original, fixed; PluginIniRepair::Report rep;
        if (!buildRepair(original, fixed, rep)) { playErrorSound(); setNotice("تعذر قراءة plugins.ini"); return; }
        if (!rep.anyChange()) { playClickSound(); setNotice("plugins.ini سليم، لا يحتاج إصلاحاً"); return; }
        repairPreview = rep;
        confirmRepair = true; repairSel = 0; playClickSound();
    };
    auto applyRepair = [&]() {
        std::string original, fixed; PluginIniRepair::Report rep;
        if (!buildRepair(original, fixed, rep)) { playErrorSound(); setNotice("تعذر قراءة plugins.ini"); return; }
        if (!rep.anyChange()) { setNotice("plugins.ini سليم، لا يحتاج إصلاحاً"); return; }
        if (!saveConfigText(original, fixed)) { playErrorSound(); setNotice("تعذر حفظ plugins.ini"); return; }
        cfg = readConfig(); savedCfg = cfg; dirty = false;   // الواجهة تعرض الملف المُصلح مباشرة
        repairStale = true;
        playSuccessSound();
        setNotice("تم إصلاح plugins.ini — أُنشئت نسخة احتياطية .bak");
    };
    auto openToken = [&](const PluginStore::Entry& e) {
        tokenName = e.name; tokenPathCur = e.tokenPath; tokenKeyCur = e.tokenKey; tokenBuf.clear();
        readTokenFile(e.tokenPath, e.tokenKey, tokenBuf);   // start from the current token so it can be edited
        static char16_t imeTitle[96];
        static const char16_t imePlaceholder[] = u"Token";
        utf8ToUtf16(tokenBuf, imeBuf, TOKEN_MAX + 1);
        utf8ToUtf16(e.name, imeTitle, 96);
        static bool imeModuleTried = false;
        if (!imeModuleTried) { imeModuleTried = true; sceSysmoduleLoadModule(ORBIS_SYSMODULE_IME_DIALOG); }
        int32_t userNow = 0; sceUserServiceGetInitialUser(&userNow);
        ImeDialogParam p; std::memset(&p, 0, sizeof p);
        p.userId = userNow; p.maxTextLength = (uint32_t)TOKEN_MAX; p.inputTextBuffer = imeBuf;
        p.posx = SW / 2.0f; p.posy = SH / 2.0f; p.horizontalAlignment = 1; p.verticalAlignment = 1;   // centered
        p.placeholder = imePlaceholder; p.title = imeTitle;
        const int rc = sceImeDialogInit(&p, nullptr);
        if (rc < 0) {
            char msg[96]; std::snprintf(msg, sizeof msg, "Could not open the keyboard (error 0x%08X)", (unsigned)rc);
            playErrorSound(); setNotice(msg);
            return;
        }
        tokenEdit = true; imeOpenedAt = SDL_GetTicks(); playClickSound();
    };
    auto openSearch = [&]() {
        static char16_t sTitle[40]; static const char16_t sPlaceholder[] = u"Search";
        utf8ToUtf16(storeQuery, searchBuf, 65);
        utf8ToUtf16(std::string("Search the store"), sTitle, 40);
        static bool sModTried = false;
        if (!sModTried) { sModTried = true; sceSysmoduleLoadModule(ORBIS_SYSMODULE_IME_DIALOG); }
        int32_t sUser = 0; sceUserServiceGetInitialUser(&sUser);
        ImeDialogParam p; std::memset(&p, 0, sizeof p);
        p.userId = sUser; p.maxTextLength = 64u; p.inputTextBuffer = searchBuf;
        p.posx = SW / 2.0f; p.posy = SH / 2.0f; p.horizontalAlignment = 1; p.verticalAlignment = 1;
        p.placeholder = sPlaceholder; p.title = sTitle;
        const int rc = sceImeDialogInit(&p, nullptr);
        if (rc < 0) { playErrorSound(); setNotice("Could not open the search keyboard"); return; }
        searchEdit = true; searchOpenedAt = SDL_GetTicks(); playClickSound();
    };
    auto tokenSave = [&]() {
        if (writeTokenFile(tokenPathCur, tokenKeyCur, tokenBuf)) {
            playSuccessSound(); tokenEdit = false;
            setNotice(languageText(kTokenSaved));
        } else {
            playErrorSound();
            setNotice(languageText(kCouldNotSaveTheToken));
        }
    };
    auto action = [&](int b) {
        sceneDirty = true;   // أي زر يغيّر الشاشة
        if (aboutPage) { if (b == 0 || b == 1) { aboutPage = false; playClickSound(); } return; }   // About page takes all input
        if (iniView) {   // عارض plugins.ini: الأسهم للتمرير، O أو X للرجوع
            if (b == BTN_UP) { const int maxS = std::max(0, (int)iniLines.size() - INI_ROWS); iniScroll = std::max(0, std::min(maxS, iniScroll - 1)); }
            else if (b == BTN_DOWN) { const int maxS = std::max(0, (int)iniLines.size() - INI_ROWS); iniScroll = std::max(0, std::min(maxS, iniScroll + 1)); }
            else if (b == 1 || b == 0) { iniView = false; playClickSound(); sceneDirty = true; }
            return;
        }
        if (confirmIniReset) {   // نافذة إعادة الضبط تستقبل الإدخال وحدها
            if (b == 0) { confirmIniReset = false; if (iniResetSel == 1) { if (iniDlgRestore) applyIniRestore(); else applyIniReset(); } else setNotice(languageText(kResetCancelled)); }
            else if (b == 1) { confirmIniReset = false; setNotice(languageText(kResetCancelled)); }
            else if (b == 13 || b == 14 || b == BTN_LEFT || b == BTN_RIGHT || b == BTN_PAGE_PREV || b == BTN_PAGE_NEXT) { iniResetSel ^= 1; playMoveSound(); }
            return;
        }
        if (confirmRepair) {   // نافذة إصلاح plugins.ini تستقبل الإدخال وحدها
            if (b == 0) { confirmRepair = false; if (repairSel == 1) applyRepair(); else setNotice("تم إلغاء الإصلاح"); }
            else if (b == 1) { confirmRepair = false; setNotice("تم إلغاء الإصلاح"); }
            else if (b == 13 || b == 14 || b == BTN_LEFT || b == BTN_RIGHT || b == BTN_PAGE_PREV || b == BTN_PAGE_NEXT) { repairSel ^= 1; playMoveSound(); }
            return;
        }
        if (settingsMenu) {
            if (b == 0) {
                settingsMenu = false;
                if (settingsSel == 0) { languageMenu = true; languageCursor = gLanguage; playClickSound(); }
                else if (settingsSel == 1) openRepair();
            else if (settingsSel == 3) openIniView();
                else if (settingsSel == 4) openIniReset();
                else if (settingsSel == 5) openIniRestore();
                else { aboutPage = true; playClickSound(); }
            } else if (b == 1) { settingsMenu = false; playClickSound(); }
            return;
        }
        if (languageMenu) {
            if (b == BTN_UP) moveLanguage(-1);
            else if (b == BTN_DOWN) moveLanguage(1);
            else if (b == 0) {
                gLanguage = languageCursor;
                if (saveLanguagePreference(gLanguage)) setNotice(languageText(kLanguageSaved));
                else setNotice(languageText(kCouldNotSaveLanguagePreference));
                languageMenu = false;
                for (auto& entry : textCache) if (entry.second.t) SDL_DestroyTexture(entry.second.t);
                textCache.clear();
            } else if (b == 1) { languageMenu = false; setNotice(languageText(kLanguageUnchanged)); }
            return;
        }
        if (tokenEdit) return;   // the system keyboard has the controller: ignore app input
        if (pkgConfirm) {   // نافذة تثبيت PKG تستقبل الإدخال وحدها
            if (b == 0) { pkgConfirm = false; if (pkgSel == 1) sendPkgNow(); else setNotice(languageText(kCanceled)); }
            else if (b == 1) { pkgConfirm = false; setNotice(languageText(kCanceled)); }
            else if (b == 13 || b == 14 || b == BTN_LEFT || b == BTN_RIGHT || b == BTN_PAGE_PREV || b == BTN_PAGE_NEXT) { pkgSel ^= 1; playMoveSound(); }
            return;
        }
        if (confirmDel) {   // نافذة التأكيد تستقبل الإدخال وحدها
            if (b == 0) { confirmDel = false; if (confirmSel == 1) doDelete(); else setNotice("تم إلغاء الحذف"); }
            else if (b == 1) { confirmDel = false; setNotice("تم إلغاء الحذف"); }
            else if (b == 13 || b == 14 || b == BTN_LEFT || b == BTN_RIGHT || b == BTN_PAGE_PREV || b == BTN_PAGE_NEXT) { confirmSel ^= 1; playMoveSound(); }
            return;
        }
        if (inStore && (confirmReplace || storeState.phase == PluginStore::Confirm)) {   // replace-confirmation dialog takes all input
            const bool midDownload = storeState.phase == PluginStore::Confirm;   // the ZIP is already downloaded; the worker waits for our answer
            const bool pkgEntry = midDownload || (storeSelected >= 0 && storeSelected < (int)storeState.entries.size() && !storeState.entries[storeSelected].packageUrl.empty());
            if (midDownload && (b == 0 || b == 2 || b == 1)) {
                const int chosen = (b == 2) ? 1 : (b == 1 ? 0 : replaceSel);   // 0 cancel, 1 .prx only, 2 replace all
                store.answerConfirm(chosen == 2 ? 1 : (chosen == 1 ? 2 : 3));
                confirmReplace = false;
                setNotice(chosen == 2 ? languageText(kReplacingTheOldVersion)
                         : chosen == 1 ? languageText(kInstallingThePrxOnlyNo)
                         : languageText(kCanceled));
            }
            else if (b == 0 || (b == 2 && pkgEntry)) {   // X = confirm the focused button, Square = ".prx only" shortcut
                confirmReplace = false;
                const int chosen = (b == 2) ? 1 : replaceSel;
                if (chosen != 0 && storeSelected >= 0 && storeSelected < (int)storeState.entries.size()) {
                    PluginStore::Entry e = storeState.entries[storeSelected];
                    const bool prxOnly = (chosen == 1 && pkgEntry);
                    if (prxOnly) e.packageUrl.clear();   // install only the .prx: no ZIP download, extraction or folder deletion
                    if (PluginStore::Rpi::progress().active) { playErrorSound(); setNotice(languageText(kWaitAnAppIsBeing)); }
                    else if (!store.download(e, PLUGIN_DIR, true)) { playErrorSound(); setNotice("Could not start download"); }
                    else setNotice(prxOnly ? languageText(kInstallingThePrxOnlyNo)
                                   : languageText(kReplacingTheOldVersion));
                } else setNotice(languageText(kCanceled));
            }
            else if (b == 1) { confirmReplace = false; setNotice(languageText(kCanceled)); }
            else if (b == BTN_LEFT || b == BTN_PAGE_PREV || b == BTN_UP) {      // move selection to the left
                int ns = replaceSel - 1; if (ns == 1 && !pkgEntry) ns = 0; if (ns < 0) ns = 0;
                if (ns != replaceSel) { replaceSel = ns; playMoveSound(); }
            }
            else if (b == BTN_RIGHT || b == BTN_PAGE_NEXT || b == BTN_DOWN) {   // move selection to the right
                int ns = replaceSel + 1; if (ns == 1 && !pkgEntry) ns = 2; if (ns > 2) ns = 2;
                if (ns != replaceSel) { replaceSel = ns; playMoveSound(); }
            }
            return;
        }
        if (inStore) {
            PluginStore::Snapshot ss = store.snapshot();
            if (b == BTN_UP) moveStore(-1);
            else if (b == BTN_DOWN) moveStore(1);
            else if (b == 0 && ss.phase == PluginStore::Offline) { store.refreshCatalog(true); setNotice(languageText(kCheckingConnection)); }
            else if (b == 0 && (ss.phase == PluginStore::Ready || ss.phase == PluginStore::Installed
                     || ss.phase == PluginStore::Canceled || ss.phase == PluginStore::Failed
                     || ss.phase == PluginStore::Offline)) installStoreEntry();   // بعد أي نتيجة نهائية يقدر يحمّل مرة ثانية
            else if (b == 1) { inStore = false; setNotice("Back"); }
            else if (BTN_TOUCHPAD >= 0 && b == BTN_TOUCHPAD && !searchEdit) openSearch();
            else if (b == 2 && (ss.phase == PluginStore::Downloading || ss.phase == PluginStore::LoadingCatalog || ss.phase == PluginStore::LoadingQr)) { store.cancel(); setNotice("Cancel requested"); }
            else if (b == 3 && ss.phase != PluginStore::Offline && storeSelected >= 0 && storeSelected < (int)ss.entries.size() && !ss.entries[storeSelected].pkgUrl.empty() && !PluginStore::Rpi::progress().active && !rpiAlreadyInstalled()) { playClickSound(); sendPkgNow(); }
            else if (b == 2 && PluginStore::Rpi::progress().active && PluginStore::Rpi::progress().percent != -1) { PluginStore::Rpi::cancelDownload(); setNotice(languageText(kDownloadCanceled)); }
            else if (b == 2 && ss.phase != PluginStore::Offline && storeSelected >= 0 && storeSelected < (int)ss.entries.size() && ss.entries[storeSelected].tokenEnabled) openToken(ss.entries[storeSelected]);
            else if (b == 3 && ss.phase != PluginStore::Downloading && ss.phase != PluginStore::LoadingCatalog && ss.phase != PluginStore::LoadingQr) { store.refreshCatalog(ss.phase == PluginStore::Offline); setNotice("Refreshing catalog"); }
            else if (b == BTN_LEFT || b == BTN_PAGE_PREV) moveStore(-STORE_ROWS);
            else if (b == BTN_RIGHT || b == BTN_PAGE_NEXT) moveStore(STORE_ROWS);
            return;
        }
        if (b == BTN_PAGE_PREV || b == BTN_LEFT || b == BTN_PAGE_NEXT || b == BTN_RIGHT) {   // صفحة كاملة
            int d = (b == BTN_PAGE_PREV || b == BTN_LEFT) ? -1 : 1;
            if (!inPlugins) moveGame(d * (ROWS_G - 1)); else movePlugin(d * (ROWS_P - 1));
            return;
        }
        if (!inPlugins) {
            if (b == 13) moveGame(-1);
            else if (b == 14) moveGame(1);
            else if (b == 0) {
                if (!tookPlugins) { setNotice("انتظر حتى تُقرأ الإضافات"); return; }
                playClickSound();
                inPlugins = true; selectedPlugin = pluginScroll = 0;
                setNotice(selectedGame == 0 ? "تعديل إعداد كل الألعاب" : "تعديل إضافات اللعبة المحددة");
            }
            else if (b == 2) reload();
            else if (b == 3) openStore();
        } else {
            if (b == 13) movePlugin(-1);
            else if (b == 14) movePlugin(1);
            else if (b == 0 && !plugins.empty()) {
                bool en = !currentEnabled(selectedPlugin);
                setState(cfg, sectionName(), plugins[selectedPlugin].path, en);
                dirty = !configEqual(cfg, savedCfg);   // toggling a plugin back to how it was is not a change
                setNotice(!dirty ? "لا توجد تغييرات" : en ? "تم التفعيل — اضغط مثلث للحفظ" : "تم التعطيل — اضغط مثلث للحفظ");
            }
            else if (b == 3) {
                if (!dirty) { setNotice(languageText(kNoChangesToSave)); }
                else if (saveConfig(cfg)) { savedCfg = cfg; dirty = false; playSuccessSound(); setNotice("plugins.ini saved"); }
                else { playErrorSound(); setNotice("Could not save plugins.ini"); }
            }
            else if (b == 1) { inPlugins = false; setNotice(dirty ? "التغييرات لم تحفظ بعد؛ اضغط مثلث للحفظ" : "اختر لعبة أخرى"); }
            else if (b == 2) {
                if (plugins.empty()) setNotice("لا توجد إضافات للحذف");
                else { confirmDel = true; confirmSel = 0; }
            }
        }
    };

    int loadsLeft = 0;
    bool iconWaiting = false;   // أيقونة مرئية ما زالت لم تُفك: تبقى الشاشة تُعاد رسمها حتى تظهر
    int navDir = 0; bool navStick = false; Uint32 navSince = 0, navNext = 0;
    int stickVal = 0; Uint32 stickEvTick = 0; bool stickArmed = false; int stickLogs = 0;
    // الشكل الافتراضي: مربع أسود ونص [default] أبيض بحجم خط واضح (يتصغر فقط إن لم يتسع)
    auto drawDefaultTile = [&](int x, int y, int w, int h, int fontIdx) {
        roundRect(x, y, w, h, (int)(std::min(w, h) * 0.14f), SDL_Color{0, 0, 0, 255});   // زوايا منحنية
        SDL_Texture* t = getText("[default]", C_WHITE, fontIdx);
        if (!t) return;
        int tw = 0, th = 0;
        SDL_QueryTexture(t, nullptr, nullptr, &tw, &th);
        if (tw <= 0 || th <= 0) return;
        float z = std::min(1.0f, std::min((w - 10) / (float)tw, (h - 10) / (float)th));
        int dw = (int)(tw * z), dh = (int)(th * z);
        SDL_Rect d{x + (w - dw) / 2, y + (h - dh) / 2, dw, dh};
        SDL_RenderCopy(r, t, nullptr, &d);
    };
    auto drawIcon = [&](int idx, int x, int y, int sz) {
        // أيقونات الألعاب الحقيقية فقط (الخانة 0 = كل الألعاب ترسم الشكل الافتراضي دائماً)
        if (idx == 0) { drawDefaultTile(x, y, sz, sz, sz >= 200 ? 3 : 1); return; }
        if (idx > 0 && idx < (int)games.size()) {
            if (!iconTried[idx] && !games[idx].iconPath.empty()) {
                iconWaiting = true;
                if (loadsLeft > 0) {
                    IconDecoded d;
                    if (takeIcon(games[idx].iconPath, d)) {
                        --loadsLeft;
                        iconTried[idx] = 1;
                        if (d.ok) iconTex[idx] = textureFromPixels(r, d.px, d.w, d.h);
                        else gIconErr = d.err;
                    } else if (scanDone) requestIcon(games[idx].iconPath);   // الأيقونات لا تُفكّ أثناء فحص الإضافات
                } else if (scanDone) requestIcon(games[idx].iconPath);   // الأيقونات لا تُفكّ أثناء فحص الإضافات
            }
            if (iconTex[idx]) { SDL_Rect d{x, y, sz, sz}; SDL_RenderCopy(r, iconTex[idx], nullptr, &d); return; }
        }
        roundRect(x, y, sz, sz, (int)(sz * 0.14f), C_PH);   // لعبة ما حمّلت أيقونتها بعد: خانة بزوايا منحنية
    };
    auto scrollBar = [&](int x, int y, int h, int total, int rows, int pos) {
        if (total <= rows) return;
        roundRect(x, y, 8, h, 4, SDL_Color{30, 41, 64, 255});
        int th = std::max(40, h * rows / total);
        int ty = y + (h - th) * pos / std::max(1, total - rows);
        roundRect(x, ty, 8, th, 4, SDL_Color{110, 130, 175, 255});
    };

    SDL_Texture* btnTex[BI_COUNT] = {nullptr};
    for (int i = 0; i < BI_COUNT; i++) btnTex[i] = (i == BI_TOUCH) ? makeTouchpadIcon(r, 44) : makeButtonIcon(r, i, 44);
    SDL_Texture* globeTex = makeGlobeIcon(r, 36);

    // الخلفية والتوهج يُرسمان مرة واحدة في صورتين مخزّنتين، فكل إطار ينسخ الصورة فقط (أسرع بكثير من رسم الدوائر كل مرة)
    const int GLOW_R = 380;
    SDL_Texture* bgTex = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, SW, SH);
    SDL_Texture* glowTex = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, GLOW_R * 2, GLOW_R * 2);
    // صورة الشاشة الكاملة المخزّنة (إن لم يدعم الجهاز الصور الهدف نرسم كالعادة كل إطار)
    SDL_Texture* sceneTex = nullptr;   // بدون تخزين الشاشة: نرسم كل إطار مباشرة مثل النسخة القديمة
    if (kBgGlow && bgTex && glowTex) {
        SDL_SetTextureBlendMode(glowTex, SDL_BLENDMODE_BLEND);
        SDL_SetRenderTarget(r, bgTex);
        const int bands = 27;                                       // 1080 / 40 شرائح
        for (int i = 0; i < bands; i++) {
            const float t = i / (float)(bands - 1);
            rect(0, i * (SH / bands), SW, SH / bands + 1,
                 SDL_Color{(Uint8)(27 - 17 * t), (Uint8)(39 - 25 * t), (Uint8)(64 - 38 * t), 255});
        }
        SDL_SetRenderTarget(r, glowTex);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 0);
        SDL_RenderClear(r);
        const int radii[4] = {360, 270, 180, 100};                  // طبقات دائرية شفافة تكوّن التوهج
        for (int k = 0; k < 4; k++) {
            const int rad = radii[k];
            roundRect(GLOW_R - rad, GLOW_R - rad, rad * 2, rad * 2, rad, SDL_Color{240, 68, 122, (Uint8)(8 + k * 5)});
        }
        SDL_SetRenderTarget(r, nullptr);
    }
    auto cl8 = [](int v) { return (Uint8)std::max(0, std::min(255, v)); };
    // زر حديث: ظل، جسم بتدرّج لوني، لمعة علوية، أيقونة زر اليد، وإطار أبيض عند التحديد
    auto drawButton = [&](int x, int y, int ww, int hh, const std::string& label, SDL_Color base, int icon, bool focused) {
        SDL_Color dark{cl8(base.r - 34), cl8(base.g - 34), cl8(base.b - 34), 255};
        roundRect(x, y + 5, ww, hh, 16, SDL_Color{0, 0, 0, 90});
        if (focused) {   // التركيز: إطار أبيض ينبض مع توهج وردي خفيف حوله
            const float fp = 0.5f;   // لون ثابت للتركيز (الشاشة مخزّنة)
            roundRect(x - 9, y - 9, ww + 18, hh + 18, 26, SDL_Color{240, 68, 122, (Uint8)(30 + 50 * fp)});
            roundRect(x - 4, y - 4, ww + 8, hh + 8, 20, SDL_Color{255, 255, 255, (Uint8)(190 + 65 * fp)});
        }
        roundRect(x, y, ww, hh, 16, dark);
        roundRect(x, y, ww, hh - 5, 16, base);
        roundRect(x + 4, y + 4, ww - 8, (hh - 8) / 2, 12, SDL_Color{255, 255, 255, 26});
        int lx = x + 16, lw = ww - 32;
        if (icon >= 0 && btnTex[icon]) {
            int D = std::min(40, hh - 16);
            SDL_Rect ir{x + ww - 14 - D, y + (hh - 5 - D) / 2, D, D};
            SDL_RenderCopy(r, btnTex[icon], nullptr, &ir);
            lw = ir.x - 12 - lx;
        }
        textBox(label, C_WHITE, 1, lx, y, lw, hh - 5, 2);
    };
    // شريط التلميحات: أيقونة الزر ثم شرحه، من اليمين إلى اليسار (اتجاه القراءة العربي)، والمجموعة في الوسط
    auto drawHints = [&](const std::vector<HintItem>& items, int cy) {
        // كل تلميح: الزر أولًا ثم اسمه على يمينه، ليرتبط النص بزره بوضوح
        const int D = 44, GAP_IN = 12, LH = 46;
        std::vector<int> lw(items.size(), 0);
        int total = 0;
        for (size_t i = 0; i < items.size(); i++) {
            SDL_Texture* t = getText(tr(items[i].label), C_TEXT, 1);
            int tw = 0, th = 0; if (t) SDL_QueryTexture(t, nullptr, nullptr, &tw, &th);
            lw[i] = tw; total += D + GAP_IN + tw;
        }
        const int n = (int)items.size();
        int gItem = 40;
        const int avail = SW - 430 - 20;   // مساحة قبل خانة تبديل اللغة
        if (n > 1 && total + (n - 1) * gItem > avail) gItem = 14;
        total += (n > 1 ? n - 1 : 0) * gItem;
        int x = std::max(20, std::min((SW - total) / 2, SW - 430 - total));
        for (size_t i = 0; i < items.size(); i++) {
            SDL_Rect ir{x, cy - D / 2, D, D};
            if (btnTex[items[i].icon]) SDL_RenderCopy(r, btnTex[items[i].icon], nullptr, &ir);
            textBox(items[i].label, C_TEXT, 1, x + D + GAP_IN, cy - LH / 2, std::max(lw[i], 1), LH, 0);
            x += D + GAP_IN + lw[i] + gItem;
        }
    };
    int notifStage = 0; Uint32 notifAt = 0;       // إشعاران عند الدخول: الأول بعد ظهور الواجهة، والثاني بعده بثانيتين
    startIconWorker();
    startScan();
    auto stepNav = [&](int dir, int n) {
        if (tokenEdit || aboutPage) return;
        if (confirmDel || confirmReplace || confirmRepair || confirmIniReset || pkgConfirm || storeState.phase == PluginStore::Confirm) return;
        if (iniView) { const int maxS = std::max(0, (int)iniLines.size() - INI_ROWS); iniScroll = std::max(0, std::min(maxS, iniScroll + dir * n)); return; }
        if (settingsMenu) { int old = settingsSel; settingsSel = std::max(0, std::min(5, settingsSel + dir * n)); if (old != settingsSel) playMoveSound(); return; }
        if (languageMenu) { moveLanguage(dir * n); return; }
        if (inStore) moveStore(dir * n); else if (!inPlugins) moveGame(dir * n); else movePlugin(dir * n);
    };

    // ================= Main loop =================
    while (running) {
        Uint32 now = SDL_GetTicks();
        {   // زر لوحة اللمس عبر Pad API: الضغطة الجديدة داخل المتجر تفتح البحث
            static bool touchPrev = false;
            if (rawPad >= 0) {
                OrbisPadData tp; std::memset(&tp, 0, sizeof tp);
                if (scePadReadState(rawPad, &tp) == 0 && tp.connected) {
                    const bool touchDown = (tp.buttons & ORBIS_PAD_BUTTON_TOUCH_PAD) != 0;
                    if (touchDown && !touchPrev && inStore && !searchEdit && !tokenEdit && !pkgConfirm && !confirmDel) openSearch();
                    touchPrev = touchDown;
                }
            }
        }
        if (searchEdit) {   // لوحة البحث: عند الانتهاء نطبّق الفلترة على القائمة
            const int sst = sceImeDialogGetStatus();
            if (sst == 2) {
                ImeDialogResult sres; std::memset(&sres, 0, sizeof sres);
                sceImeDialogGetResult(&sres); sceImeDialogTerm(); searchEdit = false;
                if (sres.endstatus == 0) {
                    storeQuery = trim(utf16ToUtf8(searchBuf, 64));
                    PluginStore::setStoreFilter(storeQuery);
                    storeState = store.snapshot(); storeSelected = 0; storeScroll = 0;
                }
            } else if (sst == 0 && SDL_GetTicks() - searchOpenedAt > 1500) { sceImeDialogTerm(); searchEdit = false; }
        }
        if (tokenEdit) {   // system keyboard: wait for it to finish, then save the typed token
            const int st = sceImeDialogGetStatus();
            if (st == 2) {
                ImeDialogResult res; std::memset(&res, 0, sizeof res);
                sceImeDialogGetResult(&res); sceImeDialogTerm(); tokenEdit = false;
                if (res.endstatus == 0) {
                    tokenBuf = trim(utf16ToUtf8(imeBuf, TOKEN_MAX));
                    if (tokenBuf.size() > TOKEN_MAX) tokenBuf.resize(TOKEN_MAX);
                    tokenSave();
                } else setNotice(languageText(kCanceled2));
            } else if (st == 0 && SDL_GetTicks() - imeOpenedAt > 1500) { sceImeDialogTerm(); tokenEdit = false; }   // never became active: give up (grace period for slow start)
        }

        // --- scan results ---
        bool doneNow = !scanDone && SDL_AtomicGet(&gDone) != 0;   // اقرأ done قبل النسخة لضمان آخر نتيجة
        int ver = SDL_AtomicGet(&gVersion);
        if (ver != seenVersion) { seenVersion = ver; syncScan(); sceneDirty = true; }
        if (doneNow) {
            if (scanThread) SDL_WaitThread(scanThread, nullptr);
            scanThread = nullptr;
            scanDone = true; sceneDirty = true;
            setNotice("اكتمل الفحص — عدد الألعاب: " + std::to_string((int)games.size() - 1));
        }
        if (repairStale && scanDone && tookPlugins) {   // dry run once per scan, never inside the scan thread
            repairStale = false;
            std::string original, fixed; PluginIniRepair::Report rp;
            repairIssues = 0;
            if (buildRepair(original, fixed, rp))
                repairIssues = rp.bracketFixes + rp.pathFixes + rp.valueFixes + rp.commentedLines + rp.mergedSections +
                               rp.duplicatePlugins + rp.removedPlugins + rp.removedSections + rp.removedMissing;
            sceneDirty = true;
        }
        if (!scanDone && !aborted() && now - scanStart > SCAN_WATCHDOG_MS) {
            SDL_AtomicSet(&gAbort, 1);
            setNotice("استغرق الفحص وقتاً طويلاً؛ جاري إيقافه");
        }

        // نسخ كتالوج المتجر كاملاً (ونفلترته عند البحث) كان يتم كل إطار؛ نحدّثه كل 100 ملي ثانية فقط
        static Uint32 storeSnapAt = 0;
        if (now - storeSnapAt >= 100 || storeSnapAt == 0) { storeState = store.snapshot(); storeSnapAt = now; }
        // مهلة لتحميل المتجر: إذا علق الطلب أكثر من 45 ثانية نعيد المحاولة (الطلب القديم يُتجاهل)
        static Uint32 catalogLoadingSince = 0;
        if (storeState.phase == PluginStore::LoadingCatalog) {
            if (!catalogLoadingSince) catalogLoadingSince = now;
            else if (now - catalogLoadingSince > 45000) {
                catalogLoadingSince = 0;
                store.refreshCatalog(true);
                setNotice("انتهت مهلة الاتصال، جاري إعادة المحاولة");
            }
        } else catalogLoadingSince = 0;
        if (storeState.phase!=lastStorePhase) {
            if (storeState.phase==PluginStore::Installed) {
                auto installed=std::find_if(storeState.entries.begin(),storeState.entries.end(),[&](const PluginStore::Entry& candidate){return candidate.id==storeState.installedId;});
                const PluginStore::Entry se=installed==storeState.entries.end()?PluginStore::Entry():*installed;
                Config next=readConfig(); Section* def=section(next,"default",true);
                for(const auto& path:storePaths(se)) {
                    for(auto& sec:next.sections) for(size_t i=0;i<sec.lines.size();) {
                        std::string old; bool enabled;
                        if(pluginLine(sec.lines[i],old,enabled) && old==path) sec.lines.erase(sec.lines.begin()+i); else ++i;
                    }
                    def=section(next,"default",true); def->lines.push_back(path+"=false");
                }
                if(se.filename.empty()) { playErrorSound(); setNotice("Installed plugin entry could not be identified"); }
                else if(!saveConfig(next)) { playErrorSound(); setNotice("Installed, but could not save plugins.ini"); }
                else {
                    cfg=next; savedCfg=cfg; dirty=false;
                    for(const auto& path:storePaths(se)) {
                        const std::string filename=path.substr(path.find_last_of('/')+1);
                        bool found=false; for(const auto& p:plugins) if(p.path==path) found=true;
                        if(!found) plugins.push_back({filename,path});
                    }
                    std::sort(plugins.begin(),plugins.end(),[](const Plugin& a,const Plugin& b){return a.name<b.name;});
                    playSuccessSound(); setNotice("تم التثبيت");
                }
            } else if (storeState.phase==PluginStore::Confirm) { replaceSel=0; playClickSound();
            } else if (storeState.phase==PluginStore::Offline) { playErrorSound();
            } else if (storeState.phase==PluginStore::Failed) { playErrorSound(); setNotice("Store failed: "+storeState.message); }
            else if (storeState.phase==PluginStore::Canceled) setNotice("Canceled");
            lastStorePhase=storeState.phase;
        }

        // Optional support QR: fetched from the internet straight into memory and shown from there.
        // Nothing is written to the console: no QR file, no cache.
        // تأجيل عمل QR (فك الصورة أو توليد الرمز) حتى يستقر الاختيار 300 ملي ثانية: أثناء التنقل السريع لا نحمّل شيئًا
        static int qrLastSel=-2; static Uint32 qrSelSince=0; static bool qrSettled=false;
        if(storeSelected!=qrLastSel){ qrLastSel=storeSelected; qrSelSince=SDL_GetTicks(); qrSettled=false; storeInfoReady=false; }
        else if(!qrSettled && SDL_GetTicks()-qrSelSince>=300){ qrSettled=true; storeInfoReady=true; sceneDirty=true; }
        if(gQrPrep && gQrPrep->state.load()==0) sceneDirty=true;   // الباركود يُجهَّز في الخلفية: نعيد الرسم حتى يكتمل
        if(inStore && qrSettled && storeSelected>=0 && storeSelected<(int)storeState.entries.size()) {
            const auto& qrEntry=storeState.entries[storeSelected];
            if(!qrEntry.supportQR.empty()) {
                auto hit=qrMem.find(qrEntry.supportQR);
                if(hit!=qrMem.end()) { supportQrTexture=hit->second; supportQrUrl=qrEntry.supportQR; }
                else if(!isQrImageUrl(qrEntry.supportQR)) {   // plain link: build the QR code locally (no download)
                    SDL_Texture* qt=makeQrTextureFromText(r,qrEntry.supportQR,QR_PX);
                    qrMem[qrEntry.supportQR]=qt; supportQrTexture=qt; supportQrUrl=qrEntry.supportQR;
                }
                else {
                    if(storeState.phase==PluginStore::Ready && storeState.qrUrl!=qrEntry.supportQR && storeState.message!="QR load failed") {
                        store.loadSupportQr(qrEntry); storeState=store.snapshot();
                    }
                    std::vector<unsigned char> qrBytes;
                    if(!gQrPrep && storeState.qrUrl==qrEntry.supportQR && store.takeQrBytes(qrEntry.supportQR,qrBytes)) {
                        gQrPrep=new QrPrep(); gQrPrep->in=std::move(qrBytes); gQrPrep->url=qrEntry.supportQR; sceneDirty=true;
                        SDL_Thread* th=SDL_CreateThread(qrPrepThread,"qr-prep",gQrPrep);
                        if(th) SDL_DetachThread(th); else { delete gQrPrep; gQrPrep=nullptr; }
                    }
                    if(gQrPrep && gQrPrep->state.load()!=0) {   // جاهزة: إنشاء النسيج على الخيط الرئيسي فقط
                        SDL_Texture* qt=nullptr;
                        if(gQrPrep->state.load()==1) { SDL_Surface* sf=SDL_CreateRGBSurfaceWithFormatFrom(gQrPrep->out.data(),QR_PX,QR_PX,32,QR_PX*4,SDL_PIXELFORMAT_RGBA32); if(sf){ qt=SDL_CreateTextureFromSurface(r,sf); SDL_FreeSurface(sf); } }
                        qrMem[gQrPrep->url]=qt; supportQrTexture=qt; supportQrUrl=gQrPrep->url; sceneDirty=true;
                        delete gQrPrep; gQrPrep=nullptr;
                    }
                }
            }
        }

        // --- input ---
        bool inputNow = false;   // ضغطة زر/اتجاه في هذا الإطار: نرسم فوراً
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_JOYBUTTONDOWN || e.type == SDL_JOYBUTTONUP || e.type == SDL_JOYHATMOTION || e.type == SDL_KEYDOWN || e.type == SDL_KEYUP || e.type == SDL_CONTROLLERBUTTONDOWN || e.type == SDL_CONTROLLERBUTTONUP) inputNow = true;
            if (e.type != SDL_JOYAXISMOTION) sceneDirty = true;   // حركة العصا المستمرة لا تعيد رسم الشاشة (كانت تسبب تقطيعاً)
            if (e.type == SDL_QUIT) running = false;
            else if (e.type == SDL_JOYBUTTONDOWN) {
                int b = e.jbutton.button;
                if (b == BTN_UP || b == BTN_DOWN) {
                    if ((languageMenu || settingsMenu) && rawPad >= 0) { navDir = 0; navNext = now; }
                    else { navDir = b == BTN_UP ? -1 : 1; navStick = false; navSince = now; navNext = now + NAV_FIRST_DELAY_MS; }
                }
                else if (stickArmed) { stickArmed = false; if (stickLogs < 60) { ++stickLogs; } }   // ضغطة زر أخرى = العصا لم تعد مرجعاً
                // أثناء قائمة اللغة، يقرأ المسار الأصلي لـ Pad الأسهم لتجنب مضاعفة الحركة عبر SDL.
                if (!((languageMenu || settingsMenu) && rawPad >= 0 && (b == BTN_UP || b == BTN_DOWN))) action(b);
            }
            else if (e.type == SDL_JOYAXISMOTION && e.jaxis.axis == 1) {
                int v = e.jaxis.value, mag = v < 0 ? -v : v;
                bool was = stickArmed;
                stickVal = v; stickEvTick = now; stickArmed = mag > STICK_DEADZONE;
                if (stickLogs < 60 && was != stickArmed) { ++stickLogs;  }
            }
            else if (e.type == SDL_KEYDOWN) {
                SDL_Keycode k = e.key.keysym.sym;
                if (k == SDLK_UP) action(13);
                else if (k == SDLK_DOWN) action(14);
                else if (k == SDLK_RETURN) action(0);
                else if (k == SDLK_ESCAPE) action(1);
            }
        }
        // OPTIONS is read from the native DS4 state, independent of SDL button numbering.
        if (rawPad >= 0) {
            OrbisPadData optionPad; std::memset(&optionPad, 0, sizeof optionPad);
            if (scePadReadState(rawPad, &optionPad) == 0 && optionPad.connected) {
                bool down = (optionPad.buttons & ORBIS_PAD_BUTTON_OPTIONS) != 0;
                if (down && !optionsWasDown && !tokenEdit && !aboutPage && !confirmRepair && !pkgConfirm && !languageMenu && !confirmDel && !confirmReplace && storeState.phase != PluginStore::Confirm) {
                    settingsMenu = !settingsMenu; settingsSel = 0; playClickSound();
                    navDir = 0; navNext = now;
                }
                optionsWasDown = down;
                const bool r3 = (optionPad.buttons & ORBIS_PAD_BUTTON_R3) != 0;   // R3 (right stick click) toggles the About page
                if (r3 && !r3WasDown && !tokenEdit && !languageMenu && !settingsMenu && !confirmDel && !confirmReplace && !confirmRepair && !pkgConfirm && storeState.phase != PluginStore::Confirm) {
                    aboutPage = !aboutPage; playClickSound(); navDir = 0; navNext = now;
                    if (aboutPage && !aboutQr) aboutQr = makeQrTextureFromText(r, "https://discord.gg/XW24zkCtP9", 232);
                }
                r3WasDown = r3;
                // L2 (زر ثابت من ORBIS_PAD_BUTTON_L2): إصلاح plugins.ini من شاشة الألعاب فقط
                const bool l2 = (optionPad.buttons & ORBIS_PAD_BUTTON_L2) != 0;
                if (l2 && !l2WasDown && !inPlugins && !inStore && !tokenEdit && !aboutPage && !languageMenu && !settingsMenu && !confirmDel && !confirmReplace && !confirmRepair && !pkgConfirm && storeState.phase != PluginStore::Confirm) {
                    sceneDirty = true; openRepair();
                }
                l2WasDown = l2;
            }
        }
        // الاتجاه المضغوط: أسهم / hat / العصا اليسرى. تكرار مبني على الزمن فلا يتأثر ببطء الإطارات
        {
            int dir = 0; float strength = 1.0f; bool stick = false;
            bool haveRaw = false;
            if (rawPad >= 0) {
                OrbisPadData pd; memset(&pd, 0, sizeof pd);
                if (scePadReadState(rawPad, &pd) == 0 && pd.connected) {
                    haveRaw = true;
                    bool up = (pd.buttons & ORBIS_PAD_BUTTON_UP) != 0, dn = (pd.buttons & ORBIS_PAD_BUTTON_DOWN) != 0;
                    if (up != dn) dir = up ? -1 : 1;
                    else {
                        int dy = (int)pd.leftStick.y - 128, mag = dy < 0 ? -dy : dy;
                        if (mag > RAW_STICK_DEADZONE) {
                            dir = dy < 0 ? -1 : 1; stick = true;
                            strength = std::min(1.0f, (mag - RAW_STICK_DEADZONE) / (127.0f - RAW_STICK_DEADZONE));
                        }
                    }
                    if (stick != rawStickOn && stickLogs < 60) { ++stickLogs;  }
                    rawStickOn = stick;
                }
            }
            if (!haveRaw && pad) {
                bool up = SDL_JoystickGetButton(pad, BTN_UP) != 0, dn = SDL_JoystickGetButton(pad, BTN_DOWN) != 0;
                if (up != dn) dir = up ? -1 : 1;
                if (!dir && stickArmed) {
                    int mag = stickVal < 0 ? -stickVal : stickVal;
                    bool fresh = (int)(now - stickEvTick) < STICK_STALE_MS;
                    if (mag > STICK_DEADZONE && (fresh || mag > STICK_HOLD_STRONG)) {
                        dir = stickVal < 0 ? -1 : 1; stick = true;
                        strength = std::min(1.0f, (mag - STICK_DEADZONE) / (32767.0f - STICK_DEADZONE));
                    } else if (!fresh) {                       // قيمة عالقة قديمة (لم يصل حدث تحرير): نتجاهلها حتى يصل حدث جديد
                        stickArmed = false;
                        if (stickLogs < 60) { ++stickLogs;  }
                    }
                }
            }
            if (dir == 0) navDir = 0;
            else if (dir != navDir) { navDir = dir; navStick = stick; navSince = now; navNext = now + NAV_FIRST_DELAY_MS; stepNav(dir, 1); }
            else if ((int)(now - navNext) >= 0) {
                if (languageMenu || settingsMenu) {
                    // قائمة اللغة: تكرار سريع ثابت (لغة واحدة كل خطوة)؛ العصا تتسارع كلما مِلتها أكثر
                    const Uint32 gap = navStick ? (Uint32)(230.0f - 90.0f * strength) : 150;
                    navNext = now + gap;
                    languageRepeat = true;
                    stepNav(dir, 1);
                    languageRepeat = false;
                } else {
                    Uint32 held = now - navSince;
                    float base = held < 900 ? 150.0f : held < 1800 ? 110.0f : 85.0f;    // يتسارع كلما طال الضغط (أبطأ من قبل)
                    if (navStick) base *= 2.4f - 1.5f * strength;                       // العصا أبطأ من قبل، وتتسارع قليلًا كلما مِلتها أكثر
                    if (inStore) base *= 1.5f;                                          // المتجر أبطأ قليلًا لسلاسة التنقل
                    Uint32 interval = (Uint32)std::max(60.0f, base);
                    int ticks = 1 + (int)((now - navNext) / interval);                  // يعوّض الإطارات البطيئة
                    if (ticks > 1) ticks = 1;   // خطوة واحدة لكل تكرار: بدون قفز مزدوج
                    navNext = now + interval;
                    stepNav(dir, ticks);
                }
            }
        }

        // --- frame ---
        ++frameNo;
        shapeNew = 0;   // ميزانية بناء الأشكال تتجدد كل إطار
        for (auto it = shapeCache.begin(); it != shapeCache.end();) {   // نحذف الأشكال التي لم تُستعمل منذ مدة
            if (frameNo - it->second.used > 120) { if (it->second.t) SDL_DestroyTexture(it->second.t); it = shapeCache.erase(it); }
            else ++it;
        }
        if (textCache.size() > 500) {      // نحذف القديم فقط بدل مسح الكل (يمنع التقطيع)
            for (auto it = textCache.begin(); it != textCache.end();) {
                if (frameNo - it->second.used > 120) { if (it->second.t) SDL_DestroyTexture(it->second.t); it = textCache.erase(it); }
                else ++it;
            }
        }
        loadsLeft = navDir ? 1 : 3;        // أيقونة أو أكثر في كل إطار (أثناء التمرير واحدة فقط حتى لا تتعطل الحركة)
        if (iconWaiting) sceneDirty = true;   // أيقونات ما زالت تُفك: نعيد الرسم حتى تظهر
        iconWaiting = false;

        // ---- هل نعيد رسم الشاشة الثابتة في هذا الإطار؟ ----
        // التمرير المستمر والفحص الجاري: نعيد الرسم كل 200-250 مللي ثانية فقط (الرسم الكامل بطيء، فلا نرسمه كل فريم)
        if ((navDir && now - sceneAt >= 200) || tokenEdit) sceneDirty = true;   // أثناء الفحص لا نعيد الرسم كل ثانية، فقط عند تغيّر النتائج
        if (storeState.phase == PluginStore::Downloading || storeState.phase == PluginStore::Verifying ||
            storeState.phase == PluginStore::LoadingCatalog || storeState.phase == PluginStore::LoadingQr) sceneDirty = true;
        {   // شريط تقدم التثبيت: نعيد رسم المشهد كل نصف ثانية فقط حتى لا يتقطع التنقل
            static Uint32 rpiSceneAt = 0;
            if (PluginStore::Rpi::progress().active && now - rpiSceneAt >= 500) { rpiSceneAt = now; sceneDirty = true; }
        }
        {   // أيقونات تُحمَّل في الخلفية: إذا تغيّر عددها نعيد الرسم
            size_t ic = 0;
            for (auto* t : iconTex) if (t) ++ic;
            if (ic != lastIconCount) { lastIconCount = ic; sceneDirty = true; }
        }
        if (!scanDone) sceneDirty = true;                 // سبنر الفحص يدور
        if (now - noticeAt < 3600) sceneDirty = true;     // الإشعار يُرسم حتى يختفي

                const bool lerpWas = lerpBusy; lerpBusy = false;
        // الرسم الكامل مكلف على المعالج: نرسم فقط عند تغيّر، أو إدخال، أو حركة لم تكتمل، أو كل ثانية كاحتياط
        const bool drawScene = lerpWas || now - sceneAt >= 1000 || (sceneDirty && (inputNow || now - sceneAt >= 250));   // التغييرات الخلفية: 4 مرات بالثانية كحد أقصى
        const Uint32 sceneT0 = SDL_GetTicks();
        if (drawScene) {
        if (sceneTex) SDL_SetRenderTarget(r, sceneTex);   // نرسم الشاشة الثابتة داخل الصورة المخزّنة
        dynBanner.on = false;   // يُفعَّل فقط إذا رُسم البانر الكبير في هذه الشاشة

        if (kBgGlow && bgTex && glowTex) {   // خلفية مستوحاة من صورة البرنامج: نسخ الصورة المخزّنة + توهج وردي ثابت
            SDL_RenderCopy(r, bgTex, nullptr, nullptr);
            SDL_SetTextureAlphaMod(glowTex, 200);
            SDL_Rect gd{SW - 260 - GLOW_R, SH - 180 - GLOW_R, GLOW_R * 2, GLOW_R * 2};
            if (navDir == 0) SDL_RenderCopy(r, glowTex, nullptr, &gd);   // التوهج يُرسم عند السكون فقط، ويُوقف أثناء التنقل لتخفيف الحمل
        } else {
            rect(0, 0, SW, SH, C_BG);   // احتياط إذا فشل إنشاء الصور المخزّنة
        }

                // Header (شبه شفاف حتى تظهر الخلفية)
        rect(0, 0, SW, 106, SDL_Color{17, 23, 39, 200});
        rect(0, 106, SW, 2, C_LINE);
        textBox("مدير الإضافات", SDL_Color{250, 251, 255, 255}, 2, LX, 8, LW, 60, 1);
        textBox("إدارة إضافات الألعاب", C_DIM, 0, LX, 68, LW, 32, 1);
        if (scanDone) textBox("ألعاب: " + std::to_string((int)games.size() - 1) + "    إضافات: " + std::to_string((int)plugins.size()), C_DIM, 1, 90, 28, 800, 52, 0);
        else textBox("جاري الفحص...", C_WARN, 1, 90, 28, 800, 52, 0);
        {   // About sticker in the header: key cap "R3" + label (R3 opens/closes the About page)
            const int ow = 330, oh = 56, ox = 460, oy = 25, cy = oy + oh / 2;
            const bool open = aboutPage;
            const SDL_Color fill = open ? C_ROWSEL : C_ROW;
            roundRect(ox, oy + 4, ow, oh, 22, SDL_Color{0, 0, 0, 90});
            roundRect(ox - 2, oy - 2, ow + 4, oh + 4, 24, open ? C_ACCENT : C_LINE);
            roundRect(ox, oy, ow, oh, 22, fill);
            const int kw = 76, kh = 36, kx = ox + ow - 14 - kw, ky = cy - kh / 2;   // key cap
            roundRect(kx, ky + 3, kw, kh, 18, SDL_Color{0, 0, 0, 110});
            roundRect(kx, ky, kw, kh, 18, SDL_Color{150, 164, 196, 255});
            roundRect(kx + 2, ky + 2, kw - 4, kh - 4, 16, SDL_Color{14, 18, 30, 255});
            textBox("R3", C_WHITE, 0, kx + 8, ky, kw - 16, kh, 2);
            rect(kx - 14, cy - 15, 2, 30, C_LINE);
            const int gx = kx - 14 - 18 - 14;
            const SDL_Color gf = open ? C_WHITE : C_DIM;
            roundRect(gx - 14, cy - 14, 28, 28, 14, gf);  roundRect(gx - 12, cy - 12, 24, 24, 12, fill);   // "i" badge
            textBox("i", gf, 0, gx - 12, cy - 12, 24, 24, 2);
            textBox(open ? languageText(kClose) : languageText(kAboutTheApp),
                    open ? C_WHITE : C_TEXT, 1, ox + 16, oy + 6, gx - 14 - 12 - (ox + 16), oh - 12, 1);
        }

                // Main panel
        rect(70, 124, SW - 140, 862, C_PANEL);
        rect(70, 124, SW - 140, 5, C_ACCENT);

        if (inStore && storeState.phase==PluginStore::Offline) {
            // Offline screen: replaces the whole store interface.
            roundRect(90,140,1740,812,18,C_CARD);
            roundRect(SW/2-70,250,140,140,70,SDL_Color{255,190,108,40});
            textBox("!",C_WARN,3,SW/2-70,250,140,140,2);
            textBox(languageText(kNoInternetConnection),C_WARN,3,120,430,SW-240,110,2);
            textBox(languageText(kCheckYourConnectionAndTry),C_TEXT,3,120,560,SW-240,110,2);
            textBox(languageText(kPressXToRetryO),C_DIM,2,120,740,SW-240,60,2);
        } else if (inStore) {
            roundRect(90,140,1740,58,14,C_CARD); textBox(tr("\u0645\u062a\u062c\u0631\u0020\u0627\u0644\u0625\u0636\u0627\u0641\u0627\u062a"),C_TEXT,2,110,140,1700,58,1);
            if(!storeState.entries.empty()) textBox(std::to_string(storeSelected+1)+" / "+std::to_string((int)storeState.entries.size()),C_DIM,1,130,140,300,58,0);
            roundRect(90,214,480,738,18,C_CARD); roundRect(590,214,1240,738,18,C_CARD);
            if(storeState.entries.empty()) { if(storeState.phase==PluginStore::LoadingCatalog || (!storeState.catalogLoaded && storeState.phase!=PluginStore::Failed && storeState.phase!=PluginStore::Offline)) { spinner(330,500,46); textBox(tr("\u062c\u0627\u0631\u064d\u0020\u062a\u062d\u0645\u064a\u0644\u0020\u0627\u0644\u0642\u0627\u0626\u0645\u0629"),C_TEXT,1,100,570,460,56,2); } else if(storeState.phase==PluginStore::Failed) textBox(storeState.message,C_WARN,1,100,450,460,110,2); else if(!storeQuery.empty()) textBox(languageText(kNoPluginWithThisName),C_DIM,1,100,480,460,60,2);
            else if(storeQuery.empty()) textBox(tr("\u0644\u0627\u0020\u062a\u0648\u062c\u062f\u0020\u0625\u0636\u0627\u0641\u0627\u062a\u0020\u0641\u064a\u0020\u0627\u0644\u0645\u062a\u062c\u0631"),C_DIM,1,100,480,460,60,2); }
            {   // which plugins are already on the console (refreshed once per second, not every frame)
                static Uint32 instTick=0; static std::map<std::string,int> instCache; static size_t instCount=0; static PluginStore::Phase instPh=PluginStore::Idle; static int instScroll=-1;
                if(instTick==0||SDL_GetTicks()-instTick>1000||instCount!=storeState.entries.size()||instPh!=storeState.phase||instScroll!=storeScroll) {
                    instCache.clear(); for(int k=storeScroll;k<(int)storeState.entries.size()&&k<storeScroll+STORE_ROWS;k++) instCache[storeState.entries[k].id]=PluginStore::Client::installedState(storeState.entries[k],PLUGIN_DIR);
                    instTick=SDL_GetTicks(); instCount=storeState.entries.size(); instPh=storeState.phase; instScroll=storeScroll;
                }
                // تسخين نصوص الإضافات تدريجيًا: أول ظهور لنص جديد يكلّف رسمه وتحويله إلى صورة، فنرسم نصًا واحدًا خارج الشاشة في كل إطار
                {   // كل إضافة تُسخَّن مرة واحدة فقط، وفقط عند السكون (بدون هذا كان يعاد إنشاء نصوص كل إطار لأن الكاش يحذف القديم)
                    static size_t warmIdx = 0;
                    static std::map<std::string, bool> warmed;
                    if (!storeState.entries.empty() && navDir == 0 && warmed.size() < storeState.entries.size()) {
                        if (warmIdx >= storeState.entries.size()) warmIdx = 0;
                        const auto& we = storeState.entries[warmIdx];
                        if (warmed.emplace(we.id, true).second) {
                            const auto wn = wrapText(we.name, 1, 388, 1);
                            if (!wn.empty()) { textNat(wn[0], C_TEXT, 1, -5000, 0, 388, 1); textNat(wn[0], C_WHITE, 1, -5000, 0, 388, 1); }
                            if (!we.version.empty()) textNat("v" + we.version, C_DIM, 1, -5000, 0, 388, 1);
                        }
                        warmIdx++;
                    }
                }
                // حركة سلسة للقائمة: الصفوف والتحديد ينزلقان نحو مواضعهما
                storeVisScroll += ((float)storeScroll - storeVisScroll) * 1.0f;   // بدون حركة انزلاقية: الموضع يصل مباشرة
                if (std::fabs((float)storeScroll - storeVisScroll) < 0.02f) storeVisScroll = (float)storeScroll;
                else lerpBusy = true;
                storeVisSel += ((float)storeSelected - storeVisSel) * 1.0f;   // بدون حركة انزلاقية: الموضع يصل مباشرة
                if (std::fabs((float)storeSelected - storeVisSel) < 0.02f) storeVisSel = (float)storeSelected;
                else lerpBusy = true;
                const int sFirst = std::max(0, (int)std::floor(storeVisScroll) - 1);
                const int sLast = std::min((int)storeState.entries.size(), (int)std::ceil(storeVisScroll) + STORE_ROWS + 1);
                for (int i = sFirst; i < sLast; i++) {   // 1) خلفيات الصفوف
                    const int y = 232 + (int)std::lround((i - storeVisScroll) * 102);
                    if (y < 232 || y + 102 > 232 + STORE_ROWS * 102) continue;   // خارج نافذة القائمة: لا نرسمه فوق العنوان أو تحت الأزرار
                    roundRect(110, y, 424, 94, 16, C_ROW);
                }
                if (!storeState.entries.empty()) {   // 2) التحديد المتحرك (فقط مع وجود إضافات)
                    const int ys = 232 + (int)std::lround((storeVisSel - storeVisScroll) * 102);
                    roundRect(110, ys, 424, 94, 16, C_ROWSEL);
                    roundRect(110, ys + 16, 7, 62, 3, C_ACCENT);
                }
                for (int i = sFirst; i < sLast; i++) {   // 3) محتوى الصفوف
                    const int y = 232 + (int)std::lround((i - storeVisScroll) * 102);
                    if (y < 232 || y + 102 > 232 + STORE_ROWS * 102) continue;   // خارج نافذة القائمة: لا نرسمه فوق العنوان أو تحت الأزرار
                    const bool sel = i == storeSelected; const auto& e = storeState.entries[i];
                    { const auto nl = wrapText(e.name, 1, 388, 1); if (!nl.empty()) textNat(nl[0], sel ? C_WHITE : C_TEXT, 1, 134, y + 30, 388, 1); }
                    if (!e.version.empty()) textNat("v" + e.version, C_DIM, 1, 134, y + 70, 388, 1);
                    auto it = instCache.find(e.id);
                    if (it != instCache.end() && it->second) {
                        const bool upd = it->second == 2;
                        roundRect(134, y + 54, 118, 32, 16, upd ? SDL_Color{84, 60, 20, 255} : SDL_Color{28, 78, 58, 255});
                        roundRect(146, y + 65, 10, 10, 5, upd ? C_WARN : C_ON);
                        textNat(upd ? languageText(kOtherVersion) : languageText(kInstalled), upd ? C_WARN : C_ON, 0, 162, y + 70, 84, 2);
                    }
                }
            }
            scrollBar(542,232,706,(int)storeState.entries.size(),STORE_ROWS,(int)std::lround(storeVisScroll));
            if(!storeState.entries.empty()&&storeSelected<(int)storeState.entries.size()) { const auto& e=storeState.entries[storeSelected];
                const int R=1790, L=630, IW=R-L;
                // ---- 1) العنوان ----
                { const auto tl=wrapText(e.name,4,IW,1); if(!tl.empty()) textNat(tl[0],C_TEXT,4,L,268,IW,1); }
                // ---- 2) شارات المعلومات (الإصدار، المطوّر، اسم الملف) من اليمين إلى اليسار ----
                {
                    int cx=R; const int cy=334, chH=44;
                    auto chip=[&](const std::string& label, SDL_Color fg, int maxW) {
                    if(!storeInfoReady) return;
                        SDL_Texture* t=getText(tr(label),fg,0); int tw=0,th=0; if(t) SDL_QueryTexture(t,nullptr,nullptr,&tw,&th);
                        int w=std::max(70,std::min(maxW,tw+36)); if(cx-w<L) return;
                        roundRect(cx-w,cy-chH/2,w,chH,14,C_ROW); { const auto cl=wrapText(label,0,w-36,1); if(!cl.empty()) textNat(cl[0],fg,0,cx-w+18,cy,w-36,2); } cx-=w+12;
                    };
                    chip(tr("\u0627\u0644\u0625\u0635\u062f\u0627\u0631")+": "+(e.version.empty()?"-":e.version),C_TEXT,300);
                    chip(tr("\u0627\u0644\u0645\u0637\u0648\u0651\u0631")+": "+(e.author.empty()?"-":e.author),C_TEXT,480);
                    chip(e.filename+(e.url2download.empty()?"":" + "+e.filename2),C_DIM,520);
                }
                rect(L,376,IW,2,SDL_Color{53,68,96,255});
                // ---- 3) الوصف (بحجم خط واحد لكل الأسطر) ----
                textNat(tr("\u0627\u0644\u0648\u0635\u0641"),C_ACCENT,0,L,408,IW,1);
                {
                    const std::string& chosenDescription = (gLanguage == 4 && !e.descriptionAr.empty()) ? e.descriptionAr : (!e.description.empty() ? e.description : e.descriptionAr);
                    const std::string description = chosenDescription.empty()?tr("\u0627\u0628\u062f\u0623\u0020\u0627\u0644\u062a\u0635\u0641\u062d\u0020\u0628\u0639\u062f\u0020\u062a\u062d\u0645\u064a\u0644\u0020\u0627\u0644\u0642\u0627\u0626\u0645\u0629"):chosenDescription;
                    // الوصف كامل بلا قص: يلتف على العرض، ويُصغَّر إن لم يتسع المكان المخصص له
                    // الوصف ينتهي قبل بطاقة الدعم (تبدأ عند y=624) حتى لا يدخل تحت الباركود
                    textFit(storeInfoReady?description:std::string(languageText(kFetchingPluginInfo)),storeInfoReady?C_TEXT:C_DIM,L,436,IW,170,gLanguage==4?1:0);
                }
                // ---- 4) بطاقة الدعم: عنوان QR، الصورة بحجم ثابت داخل إطار أبيض، ثم نص الدعم ----
                const bool hasQr=!e.supportQR.empty(), hasSupport=!e.support.empty();
                if(hasQr||hasSupport) {
                    const int SX=L, SY=624, SCW=440, SCH=312;
                    roundRect(SX,SY,SCW,SCH,18,C_ROW);
                    const bool hasLabel=hasQr&&!e.qrText.empty();
                    const int labH=hasLabel?36:0, qrH=hasQr?QR_PX+16:0, supH=hasSupport?36:0;
                    const int gaps=((labH&&(qrH||supH))?8:0)+((qrH&&supH)?8:0);
                    int cy0=SY+(SCH-(labH+qrH+supH+gaps))/2;
                    if(hasLabel) { textNat(e.qrText,C_WHITE,0,SX+16,cy0+labH/2,SCW-32,2); cy0+=labH+8; }
                    if(hasQr) {
                        const int fx=SX+(SCW-(QR_PX+16))/2;
                        if(supportQrTexture && supportQrUrl==e.supportQR) {
                            if(isQrImageUrl(e.supportQR)) roundRect(fx,cy0,QR_PX+16,QR_PX+16,14,C_WHITE);          // downloaded image: card as before
                            else roundRect(fx+4,cy0+4,QR_PX+8,QR_PX+8,12,C_WHITE);                                  // generated from a link: tighter white margin
                            SDL_Rect q{fx+8,cy0+8,QR_PX,QR_PX}; SDL_RenderCopy(r,supportQrTexture,nullptr,&q);
                        } else {
                            roundRect(fx,cy0,QR_PX+16,QR_PX+16,14,SDL_Color{30,41,64,255});
                            textNat(storeState.phase==PluginStore::LoadingQr?languageText(kLoadingSupportQr):languageText(kQrImageUnavailable),C_DIM,0,fx+10,cy0+(QR_PX+16)/2,QR_PX+16-20,2);
                        }
                        cy0+=qrH+8;
                    }
                    if(hasSupport) textNat(tr("\u0627\u0644\u062f\u0639\u0645")+": "+e.support,C_DIM,0,SX+16,cy0+supH/2,SCW-32,2);
                }
                // ---- 5) العمود الأيمن: الحالة، شريط التقدم، زر التحميل ----
                {
                    const int RX=1140, RW2=R-RX;
                    std::string st=storeState.phase==PluginStore::LoadingQr?languageText(kLoadingSupportQr2):storeState.phase==PluginStore::LoadingCatalog?tr("\u062c\u0627\u0631\u064d\u0020\u062a\u062d\u0645\u064a\u0644\u0020\u0627\u0644\u0642\u0627\u0626\u0645\u0629"):storeState.phase==PluginStore::Downloading?tr("\u062c\u0627\u0631\u064d\u0020\u062a\u0646\u0632\u064a\u0644\u0020\u0627\u0644\u0625\u0636\u0627\u0641\u0629"):storeState.phase==PluginStore::Verifying?tr("\u062c\u0627\u0631\u064d\u0020\u0627\u0644\u062a\u062d\u0642\u0642\u0020\u0645\u0646\u0020\u0633\u0644\u0627\u0645\u0629\u0020\u0627\u0644\u0645\u0644\u0641"):storeState.phase==PluginStore::Installed?tr("\u062a\u0645\u0020\u0627\u0644\u062a\u062b\u0628\u064a\u062a"):storeState.phase==PluginStore::Ready&&storeState.message=="Offline cache"?tr("\u0627\u0644\u0642\u0627\u0626\u0645\u0629\u0020\u0627\u0644\u0645\u062d\u0641\u0648\u0638\u0629\u0020\u062f\u0648\u0646\u0020\u0627\u062a\u0635\u0627\u0644"):storeState.phase==PluginStore::Ready?tr("\u0627\u0643\u062a\u0645\u0644\u062a\u0020\u0627\u0644\u0642\u0627\u0626\u0645\u0629"):storeState.message;
                    SDL_Color stCol=storeState.phase==PluginStore::Failed?C_WARN:storeState.phase==PluginStore::Installed?C_ON:C_TEXT;
                    if(storeState.phase==PluginStore::Installed) {
                        // تظهر الرسالة فقط على الإضافة اللي تم تثبيتها، وتختفي إذا اخترت غيرها
                        const bool isInstalledSel = storeSelected>=0 && storeSelected<(int)storeState.entries.size() && storeState.entries[storeSelected].id==storeState.installedId;
                        st = isInstalledSel ? languageText(kInstalled2) : std::string();
                    }
                    const std::vector<std::string> stLines=wrapText(st,0,RW2,3);
                    for(size_t li=0;li<stLines.size();++li) textNat(stLines[li],stCol,0,RX,672+(int)li*34,RW2,1);
                    if(storeState.phase==PluginStore::Downloading||storeState.phase==PluginStore::Verifying) {
                        const int by=800;
                        roundRect(RX,by,RW2,16,8,C_ROW);
                        int fill=storeState.total?(int)std::min<uint64_t>((uint64_t)RW2,(uint64_t)RW2*storeState.received/storeState.total):(int)(SDL_GetTicks()/8%RW2);
                        roundRect(RX,by,std::max(16,fill),16,8,C_ACCENT);
                        auto fmtBytes=[](uint64_t b){ char buf[40]; if(b>=1048576ull) std::snprintf(buf,sizeof buf,"%.1f MB",b/1048576.0); else std::snprintf(buf,sizeof buf,"%.0f KB",b/1024.0); return std::string(buf); };
                        std::string prog=storeState.total?(std::to_string((int)(100ull*storeState.received/storeState.total))+"%   "+fmtBytes(storeState.received)+" / "+fmtBytes(storeState.total)):fmtBytes(storeState.received);
                        {   // live download speed, sampled every 500 ms
                            static uint64_t spPrev=0; static Uint32 spTick=0; static double spRate=0;
                            Uint32 nowT=SDL_GetTicks();
                            if(storeState.phase!=PluginStore::Downloading||storeState.received<spPrev||spTick==0){spPrev=storeState.received;spTick=nowT;spRate=0;}
                            else if(nowT-spTick>=500){spRate=(storeState.received-spPrev)*1000.0/(double)(nowT-spTick);spPrev=storeState.received;spTick=nowT;}
                            if(storeState.phase==PluginStore::Downloading&&spRate>0){char sp[48];std::snprintf(sp,sizeof sp,"   %.2f MB/s",spRate/1048576.0);prog+=sp;}
                        }
                        textNat(prog,C_DIM,0,RX,by+40,RW2,1);
                    }
                    {   // شريط تقدم تثبيت BGFT بنفس شكل شريط التنزيل
                        const PluginStore::Rpi::Progress rpg = PluginStore::Rpi::progress();
                        if (rpg.active && rpg.percent >= 0) {
                            const int by = 800;
                            roundRect(RX, by, RW2, 16, 8, C_ROW);
                            roundRect(RX, by, std::max(16, (int)(RW2 * rpg.percent / 100)), 16, 8, C_ACCENT);
                            textNat(std::string(languageText(kDownloadingPkg)) + "   " + std::to_string(rpg.percent) + "%", C_DIM, 0, RX, by + 40, RW2, 1);
                        }
                        else if (rpg.active && rpg.percent != -3) {
                            textNat(rpg.percent == -2 ? languageText(kPreparing) : languageText(kInstallingApp), C_DIM, 0, RX, 800 + 40, RW2, 1);
                        }
                    }
                    {   // رسالة الحالة أسفل منطقة التقدم: التطبيق موجود مسبقًا
                        std::string stMsg;
                        if (!e.pkgUrl.empty() && PluginStore::Rpi::state(stMsg) == PluginStore::Rpi::State::AlreadyInstalled)
                            textNat(languageText(kAppIsAlreadyInstalled), C_DIM, 0, RX, 800 + 40, RW2, 1);
                    }
                    drawButton(R-340,872,340,64,languageText(kDownload),C_ACCENT,BI_CROSS,false);
                    if(!e.pkgUrl.empty()&&!PluginStore::Rpi::progress().active&&!rpiAlreadyInstalled()&&storeState.phase!=PluginStore::Downloading&&storeState.phase!=PluginStore::LoadingCatalog&&storeState.phase!=PluginStore::LoadingQr)
                        drawButton(R-340,872-76,340,64,languageText(kInstallApp),SDL_Color{60,110,190,255},BI_TRIANGLE,false);
                    if(e.tokenEnabled&&storeState.phase!=PluginStore::Downloading&&storeState.phase!=PluginStore::LoadingCatalog&&storeState.phase!=PluginStore::LoadingQr)
                        drawButton(R-340-24-360,872,360,64,languageText(kEditToken),SDL_Color{60,110,190,255},BI_SQUARE,false);
                } }
        } else if (!inPlugins) {
            // ---- list header ----
            roundRect(LX, 140, LW, 58, 14, C_CARD);
            textBox("الألعاب", C_TEXT, 1, LX + LW - 320, 140, 300, 58, 1);
            textBox(std::to_string((int)games.size() - 1), C_DIM, 1, LX + 20, 140, 200, 58, 0);
            // ---- rows ----
            const int RW = LW - 18;
            // حركة سلسة لقائمة الألعاب: الصفوف والتحديد ينزلقان نحو مواضعهما
            gameVisScroll += ((float)gameScroll - gameVisScroll) * 1.0f;   // بدون حركة انزلاقية: الموضع يصل مباشرة
            if (std::fabs((float)gameScroll - gameVisScroll) < 0.02f) gameVisScroll = (float)gameScroll;
                else lerpBusy = true;
            gameVisSel += ((float)selectedGame - gameVisSel) * 1.0f;   // بدون حركة انزلاقية: الموضع يصل مباشرة
            if (std::fabs((float)selectedGame - gameVisSel) < 0.02f) gameVisSel = (float)selectedGame;
                else lerpBusy = true;
            const int gFirst = std::max(0, (int)std::floor(gameVisScroll) - 1);
            const int gLast = std::min((int)games.size(), (int)std::ceil(gameVisScroll) + ROWS_G + 1);
            for (int i = gFirst; i < gLast; i++) {   // 1) خلفيات الصفوف
                const int y = LIST_Y + (int)std::lround((i - gameVisScroll) * STRIDE_G);
                if (y < LIST_Y || y + STRIDE_G > LIST_Y + ROWS_G * STRIDE_G) continue;   // خارج نافذة القائمة: لا نرسمه فوق العنوان أو تحت الأزرار
                roundRect(LX, y, RW, 80, 16, C_ROW);
            }
            {   // 2) التحديد المتحرك
                const int ys = LIST_Y + (int)std::lround((gameVisSel - gameVisScroll) * STRIDE_G);
                roundRect(LX, ys, RW, 80, 16, SDL_Color{58, 75, 118, 255});
                roundRect(LX + RW - 14, ys + 12, 8, 56, 4, SDL_Color{160, 40, 90, 255});
                roundRect(LX + RW - 14, ys + 12, 7, 56, 3, C_ACCENT);
            }
            for (int i = gFirst; i < gLast; i++) {   // 3) محتوى الصفوف
                const int y = LIST_Y + (int)std::lround((i - gameVisScroll) * STRIDE_G);
                if (y < LIST_Y || y + STRIDE_G > LIST_Y + ROWS_G * STRIDE_G) continue;   // خارج نافذة القائمة: لا نرسمه فوق العنوان أو تحت الأزرار
                int textRight = LX + RW - 6 - 14;
                if (i > 0) {
                    int ix = LX + RW - 6 - 12 - 64;
                    drawIcon(i, ix, y + 8, 64);
                    textRight = ix - 14;
                    textBox(games[i].title, C_TEXT, 1, LX + 14, y + 5, textRight - (LX + 14), 44, 1);
                    textBox(games[i].id, C_DIM, 0, LX + 14, y + 47, textRight - (LX + 14), 32, 1);
                } else {
                    const int tile = 72;
                    int ix = LX + RW - 6 - 12 - tile;
                    drawDefaultTile(ix, y + 4, tile, tile, 1);
                    textRight = ix - 14;
                    textBox(games[i].title, C_TEXT, 1, LX + 14, y + 8, textRight - (LX + 14), 64, 1);
                }
            }
            scrollBar(LX + LW - 8, LIST_Y, ROWS_G * STRIDE_G - 6, (int)games.size(), ROWS_G, (int)std::lround(gameVisScroll));

            // ---- details ----
   // ظل ناعم
            roundRect(DX, 140, DW, 812, 22, SDL_Color{21, 29, 47, 255});   // شبه شفافة: الخلفية المتحركة تظهر من ورائها
            const int heroX = DX + DW - 30 - 240;
            drawIcon(selectedGame, heroX, 170, 240);
            const int tx = DX + 40, tw = heroX - 24 - tx;
            textBox(games[selectedGame].title, C_TEXT, 2, tx, 172, tw, 62, 1);
            if (selectedGame > 0) textBox(games[selectedGame].id, C_DIM, 1, tx, 240, tw, 46, 1);
            else textBox("الإعداد الافتراضي لكل الألعاب", C_DIM, 1, tx, 240, tw, 46, 1);
            if (tookPlugins) {
                if (plugins.empty()) textBox("لا توجد إضافات في مجلد GoldHEN", C_DIM, 1, tx, 300, tw, 46, 1);
                else textBox("الإضافات المفعّلة: " + std::to_string(countEnabled(sectionName())) + " من " + std::to_string((int)plugins.size()), C_TEXT, 1, tx, 300, tw, 46, 1);
            }
            textBox("ملفات الإضافات بصيغتي PRX و SPRX", C_DIM, 0, tx, 352, tw, 32, 1);
            if (dirty) textBox("تغييرات غير محفوظة", C_WARN, 0, tx, 392, tw, 32, 1);
            else if (repairIssues > 0) textBox("plugins.ini فيه " + std::to_string(repairIssues) + " مشاكل — اضغط L2 للإصلاح", C_WARN, 0, tx, 392, tw, 32, 1);
            rect(DX + 40, 440, DW - 80, 2, SDL_Color{53, 68, 96, 255});
            drawButton(DX + DW - 30 - 420, 470, 420, 76, "إدارة الإضافات", C_ACCENT, BI_CROSS, false);

            const int cx = DX + 40, cw = DW - 80;
            if (!scanDone) {
                spinner(DX + DW / 2, 690, 46);
                textBox("جاري فحص الألعاب والإضافات...", C_TEXT, 1, cx, 770, cw, 46, 2);
                const char* st = stage == "plugins" ? "قراءة الإضافات" : stage == "ftp" ? "الفحص عبر خادم النقل" : stage == "icons" ? "جلب أيقونات الألعاب" : "فحص مجلدات الألعاب";
                textBox(std::string(st) + " — تم العثور على " + std::to_string((int)games.size() - 1), C_DIM, 0, cx, 822, cw, 34, 2);
                textBox("النتائج تظهر تدريجياً أثناء الفحص", C_DIM, 0, cx, 862, cw, 34, 2);
            } else if (games.size() > 1) {
                if (ftpState != 4 && !gRootWorked) textBox(languageText(kEnableTheFtpServerIn), C_WARN, 1, cx, 540, cw, 46, 1);
                int withIcon = 0;
                for (size_t i = 1; i < games.size(); i++) if (!games[i].iconPath.empty()) ++withIcon;
                if (selectedGame > 0) {
                    bool failed = iconTried[selectedGame] && !iconTex[selectedGame] && !games[selectedGame].iconPath.empty();
                    if (games[selectedGame].iconPath.empty()) textBox("لم يتم العثور على ملف أيقونة لهذه اللعبة", C_WARN, 0, cx, 630, cw, 34, 1);
                    else if (failed) textBox("تعذر فك ترميز أيقونة هذه اللعبة (" + gIconErr + ")", C_WARN, 0, cx, 630, cw, 34, 1);
                }
            } else {
                textBox("لم يتم العثور على ألعاب مثبتة", C_WARN, 1, cx, 590, cw, 46, 1);
                if (ftpState != 4) textBox(languageText(kEnableTheFtpServerIn), C_WARN, 1, cx, 640, cw, 46, 1);
                if (!ftpConnected) textBox(languageText(kTransferServerIsNotConnected), C_WARN, 1, cx, 690, cw, 46, 1);
            }

            // ---- Plugin Store banner (home screen) ----
            // Big version once the scan is finished; compact version while scanning (the scan text lives in the same area).
            {
                const bool big = scanDone;
                const bool rtl = (gLanguage == 4);
                const int bx = DX + 24, bw = DW - 48;
                const int bh = big ? 150 : 70, by = big ? 790 : 870;
                const float pulse = 0.5f;   // لون ثابت (الشاشة مخزّنة، فلا تعتمد على الوقت)
                auto mix = [](SDL_Color a, SDL_Color b, float t) {
                    return SDL_Color{(Uint8)(a.r + (b.r - a.r) * t), (Uint8)(a.g + (b.g - a.g) * t), (Uint8)(a.b + (b.b - a.b) * t), 255};
                };
                // vertical-gradient rounded rectangle (one thin row per scanline, corners follow the radius)
                auto gradRound = [&](int x, int y, int ww, int hh, int rad, SDL_Color top, SDL_Color bot) {
                    (void)bot; roundRect(x, y, ww, hh, rad, top);   // لون واحد بدل التدرج الخط بخط (أخف بكثير)
                };
                // x of an element of width w placed `off` px from the leading edge (right edge in Arabic, left edge otherwise)
                auto lead = [&](int off, int w) { return rtl ? (bx + bw - off - w) : (bx + off); };
                const int textAlign = rtl ? 1 : 0;
                const SDL_Color C_SUB = {196, 208, 232, 255};
                const SDL_Color ACC_DARK = {196, 44, 96, 255};

                // soft drop shadow + pulsing glow + thin accent ring
                roundRect(bx + 2, by + 9, bw, bh, 20, SDL_Color{0, 0, 0, 55});
                roundRect(bx + 1, by + 5, bw, bh, 19, SDL_Color{0, 0, 0, 45});
                if (big) roundRect(bx - 7, by - 7, bw + 14, bh + 14, 24, SDL_Color{255, 72, 125, (Uint8)(10 + 22 * pulse)});
                roundRect(bx - 2, by - 2, bw + 4, bh + 4, 18, mix(SDL_Color{176, 50, 96, 255}, SDL_Color{255, 118, 160, 255}, big ? pulse : 0.4f));
                gradRound(bx, by, bw, bh, 16, SDL_Color{56, 68, 104, 255}, SDL_Color{32, 42, 68, 255});
                rect(bx + 18, by + 1, bw - 36, 1, SDL_Color{255, 255, 255, 46});   // top highlight

                if (big) {
                    // --- icon: glow halo + accent ring + PlayStation triangle ---
                    const int IC = 88, icOff = 30;
                    const int icX = lead(icOff, IC), icY = by + (bh - IC) / 2, icCX = icX + IC / 2, icCY = icY + IC / 2;
                    roundRect(icCX - 62, icCY - 62, 124, 124, 62, SDL_Color{255, 72, 125, 18});
                    roundRect(icCX - 54, icCY - 54, 108, 108, 54, SDL_Color{255, 72, 125, 30});
                    // القرص ثابت هنا؛ الموجة والمثلث المتحركان يُرسمان فوق الشاشة المخزّنة (انظر dynBanner)
                    roundRect(icX, icY + 3, IC, IC, IC / 2, ACC_DARK);
                    roundRect(icX, icY, IC, IC, IC / 2, C_ACCENT);

                    // --- call-to-action pill on the trailing side ---
                    const int PW = 210, PH = 60, pOff = 30;
                    const int pX = rtl ? (bx + pOff) : (bx + bw - pOff - PW), pY = by + (bh - PH) / 2;
                    roundRect(pX, pY + 5, PW, PH, 30, SDL_Color{0, 0, 0, 90});
                    roundRect(pX, pY, PW, PH, 30, ACC_DARK);
                    roundRect(pX, pY, PW, PH - 4, (PH - 4) / 2, C_ACCENT);
                    roundRect(pX + 8, pY + 5, PW - 16, 22, 11, SDL_Color{255, 255, 255, 30});
                    const int pi = 38;
                    dynBanner = DynBanner{true, rtl, icX, icY, IC, pX, pY, PW, PH, pi};   // تفاصيل البانر المتحركة
                     if (btnTex[BI_TRIANGLE]) { SDL_FRect pr0{(float)(rtl ? (pX + PW - 14 - pi) : (pX + 14)), (float)pY + (PH - 4 - pi) / 2.0f, (float)pi, (float)pi}; SDL_RenderCopyF(r, btnTex[BI_TRIANGLE], nullptr, &pr0); }
                     if (btnTex[BI_TRIANGLE]) { SDL_FRect itri{(float)icX + 9.0f, (float)icY + 9.0f, (float)IC - 18.0f, (float)IC - 18.0f}; SDL_RenderCopyF(r, btnTex[BI_TRIANGLE], nullptr, &itri); }
                    const int lblX = rtl ? (pX + 14) : (pX + 14 + pi + 10), lblW = PW - 14 - pi - 10 - 14;
                    textBox(languageText(kOpenStore), C_WHITE, 1, lblX, pY, lblW, PH - 4, 2);
                    // thin divider between the text and the pill
                    const int divX = rtl ? (pX + PW + 22) : (pX - 22);
                    rect(divX, by + 30, 1, bh - 60, SDL_Color{255, 255, 255, 34});

                    // --- text (title + description) ---
                    const int tOff = icOff + IC + 28;
                    const int textX = rtl ? (divX + 22) : (bx + tOff);
                    const int textW = rtl ? ((bx + bw - tOff) - (divX + 22)) : ((divX - 22) - (bx + tOff));
                    textBox("PLUGIN STORE", C_WHITE, 2, textX, by + 24, textW, 52, textAlign);
                    {   // الوصف بحجم أكبر من الصغير وأصغر من العنوان، داخل البنر (سطر واحد يتصغّر ليتسع)
                        SDL_Texture* dtx = getText(tr(languageText(kBrowseAndDownloadPluginsDirectly)), C_SUB, 1);
                        if (dtx) {
                            int dtw = 0, dth = 0;
                            SDL_QueryTexture(dtx, nullptr, nullptr, &dtw, &dth);
                            const float dk = std::min(1.0f, std::min((float)36 / (float)dth, (float)textW / (float)dtw));
                            const int dw = (int)(dtw * dk), dh = (int)(dth * dk);
                            const int dx = textAlign == 1 ? textX + textW - dw : (textAlign == 2 ? textX + (textW - dw) / 2 : textX);
                            SDL_Rect dr{dx, by + 88, dw, dh};
                            SDL_RenderCopy(r, dtx, nullptr, &dr);
                        }
                    }
                } else {
                    // --- compact version (scan in progress) ---
                    const int IC = 46, icOff = 18;
                    const int icX = lead(icOff, IC), icY = by + (bh - IC) / 2;
                    roundRect(icX, icY + 2, IC, IC, IC / 2, ACC_DARK);
                    roundRect(icX, icY, IC, IC, IC / 2, C_ACCENT);
                    if (btnTex[BI_TRIANGLE]) { SDL_Rect tri{icX + 5, icY + 5, IC - 10, IC - 10}; SDL_RenderCopy(r, btnTex[BI_TRIANGLE], nullptr, &tri); }
                    const int tOff = icOff + IC + 18;
                    const int textX = rtl ? (bx + 22) : (bx + tOff), textW = bw - tOff - 22;
                    textBox("PLUGIN STORE", C_WHITE, 1, textX, by + 6, textW, 32, textAlign);
                    textFit(languageText(kBrowseAndDownloadPluginsDirectly), C_SUB, textX, by + 34, textW, bh - 38, textAlign);
                }
            }
        } else {
            // ---- plugin screen ----
            roundRect(90, 140, 1740, 58, 14, C_CARD);
            textBox(sectionName() == "default" ? "[default]" : "[" + sectionName() + "]", C_DIM, 1, 110, 140, 420, 58, 0);
            textBox(games[selectedGame].title, C_TEXT, 1, 560, 140, 1250, 58, 1);
            const int RW = 1740 - 18;
            if (plugins.empty()) {
                textBox("لا توجد إضافات في مجلد GoldHEN", C_DIM, 1, 90, 400, 1740, 50, 2);
                textBox("ضع ملفات PRX أو SPRX في المجلد: /data/GoldHEN/plugins", C_DIM, 0, 90, 460, 1740, 34, 2);
            }
            // حركة سلسة: القائمة والتحديد ينزلقان نحو مواضعهما بدل القفز
            plugVisScroll += ((float)pluginScroll - plugVisScroll) * 1.0f;   // بدون حركة انزلاقية: الموضع يصل مباشرة
            if (std::fabs((float)pluginScroll - plugVisScroll) < 0.02f) plugVisScroll = (float)pluginScroll;
                else lerpBusy = true;
            plugVisSel += ((float)selectedPlugin - plugVisSel) * 1.0f;   // بدون حركة انزلاقية: الموضع يصل مباشرة
            if (std::fabs((float)selectedPlugin - plugVisSel) < 0.02f) plugVisSel = (float)selectedPlugin;
                else lerpBusy = true;
            const int firstP = std::max(0, (int)std::floor(plugVisScroll) - 1);
            const int lastP = std::min((int)plugins.size(), (int)std::ceil(plugVisScroll) + ROWS_P + 1);
            // 1) خلفيات الصفوف
            for (int i = firstP; i < lastP; i++) {
                const int y = LIST_Y + (int)std::lround((i - plugVisScroll) * STRIDE_P);
                if (y < LIST_Y || y + STRIDE_P > LIST_Y + ROWS_P * STRIDE_P) continue;   // خارج نافذة القائمة: لا نرسمه فوق العنوان أو تحت الأزرار
                roundRect(90, y, RW, 64, 14, C_ROW);
            }
            // 2) التحديد المتحرك فوق الخلفيات وتحت النصوص
            {
                const int ys = LIST_Y + (int)std::lround((plugVisSel - plugVisScroll) * STRIDE_P);
                roundRect(90, ys, RW, 64, 14, C_ROWSEL);
                roundRect(90 + RW - 12, ys + 14, 5, 36, 2, C_ACCENT);
            }
            // 3) محتوى الصفوف
            for (int i = firstP; i < lastP; i++) {
                const int y = LIST_Y + (int)std::lround((i - plugVisScroll) * STRIDE_P);
                if (y < LIST_Y || y + STRIDE_P > LIST_Y + ROWS_P * STRIDE_P) continue;   // خارج نافذة القائمة: لا نرسمه فوق العنوان أو تحت الأزرار
                const bool on = currentEnabled(i);
                textBox(plugins[i].name, C_TEXT, 1, 90 + 290, y + 2, RW - 290 - 30, 60, 1);
                roundRect(90 + 24, y + 9, 220, 46, 23, on ? C_ONBG : C_OFFBG);
                roundRect(90 + 24 + 20, y + 9 + 17, 12, 12, 6, on ? C_ON : C_OFF);
                textBox(on ? "مفعّل" : "متوقف", on ? C_ON : C_OFF, 0, 90 + 24 + 40, y + 9, 220 - 52, 46, 2);
            }
            scrollBar(90 + 1740 - 8, LIST_Y, ROWS_P * STRIDE_P - 8, (int)plugins.size(), ROWS_P, (int)std::lround(plugVisScroll));
            roundRect(90, 880, 1740, 72, 18, C_CARD);
            drawButton(1830 - 18 - 300, 888, 300, 56, "حفظ التغييرات", SDL_Color{52, 120, 220, 255}, BI_TRIANGLE, false);
            drawButton(1830 - 18 - 300 - 18 - 300, 888, 300, 56, "حذف الإضافة", plugins.empty() ? SDL_Color{70, 80, 100, 255} : SDL_Color{210, 58, 64, 255}, BI_SQUARE, false);
            if (dirty) textBox("تغييرات غير محفوظة", C_WARN, 1, 110, 888, 1066, 56, 1);
            else textBox("ملفات الإضافات بصيغتي PRX و SPRX", C_DIM, 1, 110, 888, 1066, 56, 1);
        }

                // Plugin already exists: ask whether to replace it with this version
        if (inStore && (confirmReplace || storeState.phase == PluginStore::Confirm) && storeSelected >= 0 && storeSelected < (int)storeState.entries.size()) {
            const auto& ce = storeState.entries[storeSelected];
            rect(0, 0, SW, SH, SDL_Color{0, 0, 0, 178});
            const int MW = 1100, MH = 500, mx = (SW - MW) / 2, my = (SH - MH) / 2 - 20;
            roundRect(mx - 3, my - 3, MW + 6, MH + 6, 24, C_WARN);
            roundRect(mx, my, MW, MH, 21, C_CARD);
            textBox(languageText(kTheFileAlreadyExists), C_WARN, 2, mx + 40, my + 26, MW - 80, 62, 2);
            rect(mx + 40, my + 102, MW - 80, 2, C_LINE);
            roundRect(mx + 80, my + 124, MW - 160, 64, 16, SDL_Color{34, 46, 72, 255});
            textNat(ce.name, C_WHITE, 1, mx + 100, my + 156, MW - 200, 2);
            textBox(languageText(kDownloadThisVersionTheOld), C_TEXT, 1, mx + 40, my + 208, MW - 80, 46, 2);
            if (!ce.packageUrl.empty()) {
                if (storeState.phase == PluginStore::Confirm) textBox(languageText(kAlreadyOnTheConsole) + storeState.message, C_WARN, 0, mx + 40, my + 262, MW - 80, 36, 2);
                else textBox(languageText(kReplaceAllDeletesTheOld), C_WARN, 0, mx + 40, my + 262, MW - 80, 36, 2);
                textBox(languageText(kPrxOnlyReplacesJustThe), C_DIM, 0, mx + 40, my + 306, MW - 80, 36, 2);
            }
            const bool pkgDlg = !ce.packageUrl.empty();
            const int BW = pkgDlg ? 310 : 330, BH = 68, by = my + MH - 40 - BH;
            drawButton(mx + MW - 40 - BW, by, BW, BH, pkgDlg ? languageText(kReplaceAll) : languageText(kDownload2), SDL_Color{52, 160, 96, 255}, BI_CROSS, replaceSel == 2);
            if (pkgDlg) drawButton(mx + MW - 40 - BW - 28 - BW, by, BW, BH, languageText(kPrxOnlyNoZip), SDL_Color{60, 110, 190, 255}, BI_SQUARE, replaceSel == 1);
            drawButton(mx + 40, by, BW, BH, languageText(kCancel), SDL_Color{74, 86, 112, 255}, BI_CIRCLE, replaceSel == 0);
        }

        // نافذة تأكيد الحذف
        if (confirmDel && selectedPlugin >= 0 && selectedPlugin < (int)plugins.size()) {
            rect(0, 0, SW, SH, SDL_Color{0, 0, 0, 178});
            const int MW = 960, MH = 440, mx = (SW - MW) / 2, my = (SH - MH) / 2 - 20;
            roundRect(mx - 3, my - 3, MW + 6, MH + 6, 24, SDL_Color{210, 58, 64, 255});
            roundRect(mx, my, MW, MH, 21, C_CARD);
            textBox("تأكيد الحذف", SDL_Color{255, 120, 120, 255}, 2, mx + 40, my + 26, MW - 80, 62, 2);
            rect(mx + 40, my + 102, MW - 80, 2, C_LINE);
            textBox("هل أنت متأكد من حذف هذه الإضافة نهائياً؟", C_TEXT, 1, mx + 40, my + 120, MW - 80, 46, 2);
            roundRect(mx + 80, my + 184, MW - 160, 64, 16, SDL_Color{34, 46, 72, 255});
            textBox(plugins[selectedPlugin].name, C_WHITE, 1, mx + 100, my + 184, MW - 200, 64, 2);
            textBox("سيُحذف الملف من الجهاز ولا يمكن التراجع عن هذا الإجراء", C_WARN, 0, mx + 40, my + 262, MW - 80, 36, 2);
            const int BW = 300, BH = 68, by = my + MH - 40 - BH;
            drawButton(mx + MW - 40 - BW, by, BW, BH, "حذف", SDL_Color{210, 58, 64, 255}, BI_CROSS, confirmSel == 1);
            drawButton(mx + MW - 40 - BW - 28 - BW, by, BW, BH, "إلغاء", SDL_Color{74, 86, 112, 255}, BI_CIRCLE, confirmSel == 0);
        }

        // نافذة إصلاح plugins.ini: ما سيتغير ثم موافق / لا
        // نافذة تثبيت PKG: تأكيد صريح قبل الإرسال إلى Remote Package Installer
        if (pkgConfirm && storeSelected >= 0 && storeSelected < (int)storeState.entries.size()) {
            rect(0, 0, SW, SH, SDL_Color{0, 0, 0, 178});
            const int MW = 1000, MH = 520, mx = (SW - MW) / 2, my = (SH - MH) / 2 - 20;
            roundRect(mx - 3, my - 3, MW + 6, MH + 6, 24, C_ACCENT);
            roundRect(mx, my, MW, MH, 21, C_CARD);
            textBox(languageText(kInstallThisAppOnThe), C_WHITE, 2, mx + 40, my + 26, MW - 80, 62, 2);
            rect(mx + 40, my + 102, MW - 80, 2, C_LINE);
            textBox(languageText(kTheAppWillBeInstalled), C_TEXT, 1, mx + 40, my + 124, MW - 80, 46, 2);
            textFit(storeState.entries[storeSelected].pkgUrl, C_DIM, mx + 40, my + 190, MW - 80, 80, 1);
            const int BW = 300, BH = 68, by = my + MH - 40 - BH;
            drawButton(mx + MW - 40 - BW, by, BW, BH, languageText(kOk), SDL_Color{52, 160, 96, 255}, BI_CROSS, pkgSel == 1);
            drawButton(mx + MW - 40 - BW - 28 - BW, by, BW, BH, languageText(kNo), SDL_Color{74, 86, 112, 255}, BI_CIRCLE, pkgSel == 0);
        }
        if (confirmIniReset) {
            rect(0, 0, SW, SH, SDL_Color{0, 0, 0, 178});
            const int MW = 1000, MH = 560, mx = (SW - MW) / 2, my = (SH - MH) / 2 - 20;
            roundRect(mx - 3, my - 3, MW + 6, MH + 6, 24, C_ACCENT);
            roundRect(mx, my, MW, MH, 21, C_CARD);
            textBox(iniDlgRestore ? languageText(kRestorePluginsIniFromBackup) : languageText(kResetPluginsIni), C_WHITE, 2, mx + 40, my + 26, MW - 80, 62, 2);
            rect(mx + 40, my + 102, MW - 80, 2, C_LINE);
            textBox(iniDlgRestore ? languageText(kTheCurrentFileWillBe) : languageText(kTheFileWillBeEmptied), C_TEXT, 1, mx + 40, my + 150, MW - 80, 200, 2);
            const int BW = 300, BH = 68, by = my + MH - 40 - BH;
            drawButton(mx + MW - 40 - BW, by, BW, BH, languageText(kOk), SDL_Color{52, 160, 96, 255}, BI_CROSS, iniResetSel == 1);
            drawButton(mx + MW - 40 - BW - 28 - BW, by, BW, BH, languageText(kNo), SDL_Color{74, 86, 112, 255}, BI_CIRCLE, iniResetSel == 0);
        }
        if (confirmRepair) {
            rect(0, 0, SW, SH, SDL_Color{0, 0, 0, 178});
            const int MW = 1000, MH = 680, mx = (SW - MW) / 2, my = (SH - MH) / 2 - 20;
            roundRect(mx - 3, my - 3, MW + 6, MH + 6, 24, C_ACCENT);
            roundRect(mx, my, MW, MH, 21, C_CARD);
            textBox("إصلاح plugins.ini؟", C_WHITE, 2, mx + 40, my + 26, MW - 80, 62, 2);
            rect(mx + 40, my + 102, MW - 80, 2, C_LINE);
            std::vector<std::pair<std::string, SDL_Color>> lines;
            const PluginIniRepair::Report& rp = repairPreview;
            if (rp.bracketFixes) lines.push_back({"أقواس ناقصة أو زائدة في العناوين: " + std::to_string(rp.bracketFixes), C_TEXT});
            if (rp.pathFixes) lines.push_back({"مسارات خاطئة ستُصحح: " + std::to_string(rp.pathFixes), C_TEXT});
            if (rp.valueFixes) lines.push_back({"قيم غير صحيحة ستُصحح: " + std::to_string(rp.valueFixes), C_TEXT});
            if (rp.commentedLines) lines.push_back({"نصوص لا معنى لها ستُعلَّق بـ ; : " + std::to_string(rp.commentedLines), C_TEXT});
            if (rp.mergedSections) lines.push_back({"أقسام مكررة ستُدمج: " + std::to_string(rp.mergedSections), C_TEXT});
            if (rp.duplicatePlugins) lines.push_back({"إضافات مكررة ستُحذف: " + std::to_string(rp.duplicatePlugins), C_TEXT});
            if (rp.removedPlugins) lines.push_back({"إضافات لألعاب غير مثبتة ستُحذف: " + std::to_string(rp.removedPlugins), C_TEXT});
            if (rp.removedMissing) lines.push_back({"إضافات بمسار أو اسم غير موجود ستُحذف: " + std::to_string(rp.removedMissing), C_TEXT});
            if (rp.removalSkipped) lines.push_back({"تعذر قراءة قائمة الألعاب، لن تُحذف إضافات الألعاب", C_WARN});
            int ly = my + 124;
            for (const auto& ln : lines) { textBox(ln.first, ln.second, 2, mx + 40, ly, MW - 80, 42, 2); ly += 44; }   // حتى 8 أسطر ضمن المساحة
            textBox("سيُنشأ نسخة احتياطية plugins.ini.bak قبل الحفظ", C_DIM, 1, mx + 40, my + MH - 40 - 68 - 40, MW - 80, 36, 2);
            const int BW = 300, BH = 68, by = my + MH - 40 - BH;
            drawButton(mx + MW - 40 - BW, by, BW, BH, languageText(kOk), SDL_Color{52, 160, 96, 255}, BI_CROSS, repairSel == 1);
            drawButton(mx + MW - 40 - BW - 28 - BW, by, BW, BH, languageText(kNo), SDL_Color{74, 86, 112, 255}, BI_CIRCLE, repairSel == 0);
        }

        // Language picker opened by the physical Options button.
        if (settingsMenu) {   // قائمة الإعدادات (زر OPTIONS)
            rect(0, 0, SW, SH, SDL_Color{0, 0, 0, 188});
            const int MW = 800, MH = 830, mx = (SW - MW) / 2, my = (SH - MH) / 2;
            roundRect(mx - 3, my - 3, MW + 6, MH + 6, 26, C_ACCENT);
            roundRect(mx, my, MW, MH, 23, C_CARD);
            textBox(std::string(languageText(kSettings)), C_TEXT, 2, mx + 40, my + 28, MW - 80, 66, 0);
            rect(mx + 40, my + 110, MW - 80, 2, C_LINE);
            const std::string labels[6] = {
                std::string(languageText(kLanguage)),
                std::string(languageText(kRepairPluginsIni)),
                std::string(languageText(kAboutTheApp2)),
                std::string(languageText(kBrowsePluginsIni)),
                std::string(languageText(kResetPluginsIni2)),
                std::string(languageText(kRestoreBackup))};
            const std::string values[6] = {
                std::string(LANGUAGE_NAMES[gLanguage]) + " ›",
                repairIssues > 0 ? std::to_string(repairIssues) + " " + std::string(languageText(kIssues)) + " ›"
                                 : std::string(languageText(kOk2)) + " ›",
                "R3 ›",
                std::string(languageText(kView)) + " ›",
                std::string(languageText(kReset)) + " ›",
                std::string(languageText(kRestore)) + " ›"};
            for (int i = 0; i < 6; ++i) {
                const int y = my + 140 + i * 113;
                const bool sel = i == settingsSel;
                roundRect(mx + 40, y, MW - 80, 95, 16, sel ? C_ROWSEL : C_ROW);
                if (sel) roundRect(mx + 40, y + 12, 5, 71, 2, C_ACCENT);
                const int labelCut = (i == 0 && globeTex) ? 44 : 0;   // نترك مكاناً للكرة بعد كلمة "اللغة"
                if (i == 0 && globeTex) { SDL_Rect gr{mx + MW - 112, y + (95 - 36) / 2, 36, 36}; SDL_RenderCopy(r, globeTex, nullptr, &gr); }
                textBox(labels[i], C_TEXT, 1, mx + 70, y, MW - 140 - labelCut, 95, 1);
                textBox(values[i], (i == 1 && repairIssues > 0) ? C_WARN : C_DIM, 1, mx + 70, y, 300, 95, 0);
            }
        }
        if (iniView) {   // عارض plugins.ini: صفحة بيضاء بمحتوى الملف كاملًا، مع تمرير لأسفل للملفات الطويلة
            const SDL_Color ink{30, 36, 52, 255}, mute{110, 118, 136, 255};
            rect(0, 0, SW, SH, SDL_Color{236, 239, 244, 255});
            roundRect(40, 30, SW - 80, SH - 110, 18, SDL_Color{255, 255, 255, 255});
            textBox("plugins.ini", ink, 2, 80, 44, SW - 420, 56, 0);
            const std::string pageInfo = (iniLines.empty() ? std::string("0") : std::to_string(iniScroll + 1)) + " / " + std::to_string(iniLines.size());
            textBox(pageInfo, mute, 0, SW - 340, 52, 260, 40, 2);
            rect(80, 112, SW - 160, 2, SDL_Color{210, 215, 225, 255});
            if (!iniStatus.empty()) textBox(iniStatus, SDL_Color{200, 60, 60, 255}, 1, 80, 140, SW - 160, 60, 0);
            const int top = 130, LH = 32;
            for (int i = 0; i < INI_ROWS; i++) {
                const int li = iniScroll + i;
                if (li >= (int)iniLines.size()) break;
                const auto nl = wrapText(iniLines[li], 0, SW - 220, 1);
                if (!nl.empty()) textBox(nl[0], ink, 0, 80, top + i * LH, SW - 220, LH, 0);
            }
            if ((int)iniLines.size() > INI_ROWS) {   // شريط التمرير
                const int trackH = INI_ROWS * LH;
                const int maxS = (int)iniLines.size() - INI_ROWS;
                const int thumbH = std::max(40, trackH * INI_ROWS / (int)iniLines.size());
                const int thumbY = top + (trackH - thumbH) * iniScroll / std::max(1, maxS);
                roundRect(SW - 90, top, 8, trackH, 4, SDL_Color{228, 232, 240, 255});
                roundRect(SW - 90, thumbY, 8, thumbH, 4, SDL_Color{120, 130, 150, 255});
            }
        }
        if (languageMenu) {
            rect(0, 0, SW, SH, SDL_Color{0, 0, 0, 188});
            const int MW = 800, MH = 650, mx = (SW - MW) / 2, my = (SH - MH) / 2;
            roundRect(mx - 3, my - 3, MW + 6, MH + 6, 26, C_ACCENT);
            roundRect(mx, my, MW, MH, 23, C_CARD);
            textBox(languageText(kChooseLanguage), C_TEXT, 2, mx + 40, my + 28, MW - 80, 66, 2);
            for (int i = 0; i < 6; ++i) {
                int y = my + 150 + i * 75;
                roundRect(mx + 64, y, MW - 128, 58, 14, i == languageCursor ? C_ROWSEL : C_ROW);
                if (i == languageCursor) roundRect(mx + 64, y + 10, 5, 38, 2, C_ACCENT);
                textBox(LANGUAGE_NAMES[i], C_TEXT, 1, mx + 92, y, MW - 184, 58, 2);
                if (i == gLanguage) textBox(languageText(kCurrent), C_ON, 0, mx + 82, y, 120, 58, 0);
            }
        }

        if (aboutPage && !aboutQr) aboutQr = makeQrTextureFromText(r, "https://discord.gg/XW24zkCtP9", 232);
        // About page (R3)
        if (aboutPage) {
            const bool rtl = (gLanguage == 4);
            const int AW = 1040, AH = 565, ax = (SW - AW) / 2, ay = (SH - AH) / 2 - 10;
            rect(0, 0, SW, SH, SDL_Color{0, 0, 0, 150});
            roundRect(ax - 3, ay - 3, AW + 6, AH + 6, 26, C_ACCENT);
            roundRect(ax, ay, AW, AH, 23, C_CARD);
            textBox(languageText(kAboutTheApp), C_TEXT, 2, ax + 40, ay + 24, AW - 80, 64, 2);
            textBox("GoldHEN Plugin Manager", C_DIM, 0, ax + 40, ay + 88, AW - 80, 30, 2);
            rect(ax + 40, ay + 130, AW - 80, 2, C_LINE);
            const int QW = 260, qx = rtl ? (ax + 50) : (ax + AW - 50 - QW), qy = ay + 160;
            roundRect(qx, qy, QW, QW, 16, C_WHITE);
            if (aboutQr) { SDL_Rect q{qx + 14, qy + 14, 232, 232}; SDL_RenderCopy(r, aboutQr, nullptr, &q); }
            textBox(languageText(kDiscordServer), C_TEXT, 1, qx, qy + QW + 10, QW, 40, 2);
            textBox("discord.gg/XW24zkCtP9", C_DIM, 0, qx, qy + QW + 52, QW, 30, 2);
            const int ix = rtl ? (ax + 50 + QW + 40) : (ax + 50), iw = AW - 100 - QW - 40, ial = rtl ? 1 : 0;
            const std::string cnt = storeState.catalogLoaded ? std::to_string((int)storeState.entries.size()) : std::string("—");
            const std::string upd = (storeState.catalogLoaded && !storeState.catalogUpdated.empty()) ? storeState.catalogUpdated : std::string("—");
            struct InfoRow { std::string label, value; };
            const InfoRow rows[3] = {
                {languageText(kDeveloper), "ayoub_bl"},
                {languageText(kPluginsInTheStore), cnt},
                {languageText(kCatalogLastUpdated), upd}};
            for (int k = 0; k < 3; k++) {
                const int cy = qy + k * 110;
                roundRect(ix, cy, iw, 92, 16, C_ROW);
                roundRect(rtl ? (ix + iw - 5) : ix, cy + 18, 5, 56, 2, C_ACCENT);
                textBox(rows[k].label, C_DIM, 0, ix + 24, cy + 8, iw - 48, 30, ial);
                textBox(rows[k].value, C_WHITE, 1, ix + 24, cy + 40, iw - 48, 44, ial);
            }
            if (!storeState.catalogLoaded)
                textBox(languageText(kOpenTheStoreOnceTo), C_WARN, 0, ix, qy + 3 * 110 + 6, iw, 30, ial);
        }

        // Token editing uses the system keyboard (drawn by the PS4 itself); only dim the app behind it
        if (tokenEdit) rect(0, 0, SW, SH, SDL_Color{0, 0, 0, 150});

        // Footer: سطر للإشعار وسطر للتلميحات بأيقونات الأزرار (منفصلان فلا يتداخلان)
        rect(0, 992, SW, 88, C_BAR);
        rect(0, 992, SW, 2, C_LINE);
        if (pkgPending) {   // نتيجة إرسال PKG إلى Remote Package Installer
            std::string rmsg;
            const PluginStore::Rpi::State rs = PluginStore::Rpi::state(rmsg);
            if (rs != PluginStore::Rpi::State::Sending) {
                pkgPending = false;
                if (rs == PluginStore::Rpi::State::Started) { playClickSound(); setNotice(languageText(kInstallStarted)); }
                else if (rs == PluginStore::Rpi::State::AlreadyInstalled) { playClickSound(); setNotice(languageText(kAppIsAlreadyInstalled2)); }
                else if (rs == PluginStore::Rpi::State::NotRunning) { playErrorSound(); setNotice(languageText(kCouldNotStartTheInstallation)); }
                else { playErrorSound(); std::string m = languageText(kInstallFailed); if (!rmsg.empty()) m += ": " + rmsg; setNotice(m); }
            }
        }
        {   // تقدم التثبيت وإشعار الانتهاء أو الفشل
            const PluginStore::Rpi::Progress pg = PluginStore::Rpi::progress();
            if (pg.active && pg.percent >= 0) setNotice(std::string(languageText(kDownloadingPkg)) + " " + std::to_string(pg.percent) + "%");
            else if (pg.active && pg.percent != -3) setNotice(pg.percent == -2 ? languageText(kPreparing) : languageText(kInstallingApp));
            else if (pg.done) { playClickSound(); setNotice(languageText(kInstallationComplete)); }
            else if (pg.failed) { playErrorSound(); setNotice(languageText(kInstallationFailed)); }
        }
        if (now - noticeAt < 3500) textBox(notice, SDL_Color{238, 243, 252, 255}, 1, 90, 996, SW - 180, 32, 2);
        if (tokenEdit) { /* the system keyboard shows its own hints */ }
        else if (aboutPage) drawHints({{BI_CIRCLE, languageText(kBack)}}, 1054);
        else if (inStore && (confirmReplace || storeState.phase == PluginStore::Confirm)) drawHints({{BI_DPAD, "تبديل الاختيار"}, {BI_CROSS, "تأكيد"}, {BI_CIRCLE, "إلغاء"}}, 1054);
        else if (confirmIniReset) drawHints({{BI_DPAD, languageText(kChangeSelection)}, {BI_CROSS, languageText(kConfirm)}, {BI_CIRCLE, languageText(kCancel)}}, 1054);
        else if (pkgConfirm) drawHints({{BI_DPAD, "تبديل الاختيار"}, {BI_CROSS, "تأكيد"}, {BI_CIRCLE, "إلغاء"}}, 1054);
        else if (confirmRepair) drawHints({{BI_DPAD, "تبديل الاختيار"}, {BI_CROSS, "تأكيد"}, {BI_CIRCLE, "إلغاء"}}, 1054);
        else if (confirmDel) drawHints({{BI_DPAD, "تبديل الاختيار"}, {BI_CROSS, "تأكيد"}, {BI_CIRCLE, "إلغاء"}}, 1054);
        else if (settingsMenu) drawHints({{BI_DPAD, "التنقل"}, {BI_CROSS, "اختيار"}, {BI_CIRCLE, "إغلاق"}}, 1054);
        else if (languageMenu) drawHints({{BI_DPAD, "التنقل"}, {BI_CROSS, "حفظ"}, {BI_CIRCLE, "إلغاء"}}, 1054);
        else if (inStore && storeState.phase==PluginStore::Offline) drawHints({{BI_CROSS, languageText(kRetry)}, {BI_CIRCLE, languageText(kBack)}}, 1054);
        else if (inStore) {
            const bool storeBusy = storeState.phase == PluginStore::Downloading || storeState.phase == PluginStore::LoadingCatalog || storeState.phase == PluginStore::LoadingQr;
            const bool hasTok = !storeBusy && storeSelected >= 0 && storeSelected < (int)storeState.entries.size() && storeState.entries[storeSelected].tokenEnabled;
            const bool hasPkgBtn = !storeBusy && !PluginStore::Rpi::progress().active && !rpiAlreadyInstalled() && storeSelected >= 0 && storeSelected < (int)storeState.entries.size() && !storeState.entries[storeSelected].pkgUrl.empty();
            drawHints({{BI_DPAD, "\u0627\u0644\u062a\u0646\u0642\u0644"}, {BI_CROSS, languageText(kDownload)}, {BI_TRIANGLE, hasPkgBtn ? languageText(kInstallApp) : "\u062a\u062d\u062f\u064a\u062b\u0020\u0627\u0644\u0642\u0627\u0626\u0645\u0629"},
                       {BI_SQUARE, hasTok ? languageText(kEditToken) : "\u0625\u0644\u063a\u0627\u0621\u0020\u0627\u0644\u062a\u062d\u0645\u064a\u0644"}, {BI_TOUCH, languageText(kSearch)}, {BI_CIRCLE, "\u0631\u062c\u0648\u0639"}}, 1054);
        }
        else if (iniView) drawHints({{BI_DPAD, languageText(kNavigate)}, {BI_CIRCLE, languageText(kBack)}}, 1054);
        else if (inPlugins) drawHints({{BI_DPAD, "التنقل"}, {BI_CROSS, "تفعيل وتعطيل"}, {BI_TRIANGLE, "حفظ"}, {BI_SQUARE, "حذف"}, {BI_CIRCLE, "رجوع"}}, 1054);
        else {
            std::vector<HintItem> hints{{BI_DPAD, languageText(kNavigate2)}, {BI_CROSS, languageText(kSelect)}, {BI_TRIANGLE, languageText(kStore)}, {BI_SQUARE, languageText(kRescan)}};
            if (repairIssues > 0) hints.insert(hints.end() - 1, HintItem{BI_L2, "إصلاح plugins.ini"});
            drawHints(hints, 1054);
        }
        {   // settings switch: pill = [gear + label] and a key cap that says OPTIONS (the physical button that opens it)
            const int ow = 350, ox = SW - 70 - ow, oy = 1008, oh = 60, cy = oy + oh / 2;
            const bool open = languageMenu || settingsMenu;
            const SDL_Color fill = open ? C_ROWSEL : C_ROW;
            roundRect(ox, oy + 4, ow, oh, 22, SDL_Color{0, 0, 0, 90});
            roundRect(ox - 2, oy - 2, ow + 4, oh + 4, 24, open ? C_ACCENT : C_LINE);
            roundRect(ox, oy, ow, oh, 22, fill);
            const int kw = 128, kh = 38, kx = ox + ow - 14 - kw, ky = cy - kh / 2;   // key cap
            roundRect(kx, ky + 3, kw, kh, 19, SDL_Color{0, 0, 0, 110});
            roundRect(kx, ky, kw, kh, 19, SDL_Color{150, 164, 196, 255});
            roundRect(kx + 2, ky + 2, kw - 4, kh - 4, 17, SDL_Color{14, 18, 30, 255});
            textBox("OPTIONS", C_WHITE, 0, kx + 8, ky, kw - 16, kh, 2);
            rect(kx - 14, cy - 15, 2, 30, C_LINE);
            const int gx = kx - 14 - 18 - 14;
            const SDL_Color gf = open ? C_WHITE : C_DIM;
            // ترس (إعدادات): جسم دائري وأسنان حوله وفتحة في الوسط
            roundRect(gx - 13, cy - 13, 26, 26, 13, gf);
            for (int k = 0; k < 8; ++k) {
                const float ang = k * 0.785398f;
                const int tx = gx + (int)(14.0f * std::cos(ang)), ty = cy + (int)(14.0f * std::sin(ang));
                roundRect(tx - 3, ty - 3, 6, 6, 2, gf);
            }
            roundRect(gx - 5, cy - 5, 10, 10, 5, fill);
            textBox(open ? languageText(kCloseSettings)
                         : languageText(kSettings),
                    open ? C_WHITE : C_TEXT, 1, ox + 16, oy + 6, gx - 14 - 12 - (ox + 16), oh - 12, 1);
        }

                if (sceneTex) SDL_SetRenderTarget(r, nullptr);
        sceneDirty = false; sceneAt = now;
        }   // نهاية رسم الشاشة الثابتة
        if (drawScene) {
            const Uint32 sceneDt = SDL_GetTicks() - sceneT0;
        }

        // ---- عرض الشاشة المخزّنة + العناصر المتحركة فوقها ----
        if (sceneTex) SDL_RenderCopy(r, sceneTex, nullptr, nullptr);


        if (drawScene) {
            SDL_RenderPresent(r);
            SDL_UpdateWindowSurface(w);
        }
        
        if (gNotifyIconDrop && SDL_GetTicks() >= gNotifyIconDropAt) { std::remove(NOTIFY_ICON_DST); gNotifyIconDrop = false; }
        if (notifStage == 0) { sysNotify("Welcome To Plugins Manager"); notifStage = 1; notifAt = SDL_GetTicks();  }
        else if (notifStage == 1 && SDL_GetTicks() - notifAt >= 2200) { sysNotify("Powered by ayoub_bl"); notifStage = 2;  }
        {   // keep asking the system to drop the pic1 splash during the first ~6 s (some firmware ignores the very first call)
            static int splashTries = 0; static Uint32 splashAt = 0;
            if (splashTries < 12 && SDL_GetTicks() - splashAt >= 500) {
                splashAt = SDL_GetTicks(); splashTries++;
                sceSystemServiceHideSplashScreen();
            }
        }
        SDL_Delay(8);    // يترك وقتاً لخيط الفحص ولا يستهلك المعالج
    }

    // ---- Shutdown ----
    std::remove(NOTIFY_ICON_DST);   // لا تُبقي نسخة الأيقونة على الجهاز بعد الخروج
    if (!scanDone) {
        SDL_AtomicSet(&gAbort, 1);
        for (int i = 0; i < 100 && SDL_AtomicGet(&gDone) == 0; i++) SDL_Delay(15);
        if (scanThread && SDL_AtomicGet(&gDone) != 0) SDL_WaitThread(scanThread, nullptr);
    }
    stopIconWorker();
    for (auto* t : iconTex) if (t) SDL_DestroyTexture(t);
    for (auto& kv : textCache) if (kv.second.t) SDL_DestroyTexture(kv.second.t);
    for (auto& kv : sliceCache) if (kv.second) SDL_DestroyTexture(kv.second);
    for (auto& kv : shapeCache) if (kv.second.t) SDL_DestroyTexture(kv.second.t);
    for (auto& kv : qrMem) if (kv.second) SDL_DestroyTexture(kv.second);
    for (int i = 0; i < BI_COUNT; i++) if (btnTex[i]) SDL_DestroyTexture(btnTex[i]);
    if (globeTex) SDL_DestroyTexture(globeTex);
    if (aboutQr) SDL_DestroyTexture(aboutQr);
    for (int i = 0; i < 5; i++) fonts[i].shutdown();
    if (moveAudio) SDL_CloseAudioDevice(moveAudio);
    if (moveAudioData) SDL_FreeWAV(moveAudioData);
    if (clickAudioConverted) std::free(clickAudioData);
    else if (clickAudioData) SDL_FreeWAV(clickAudioData);
    if (successAudioConverted) std::free(successAudioData);
    else if (successAudioData) SDL_FreeWAV(successAudioData);
    if (errorAudioConverted) std::free(errorAudioData);
    else if (errorAudioData) SDL_FreeWAV(errorAudioData);
    SDL_DestroyRenderer(r);
    SDL_DestroyWindow(w);
    if (pad) SDL_JoystickClose(pad);
    IMG_Quit();
    SDL_Quit();
    return 0;
}