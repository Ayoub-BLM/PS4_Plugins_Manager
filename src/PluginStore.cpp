#include "PluginStore.h"
#include "ZipPackage.h"
#include <orbis/Http.h>
#include <orbis/Net.h>
#include <orbis/Ssl.h>
#include <orbis/Sysmodule.h>
#include <orbis/Bgft.h>
#include <orbis/UserService.h>
#include <orbis/AppInstUtil.h>
#include <orbis/libkernel.h>
#include <cstring>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fstream>
#include <iterator>
#include <map>
#include <sys/stat.h>
#include <atomic>
#include <unistd.h>
#include <arpa/inet.h>
#include <cerrno>
#include <cstdlib>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>

namespace PluginStore {
static const char* CATALOG_URL = "https://raw.githubusercontent.com/Ayoub-BLM/pluginsJSON/refs/heads/main/plugins.json";
static const char* CATALOG_CACHE = "/data/PluginManager/plugins.json";
static const size_t MAX_CATALOG = 512 * 1024;
static const uint64_t MAX_PLUGIN = 128ull * 1024ull * 1024ull;
static const uint64_t MAX_PACKAGE = 256ull * 1024ull * 1024ull;

struct Json {
    int type = 0; // null, bool, number, string, array, object
    std::string text;
    std::vector<Json> array;
    std::map<std::string, Json> object;
    std::vector<std::string> keys;   // object keys in file order (so a rewrite keeps the user's order)
};

// Sets object[k] and remembers the key's position the first time it appears.
static void jsonSetKey(Json& obj, const std::string& k, Json v) {
    if (obj.object.find(k) == obj.object.end()) obj.keys.push_back(k);
    obj.object[k] = std::move(v);
}

class JsonParser {
public:
    explicit JsonParser(const std::string& src) : s(src) {}
    bool parse(Json& out) { ws(); if (!value(out, 0)) return false; ws(); return p == s.size(); }
private:
    const std::string& s; size_t p = 0;
    void ws() { while (p < s.size() && (s[p] == ' ' || s[p] == '\n' || s[p] == '\r' || s[p] == '\t')) ++p; }
    static void utf8(std::string& o, uint32_t c) {
        if (c <= 0x7f) o.push_back((char)c);
        else if (c <= 0x7ff) { o.push_back((char)(0xc0 | (c >> 6))); o.push_back((char)(0x80 | (c & 63))); }
        else if (c <= 0xffff) { o.push_back((char)(0xe0 | (c >> 12))); o.push_back((char)(0x80 | ((c >> 6) & 63))); o.push_back((char)(0x80 | (c & 63))); }
        else { o.push_back((char)(0xf0 | (c >> 18))); o.push_back((char)(0x80 | ((c >> 12) & 63))); o.push_back((char)(0x80 | ((c >> 6) & 63))); o.push_back((char)(0x80 | (c & 63))); }
    }
    bool hex4(uint32_t& v) {
        if (p + 4 > s.size()) return false; v = 0;
        for (int i = 0; i < 4; ++i) { char c = s[p++]; v <<= 4; if (c >= '0' && c <= '9') v |= c - '0'; else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10; else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10; else return false; }
        return true;
    }
    bool string(std::string& out) {
        if (p >= s.size() || s[p++] != '"') return false;
        out.clear();
        while (p < s.size()) {
            unsigned char c = (unsigned char)s[p++];
            if (c == '"') return true;
            if (c < 0x20) return false;
            if (c != '\\') { out.push_back((char)c); continue; }
            if (p >= s.size()) return false;
            char e = s[p++];
            switch (e) {
                case '"': out.push_back('"'); break; case '\\': out.push_back('\\'); break; case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break; case 'f': out.push_back('\f'); break; case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break; case 't': out.push_back('\t'); break;
                case 'u': {
                    uint32_t cp; if (!hex4(cp)) return false;
                    if (cp >= 0xd800 && cp <= 0xdbff) {
                        if (p + 2 > s.size() || s[p++] != '\\' || s[p++] != 'u') return false;
                        uint32_t lo; if (!hex4(lo) || lo < 0xdc00 || lo > 0xdfff) return false;
                        cp = 0x10000 + ((cp - 0xd800) << 10) + (lo - 0xdc00);
                    } else if (cp >= 0xdc00 && cp <= 0xdfff) return false;
                    utf8(out, cp); break;
                }
                default: return false;
            }
        }
        return false;
    }
    bool value(Json& out, int depth) {
        if (depth > 20 || p >= s.size()) return false;
        ws(); if (p >= s.size()) return false;
        if (s[p] == '"') { out.type = 3; return string(out.text); }
        if (s[p] == '{') {
            ++p; out.type = 5; ws(); if (p < s.size() && s[p] == '}') { ++p; return true; }
            for (;;) { ws(); std::string k; if (!string(k)) return false; ws(); if (p >= s.size() || s[p++] != ':') return false; Json v; if (!value(v, depth + 1)) return false; jsonSetKey(out, k, std::move(v)); ws(); if (p >= s.size()) return false; char c = s[p++]; if (c == '}') return true; if (c != ',') return false; }
        }
        if (s[p] == '[') {
            ++p; out.type = 4; ws(); if (p < s.size() && s[p] == ']') { ++p; return true; }
            for (;;) { Json v; if (!value(v, depth + 1)) return false; out.array.push_back(std::move(v)); ws(); if (p >= s.size()) return false; char c = s[p++]; if (c == ']') return true; if (c != ',') return false; }
        }
        size_t a = p; while (p < s.size() && s[p] != ',' && s[p] != ']' && s[p] != '}' && s[p] != ' ' && s[p] != '\r' && s[p] != '\n' && s[p] != '\t') ++p;
        if (a == p) return false; out.text = s.substr(a, p - a);
        if (out.text == "true" || out.text == "false") out.type = 1;
        else if (out.text == "null") out.type = 0;
        else out.type = 2;
        return true;
    }
};

static const Json* field(const Json& o, const char* a, const char* b = nullptr, const char* c = nullptr) {
    if (o.type != 5) return nullptr;
    auto i = o.object.find(a); if (i != o.object.end()) return &i->second;
    if (b) { i = o.object.find(b); if (i != o.object.end()) return &i->second; }
    if (c) { i = o.object.find(c); if (i != o.object.end()) return &i->second; }
    return nullptr;
}
static std::string strField(const Json& o, const char* a, const char* b = nullptr, const char* c = nullptr) {
    const Json* v = field(o, a, b, c); if (!v) return {};
    if (v->type == 3 || v->type == 2 || v->type == 1) return v->text;
    if (v->type == 4) { std::string s; for (const auto& x : v->array) if (x.type == 3) { if (!s.empty()) s += ", "; s += x.text; } return s; }
    return {};
}
static bool safeName(const std::string& n) {
    if (n.size() < 5 || n.size() > 96 || n.find("..") != std::string::npos) return false;
    for (unsigned char c : n) if (!std::isalnum(c) && c != '.' && c != '_' && c != '-') return false;
    std::string l = n; for (char& c : l) c = (char)std::tolower((unsigned char)c);
    return l.size() > 4 && (l.compare(l.size() - 4, 4, ".prx") == 0 || (l.size() > 5 && l.compare(l.size() - 5, 5, ".sprx") == 0));
}
static bool safeInstallPath(const std::string& p) {
    // " . " is an internal marker for the GoldHEN root, never accepted as a
    // user supplied relative path by itself; normalizeInstallPath sets it only
    // for the explicitly allow-listed absolute root below.
    if (p == "." || p == "@data") return true;
    if (p.empty() || p.size() > 480 || p[0] == '/' || p[0] == '\\') return false;
    size_t start = 0;
    for (size_t i = 0; i <= p.size(); ++i) {
        if (i < p.size() && p[i] != '/') {
            unsigned char c = (unsigned char)p[i];
            if (!std::isalnum(c) && c != '_' && c != '-' && c != '.') return false;
            continue;
        }
        if (i == start) return false;
        const std::string part = p.substr(start, i - start);
        if (part == "." || part == "..") return false;
        start = i + 1;
    }
    return true;
}
static bool normalizeInstallPath(std::string& p) {
    if (p == "/data" || p == "/data/") { p = "@data"; return true; }   // ZIP is extracted straight into /data
    if (p == "/data/GoldHEN" || p == "/data/GoldHEN/") { p = "."; return true; }
    if (p == "/data/GoldHEN/plugins" || p == "/data/GoldHEN/plugins/") { p = "."; return true; }
    static const std::string root = "/data/GoldHEN/plugins/";
    if (p.compare(0, root.size(), root) == 0) p.erase(0, root.size());
    return safeInstallPath(p);
}
static bool validUrl(const std::string& u) {
    if (u.size() < 9 || u.size() > 768 || u.compare(0, 8, "https://") != 0) return false;
    for (unsigned char c : u) if (c <= 0x20 || c == '\\') return false;
    return u.find('@', 8) == std::string::npos;
}
static bool validSha(const std::string& s) {
    if (s.size() != 64) return false;
    for (unsigned char c : s) if (!std::isxdigit(c)) return false;
    return true;
}
static std::string urlFilename(const std::string& url) {
    size_t end = url.find_first_of("?#"); if (end == std::string::npos) end = url.size();
    size_t slash = url.rfind('/', end); return slash == std::string::npos ? std::string() : url.substr(slash + 1, end - slash - 1);
}
static bool validTokenKey(const std::string& k) {
    if (k.empty() || k.size() > 96 || k.front() == '.' || k.back() == '.' || k.find("..") != std::string::npos) return false;
    for (unsigned char c : k) if (!std::isalnum(c) && c != '_' && c != '-' && c != '.') return false;
    return true;
}
static std::vector<std::string> splitTokenKey(const std::string& k) {
    std::vector<std::string> out; std::string cur;
    for (char c : k) { if (c == '.') { out.push_back(cur); cur.clear(); } else cur += c; }
    out.push_back(cur);
    return out;
}
static void jsonEscapeTo(const std::string& s, std::string& o) {
    o += '"';
    for (unsigned char c : s) {
        if (c == '"') o += "\\\"";
        else if (c == '\\') o += "\\\\";
        else if (c == '\n') o += "\\n";
        else if (c == '\r') o += "\\r";
        else if (c == '\t') o += "\\t";
        else if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", (unsigned)c); o += b; }
        else o += (char)c;
    }
    o += '"';
}
// Pretty-printed output: one key per line, 2-space indent, so the file stays readable by hand.
static void jsonIndent(std::string& o, int depth) { o.append((size_t)depth * 2, ' '); }
static void jsonSerializeTo(const Json& j, std::string& o, int depth = 0) {
    switch (j.type) {
    case 0: o += "null"; break;
    case 1: o += j.text.empty() ? std::string("false") : j.text; break;
    case 2: o += j.text.empty() ? std::string("0") : j.text; break;
    case 3: jsonEscapeTo(j.text, o); break;
    case 4: {
        if (j.array.empty()) { o += "[]"; break; }
        o += "[\n";
        for (size_t i = 0; i < j.array.size(); ++i) {
            if (i) o += ",\n";
            jsonIndent(o, depth + 1);
            jsonSerializeTo(j.array[i], o, depth + 1);
        }
        o += '\n'; jsonIndent(o, depth); o += ']'; break;
    }
    case 5: {
        if (j.keys.empty()) { o += "{}"; break; }
        o += "{\n";
        bool first = true;
        for (const std::string& k : j.keys) {
            auto it = j.object.find(k);
            if (it == j.object.end()) continue;
            if (!first) o += ",\n";
            first = false;
            jsonIndent(o, depth + 1);
            jsonEscapeTo(k, o); o += ": ";
            jsonSerializeTo(it->second, o, depth + 1);
        }
        o += '\n'; jsonIndent(o, depth); o += '}'; break;
    }
    }
}
static bool loadJsonDoc(const std::string& path, Json& doc) {
    std::ifstream f(path, std::ios::binary); if (!f) return false;
    std::string src((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    JsonParser ps(src);
    return ps.parse(doc) && doc.type == 5;
}
bool readTokenJson(const std::string& path, const std::string& key, std::string& out) {
    out.clear();
    if (!validTokenKey(key)) return false;
    Json doc;
    if (!loadJsonDoc(path, doc)) return false;
    const Json* cur = &doc;
    for (const auto& seg : splitTokenKey(key)) {
        if (cur->type != 5) return false;
        auto it = cur->object.find(seg);
        if (it == cur->object.end()) return false;
        cur = &it->second;
    }
    if (cur->type == 3 || cur->type == 2) { out = cur->text; return true; }
    return false;
}
bool writeTokenJson(const std::string& path, const std::string& key, const std::string& value) {
    if (!validTokenKey(key)) return false;
    Json doc; doc.type = 5;
    struct stat st{};
    if (stat(path.c_str(), &st) == 0) {
        if (!loadJsonDoc(path, doc)) return false;   // لا نكتب فوق ملف JSON غير صالح
    }
    auto segs = splitTokenKey(key);
    Json* cur = &doc;
    for (size_t i = 0; i + 1 < segs.size(); ++i) {
        auto it = cur->object.find(segs[i]);
        if (it == cur->object.end() || it->second.type != 5) {   // ينشئ الكائن الناقص
            Json obj; obj.type = 5;
            jsonSetKey(*cur, segs[i], std::move(obj));
        }
        cur = &cur->object[segs[i]];
    }
    Json v; v.type = 3; v.text = value;
    jsonSetKey(*cur, segs.back(), std::move(v));
    std::string out; jsonSerializeTo(doc, out); out += "\n";
    const std::string tmp = path + ".tmp";
    FILE* f = std::fopen(tmp.c_str(), "wb"); if (!f) return false;
    bool ok = std::fwrite(out.data(), 1, out.size(), f) == out.size();
    ok = (std::fclose(f) == 0) && ok;
    if (!ok) { unlink(tmp.c_str()); return false; }
    return std::rename(tmp.c_str(), path.c_str()) == 0;
}
// ---------- INI token files ----------
// tokenKey "token"          -> line "token = ..." before any [section]
// tokenKey "Auth.token"     -> line "token = ..." inside [Auth]  (the last dot separates section and name)
// Matching is case-insensitive. Comments (; or #), other sections and other keys are kept as they are.
static std::string iniTrim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) ++a;
    while (b > a && std::isspace((unsigned char)s[b - 1])) --b;
    return s.substr(a, b - a);
}
static std::string iniLower(std::string s) { for (char& c : s) c = (char)std::tolower((unsigned char)c); return s; }
static void iniSplitKey(const std::string& k, std::string& section, std::string& name) {
    size_t dot = k.rfind('.');
    if (dot == std::string::npos) { section.clear(); name = k; }
    else { section = k.substr(0, dot); name = k.substr(dot + 1); }
}
// Returns the section name of a "[name]" header line, or false if the line is not a header.
static bool iniHeader(const std::string& t, std::string& name) {
    if (t.empty() || t[0] != '[') return false;
    size_t e = t.find(']');
    name = iniTrim(t.substr(1, e == std::string::npos ? std::string::npos : e - 1));
    return true;
}
static bool readIniLines(const std::string& path, std::vector<std::string>& lines) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::string line; bool first = true;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (first && line.compare(0, 3, "\xEF\xBB\xBF") == 0) line.erase(0, 3);   // UTF-8 BOM
        first = false;
        lines.push_back(line);
    }
    return true;
}
bool readTokenIni(const std::string& path, const std::string& key, std::string& out) {
    out.clear();
    if (!validTokenKey(key)) return false;
    std::string section, name; iniSplitKey(key, section, name);
    std::vector<std::string> lines;
    if (!readIniLines(path, lines)) return false;
    std::string cur;   // current section ("" = before any header)
    for (const std::string& raw : lines) {
        std::string t = iniTrim(raw);
        if (t.empty() || t[0] == ';' || t[0] == '#') continue;
        std::string hdr;
        if (iniHeader(t, hdr)) { cur = hdr; continue; }
        if (iniLower(cur) != iniLower(section)) continue;
        size_t eq = t.find('=');
        if (eq == std::string::npos) continue;
        if (iniLower(iniTrim(t.substr(0, eq))) != iniLower(name)) continue;
        std::string v = iniTrim(t.substr(eq + 1));
        if (v.size() >= 2 && (v[0] == '"' || v[0] == '\'') && v.back() == v[0]) v = v.substr(1, v.size() - 2);
        out = v;
        return true;
    }
    return false;
}
bool writeTokenIni(const std::string& path, const std::string& key, const std::string& value) {
    if (!validTokenKey(key)) return false;
    std::string section, name; iniSplitKey(key, section, name);
    std::string val = value;
    for (char& c : val) if (c == '\r' || c == '\n') c = ' ';   // one line only
    val = iniTrim(val);

    std::vector<std::string> lines;
    struct stat st{};
    if (stat(path.c_str(), &st) == 0 && !readIniLines(path, lines)) return false;

    // Pass 1: replace the value in place if the key already exists.
    std::string cur;
    size_t firstHeader = lines.size();
    bool replaced = false;
    for (size_t i = 0; i < lines.size() && !replaced; ++i) {
        std::string t = iniTrim(lines[i]);
        std::string hdr;
        if (iniHeader(t, hdr)) { if (firstHeader == lines.size()) firstHeader = i; cur = hdr; continue; }
        if (t.empty() || t[0] == ';' || t[0] == '#') continue;
        if (iniLower(cur) != iniLower(section)) continue;
        size_t eq = lines[i].find('=');
        if (eq == std::string::npos) continue;
        if (iniLower(iniTrim(lines[i].substr(0, eq))) != iniLower(name)) continue;
        std::string keyPart = lines[i].substr(0, eq);
        while (!keyPart.empty() && std::isspace((unsigned char)keyPart.back())) keyPart.pop_back();
        lines[i] = keyPart + " = " + val;
        replaced = true;
    }

    // Pass 2: the key is missing. Insert it at the end of its section (or create the section).
    if (!replaced && section.empty()) {
        // global keys must come before the first [section] header
        size_t at = firstHeader;
        while (at > 0 && iniTrim(lines[at - 1]).empty()) --at;
        lines.insert(lines.begin() + (long)at, name + " = " + val);
    } else if (!replaced) {
        size_t secStart = lines.size(), secEnd = lines.size();
        bool found = false;
        for (size_t i = 0; i < lines.size(); ++i) {
            std::string hdr;
            if (iniHeader(iniTrim(lines[i]), hdr)) {
                if (found) { secEnd = i; break; }
                if (iniLower(hdr) == iniLower(section)) { found = true; secStart = i; }
            }
        }
        if (found) {
            size_t at = secEnd;
            while (at > secStart + 1 && iniTrim(lines[at - 1]).empty()) --at;
            lines.insert(lines.begin() + (long)at, name + " = " + val);
        } else {
            if (!lines.empty() && !iniTrim(lines.back()).empty()) lines.push_back("");
            lines.push_back("[" + section + "]");
            lines.push_back(name + " = " + val);
        }
    }

    std::string out;
    for (const std::string& l : lines) { out += l; out += '\n'; }
    const std::string tmp = path + ".tmp";
    FILE* f = std::fopen(tmp.c_str(), "wb"); if (!f) return false;
    bool ok = std::fwrite(out.data(), 1, out.size(), f) == out.size();
    ok = (std::fclose(f) == 0) && ok;
    if (!ok) { unlink(tmp.c_str()); return false; }
    return std::rename(tmp.c_str(), path.c_str()) == 0;
}
static bool validTokenPath(const std::string& p) {
    // Token file: absolute .txt path under /data/ (or a mounted USB). Keeps a remote catalog from
    // pointing the writer at plugins, plugins.ini or anything outside the console's data area.
    if (p.size() < 12 || p.size() > 240 || p.find("..") != std::string::npos || p.find("//") != std::string::npos) return false;
    if (p.compare(0, 6, "/data/") != 0 && p.compare(0, 5, "/mnt/") != 0) return false;
    const bool isTxt = p.size() > 4 && p.compare(p.size() - 4, 4, ".txt") == 0;
    const bool isJson = p.size() > 5 && p.compare(p.size() - 5, 5, ".json") == 0;
    const bool isIni = p.size() > 4 && p.compare(p.size() - 4, 4, ".ini") == 0;
    if (!isTxt && !isJson && !isIni) return false;
    for (unsigned char c : p) if (!std::isalnum(c) && c != '/' && c != '.' && c != '_' && c != '-') return false;
    // Never let a catalog aim the writer at the app's own config files (GoldHEN's plugins.ini, language.ini, app data).
    if (p.rfind("/data/GoldHEN/", 0) == 0) return false;
    if (p.rfind("/data/PluginManager/", 0) == 0) return false;
    const size_t slash = p.rfind('/');
    const std::string base = slash == std::string::npos ? p : p.substr(slash + 1);
    if (base == "plugins.ini" || base == "language.ini") return false;
    return true;
}
static bool boolField(const Json* v) {
    if (!v) return false;
    if (v->type == 1) return v->text == "true";
    if (v->type == 3) { std::string t = v->text; for (char& ch : t) ch = (char)std::tolower((unsigned char)ch); return t == "true" || t == "yes" || t == "1" || t == "on"; }
    if (v->type == 2) return v->text == "1";
    return false;
}
// packageUrl may point to a plain .txt file instead of a ZIP: it is then copied as-is into installPath.
static bool isTextPackage(const std::string& url) {
    std::string n = urlFilename(url); for (char& c : n) c = (char)std::tolower((unsigned char)c);
    return n.size() > 4 && n.compare(n.size() - 4, 4, ".txt") == 0;
}
static bool safeTxtName(const std::string& n) {
    if (n.size() < 5 || n.size() > 96 || n.find("..") != std::string::npos) return false;
    for (unsigned char c : n) if (!std::isalnum(c) && c != '.' && c != '_' && c != '-') return false;
    return true;
}
static std::string gParsedUpdated;   // filled by parseCatalog (only the catalog worker thread touches it)
static std::string nowStamp() {      // device clock, "YYYY-MM-DD HH:MM"; empty if the clock looks unset
    time_t t = time(nullptr); struct tm* m = localtime(&t); char b[32];
    if (!m || m->tm_year < 120 || !strftime(b, sizeof b, "%Y-%m-%d %H:%M", m)) return {};
    return b;
}
static bool parseCatalog(const std::string& src, std::vector<Entry>& out, std::string& error) {
    out.clear(); gParsedUpdated.clear();
    size_t first = src.find_first_not_of(" \r\n\t");
    if (first == std::string::npos) return true; // ملف المستودع فارغ حالياً
    Json root; JsonParser p(src); if (!p.parse(root)) { error = "JSON is invalid"; return false; }
    if (root.type == 5) {   // optional "last update" field written by the catalog owner
        std::string u = strField(root, "lastUpdate", "updated", "lastUpdated");
        if (u.empty()) u = strField(root, "updatedAt", "last_update", "update");
        if (u.size() > 32) u.resize(32);
        gParsedUpdated = u;
    }
    const std::vector<Json>* rows = nullptr;
    if (root.type == 4) rows = &root.array;
    else if (root.type == 5) {
        const Json* a = field(root, "plugins", "Plugins");
        if (a && a->type == 4) rows = &a->array;
        else if (root.object.empty()) { static const std::vector<Json> empty; rows = &empty; }
    }
    if (!rows) { error = "Expected a plugins array"; return false; }
    if (rows->size() > 200) { error = "Catalog exceeds 200 entries"; return false; }
    for (const Json& o : *rows) {
        Entry e;
        e.name = strField(o, "name", "Name");
        e.version = strField(o, "version", "Version");
        e.description = strField(o, "description", "descreption", "descreption");
        e.descriptionAr = strField(o, "descriptionAr", "description_ar", "descriptionArabic");
        if (e.descriptionAr.empty()) e.descriptionAr = strField(o, "arabicDescription", "desc_ar", "الوصف");
        e.author = strField(o, "author", "Author");
        e.urlDownload = strField(o, "urlDownload", "url_download");
        e.url2download = strField(o, "url2downlouad", "url2download", "url2Download");
        e.packageUrl = strField(o, "packageUrl", "zipUrl", "packageURL");
        e.installPath = strField(o, "installPath", "install_path", "packagePath");
        // Treat JSON false/null as an omitted optional second file, not as the URL "false".
        if (e.url2download == "false" || e.url2download == "null") e.url2download.clear();
        if (e.packageUrl == "false" || e.packageUrl == "null") e.packageUrl.clear();
        e.pkgUrl = strField(o, "pkg");   // optional: "false", "null" or an invalid URL means no PKG
        if (e.pkgUrl == "false" || e.pkgUrl == "null" || !Rpi::isValidPkgUrl(e.pkgUrl)) e.pkgUrl.clear();
        if (e.installPath == "false" || e.installPath == "null") e.installPath.clear();
        e.support = strField(o, "support", "suport of plugins", "suport");
        if (e.support.empty()) e.support = strField(o, "suport of plugins(defult)", "supportOfPlugins", "supportedGames");
        e.sha256 = strField(o, "sha256", "SHA256", "checksum");
        { const std::string sz = strField(o, "size", "sizeBytes", "fileSize"); e.sizeBytes = sz.empty() ? 0ULL : std::strtoull(sz.c_str(), nullptr, 10); }
        e.sha256_2 = strField(o, "sha256_2", "sha2562", "checksum2");
        e.packageSha256 = strField(o, "packageSha256", "zipSha256", "package_sha256");
        e.supportQR = strField(o, "supportQR", "supportQr", "qrCodeUrl");
        e.qrText = strField(o, "QRtext", "qrText", "QRText");
        if (e.supportQR.empty()) e.supportQR = strField(o, "supportImage", "support_image", "qr");
        e.id = strField(o, "id", "ID");
        // Token editor: only when the JSON turns it on AND gives a usable .txt path (otherwise silently off).
        e.tokenPath = strField(o, "tokenPath", "token_path", "tokenFile");
        e.tokenEnabled = boolField(field(o, "tokenEnabled", "token_enabled", "editToken")) && validTokenPath(e.tokenPath);
        if (!e.tokenEnabled) e.tokenPath.clear();
        e.tokenKey = strField(o, "tokenKey", "token_key");
        if (e.tokenKey.empty() || !validTokenKey(e.tokenKey)) e.tokenKey = "token";
        e.filename = e.name; // name is the primary local filename, including .prx/.sprx

        e.filename2 = strField(o, "filename2", "name2", "file2");
        if (e.filename2.empty() && !e.url2download.empty()) e.filename2 = urlFilename(e.url2download);
        if (e.id.empty()) e.id = e.filename;
        const bool package = !e.packageUrl.empty();
        if (package && !normalizeInstallPath(e.installPath)) { error = "Package installPath must be /data/, /data/GoldHEN/, /data/GoldHEN/plugins/, or a safe subfolder"; return false; }
        if (package && isTextPackage(e.packageUrl) && !safeTxtName(urlFilename(e.packageUrl))) { error = "Text package filename is not valid"; return false; }
        const bool validDownload = package
            ? validUrl(e.packageUrl) && validUrl(e.urlDownload) && safeInstallPath(e.installPath)
            : validUrl(e.urlDownload);
        if (e.id.empty() || e.id.size() > 96 || e.name.empty() || e.name.size() > 128 ||
            e.version.size() > 48 || e.description.size() > 2000 || e.descriptionAr.size() > 3000 || e.author.size() > 96 || e.support.size() > 200 ||
            !safeName(e.filename) || !validDownload || (!e.supportQR.empty() && !validUrl(e.supportQR)) ||
            (!e.url2download.empty() && (!validUrl(e.url2download) || !safeName(e.filename2) || e.filename2 == e.filename)) ||
            (!e.sha256.empty() && !validSha(e.sha256)) || (!e.sha256_2.empty() && !validSha(e.sha256_2)) ||
            (!e.packageSha256.empty() && !validSha(e.packageSha256))) {
            error = "Entry needs a filename and valid HTTPS file/package URL; package installPath and hashes must be valid"; return false;
        }
        out.push_back(std::move(e));
    }
    return true;
}

