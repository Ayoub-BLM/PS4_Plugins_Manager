#include "ArabicText.h"
#include <algorithm>
#include <vector>

namespace {

struct Forms { uint32_t cp, iso, fin, ini, med; };
static const Forms forms[] = {
{0x0621,0xFE80,0,0,0},{0x0622,0xFE81,0xFE82,0,0},{0x0623,0xFE83,0xFE84,0,0},{0x0624,0xFE85,0xFE86,0,0},{0x0625,0xFE87,0xFE88,0,0},{0x0626,0xFE89,0xFE8A,0xFE8B,0xFE8C},
{0x0627,0xFE8D,0xFE8E,0,0},{0x0628,0xFE8F,0xFE90,0xFE91,0xFE92},{0x0629,0xFE93,0xFE94,0,0},{0x062A,0xFE95,0xFE96,0xFE97,0xFE98},{0x062B,0xFE99,0xFE9A,0xFE9B,0xFE9C},{0x062C,0xFE9D,0xFE9E,0xFE9F,0xFEA0},
{0x062D,0xFEA1,0xFEA2,0xFEA3,0xFEA4},{0x062E,0xFEA5,0xFEA6,0xFEA7,0xFEA8},{0x062F,0xFEA9,0xFEAA,0,0},{0x0630,0xFEAB,0xFEAC,0,0},{0x0631,0xFEAD,0xFEAE,0,0},{0x0632,0xFEAF,0xFEB0,0,0},
{0x0633,0xFEB1,0xFEB2,0xFEB3,0xFEB4},{0x0634,0xFEB5,0xFEB6,0xFEB7,0xFEB8},{0x0635,0xFEB9,0xFEBA,0xFEBB,0xFEBC},{0x0636,0xFEBD,0xFEBE,0xFEBF,0xFEC0},{0x0637,0xFEC1,0xFEC2,0xFEC3,0xFEC4},{0x0638,0xFEC5,0xFEC6,0xFEC7,0xFEC8},
{0x0639,0xFEC9,0xFECA,0xFECB,0xFECC},{0x063A,0xFECD,0xFECE,0xFECF,0xFED0},{0x0640,0x0640,0x0640,0x0640,0x0640},{0x0641,0xFED1,0xFED2,0xFED3,0xFED4},{0x0642,0xFED5,0xFED6,0xFED7,0xFED8},{0x0643,0xFED9,0xFEDA,0xFEDB,0xFEDC},
{0x0644,0xFEDD,0xFEDE,0xFEDF,0xFEE0},{0x0645,0xFEE1,0xFEE2,0xFEE3,0xFEE4},{0x0646,0xFEE5,0xFEE6,0xFEE7,0xFEE8},{0x0647,0xFEE9,0xFEEA,0xFEEB,0xFEEC},{0x0648,0xFEED,0xFEEE,0,0},{0x0649,0xFEEF,0xFEF0,0,0},{0x064A,0xFEF1,0xFEF2,0xFEF3,0xFEF4}
};

uint32_t decodeOne(const std::string& s, size_t& i) {
    unsigned char c = (unsigned char)s[i++];
    if (c < 0x80) return c;
    int n;
    if ((c & 0xE0) == 0xC0) n = 1;
    else if ((c & 0xF0) == 0xE0) n = 2;
    else if ((c & 0xF8) == 0xF0) n = 3;
    else return 0xFFFD;
    uint32_t v = c & ((1u << (6 - n)) - 1);
    while (n-- && i < s.size()) v = (v << 6) | ((unsigned char)s[i++] & 0x3F);
    return v;
}

std::vector<uint32_t> decodeAll(const std::string& s) {
    std::vector<uint32_t> v;
    for (size_t i = 0; i < s.size();) v.push_back(decodeOne(s, i));
    return v;
}

const Forms* findForm(uint32_t cp) {
    for (const auto& f : forms) if (f.cp == cp) return &f;
    return nullptr;
}

bool isRtlChar(uint32_t cp) { return (cp >= 0x0600 && cp <= 0x08FF) || (cp >= 0xFB50 && cp <= 0xFEFF); }
bool isMark(uint32_t cp) { return (cp >= 0x064B && cp <= 0x065F) || cp == 0x0670 || (cp >= 0x06D6 && cp <= 0x06ED); }

// 1 = strong RTL, 2 = strong LTR, 0 = neutral (spaces, punctuation...)
int strongClass(uint32_t cp) {
    if (isRtlChar(cp)) return 1;
    if (cp < 0x80) return ((cp >= '0' && cp <= '9') || (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z')) ? 2 : 0;
    if (cp >= 0xC0 && cp < 0x0600) return 2;
    return 0;
}

// Resolve the direction of every char: neutrals between two chars of the same
// direction follow them, otherwise they follow the base direction of the text.
std::vector<char> resolveDirections(const std::vector<uint32_t>& v, bool& baseRtl) {
    size_t n = v.size();
    std::vector<int> cls(n);
    baseRtl = true;
    bool found = false;
    for (size_t i = 0; i < n; i++) {
        cls[i] = strongClass(v[i]);
        if (cls[i] && !found) { baseRtl = (cls[i] == 1); found = true; }
    }
    std::vector<int> prevS(n, 0), nextS(n, 0);
    int last = 0;
    for (size_t i = 0; i < n; i++) { if (cls[i]) last = cls[i]; prevS[i] = last; }
    last = 0;
    for (size_t i = n; i-- > 0;) { if (cls[i]) last = cls[i]; nextS[i] = last; }
    std::vector<char> d(n);
    for (size_t i = 0; i < n; i++) {
        if (cls[i]) d[i] = (cls[i] == 1);
        else d[i] = (prevS[i] && prevS[i] == nextS[i]) ? (prevS[i] == 1) : baseRtl;
    }
    return d;
}

// الأقواس: كل زوج أقواس يتبع اتجاه محتواه (مثل "(Plugin)" تبقى كما هي بين نصوص عربية)،
// وإذا صار الزوج RTL تُستبدل الأقواس بمرآتها (لأن الخط يرسم الرمز كما هو) كما يفعل المعيار Unicode.
bool isOpenBracket(uint32_t c) { return c == '(' || c == '[' || c == '{'; }
bool isCloseBracket(uint32_t c) { return c == ')' || c == ']' || c == '}'; }
uint32_t matchingOpen(uint32_t c) { return c == ')' ? '(' : c == ']' ? '[' : '{'; }
uint32_t mirrorBracket(uint32_t c) {
    switch (c) {
        case '(': return ')'; case ')': return '(';
        case '[': return ']'; case ']': return '[';
        case '{': return '}'; case '}': return '{';
        default: return c;
    }
}
void resolveBrackets(std::vector<uint32_t>& v, std::vector<char>& d, bool baseRtl) {
    std::vector<std::pair<size_t, size_t>> pairs;
    std::vector<size_t> stack;
    for (size_t i = 0; i < v.size(); i++) {
        if (isOpenBracket(v[i])) stack.push_back(i);
        else if (isCloseBracket(v[i])) {
            for (size_t k = stack.size(); k-- > 0;) {
                if (v[stack[k]] == matchingOpen(v[i])) { pairs.push_back({stack[k], i}); stack.resize(k); break; }
            }
        }
    }
    const int emb = baseRtl ? 1 : 2, opp = baseRtl ? 2 : 1;
    for (const auto& pr : pairs) {
        bool hasEmb = false, hasOpp = false;
        for (size_t i = pr.first + 1; i < pr.second; i++) {
            const int k = strongClass(v[i]);
            if (k == emb) hasEmb = true;
            else if (k == opp) hasOpp = true;
        }
        if (!hasEmb && !hasOpp) continue;                 // no letters inside: keep the neutral rule
        const bool rtl = hasEmb ? baseRtl : (opp == 1);   // content decides; mixed content follows the base
        d[pr.first] = d[pr.second] = rtl;
    }
    for (size_t i = 0; i < v.size(); i++)
        if ((isOpenBracket(v[i]) || isCloseBracket(v[i])) && d[i]) v[i] = mirrorBracket(v[i]);
}

bool isLigFinal(uint32_t cp) { return cp == 0xFEF6 || cp == 0xFEF8 || cp == 0xFEFA || cp == 0xFEFC; }

// lam + alef (plain / madda / hamza above / hamza below) must become ONE ligature glyph; written as lam-then-alef the
// two shapes meet at an angle and look cut apart. Only used when the font actually has the ligature glyph.
void ligate(std::vector<uint32_t>& v, bool (*has)(void*, uint32_t), void* ctx) {
    for (size_t i = 0; i + 1 < v.size(); i++) {
        if (v[i] != 0x0644) continue;
        uint32_t alef = v[i + 1], iso, fin;
        switch (alef) {
            case 0x0622: iso = 0xFEF5; fin = 0xFEF6; break;
            case 0x0623: iso = 0xFEF7; fin = 0xFEF8; break;
            case 0x0625: iso = 0xFEF9; fin = 0xFEFA; break;
            case 0x0627: iso = 0xFEFB; fin = 0xFEFC; break;
            default: continue;
        }
        bool prevJoins = false;
        size_t p = i;
        while (p > 0) { --p; if (!isMark(v[p])) { const Forms* pf = findForm(v[p]); prevJoins = pf && pf->ini; break; } }
        const uint32_t lig = prevJoins ? fin : iso;
        if (!has(ctx, lig)) continue;
        v[i] = lig;
        v.erase(v.begin() + (long)i + 1);
    }
}

void shape(std::vector<uint32_t>& v) {
    // Decide every letter from the ORIGINAL letters: v[] gets overwritten with presentation forms as we go, and those are
    // no longer found by findForm(), which used to make each letter think its left neighbour never joins (so every
    // letter came out isolated/initial and the text looked cut apart).
    const std::vector<uint32_t> src = v;
    for (size_t i = 0; i < v.size(); i++) {
        const Forms* f = findForm(src[i]);
        if (!f) continue;
        bool prev = false, next = false;
        size_t p = i;
        while (p > 0) { --p; if (!isMark(src[p])) break; }
        if (i > 0 && !isMark(src[p])) {
            const Forms* pf = findForm(src[p]);
            prev = pf && pf->ini && f->fin;
        }
        size_t n = i + 1;
        while (n < v.size() && isMark(src[n])) ++n;
        if (n < v.size()) {
            const Forms* nf = findForm(src[n]);
            next = f->ini && ((nf && nf->fin) || isLigFinal(src[n]));
        }
        uint32_t selected = (prev && next) ? f->med : prev ? f->fin : next ? f->ini : f->iso;
        if (selected) v[i] = selected;
    }
}

} // namespace

bool ArabicText::initialize(const char* path, int px, const char* fallbackPath) {
    if (FT_Init_FreeType(&library_) != 0) return false;
    if (FT_New_Face(library_, path, 0, &face_) != 0) return false;
    if (FT_Set_Char_Size(face_, 0, px * 64, 72, 72) != 0) return false;
    if (fallbackPath && FT_New_Face(library_, fallbackPath, 0, &fallbackFace_) == 0)
        FT_Set_Char_Size(fallbackFace_, 0, px * 64, 72, 72);
    return true;
}

SDL_Texture* ArabicText::texture(SDL_Renderer* renderer, const std::string& text, SDL_Color color) {
    if (!face_ || text.empty()) return nullptr;

    std::vector<uint32_t> cp = decodeAll(text);
    ligate(cp, [](void* c, uint32_t u) { return FT_Get_Char_Index((FT_Face)c, u) != 0; }, face_);
    shape(cp);
    bool baseRtl = true;
    std::vector<char> dir = resolveDirections(cp, baseRtl);
    resolveBrackets(cp, dir, baseRtl);

    // Measure every glyph once.
    struct G { FT_Face face; FT_UInt index; int adv; };   // adv in 26.6 pixels (unhinted, so cursive joins touch exactly)
    std::vector<G> glyphs(cp.size());
    int sum = 0;
    for (size_t i = 0; i < cp.size(); i++) {
        FT_Face f = (FT_Get_Char_Index(face_, cp[i]) || !fallbackFace_) ? face_ : fallbackFace_;
        FT_UInt gi = FT_Get_Char_Index(f, cp[i]);
        int adv = 0;
        if (FT_Load_Glyph(f, gi, FT_LOAD_NO_HINTING) == 0)
            adv = f->glyph->linearHoriAdvance ? (int)(f->glyph->linearHoriAdvance >> 10) : (int)f->glyph->advance.x;
        glyphs[i] = {f, gi, adv};
        sum += adv;
    }

    const int pad = 4;
    int total = std::max(12, ((sum + 63) >> 6) + pad * 2);
    int asc = (int)(face_->size->metrics.ascender >> 6);
    int desc = -(int)(face_->size->metrics.descender >> 6);
    if (fallbackFace_) {
        asc = std::max(asc, (int)(fallbackFace_->size->metrics.ascender >> 6));
        desc = std::max(desc, -(int)(fallbackFace_->size->metrics.descender >> 6));
    }
    int height = asc + desc + 4;
    int baseline = asc + 2;

    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, total, height, 32, SDL_PIXELFORMAT_RGBA32);
    if (!surface) return nullptr;
    SDL_FillRect(surface, nullptr, SDL_MapRGBA(surface->format, 0, 0, 0, 0));

    // Split into runs of the same direction.
    struct Run { size_t begin, end; bool rtl; };
    std::vector<Run> runs;
    for (size_t i = 0; i < cp.size();) {
        size_t j = i + 1;
        while (j < cp.size() && dir[j] == dir[i]) ++j;
        runs.push_back({i, j, dir[i] != 0});
        i = j;
    }

    int cursor = (baseRtl ? total - pad : pad) * 64;
    for (const Run& run : runs) {
        int rw = 0;
        for (size_t i = run.begin; i < run.end; i++) rw += glyphs[i].adv;
        int left, right;
        if (baseRtl) { right = cursor; left = cursor - rw; cursor = left; }
        else         { left = cursor; right = cursor + rw; cursor = right; }
        int pen = run.rtl ? right : left;

        for (size_t i = run.begin; i < run.end; i++) {
            const G& g = glyphs[i];
            int x;
            if (run.rtl) { pen -= g.adv; x = (pen + 32) >> 6; }     // exact 26.6 pen, rounded only once per glyph (no accumulated gaps)
            else         { x = (pen + 32) >> 6; pen += g.adv; }
            if (FT_Load_Glyph(g.face, g.index, FT_LOAD_NO_HINTING) != 0) continue;
            if (FT_Render_Glyph(g.face->glyph, FT_RENDER_MODE_NORMAL) != 0) continue;
            FT_GlyphSlot slot = g.face->glyph;
            int gx = x + slot->bitmap_left;
            int gy = baseline - slot->bitmap_top;
            for (unsigned yy = 0; yy < slot->bitmap.rows; yy++) {
                const unsigned char* row = slot->bitmap.buffer +
                    (slot->bitmap.pitch >= 0 ? (size_t)yy * slot->bitmap.pitch
                                             : (size_t)(slot->bitmap.rows - 1 - yy) * (-slot->bitmap.pitch));
                for (unsigned xx = 0; xx < slot->bitmap.width; xx++) {
                    unsigned char a = row[xx];
                    if (!a) continue;
                    int dx = gx + (int)xx, dy = gy + (int)yy;
                    if (dx < 0 || dx >= surface->w || dy < 0 || dy >= surface->h) continue;
                    Uint8* px = (Uint8*)surface->pixels + (size_t)dy * surface->pitch + (size_t)dx * 4;   // RGBA32: بايتات R,G,B,A
                    if (a > px[3]) { px[0] = color.r; px[1] = color.g; px[2] = color.b; px[3] = a; }
                }
            }
        }
    }

    SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, surface);
    if (t) SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    SDL_FreeSurface(surface);
    return t;
}

void ArabicText::shutdown() {
    if (fallbackFace_) { FT_Done_Face(fallbackFace_); fallbackFace_ = nullptr; }
    if (face_) { FT_Done_Face(face_); face_ = nullptr; }
    if (library_) { FT_Done_FreeType(library_); library_ = nullptr; }
}