class Sha256 {
    uint32_t h[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    uint64_t bits = 0; unsigned char block[64]{}; size_t used = 0;
    static uint32_t r(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
    void transform(const unsigned char* b) {
        static const uint32_t k[64] = {0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
        uint32_t w[64]; for (int i=0;i<16;i++) w[i]=((uint32_t)b[i*4]<<24)|((uint32_t)b[i*4+1]<<16)|((uint32_t)b[i*4+2]<<8)|b[i*4+3];
        for (int i=16;i<64;i++){uint32_t x=w[i-15],y=w[i-2];w[i]=w[i-16]+(r(x,7)^r(x,18)^(x>>3))+w[i-7]+(r(y,17)^r(y,19)^(y>>10));}
        uint32_t a=h[0],bb=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],z=h[7];
        for(int i=0;i<64;i++){uint32_t t1=z+(r(e,6)^r(e,11)^r(e,25))+((e&f)^(~e&g))+k[i]+w[i];uint32_t t2=(r(a,2)^r(a,13)^r(a,22))+((a&bb)^(a&c)^(bb&c));z=g;g=f;f=e;e=d+t1;d=c;c=bb;bb=a;a=t1+t2;}
        h[0]+=a;h[1]+=bb;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=z;
    }
public:
    void update(const unsigned char* p, size_t n) { bits += (uint64_t)n * 8; while(n){size_t take=std::min(n,64-used);std::memcpy(block+used,p,take);used+=take;p+=take;n-=take;if(used==64){transform(block);used=0;}} }
    std::string finish() { uint64_t original=bits; unsigned char one=0x80;update(&one,1);unsigned char zero=0;while(used!=56)update(&zero,1);unsigned char len[8];for(int i=0;i<8;i++)len[7-i]=(unsigned char)(original>>(i*8));update(len,8);static const char hex[]="0123456789abcdef";std::string out;out.reserve(64);for(uint32_t x:h)for(int i=7;i>=0;i--)out.push_back(hex[(x>>(i*4))&15]);return out; }
};

struct Http {
    int pool=-1, ssl=-1, ctx=-1;
    bool init() {
        if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NET) < 0 ||
            sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_HTTP) < 0 ||
            sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SSL) < 0) return false;
        // قد تكون الشبكة بدأت مسبقاً لمسح FTP؛ فشل Already initialized لا يمنع إنشاء pool مستقل.
        sceNetInit();
        pool = sceNetPoolCreate("pluginmgr-store", 64 * 1024, 0); if (pool < 0) return false;
        ssl = sceSslInit(SSL_POOLSIZE); if (ssl < 0) return false;
        ctx = sceHttpInit(pool, ssl, LIBHTTP_POOLSIZE); return ctx >= 0;
    }
    void close() { if (ctx >= 0) { sceHttpTerm(ctx); ctx=-1; } if (ssl >= 0) { sceSslTerm(); ssl=-1; } if (pool >= 0) { sceNetPoolDestroy(pool); pool=-1; } }
    ~Http() { close(); }
};

typedef bool (*SinkFn)(void*, const unsigned char*, size_t);
typedef void (*ProgressFn)(void*, uint64_t, uint64_t);
static bool getUrl(Http& h, const std::string& url, uint64_t maxBytes, SDL_atomic_t* cancel,
                   SinkFn sink, void* sinkCtx, ProgressFn progress, void* progressCtx, int& status, std::string& error) {
    if (!validUrl(url)) { error="HTTPS URL required"; return false; }
    int tpl=sceHttpCreateTemplate(h.ctx,"GoldHEN-Plugin-Manager/1.0",ORBIS_HTTP_VERSION_1_1,1);
    if(tpl<0){error="HTTP template failed";return false;}
    sceHttpSetResolveTimeOut(tpl,15u*1000000u); sceHttpSetConnectTimeOut(tpl,20u*1000000u); sceHttpSetSendTimeOut(tpl,20u*1000000u);
    // Header lacks the generated prototype for this SDK function; use the documented (id, usec) ABI.
    typedef int32_t (*TimeoutFn)(int32_t,uint32_t);
    reinterpret_cast<TimeoutFn>(&sceHttpSetRecvTimeOut)(tpl,20u*1000000u);
    int conn=sceHttpCreateConnectionWithURL(tpl,url.c_str(),0);
    if(conn<0){sceHttpDeleteTemplate(tpl);error="HTTP connection failed";return false;}
    int req=sceHttpCreateRequestWithURL(conn,ORBIS_METHOD_GET,url.c_str(),0);
    if(req<0){sceHttpDeleteConnection(conn);sceHttpDeleteTemplate(tpl);error="HTTP request failed";return false;}
    sceHttpAddRequestHeader(req,"Accept","application/json, application/octet-stream;q=0.9, */*;q=0.8",0);
    sceHttpAddRequestHeader(req,"Cache-Control","no-cache, no-store",0);
    sceHttpAddRequestHeader(req,"Pragma","no-cache",0);
    bool ok=false; uint64_t total=0, received=0; int type=ORBIS_HTTP_CONTENTLEN_NOT_FOUND; size_t length=0;
    if(SDL_AtomicGet(cancel)){error="Canceled";goto done;}
    if(sceHttpSendRequest(req,nullptr,0)<0){error="HTTPS request failed (network or certificate)";goto done;}
    if(sceHttpGetStatusCode(req,&status)<0){error="Could not read HTTP status";goto done;}
    if(status<200||status>=300){error="HTTP status "+std::to_string(status);goto done;}
    if(sceHttpGetResponseContentLength(req,&type,&length)>=0 && type==ORBIS_HTTP_CONTENTLEN_EXIST) total=(uint64_t)length;
    if(total>maxBytes){error="Response exceeds size limit";goto done;}
    {
        unsigned char buf[32768];
        for(;;){if(SDL_AtomicGet(cancel)){error="Canceled";goto done;}int n=sceHttpReadData(req,buf,sizeof(buf));if(n<0){error="HTTPS read failed";goto done;}if(n==0)break;
            received+=(uint64_t)n;if(received>maxBytes){error="Response exceeds size limit";goto done;}
            if(!sink(sinkCtx,buf,(size_t)n)){error="Could not write response";goto done;}
            if(progress)progress(progressCtx,received,total);
        }
    }
    ok=true; // An empty HTTP 200 catalog is valid; downloads are checked separately.
done:
    sceHttpDeleteRequest(req); sceHttpDeleteConnection(conn); sceHttpDeleteTemplate(tpl); return ok;
}

struct StringSink { std::string* out; size_t max; };
static bool appendString(void* c,const unsigned char* p,size_t n){auto* s=(StringSink*)c;if(s->out->size()+n>s->max)return false;s->out->append((const char*)p,n);return true;}
struct FileSink { FILE* f; Sha256 sha; uint64_t bytes=0; };
static bool writeFile(void* c,const unsigned char* p,size_t n){auto* f=(FileSink*)c;if(std::fwrite(p,1,n,f->f)!=n)return false;f->sha.update(p,n);f->bytes+=n;return true;}

struct Client::Job { Client* owner; int kind; Entry entry; std::string directory; std::string outputPath; bool replace = false; int gen = 0; };
Client::Client() { mutex_=SDL_CreateMutex(); }
Client::~Client() { shutdown(); }
bool Client::initialize() {
    // QR images are no longer stored: remove leftovers (qr_*.png / .part) saved by earlier versions.
    if (DIR* d = opendir("/data/PluginManager")) {
        std::vector<std::string> old;
        while (struct dirent* de = readdir(d)) {
            const std::string n = de->d_name;
            if (n.size() > 4 && n.compare(0, 3, "qr_") == 0 && (n.compare(n.size() - 4, 4, ".png") == 0 || n.compare(n.size() - 5 < n.size() ? n.size() - 5 : 0, 5, ".part") == 0)) old.push_back(n);
        }
        closedir(d);
        for (const auto& n : old) unlink(("/data/PluginManager/" + n).c_str());
    }
    return mutex_ != nullptr;
}
void Client::publish(Phase p,const std::string& m){SDL_LockMutex(mutex_);phase_=p;message_=m;SDL_UnlockMutex(mutex_);}
void Client::onProgress(void* opaque,uint64_t received,uint64_t total){Client* self=(Client*)opaque;SDL_LockMutex(self->mutex_);self->received_=received;self->total_=total;SDL_UnlockMutex(self->mutex_);}
void Client::reap(){if(!thread_)return;SDL_LockMutex(mutex_);bool done=done_;SDL_UnlockMutex(mutex_);if(done){SDL_WaitThread(thread_,nullptr);thread_=nullptr;}}
bool Client::refreshCatalog(bool reset){if(!mutex_)return false;reap();if(thread_){SDL_DetachThread(thread_);thread_=nullptr;}SDL_AtomicAdd(&gen_,1);const int myGen=SDL_AtomicGet(&gen_);SDL_AtomicSet(&cancel_,0);SDL_LockMutex(mutex_);if(reset){entries_.clear();catalogLoaded_=false;qrUrl_.clear();qrPath_.clear();qrBytes_.clear();}phase_=LoadingCatalog;message_="Loading catalog";received_=total_=0;done_=false;SDL_UnlockMutex(mutex_);Job* j=new Job{this,0,Entry(),{},{},false,myGen};thread_=SDL_CreateThread(worker,"plugin-catalog",j);if(!thread_){delete j;publish(Failed,"Could not start network thread");return false;}return true;}
bool Client::loadSupportQr(const Entry& e){
    if(!mutex_||e.supportQR.empty()||!validUrl(e.supportQR))return false;
    reap();SDL_LockMutex(mutex_);
    if(qrUrl_==e.supportQR){if(!qrBytes_.empty()){SDL_UnlockMutex(mutex_);return true;}if(message_=="QR load failed"){SDL_UnlockMutex(mutex_);return false;}}
    if(thread_){SDL_UnlockMutex(mutex_);return false;}
    SDL_AtomicSet(&cancel_,0);qrUrl_=e.supportQR;qrPath_.clear();qrBytes_.clear();phase_=LoadingQr;message_="Loading support QR";received_=total_=0;done_=false;SDL_UnlockMutex(mutex_);
    Job* j=new Job{this,2,e,{},{}};thread_=SDL_CreateThread(worker,"plugin-support-qr",j);
    if(!thread_){delete j;SDL_LockMutex(mutex_);phase_=Ready;message_="QR load failed";done_=true;SDL_UnlockMutex(mutex_);return false;}return true;
}
bool Client::takeQrBytes(const std::string& url,std::vector<unsigned char>& out){
    if(!mutex_)return false;
    SDL_LockMutex(mutex_);
    if(qrUrl_!=url||qrBytes_.empty()){SDL_UnlockMutex(mutex_);return false;}
    out.assign(qrBytes_.begin(),qrBytes_.end());qrBytes_.clear();
    SDL_UnlockMutex(mutex_);return true;
}
bool Client::exists(const Entry& e,const std::string& dir){
    struct stat st{};
    if(!e.filename.empty()&&stat((dir+"/"+e.filename).c_str(),&st)==0)return true;
    if(!e.url2download.empty()&&!e.filename2.empty()&&stat((dir+"/"+e.filename2).c_str(),&st)==0)return true;
    if(!e.packageUrl.empty()&&e.installPath!="."&&e.installPath!="@data"&&!e.installPath.empty()&&stat((dir+"/"+e.installPath).c_str(),&st)==0)return true;
    return false;
}
int Client::installedState(const Entry& e,const std::string& dir){
    if(!exists(e,dir)) return 0;
    if(e.filename.empty()) return 3;   // الحزم ZIP: الملف المثبت هو نفس ملف الإضافة (.prx/.sprx) فنقارنه أيضاً
    const std::string path=dir+"/"+e.filename;
    struct stat st{};
    if(stat(path.c_str(),&st)!=0) return 3;
    if(e.sha256.empty()) {   // لا هاش في الكتالوج: نقارن الحجم إن وُجد
        if(e.sizeBytes==0) return 3;
        return (unsigned long long)st.st_size==e.sizeBytes?1:2;
    }
    // الهاش يُحسب مرة واحدة لكل ملف (يُعاد الحساب فقط إذا تغيّر حجم الملف أو وقت تعديله)
    static std::map<std::string,std::pair<std::string,std::string>> hashCache;
    const std::string stamp=std::to_string((long long)st.st_size)+":"+std::to_string((long long)st.st_mtime);
    std::string hex;
    auto hit=hashCache.find(path);
    if(hit!=hashCache.end()&&hit->second.first==stamp) hex=hit->second.second;
    else {
        FILE* f=fopen(path.c_str(),"rb");
        if(!f) return 3;
        Sha256 sha; std::vector<unsigned char> buf(64*1024);
        size_t n=0;
        while((n=fread(buf.data(),1,buf.size(),f))>0) sha.update(buf.data(),n);
        fclose(f);
        hex=sha.finish();
        hashCache[path]={stamp,hex};
    }
    std::string want=e.sha256;
    for(auto& ch:want) ch=(char)std::tolower((unsigned char)ch);
    return hex==want?1:2;
}
bool Client::download(const Entry& e,const std::string& dir,bool replace){const bool package=!e.packageUrl.empty();if(!mutex_||!safeName(e.filename)||(package?(!validUrl(e.packageUrl)||!validUrl(e.urlDownload)||!safeInstallPath(e.installPath)):!validUrl(e.urlDownload))||(!e.url2download.empty()&&(!safeName(e.filename2)||!validUrl(e.url2download)))||(!e.sha256.empty()&&!validSha(e.sha256))||(!e.sha256_2.empty()&&!validSha(e.sha256_2))||(!e.packageSha256.empty()&&!validSha(e.packageSha256)))return false;reap();if(thread_)return false;SDL_AtomicSet(&cancel_,0);SDL_LockMutex(mutex_);phase_=Downloading;message_="Downloading";received_=total_=0;done_=false;installedId_.clear();SDL_UnlockMutex(mutex_);Job* j=new Job{this,1,e,dir,{},replace};thread_=SDL_CreateThread(worker,"plugin-download",j);if(!thread_){delete j;publish(Failed,"Could not start download thread");return false;}return true;}
void Client::settle(){
    if(!mutex_)return;
    reap();
    SDL_LockMutex(mutex_);
    if(!thread_&&(phase_==Installed||phase_==Failed||phase_==Canceled||phase_==Offline)){
        phase_=catalogLoaded_?Ready:Idle;
        message_.clear();
        installedId_.clear();
    }
    SDL_UnlockMutex(mutex_);
}
void Client::cancel(){SDL_AtomicSet(&cancel_,1);}
void Client::answerConfirm(int answer){SDL_AtomicSet(&answer_,answer);}
// فلترة البحث: الاسم والمعرّف، بدون تمييز لحالة الأحرف الإنجليزية
static std::string gStoreFilter;
static std::string lowerAscii(std::string s) { for (auto& ch : s) if ((unsigned char)ch < 128) ch = (char)std::tolower((unsigned char)ch); return s; }
void setStoreFilter(const std::string& q) { gStoreFilter = lowerAscii(q); }
// الترتيب: الاسم يبدأ بالبحث (0) ثم المعرّف يبدأ به (1) ثم الاسم يحتويه (2) ثم المعرّف يحتويه (3)
static int matchRank(const Entry& e) {
    const std::string n = lowerAscii(e.name), id = lowerAscii(e.id);
    if (n.compare(0, gStoreFilter.size(), gStoreFilter) == 0) return 0;
    if (id.compare(0, gStoreFilter.size(), gStoreFilter) == 0) return 1;
    if (n.find(gStoreFilter) != std::string::npos) return 2;
    return 3;
}
static void applyStoreFilter(std::vector<Entry>& v) {
    if (gStoreFilter.empty()) return;
    std::vector<std::pair<int, Entry>> ranked;
    for (const auto& e : v) {
        const int r = matchRank(e);
        if (r < 3 || lowerAscii(e.id).find(gStoreFilter) != std::string::npos) ranked.emplace_back(r, e);
    }
    std::stable_sort(ranked.begin(), ranked.end(), [](const std::pair<int, Entry>& a, const std::pair<int, Entry>& b) { return a.first < b.first; });
    v.clear();
    for (auto& p : ranked) v.push_back(std::move(p.second));
}
Snapshot Client::snapshot(){reap();SDL_LockMutex(mutex_);Snapshot s{phase_,entries_,received_,total_,message_,catalogLoaded_,qrUrl_,qrPath_,installedId_,catalogUpdated_};SDL_UnlockMutex(mutex_);applyStoreFilter(s.entries);return s;}
void Client::shutdown(){if(thread_){cancel();SDL_WaitThread(thread_,nullptr);thread_=nullptr;}if(mutex_){SDL_DestroyMutex(mutex_);mutex_=nullptr;}}
int Client::worker(void* p){Job* j=(Job*)p;j->owner->run(j);delete j;return 0;}

void Client::run(Job* j){
    Http http;
    // الشبكة وخدمات HTTP قد تكون ما جهزت بعد إذا فُتح المتجر بسرعة بعد تشغيل التطبيق: نعيد المحاولة
    bool httpReady=false;
    for(int attempt=0; attempt<4 && !httpReady; ++attempt){
        if(attempt>0){
            if(j->kind==0 && j->gen!=SDL_AtomicGet(&gen_)) goto finish;
            if(SDL_AtomicGet(&cancel_)) goto finish;
            http.close();
            SDL_Delay(1500);
        }
        httpReady=http.init();
    }
    if(!httpReady){
        publish(j->kind==2?Ready:Failed,"Could not initialize PS4 HTTPS modules");
        if(j->kind==2){SDL_LockMutex(mutex_);message_="QR load failed";SDL_UnlockMutex(mutex_);}
        goto finish;
    }
    if(j->kind==2){
        // Support QR: fetched from the internet straight into memory; no file is created.
        std::string body,error; int status=0; StringSink sink{&body,2*1024*1024};
        bool ok=getUrl(http,j->entry.supportQR,2*1024*1024,&cancel_,appendString,&sink,Client::onProgress,this,status,error);
        const unsigned char png[8]={137,80,78,71,13,10,26,10};
        if(SDL_AtomicGet(&cancel_)||!ok||body.size()<24||std::memcmp(body.data(),png,8)!=0){SDL_LockMutex(mutex_);phase_=Ready;message_="QR load failed";SDL_UnlockMutex(mutex_);goto finish;}
        SDL_LockMutex(mutex_);qrBytes_.swap(body);phase_=Ready;message_="Catalog ready";received_=total_=0;SDL_UnlockMutex(mutex_);goto finish;
    }
    if(j->kind==0){
        unlink(CATALOG_CACHE);   // plugins.json is no longer kept on the console: delete any copy left by older versions
        std::string body,error; int status=0; StringSink sink{&body,MAX_CATALOG};
        // Raw GitHub and intermediate CDNs may cache a branch URL. Vary the query on each refresh.
        std::string freshUrl=std::string(CATALOG_URL)+"?v="+std::to_string((unsigned long long)std::time(nullptr))+"-"+std::to_string(SDL_GetTicks());
        bool ok=getUrl(http,freshUrl,MAX_CATALOG,&cancel_,appendString,&sink,nullptr,nullptr,status,error);
        if(j->gen!=SDL_AtomicGet(&gen_)) goto finish;   // طلب قديم: نتجاهل نتيجته
        std::vector<Entry> parsed;
        if(ok && parseCatalog(body,parsed,error)){
            SDL_LockMutex(mutex_);entries_.swap(parsed);catalogLoaded_=true;catalogUpdated_=gParsedUpdated.empty()?nowStamp():gParsedUpdated;received_=total_=0;phase_=Ready;message_="Catalog ready";SDL_UnlockMutex(mutex_);goto finish;
        }
        if(SDL_AtomicGet(&cancel_)){publish(Canceled,"Canceled");goto finish;}
        // No HTTP response at all (status 0) means the console has no working internet connection:
        // do NOT fall back to the cached catalog; the UI shows an "offline" screen instead of the store.
        if(!ok && status==0){
            SDL_LockMutex(mutex_);entries_.clear();catalogLoaded_=false;catalogUpdated_.clear();received_=total_=0;phase_=Offline;message_="No internet connection";SDL_UnlockMutex(mutex_);goto finish;
        }
        publish(Failed,error.empty()?"Catalog request failed":error);goto finish;
    }
    {
        const Entry& e=j->entry;
        if (!e.packageUrl.empty()) {
            const bool isTxt = isTextPackage(e.packageUrl);   // packageUrl is a plain .txt file, not a ZIP
            const std::string txtName = isTxt ? urlFilename(e.packageUrl) : std::string();
            const std::string packagePath = isTxt ? "/data/PluginManager/plugin-package.txt" : "/data/PluginManager/plugin-package.zip";
            const std::string partPath = packagePath + ".part";
            mkdir("/data/PluginManager", 0777);
            const bool installAtDataRoot = e.installPath == "@data";                       // "/data/"
            const bool installAtGoldHENRoot = e.installPath == "." || installAtDataRoot;   // extracted straight into a root folder
            const std::string packageRoot = installAtDataRoot ? "/data" : (installAtGoldHENRoot ? "/data/GoldHEN" : j->directory);
            const std::string packageSubdir = installAtGoldHENRoot ? "." : e.installPath;
            const std::string installDir = installAtGoldHENRoot ? packageRoot : packageRoot + "/" + packageSubdir;
            const std::string names[2] = {e.filename, e.filename2};
            const std::string urls[2] = {e.urlDownload, e.url2download};
            const std::string hashes[2] = {e.sha256, e.sha256_2};
            const int count = e.url2download.empty() ? 1 : 2;
            std::vector<std::string> payloadPaths;
            std::vector<std::string> pluginFinalPaths, publishedPlugins;
            bool skipZip = false;
            struct stat already{};
            (void)installDir;
            const std::string txtTarget = installDir + "/" + txtName;
            // (An existing package folder / ZIP files are no longer a hard error: after the ZIP is downloaded we ask the user what to do.)
            for (int i = 0; i < count; ++i) {
                const std::string pluginFinal = j->directory + "/" + names[i];
                if (!j->replace && stat(pluginFinal.c_str(), &already) == 0) { publish(Failed, "Plugin already exists: " + names[i]); goto cleanup_package; }
                pluginFinalPaths.push_back(pluginFinal);
                if (SDL_AtomicGet(&cancel_)) { publish(Canceled, "Canceled"); goto cleanup_package; }
                const std::string payloadPath = "/data/PluginManager/plugin-payload-" + std::to_string(i) + ".part";
                payloadPaths.push_back(payloadPath);
                publish(Downloading, count == 2 ? "Downloading plugin file " + std::to_string(i + 1) + "/2" : "Downloading plugin file");
                FILE* pluginFile = std::fopen(payloadPath.c_str(), "wb");
                if (!pluginFile) { publish(Failed, "Cannot create temporary plugin file"); goto cleanup_package; }
                FileSink pluginSink{pluginFile, Sha256(), 0}; int pluginStatus = 0; std::string pluginError;
                bool pluginOk = getUrl(http, urls[i], MAX_PLUGIN, &cancel_, writeFile, &pluginSink, Client::onProgress, this, pluginStatus, pluginError);
                if (std::fflush(pluginFile) != 0) pluginOk = false;
                std::fclose(pluginFile);
                std::string actualPluginHash = pluginSink.sha.finish(), expectedPluginHash = hashes[i];
                for (char& c : expectedPluginHash) c = (char)std::tolower((unsigned char)c);
                FILE* checkPlugin = std::fopen(payloadPath.c_str(), "rb"); unsigned char pluginMagic[4]{}; size_t gotPlugin = checkPlugin ? std::fread(pluginMagic, 1, 4, checkPlugin) : 0; if (checkPlugin) std::fclose(checkPlugin);
                const bool pluginHeader = gotPlugin == 4 && ((pluginMagic[0] == 0x7f && pluginMagic[1] == 'E' && pluginMagic[2] == 'L' && pluginMagic[3] == 'F') || (pluginMagic[0] == 0x4f && pluginMagic[1] == 0x15 && pluginMagic[2] == 0x3d && pluginMagic[3] == 0x1d));
                if (!pluginOk || pluginSink.bytes < 64 || !pluginHeader || (!expectedPluginHash.empty() && actualPluginHash != expectedPluginHash)) {
                    publish(Failed, !pluginOk ? pluginError : (!pluginHeader ? "Downloaded plugin is not ELF/SELF" : (!expectedPluginHash.empty() ? "Plugin SHA-256 mismatch" : "Downloaded plugin is too small")));
                    goto cleanup_package;
                }
            }
            if (SDL_AtomicGet(&cancel_)) { publish(Canceled, "Canceled"); goto cleanup_package; }
            publish(Downloading, isTxt ? "Downloading text file" : "Downloading ZIP package");
            {
                FILE* f = std::fopen(partPath.c_str(), "wb");
                if (!f) { publish(Failed, "Cannot create temporary package file"); goto cleanup_package; }
                FileSink sink{f, Sha256(), 0}; int status = 0; std::string err;
                bool ok = getUrl(http, e.packageUrl, MAX_PACKAGE, &cancel_, writeFile, &sink, Client::onProgress, this, status, err);
                if (std::fflush(f) != 0) ok = false;
                std::fclose(f);
                if (SDL_AtomicGet(&cancel_)) { publish(Canceled, "Canceled"); goto cleanup_package; }
                publish(Verifying, isTxt ? "Verifying text file" : "Verifying ZIP package");
                std::string actual = sink.sha.finish(), expected = e.packageSha256;
                for (char& c : expected) c = (char)std::tolower((unsigned char)c);
                if (!ok || sink.bytes < (isTxt ? 0u : 22u) || (!expected.empty() && actual != expected)) {
                    publish(Failed, !ok ? err : (!expected.empty() ? "Package SHA-256 mismatch" : "Downloaded ZIP is too small"));
                    goto cleanup_package;
                }
            }
            if (rename(partPath.c_str(), packagePath.c_str()) != 0) { publish(Failed, "Could not finalize ZIP download"); goto cleanup_package; }
            // Before touching anything: does the ZIP collide with files/folders already on the console?
            // If so, pause and let the user choose: replace everything / install the .prx only / cancel.
            if (!j->replace) {
                std::vector<std::string> existing; std::string listErr;
                bool haveExisting = false;
                if (isTxt) { if (stat(txtTarget.c_str(), &already) == 0) existing.push_back(txtName); haveExisting = !existing.empty(); }
                else haveExisting = ZipPackage::listExisting(packagePath, packageRoot, packageSubdir, existing, listErr) && !existing.empty();
                if (haveExisting) {
                    std::string summary = existing[0];
                    if (existing.size() > 1) summary += "  (+" + std::to_string(existing.size() - 1) + ")";
                    SDL_AtomicSet(&answer_, 0);
                    publish(Confirm, summary);
                    while (SDL_AtomicGet(&answer_) == 0 && !SDL_AtomicGet(&cancel_)) SDL_Delay(50);
                    const int answer = SDL_AtomicGet(&answer_);
                    if (SDL_AtomicGet(&cancel_) || answer == 0 || answer == 3) { publish(Canceled, "Canceled"); goto cleanup_package; }
                    if (answer == 1) j->replace = true;       // replace everything
                    else if (answer == 2) skipZip = true;     // install the plugin file(s) only
                    publish(Verifying, "Installing");
                }
            }
            // The plugin binaries always live in GoldHEN's plugins directory;
            // installPath applies only to companion files from the ZIP.
            for (int i = 0; i < count; ++i) {
                if (j->replace) unlink(pluginFinalPaths[i].c_str()); // everything downloaded and verified: now remove the old version
                if (rename(payloadPaths[i].c_str(), pluginFinalPaths[i].c_str()) != 0) {
                    for (const auto& path : publishedPlugins) unlink(path.c_str());
                    publish(Failed, "Could not install plugin into /data/GoldHEN/plugins");
                    goto cleanup_package;
                }
                publishedPlugins.push_back(pluginFinalPaths[i]);
            }
            if (!skipZip) publish(Verifying, "Extracting and validating ZIP package");
            if (!skipZip) {
                std::string error;
                const std::vector<ZipPackage::PayloadFile> noPluginPayloads;
                bool zipOk;
                if (isTxt) {
                    for (size_t k = 1; k < installDir.size(); ++k) if (installDir[k] == '/') mkdir(installDir.substr(0, k).c_str(), 0777);
                    mkdir(installDir.c_str(), 0777);
                    zipOk = rename(packagePath.c_str(), txtTarget.c_str()) == 0;
                    if (!zipOk) error = "Could not write " + txtName;
                } else zipOk = ZipPackage::install(packagePath, packageRoot, packageSubdir, noPluginPayloads, &cancel_, error, j->replace);
                if (!zipOk) {
                    for (const auto& path : publishedPlugins) unlink(path.c_str());
                    if (SDL_AtomicGet(&cancel_)) publish(Canceled, "Canceled");
                    else publish(Failed, error.empty() ? "ZIP extraction failed" : error);
                    goto cleanup_package;
                }
            }
            unlink(packagePath.c_str());
            SDL_LockMutex(mutex_); installedId_ = e.id; phase_ = Installed; message_ = skipZip ? "Plugin installed and disabled (ZIP skipped)" : "Package installed and disabled"; SDL_UnlockMutex(mutex_);
            goto finish;
cleanup_package:
            unlink(partPath.c_str()); unlink(packagePath.c_str());
            for (int i = 0; i < 2; ++i) { const std::string path = "/data/PluginManager/plugin-payload-" + std::to_string(i) + ".part"; unlink(path.c_str()); }
            goto finish;
        }
        const std::string names[2] = {e.filename, e.filename2};
        const std::string urls[2] = {e.urlDownload, e.url2download};
        const std::string hashes[2] = {e.sha256, e.sha256_2};
        const int count = e.url2download.empty() ? 1 : 2;
        std::string finals[2], parts[2];
        for (int i=0; i<count; ++i) {
            finals[i]=j->directory+"/"+names[i]; parts[i]=finals[i]+".part";
            struct stat st{}; if(!j->replace && stat(finals[i].c_str(),&st)==0){publish(Failed,"Plugin already exists: "+names[i]);goto cleanup_parts;}
        }
        for (int i=0; i<count; ++i) {
            if(SDL_AtomicGet(&cancel_)){publish(Canceled,"Canceled");goto cleanup_parts;}
            publish(Downloading, count==2 ? "Downloading file "+std::to_string(i+1)+"/2" : "Downloading plugin");
            FILE* f=std::fopen(parts[i].c_str(),"wb"); if(!f){publish(Failed,"Cannot create temporary file");goto cleanup_parts;}
            FileSink sink{f,Sha256(),0}; int status=0; std::string err;
            bool ok=getUrl(http,urls[i],MAX_PLUGIN,&cancel_,writeFile,&sink,Client::onProgress,this,status,err);
            if(std::fflush(f)!=0)ok=false; std::fclose(f);
            if(SDL_AtomicGet(&cancel_)){publish(Canceled,"Canceled");goto cleanup_parts;}
            publish(Verifying,"Verifying downloaded file");
            std::string digest=sink.sha.finish(), expected=hashes[i];
            for(char& c:expected)c=(char)std::tolower((unsigned char)c);
            FILE* check=std::fopen(parts[i].c_str(),"rb"); unsigned char magic[4]={}; size_t got=check?std::fread(magic,1,4,check):0; if(check)std::fclose(check);
            bool header=(got==4 && ((magic[0]==0x7f && magic[1]=='E' && magic[2]=='L' && magic[3]=='F') || (magic[0]==0x4f && magic[1]==0x15 && magic[2]==0x3d && magic[3]==0x1d)));
            if(!ok || sink.bytes<64 || !header || (!expected.empty() && digest!=expected)) {
                publish(Failed, !ok?err:(!header?"Downloaded file is not ELF/SELF":(!expected.empty()?"SHA-256 mismatch":"Downloaded file is too small")));
                goto cleanup_parts;
            }
        }
        // Publish only after all files pass checks; roll back any newly moved file if a later rename fails.
        for(int i=0;i<count;++i) { if(j->replace) unlink(finals[i].c_str()); if(rename(parts[i].c_str(),finals[i].c_str())!=0) {
            for(int k=0;k<i;++k) unlink(finals[k].c_str());
            publish(Failed,"Could not move verified file into plugins directory"); goto cleanup_parts;
        } }
        SDL_LockMutex(mutex_);installedId_=e.id;phase_=Installed;message_="Installed and disabled";SDL_UnlockMutex(mutex_);
        goto finish;
cleanup_parts:
        for(int i=0;i<count;++i) unlink(parts[i].c_str());
    }
finish:
    http.close();SDL_LockMutex(mutex_);if(!(j->kind==0&&j->gen!=SDL_AtomicGet(&gen_)))done_=true;SDL_UnlockMutex(mutex_);
}
}

// ---------- Remote Package Installer (RPI) : POST /api/install sur la console ----------
namespace PluginStore { namespace Rpi {
namespace {
const int kRpiPort = 12800;
const int kRpiTimeoutSec = 10;
const size_t kRpiMaxResponse = 64 * 1024;

SDL_mutex* gRpiMutex = nullptr;
SDL_Thread* gRpiThread = nullptr;
State gRpiState = State::Idle;
std::string gRpiMessage;

void setRpiState(State s, const std::string& m) {
    SDL_LockMutex(gRpiMutex);
    gRpiState = s;
    gRpiMessage = m;
    SDL_UnlockMutex(gRpiMutex);
}

std::string jsonEscape(const std::string& in) {
    std::string out;
    for (char c : in) {
        if (c == '"' || c == '\\') { out.push_back('\\'); out.push_back(c); }
        else out.push_back(c);
    }
    return out;
}

// Lecture minimale d'un champ texte "key":"value" (sans parseur JSON complet).
bool findStringField(const std::string& body, const char* key, std::string& out) {
    const std::string pat = std::string("\"") + key + "\"";
    size_t p = body.find(pat);
    if (p == std::string::npos) return false;
    p = body.find(':', p + pat.size());
    if (p == std::string::npos) return false;
    p = body.find('"', p);
    if (p == std::string::npos) return false;
    std::string v;
    for (size_t i = p + 1; i < body.size(); i++) {
        char c = body[i];
        if (c == '\\' && i + 1 < body.size()) { v.push_back(body[++i]); continue; }
        if (c == '"') { out = v; return true; }
        v.push_back(c);
    }
    return false;
}

int connectRpi() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    timeval tv{kRpiTimeoutSec, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(kRpiPort);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(fd, (sockaddr*)&a, sizeof(a)) < 0) { close(fd); return -1; }
    return fd;
}

// ---------- Installation directe via BGFT (service de téléchargement du système) ----------
// Les valeurs marquées « À CONFIRMER » ne sont pas vérifiées sur console.
const int32_t kBgftEntitlementType = 0;          // À CONFIRMER
const char* const kBgftPackageType = "PS4GD";    // À CONFIRMER
const char* const kBgftPackageSubType = "PS4GD"; // À CONFIRMER
const size_t kBgftHeapSize = 1024 * 1024;        // À CONFIRMER
const size_t kPkgHeaderBytes = 256;
const size_t kPkgContentIdOffset = 0x40;         // vérifié sur un PKG réel
const size_t kPkgContentIdLen = 0x24;
const char* const kBgftLogPath = "/data/PluginManager/bgft_log.txt";
// مرحلة الاختبار: 0 = معطّل، 1 = تنزيل الـ PKG فقط (بدون هروب ولا BGFT)، 2 = التثبيت الكامل
const int kBgftStage = 2;
bool (*gEscapeHook)() = nullptr;
void (*gRestoreHook)() = nullptr;
bool gEscapedByBgft = false;
void bgftLog(const char* fmt, ...);   // تصريح مسبق: التعريف الفعلي أدناه
// يرجّع الصلاحيات الأصلية إذا كان التطبيق قد هرب من الـ sandbox من أجل BGFT
void endEscape() {
    if (gEscapedByBgft && gRestoreHook) { gRestoreHook(); bgftLog("[ROOT] credentials restored after BGFT"); }
    gEscapedByBgft = false;
}
int32_t gInstallTaskId = -1;
std::string gInstallLocalPath;
std::atomic<bool> gInstallActive{false};
std::atomic<int> gMonState{0};     // 0 = لا شيء، 2 = اكتمل، 3 = فشل (يُقرأ مرة واحدة من الواجهة)
std::atomic<bool> gMonStarted{false};
uint32_t gLastProgressTick = 0;
std::string gInstallTitleId;
std::string gInstallContentId;
bool gSawTask = false;
std::atomic<bool> gCancelDl{false};
bool gLastFolderExists = false;
uint32_t gInstallStartTick = 0;
uint32_t gInstallDeleteDelayMs = 20000;   // 20 ثانية + ثانية لكل ميغابايت من حجم الملف
std::atomic<uint64_t> gDlWritten{0};
std::atomic<uint64_t> gDlTotal{0};
std::atomic<bool> gDlActive{false};
std::atomic<bool> gPreparing{false};
int gLastPercent = 0;

bool gBgftReady = false;
void* gBgftHeap = nullptr;      // مُحاذى على 16KB
void* gBgftHeapRaw = nullptr;   // المؤشر الأصلي لـ free
const size_t kBgftHeapAlign = 0x4000;

// سجل تشخيصي: يُكتب في ملف على الجهاز. كل محاولة تبدأ بملف جديد.
void bgftLogBegin(const std::string&) {}   // السجل مُلغى
void bgftLog(const char*, ...) {}   // السجل مُلغى

// مثيل HTTP دائم لمسار BGFT: لا نغلقه بعد كل محاولة، لأن Http::close() ينهي SSL للتطبيق كله (يكسر شبكة المتجر)
Http gBgftHttp;
bool gBgftHttpReady = false;
void releaseBgftHttp() {
    if (gBgftHttpReady) { gBgftHttp.close(); gBgftHttpReady = false; bgftLog("[HTTP] shared instance released"); }
}

Http* bgftHttp() {
    if (!gBgftHttpReady) { gBgftHttpReady = gBgftHttp.init(); bgftLog("[HTTP] shared init=%d", gBgftHttpReady ? 1 : 0); }
    return gBgftHttpReady ? &gBgftHttp : nullptr;
}

// يقرأ أول 256 بايت من الرابط ويغلق الاتصال، ويأخذ الحجم الكامل من Content-Length
bool fetchPkgHeader(const std::string& url, std::string& head, uint64_t& total) {
    Http* hp = bgftHttp();
    if (!hp) { bgftLog("[HTTP] Http::init FAILED"); return false; }
    Http& h = *hp;
    bgftLog("[HTTP] init OK, ctx=%d", h.ctx);
    int tpl = sceHttpCreateTemplate(h.ctx, "GoldHEN-Plugin-Manager/1.0", ORBIS_HTTP_VERSION_1_1, 1);
    bgftLog("[HTTP] CreateTemplate=0x%08X", (unsigned)tpl);
    if (tpl < 0) return false;
    sceHttpSetResolveTimeOut(tpl, 10u * 1000000u); sceHttpSetConnectTimeOut(tpl, 15u * 1000000u); sceHttpSetSendTimeOut(tpl, 15u * 1000000u);
    int conn = sceHttpCreateConnectionWithURL(tpl, url.c_str(), 0);
    bgftLog("[HTTP] CreateConnection=0x%08X", (unsigned)conn);
    int req = conn >= 0 ? sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_GET, url.c_str(), 0) : -1;
    bgftLog("[HTTP] CreateRequest=0x%08X", (unsigned)req);
    bool ok = false;
    if (req >= 0) {
        const int sr = sceHttpSendRequest(req, nullptr, 0);
        bgftLog("[HTTP] SendRequest=0x%08X", (unsigned)sr);
        if (sr >= 0) {
            int status = 0;
            const int sc = sceHttpGetStatusCode(req, &status);
            int type = ORBIS_HTTP_CONTENTLEN_NOT_FOUND;
            size_t length = 0;
            const int cl = sceHttpGetResponseContentLength(req, &type, &length);
            bgftLog("[HTTP] StatusCode ret=0x%08X status=%d | ContentLength ret=0x%08X type=%d length=%zu",
                    (unsigned)sc, status, (unsigned)cl, type, length);
            if (sc >= 0 && status >= 200 && status < 300 && cl >= 0 &&
                type == ORBIS_HTTP_CONTENTLEN_EXIST && length >= kPkgHeaderBytes) {
                char buf[kPkgHeaderBytes];
                size_t got = 0;
                while (got < kPkgHeaderBytes) {
                    int n = sceHttpReadData(req, buf + got, (unsigned int)(kPkgHeaderBytes - got));
                    if (n <= 0) { bgftLog("[HTTP] ReadData stopped at %zu (ret=%d)", got, n); break; }
                    got += (size_t)n;
                }
                bgftLog("[HTTP] read %zu of %zu header bytes", got, kPkgHeaderBytes);
                if (got == kPkgHeaderBytes) { head.assign(buf, got); total = (uint64_t)length; ok = true; }
            } else {
                bgftLog("[HTTP] REJECTED: need 2xx status and Content-Length >= 256 (EXIST type)");
            }
        }
    }
    if (req >= 0) sceHttpDeleteRequest(req);
    if (conn >= 0) sceHttpDeleteConnection(conn);
    sceHttpDeleteTemplate(tpl);
    bgftLog("[HTTP] header fetch result=%d", ok ? 1 : 0);
    return ok;
}

// يستخرج معرّف المحتوى (حقل نصي بطول 0x24 يبدأ عند 0x40) ويتحقق أنه أحرف مطبوعة
bool pkgContentId(const std::string& head, std::string& id) {
    if (head.size() < kPkgContentIdOffset + kPkgContentIdLen) return false;
    const char* p = head.data() + kPkgContentIdOffset;
    size_t n = 0;
    while (n < kPkgContentIdLen && p[n] != '\0') {
        const unsigned char c = (unsigned char)p[n];
        if (c < 0x21 || c > 0x7e) return false;
        ++n;
    }
    if (n == 0) return false;
    id.assign(p, n);
    return true;
}

bool bgftInit() {
    if (gBgftReady) { bgftLog("[BGFT] already initialized"); return true; }
    const int32_t sm = sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_BGFT);
    bgftLog("[SYS] LoadModuleInternal(BGFT) ret=0x%08X", (unsigned)sm);
    if (sm < 0) return false;
    // مطابق لـ ps4-ipi: الـ heap مُصفّر قبل التهيئة (malloc وحده يترك بيانات عشوائية)
    gBgftHeap = std::malloc(kBgftHeapSize);
    if (!gBgftHeap) { bgftLog("[BGFT] heap malloc FAILED"); return false; }
    std::memset(gBgftHeap, 0, kBgftHeapSize);
    bgftLog("[BGFT] heap=%p size=%zu (zeroed)", gBgftHeap, kBgftHeapSize);
    OrbisBgftInitParams ip{};
    ip.heap = gBgftHeap;
    ip.heapSize = kBgftHeapSize;
    const int32_t ir = sceBgftServiceIntInit(&ip);
    bgftLog("[BGFT] IntInit ret=0x%08X", (unsigned)ir);
    if (ir < 0) { std::free(gBgftHeap); gBgftHeap = nullptr; return false; }
    gBgftReady = true;
    return true;
}

// قراءة معرّف العنوان من ملف PKG محلي عبر libSceAppInstUtil (محمّلة ديناميكيًا كما في ps4-ipi)
int (*gAppInstGetTitleId)(const char*, char*, int*) = nullptr;

bool readTitleId(const std::string& pkgPath, std::string& titleId) {
    if (!gAppInstGetTitleId) {
        const int h = sceKernelLoadStartModule("/system/common/lib/libSceAppInstUtil.sprx", 0, nullptr, 0, nullptr, nullptr);
        bgftLog("[APPINST] load handle=0x%08X", (unsigned)h);
        if (h == 0) return false;
        sceKernelDlsym(h, "sceAppInstUtilGetTitleIdFromPkg", (void**)&gAppInstGetTitleId);
        if (!gAppInstGetTitleId) { bgftLog("[APPINST] dlsym FAILED"); return false; }
    }
    char buf[18];
    std::memset(buf, 0, sizeof(buf));
    int isApp = -1;
    const int r = gAppInstGetTitleId(pkgPath.c_str(), buf, &isApp);
    bgftLog("[APPINST] GetTitleIdFromPkg ret=0x%08X titleId=%s isApp=%d", (unsigned)r, buf, isApp);
    if (r != 0 || buf[0] == '\0') return false;
    titleId = buf;
    return true;
}

// ---------- تنزيل PKG كاملًا إلى القرص (خطوة مستقلة لاختبار التحميل المحلي) ----------
// جذر /data كما يفعل ps4-ipi: BGFT يقرأ الملف من /user/data/<name> مباشرة
const char* const kPkgDir = "/data";
const uint64_t kMaxPkgBytes = 256ull * 1024ull * 1024ull;   // نفس حد MAX_PACKAGE في المشروع

// يحمّل الرابط إلى kPkgDir/fileName عبر ملف مؤقت .part ثم يعيد تسميته عند النجاح
bool downloadPkgToDisk(const std::string& url, const std::string& fileName, std::string& localPath) {
    mkdir("/data/PluginManager", 0777);
    mkdir(kPkgDir, 0777);
    localPath = std::string(kPkgDir) + "/" + fileName;
    const std::string tmpPath = localPath + ".part";
    struct DlGuard { ~DlGuard() { gDlActive = false; } } dlGuard;

    Http* hp = bgftHttp();
    if (!hp) { bgftLog("[DL] Http::init FAILED"); return false; }
    Http& h = *hp;
    int tpl = sceHttpCreateTemplate(h.ctx, "GoldHEN-Plugin-Manager/1.0", ORBIS_HTTP_VERSION_1_1, 1);
    if (tpl < 0) { bgftLog("[DL] CreateTemplate=0x%08X", (unsigned)tpl); return false; }
    sceHttpSetResolveTimeOut(tpl, 10u * 1000000u); sceHttpSetConnectTimeOut(tpl, 15u * 1000000u); sceHttpSetSendTimeOut(tpl, 15u * 1000000u);
    int conn = sceHttpCreateConnectionWithURL(tpl, url.c_str(), 0);
    int req = conn >= 0 ? sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_GET, url.c_str(), 0) : -1;
    bool ok = false;
    const int sendRet = req >= 0 ? sceHttpSendRequest(req, nullptr, 0) : -1;
    bgftLog("[DL] CreateRequest=0x%08X SendRequest=0x%08X", (unsigned)req, (unsigned)sendRet);
    if (req >= 0 && sendRet >= 0) {
        int status = 0;
        int type = ORBIS_HTTP_CONTENTLEN_NOT_FOUND;
        size_t length = 0;
        const int sc = sceHttpGetStatusCode(req, &status);
        const int cl = sceHttpGetResponseContentLength(req, &type, &length);
        bgftLog("[DL] status=%d (ret=0x%08X) contentLength=%zu type=%d (ret=0x%08X)", status, (unsigned)sc, length, type, (unsigned)cl);
        if (sc >= 0 && status >= 200 && status < 300 && cl >= 0 &&
            type == ORBIS_HTTP_CONTENTLEN_EXIST && length > 0 && (uint64_t)length <= kMaxPkgBytes) {
            gDlWritten = 0;
            gDlTotal = (uint64_t)length;
            gDlActive = true;
            FILE* f = std::fopen(tmpPath.c_str(), "wb");
            if (!f) { bgftLog("[DL] cannot open %s", tmpPath.c_str()); }
            else {
                static char buf[64 * 1024];
                uint64_t written = 0;
                bool readOk = true;
                for (;;) {
                    if (gCancelDl) { readOk = false; bgftLog("[DL] canceled by user"); break; }
                    const int n = sceHttpReadData(req, buf, (unsigned int)sizeof(buf));
                    if (n < 0) { bgftLog("[DL] ReadData error=0x%08X at %llu", (unsigned)n, (unsigned long long)written); readOk = false; break; }
                    if (n == 0) break;
                    if (std::fwrite(buf, 1, (size_t)n, f) != (size_t)n) { bgftLog("[DL] fwrite FAILED (disk full?)"); readOk = false; break; }
                    written += (uint64_t)n;
                    gDlWritten = written;
                }
                std::fclose(f);
                bgftLog("[DL] written=%llu expected=%zu readOk=%d", (unsigned long long)written, length, readOk ? 1 : 0);
                if (readOk && written == (uint64_t)length) {
                    std::remove(localPath.c_str());
                    ok = std::rename(tmpPath.c_str(), localPath.c_str()) == 0;
                    bgftLog("[DL] rename ok=%d -> %s", ok ? 1 : 0, localPath.c_str());
                } else {
                    std::remove(tmpPath.c_str());
                }
            }
        } else {
            bgftLog("[DL] REJECTED: need 2xx and known Content-Length within limit");
        }
    } else {
        bgftLog("[DL] request failed before sending");
    }
    if (req >= 0) sceHttpDeleteRequest(req);
    if (conn >= 0) sceHttpDeleteConnection(conn);
    sceHttpDeleteTemplate(tpl);
    return ok;
}

// true = BGFT سجّل المهمة وبدأها من الملف المحلي. false = أي خطأ، فيُستخدم RPI كاحتياط.
bool tryBgft(const std::string& url) {
    if (kBgftStage < 1) return false;
    struct PrepGuard { ~PrepGuard() { gPreparing = false; } };
    gPreparing = true;
    PrepGuard prepGuard;
    gCancelDl = false;
    std::string head;
    uint64_t total = 0;
    bool gotHeader = false;
    for (int attempt = 1; attempt <= 3 && !gotHeader; attempt++) {
        if (gCancelDl) break;
        bgftLog("[HTTP] attempt %d/3", attempt);
        gotHeader = fetchPkgHeader(url, head, total);
        if (!gotHeader && attempt < 3) SDL_Delay(2000);
    }
    if (!gotHeader && gCancelDl) { bgftLog("RESULT: canceled by user"); setRpiState(State::Failed, ""); return true; }
    if (!gotHeader) { bgftLog("RESULT: header fetch failed after 3 attempts -> fallback"); return false; }

    std::string contentId;
    if (!pkgContentId(head, contentId)) { bgftLog("RESULT: content ID invalid -> fallback"); return false; }
    bgftLog("[PKG] contentId=%s size=%llu", contentId.c_str(), (unsigned long long)total);
    if (total == 0 || total > 0xFFFFFFFFull) { bgftLog("RESULT: size out of range -> fallback"); return false; }

    // 1) تنزيل الـ PKG إلى القرص
    std::string localPath;
    bool downloaded = false;
    for (int attempt = 1; attempt <= 3 && !downloaded; attempt++) {
        bgftLog("[DL] attempt %d/3", attempt);
        if (gCancelDl) break;
        downloaded = downloadPkgToDisk(url, contentId + ".pkg", localPath);
        if (!downloaded && attempt < 3) SDL_Delay(2000);
    }
    if (!downloaded && gCancelDl) { bgftLog("RESULT: canceled by user"); setRpiState(State::Failed, ""); return true; }
    if (!downloaded) { bgftLog("RESULT: download failed after 3 attempts -> fallback"); return false; }

    // مسار BGFT: نفس أسلوب ps4-ipi، أي /user/data بدل /data
    const std::string bgftPath = "/user/data/" + localPath.substr(std::strlen("/data/"));
    bgftLog("[BGFT] local path for BGFT=%s", bgftPath.c_str());
    if (kBgftStage < 2) { bgftLog("[STAGE] download-only test: stop before escape/BGFT"); return false; }
    if (gEscapeHook) { const bool esc = gEscapeHook(); gEscapedByBgft = esc; bgftLog("[ROOT] escape hook -> %d", esc ? 1 : 0); }
    if (kBgftStage == 3) { bgftLog("[STAGE] escape-only test: restoring and stopping before BGFT"); endEscape(); return false; }

    // 2) تهيئة BGFT ثم قراءة معرّف العنوان من الـ PKG
    if (!bgftInit()) { bgftLog("RESULT: bgftInit failed -> fallback"); return false; }
    if (kBgftStage == 4) { bgftLog("[STAGE] BGFT init-only test: restoring and stopping"); endEscape(); return false; }
    std::string titleId;
    if (!readTitleId(bgftPath, titleId)) { bgftLog("RESULT: title id read failed -> fallback"); endEscape(); return false; }
    if (kBgftStage == 5) { bgftLog("[STAGE] title-id test: restoring and stopping before register"); endEscape(); return false; }
    {   // إذا كان التطبيق مثبتًا مسبقًا: نحذف الملف المحلي ونخبر المستخدم
        struct stat st;
        const std::string appDir = "/user/app/" + titleId;
        if (stat(appDir.c_str(), &st) == 0) {
            bgftLog("[APP] already installed: %s -> removing local PKG", appDir.c_str());
            std::remove(localPath.c_str());
            setRpiState(State::AlreadyInstalled, "");
            return true;
        }
    }

    // 3) تسجيل المهمة من التخزين المحلي (كما في ps4-ipi)
    OrbisBgftDownloadParamEx p{};
    p.params.entitlementType = 5;
    p.params.id = "";
    p.params.contentUrl = bgftPath.c_str();
    p.params.contentName = titleId.c_str();
    p.params.iconPath = "";
    p.params.playgoScenarioId = "0";
    p.params.option = ORBIS_BGFT_TASK_OPT_DISABLE_CDN_QUERY_PARAM;
    p.slot = 0;

    OrbisBgftTaskId taskId = -1;
    const int32_t rr = sceBgftServiceIntDownloadRegisterTaskByStorageEx(&p, &taskId);
    bgftLog("[BGFT] RegisterTaskByStorageEx ret=0x%08X taskId=%d", (unsigned)rr, taskId);
    if (rr < 0) {
        // قد يرفض BGFT التسجيل لأن التطبيق مثبت مسبقًا: نتحقق من المجلد قبل الاعتبار بالفشل
        struct stat st;
        const std::string appDir = "/user/app/" + titleId;
        if (stat(appDir.c_str(), &st) == 0) {
            bgftLog("[APP] register rejected (0x%08X) and app folder exists -> already installed", (unsigned)rr);
            std::remove(localPath.c_str());
            setRpiState(State::AlreadyInstalled, "");
            return true;
        }
        bgftLog("RESULT: register failed (0x%08X) -> fallback", (unsigned)rr);
        return false;
    }

    // 4) بدء المهمة
    if (kBgftStage == 6) {
        bgftLog("[STAGE] register-only test: unregistering, restoring, not starting");
        const int32_t ur3 = sceBgftServiceIntDownloadUnregisterTask(taskId);
        bgftLog("[BGFT] unregister ret=0x%08X", (unsigned)ur3);
        endEscape();
        return false;
    }
    const int32_t sr = sceBgftServiceDownloadStartTask(taskId);
    bgftLog("[BGFT] DownloadStartTask ret=0x%08X", (unsigned)sr);
    if (sr < 0) {
        const int32_t ur2 = sceBgftServiceIntDownloadUnregisterTask(taskId);
        bgftLog("[BGFT] IntDownloadUnregisterTask ret=0x%08X", (unsigned)ur2);
        bgftLog("RESULT: start failed -> fallback");
        return false;
    }
    bgftLog("RESULT: BGFT started local install task %d", taskId);
    gInstallTaskId = taskId;
    gInstallLocalPath = localPath;
    gInstallTitleId = titleId;
    gInstallContentId = contentId;
    gSawTask = false;
    gInstallStartTick = SDL_GetTicks();
    gInstallDeleteDelayMs = 20000u + (uint32_t)(total / (1024ull * 1024ull)) * 1000u;
    gLastPercent = 0;
    gLastProgressTick = 0;
    gMonStarted = false;
    gInstallActive = true;
    setRpiState(State::Started, "");
    endEscape();   // BGFT قبل المهمة: نرجّع الصلاحيات فورًا حتى لا تبقى العملية مهروبة
    return true;
}

int rpiSendThread(void* opaque) {
    std::string* url = (std::string*)opaque;
    bgftLogBegin(*url);
    const bool bgftOk = tryBgft(*url);
    releaseBgftHttp();   // نحرر موارد الشبكة بعد كل محاولة
    if (!bgftOk) endEscape();   // فشل BGFT: نرجع الصلاحيات فورًا
    if (bgftOk) { delete url; return 0; }
    bgftLog("-> falling back to RPI on 127.0.0.1:12800");   // BGFT أولاً، وRPI احتياط
    sceNetInit();   // déjà initialisé par le reste de l'application : un échec "déjà initialisé" est sans gravité
    const std::string body = "{\"type\":\"direct\",\"packages\":[\"" + jsonEscape(*url) + "\"]}";
    delete url;
    const std::string req = "POST /api/install HTTP/1.1\r\nHost: 127.0.0.1:12800\r\n"
                            "Content-Type: application/json\r\nContent-Length: " + std::to_string(body.size()) +
                            "\r\nConnection: close\r\n\r\n" + body;

    int fd = connectRpi();
    bgftLog("[RPI] connect fd=%d errno=%d", fd, fd < 0 ? errno : 0);
    if (fd < 0) { setRpiState(State::NotRunning, ""); return 0; }

    bool sent = true;
    for (size_t at = 0; at < req.size();) {
        ssize_t n = send(fd, req.data() + at, req.size() - at, 0);
        if (n <= 0) { sent = false; break; }
        at += (size_t)n;
    }
    std::string resp;
    if (sent) {
        char buf[1024];
        for (;;) {
            ssize_t n = recv(fd, buf, sizeof(buf), 0);
            if (n <= 0) break;   // fin de réponse (Connection: close) ou délai dépassé
            resp.append(buf, (size_t)n);
            if (resp.size() > kRpiMaxResponse) break;
        }
    }
    close(fd);

    if (!sent) { setRpiState(State::NotRunning, ""); return 0; }

    int status = 0;
    if (resp.compare(0, 5, "HTTP/") == 0) {
        size_t sp = resp.find(' ');
        if (sp != std::string::npos) status = std::atoi(resp.c_str() + sp + 1);
    }
    bgftLog("[RPI] status=%d response(first 300B): %.300s", status, resp.c_str());
    if (status == 0) { setRpiState(State::Failed, ""); return 0; }

    if (status >= 200 && status < 300) {
        setRpiState(State::Started, "");
    } else {
        size_t he = resp.find("\r\n\r\n");
        const std::string respBody = he == std::string::npos ? std::string() : resp.substr(he + 4);
        std::string msg;
        if (!findStringField(respBody, "error", msg) && !findStringField(respBody, "message", msg))
            msg = "HTTP " + std::to_string(status);
        setRpiState(State::Failed, msg);
    }
    return 0;
}
} // namespace

void setEscapeHook(bool (*hook)()) { gEscapeHook = hook; }
void setRestoreHook(void (*hook)()) { gRestoreHook = hook; }
void cancelDownload() { gCancelDl = true; bgftLog("[CANCEL] user pressed cancel (downloadActive=%d)", gDlActive.load() ? 1 : 0); }

// فحص حالة التثبيت: يهرب لحظة قصيرة فقط ثم يرجّع الصلاحيات فورًا
// 0 = لم تظهر المهمة بعد، 1 = المهمة ما زالت جارية، 2 = المهمة انتهت ومجلد التطبيق موجود، 3 = المهمة انتهت بدون مجلد
int checkInstallState() {
    const bool escaped = gEscapeHook && gEscapeHook();
    int result;
    OrbisBgftTaskId found = -1;
    struct stat st0;
    gLastFolderExists = stat(("/user/app/" + gInstallTitleId).c_str(), &st0) == 0;
    const int32_t fr = sceBgftServiceDownloadFindTaskByContentId(gInstallContentId.c_str(), ORBIS_BGFT_TASK_SUB_TYPE_GAME, &found);
    bgftLog("[PROG] FindTaskByContentId ret=0x%08X found=%d sawTask=%d escaped=%d", (unsigned)fr, (int)found, gSawTask ? 1 : 0, escaped ? 1 : 0);
    if (fr >= 0) { gSawTask = true; result = 1; }
    else if (!gSawTask) { result = 0; }
    else {
        result = gLastFolderExists ? 2 : 3;
    }
    if (escaped && gRestoreHook) gRestoreHook();
    return result;
}

// خيط المراقبة: يفحص حالة التثبيت كل ثانيتين خارج الخيط الرئيسي (الفحص يستخدم الهروب من الـ sandbox وهو ثقيل)
int installMonitor(void*) {
    while (gInstallActive) {
        SDL_Delay(2000);
        if (!gInstallActive) break;
        const int st = checkInstallState();
        const uint32_t elapsed = SDL_GetTicks() - gInstallStartTick;
        bgftLog("[PROG] decision: state=%d folder=%d elapsedSec=%u", st, gLastFolderExists ? 1 : 0, (unsigned)(elapsed / 1000u));
        bool finished = false, failedNow = false;
        if (st == 2) finished = true;
        else if ((st == 1 || st == 0) && gLastFolderExists && elapsed > gInstallDeleteDelayMs) finished = true;
        else if (st == 3) failedNow = true;
        else if (st == 0 && elapsed > 600000u) failedNow = true;
        else if (st == 1 && elapsed > 3600000u) failedNow = true;
        if (!finished && !failedNow) continue;
        gInstallActive = false;
        if (finished) {
            bgftLog("[INSTALL] install complete -> removing local PKG");
            std::remove(gInstallLocalPath.c_str());
            gMonState = 2;
        } else {
            bgftLog("[INSTALL] failed or timed out -> keep PKG");
            gMonState = 3;
        }
    }
    return 0;
}

Progress progress() {
    Progress pr{false, 0, false, false};
    if (gPreparing && !gDlActive) { pr.active = true; pr.percent = -2; return pr; }   // -2 = تحضير (اتصال/ترويسة)
    // 1) تحميل الـ PKG: نسبة حقيقية من البايتات المكتوبة
    if (gDlActive) {
        const uint64_t t = gDlTotal.load();
        const uint64_t w = gDlWritten.load();
        pr.active = true;
        pr.percent = t ? (int)(w * 100ull / t) : 0;
        if (pr.percent > 100) pr.percent = 100;
        return pr;
    }
    // 2) التثبيت: نعرض رسالة لمدة 10 ثوانٍ فقط، والفحص يتم في خيط المراقبة
    if (gInstallActive) {
        if (!gMonStarted) {
            gMonStarted = true;
            SDL_Thread* t = SDL_CreateThread(installMonitor, "bgft-mon", nullptr);
            if (t) SDL_DetachThread(t);
        }
        pr.active = true;
        pr.percent = (SDL_GetTicks() - gInstallStartTick < 10000u) ? -1 : -3;   // -3 = لا نعرض رسالة
        return pr;
    }
    // 3) نتيجة نهائية تُقرأ مرة واحدة
    const int ms = gMonState.exchange(0);
    if (ms == 2) { pr.done = true; pr.percent = 100; }
    else if (ms == 3) { pr.failed = true; }
    return pr;
}

bool isValidPkgUrl(const std::string& url) {
    if (url.size() < 12) return false;
    if (url.compare(0, 7, "http://") != 0 && url.compare(0, 8, "https://") != 0) return false;
    for (unsigned char c : url) if (c <= 32 || c == '"' || c == '\\' || c == 127) return false;
    return true;
}

bool sendPkg(const std::string& url) {
    if (!isValidPkgUrl(url)) return false;
    if (!gRpiMutex) gRpiMutex = SDL_CreateMutex();
    if (!gRpiMutex) return false;
    SDL_LockMutex(gRpiMutex);
    const bool busy = gRpiState == State::Sending;
    SDL_UnlockMutex(gRpiMutex);
    if (busy) return false;   // un seul envoi à la fois

    if (gRpiThread) { SDL_WaitThread(gRpiThread, nullptr); gRpiThread = nullptr; }   // thread précédent déjà terminé
    setRpiState(State::Sending, "");
    std::string* u = new std::string(url);
    gRpiThread = SDL_CreateThread(rpiSendThread, "rpi-install", u);
    if (!gRpiThread) { delete u; setRpiState(State::Failed, ""); return false; }
    return true;
}

State state(std::string& msg) {
    if (!gRpiMutex) { msg.clear(); return State::Idle; }
    SDL_LockMutex(gRpiMutex);
    State s = gRpiState;
    msg = gRpiMessage;
    SDL_UnlockMutex(gRpiMutex);
    return s;
}

void reset() {
    if (!gRpiMutex) return;
    SDL_LockMutex(gRpiMutex);
    const bool busy = gRpiState == State::Sending;
    SDL_UnlockMutex(gRpiMutex);
    if (!busy) setRpiState(State::Idle, "");
}
}}
