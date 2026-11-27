#pragma once
// إصلاح plugins.ini: منطق نقي بلا SDL ولا OpenOrbis، ليمكن اختباره على أي حاسوب.
// يقرأ النص كاملاً ويرجع نصاً مصححاً + تقريراً بما تغيّر. لا يكتب على القرص بنفسه.
#include <cctype>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace PluginIniRepair {

struct Options {
    std::string pluginDir = "/data/GoldHEN/plugins";
    std::vector<std::string> pluginFiles;   // أسماء ملفات .prx/.sprx الموجودة فعلاً في المجلد
    std::set<std::string> installedIds;     // معرّفات الألعاب المثبتة بأحرف كبيرة (CUSA00411 ...)
    bool installedKnown = false;            // false = تعذّرت قراءة الألعاب: لا تُحذف إضافات أي لعبة
};

struct Report {
    int bracketFixes = 0;        // [CUSA00411  أو  CUSA00411]  ->  [CUSA00411]
    int pathFixes = 0;           // مسار خاطئ  ->  /data/GoldHEN/plugins/<اسم>
    int valueFixes = 0;          // قيمة غير true/false (أو "=" بلا قيمة)
    int commentedLines = 0;      // نص لا معنى له  ->  ; نص
    int mergedSections = 0;      // قسم مكرر دُمج مع الأول
    int duplicatePlugins = 0;    // إضافة مكررة داخل القسم نفسه
    int removedPlugins = 0;      // إضافات لألعاب غير مثبتة على الجهاز
    int removedSections = 0;     // أقسام صارت فارغة بعد الحذف
    int removedMissing = 0;      // إضافة مسارها أو اسمها لا يطابق أي ملف في مجلد الإضافات فحُذفت
    bool removalSkipped = false; // فيه إضافات لألعاب لكن قائمة الألعاب غير معروفة فلم تُحذف
    bool anyChange() const {
        return bracketFixes + pathFixes + valueFixes + commentedLines + mergedSections +
               duplicatePlugins + removedPlugins + removedSections + removedMissing > 0;
    }
};

inline std::string trimS(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) ++a;
    while (b > a && std::isspace((unsigned char)s[b - 1])) --b;
    return s.substr(a, b - a);
}
inline std::string lowerS(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}
inline std::string upperS(std::string s) {
    for (char& c : s) c = (char)std::toupper((unsigned char)c);
    return s;
}
inline bool endsWithS(const std::string& s, const char* tail) {
    const size_t n = std::strlen(tail);
    return s.size() >= n && s.compare(s.size() - n, n, tail) == 0;
}
// معرّف لعبة: 4 حروف/أرقام ثم 5 أرقام (CUSA00411، PPSA01234 ...)
inline bool isTitleId(const std::string& s) {
    if (s.size() != 9) return false;
    for (int i = 0; i < 4; ++i) if (!std::isalnum((unsigned char)s[i])) return false;
    for (int i = 4; i < 9; ++i) if (!std::isdigit((unsigned char)s[i])) return false;
    return true;
}

// رأس قسم، حتى لو فيه خطأ: [CUSA00411 أو CUSA00411] أو CUSA00411 وحدها أو [default]
// يرجع true ومعه الاسم الصحيح. broken = true إذا احتاج السطر تصحيحاً.
inline bool parseHeader(const std::string& t, std::string& name, bool& broken) {
    std::string core;
    const bool open = !t.empty() && t[0] == '[';
    const bool close = !t.empty() && t[t.size() - 1] == ']';
    if (open || close) {
        size_t a = 0;
        while (a < t.size() && (t[a] == '[' || std::isspace((unsigned char)t[a]))) ++a;
        size_t b = t.size();
        while (b > a && (t[b - 1] == ']' || std::isspace((unsigned char)t[b - 1]))) --b;
        core = t.substr(a, b - a);
    } else {
        core = t;   // سطر بلا أقواس: يُقبل فقط إذا كان معرّف لعبة أو default
    }
    if (isTitleId(core)) name = upperS(core);
    else if (lowerS(core) == "default") name = "default";
    else return false;
    // الأقواس ناقصة أو زائدة؟ (المسافات حول الاسم ليست خطأ)
    broken = !(t.size() >= 2 && t[0] == '[' && t[t.size() - 1] == ']' &&
               trimS(t.substr(1, t.size() - 2)) == name);
    return true;
}

// سطر إضافة: النص قبل "=" هو المسار. value: -1 بلا قيمة، 0 = false، 1 = true
inline bool parsePlugin(const std::string& t, std::string& path, int& value, bool& badValue) {
    const size_t eq = t.find('=');
    std::string left = trimS(eq == std::string::npos ? t : t.substr(0, eq));
    const std::string right = eq == std::string::npos ? std::string() : trimS(t.substr(eq + 1));
    if (left.size() >= 2 && (left[0] == '"' || left[0] == '\'') && left.back() == left[0])
        left = left.substr(1, left.size() - 2);
    const std::string l = lowerS(left);
    const bool looksPath = endsWithS(l, ".prx") || endsWithS(l, ".sprx") ||
                           (!left.empty() && left[0] == '/' && l.find("plugin") != std::string::npos);
    if (left.empty() || !looksPath) return false;
    path = left;
    badValue = false;
    if (eq == std::string::npos) {
        value = -1;
    } else {
        const std::string r = lowerS(right);
        if (r.empty()) { value = -1; badValue = true; }
        else if (r == "false" || r == "0" || r == "no" || r == "off" || r == "disabled") value = 0;
        else if (r == "true" || r == "1" || r == "yes" || r == "on" || r == "enabled") value = 1;
        else { value = 1; badValue = true; }   // القيمة غير معروفة: تُعامل كمفعّلة (مثل التطبيق) وتُصحح
    }
    return true;
}

// يصحح المسار إلى /data/GoldHEN/plugins/<اسم الملف>. false = لا يمكن تحديد الملف المقصود.
inline bool resolvePath(const std::string& raw, const Options& o, std::string& out) {
    std::string p = raw;
    for (char& c : p) if (c == '\\') c = '/';
    p = trimS(p);
    const std::string dirSlash = o.pluginDir + "/";
    if (p.size() > dirSlash.size() && lowerS(p.substr(0, dirSlash.size())) == lowerS(dirSlash)) {
        const std::string rest = p.substr(dirSlash.size());
        if (rest.find('/') != std::string::npos) { out = p; return true; }   // مجلد فرعي: لا نغيّره
    }
    const size_t slash = p.rfind('/');
    const std::string base = slash == std::string::npos ? p : p.substr(slash + 1);
    if (base.empty()) return false;
    const std::string lb = lowerS(base);
    const bool hasExt = endsWithS(lb, ".prx") || endsWithS(lb, ".sprx");
    const std::string stem = hasExt ? lb.substr(0, lb.size() - (endsWithS(lb, ".sprx") ? 5 : 4)) : lb;

    // 1) ملف بالاسم نفسه (بدون التفات لحالة الأحرف)
    for (const auto& f : o.pluginFiles) {
        if (lowerS(f) == lb) { out = dirSlash + f; return true; }
    }
    // 2) ملف بالاسم نفسه مع امتداد مختلف أو بلا امتداد، إذا كان واحداً فقط
    int hits = 0;
    std::string only;
    for (const auto& f : o.pluginFiles) {
        const std::string lf = lowerS(f);
        const std::string fs = lf.substr(0, lf.size() - (endsWithS(lf, ".sprx") ? 5 : 4));
        if (fs == stem) { ++hits; only = f; }
    }
    if (hits == 1) { out = dirSlash + only; return true; }
    // 3) لا يوجد ملف بهذا الاسم: نُبقي الاسم (امتداد صحيح) ونصحح المجلد فقط
    if (hasExt && o.pluginFiles.empty()) { out = dirSlash + base; return true; }   // لا نعرف محتوى المجلد: نصلح المجلد فقط
    // ملف غير موجود في المجلد: لا يُعتبر إصلاحاً، والسطر يُحذف في repair()
    return false;
}

// الإصلاح الكامل. text = محتوى الملف الأصلي كاملاً.
inline std::string repair(const std::string& text, const Options& o, Report& rep) {
    struct Item { bool comment; std::string text; int value; };
    struct Sec { std::string name; std::vector<Item> items; };
    std::vector<Sec> secs;
    int cur = -1;

    auto findSec = [&](const std::string& name) -> int {
        for (size_t i = 0; i < secs.size(); ++i) if (secs[i].name == name) return (int)i;
        return -1;
    };
    auto ensureCur = [&]() {
        if (cur >= 0) return;
        int i = findSec("default");
        if (i < 0) { secs.push_back({"default", {}}); i = (int)secs.size() - 1; }
        cur = i;
    };
    auto addComment = [&](const std::string& s) {
        ensureCur();
        secs[cur].items.push_back({true, s, -1});
    };

    size_t start = 0;
    while (start < text.size()) {
        const size_t nl = text.find('\n', start);
        const std::string line = text.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
        start = nl == std::string::npos ? text.size() : nl + 1;
        const std::string t = trimS(line);
        if (t.empty()) continue;
        if (t[0] == ';' || t[0] == '#') { addComment(t); continue; }

        std::string name;
        bool broken = false;
        if (parseHeader(t, name, broken)) {
            int idx = findSec(name);
            if (idx >= 0) rep.mergedSections++;
            else { secs.push_back({name, {}}); idx = (int)secs.size() - 1; }
            if (broken) rep.bracketFixes++;
            cur = idx;
            continue;
        }

        std::string path;
        int value = -1;
        bool badValue = false;
        if (parsePlugin(t, path, value, badValue)) {
            std::string fixed;
            if (resolvePath(path, o, fixed)) {
                if (fixed != path) rep.pathFixes++;
                if (badValue) rep.valueFixes++;
                ensureCur();
                secs[cur].items.push_back({false, fixed, value});
                continue;
            }
            // مسار أو اسم إضافة لا يطابق أي ملف موجود في مجلد الإضافات: يُحذف السطر
            // إذا كانت قائمة الملفات فارغة (المجلد غير معروف) لا نحذف شيئاً حتى لا نخسر الإعدادات
            if (!o.pluginFiles.empty()) { rep.removedMissing++; continue; }
        }
        // نص لا معنى له (أو مسار لا يمكن تحديد ملفه): يُعلَّق ولا يُحذف
        rep.commentedLines++;
        addComment("; " + t);
    }

    // إزالة التكرار داخل كل قسم: يبقى موضع أول ظهور، والقيمة الأخيرة هي التي تُعتمد
    for (auto& s : secs) {
        std::vector<Item> kept;
        std::map<std::string, size_t> at;
        for (const Item& it : s.items) {
            if (it.comment) { kept.push_back(it); continue; }
            auto f = at.find(it.text);
            if (f != at.end()) { rep.duplicatePlugins++; kept[f->second].value = it.value; }
            else { at[it.text] = kept.size(); kept.push_back(it); }
        }
        s.items.swap(kept);
    }

    // حذف إضافات الألعاب غير المثبتة (القسم default لا يُمس)
    for (size_t i = 0; i < secs.size();) {
        Sec& s = secs[i];
        if (isTitleId(s.name)) {
            bool hasPlugin = false;
            for (const Item& it : s.items) if (!it.comment) { hasPlugin = true; break; }
            if (!o.installedKnown) {
                if (hasPlugin) rep.removalSkipped = true;
            } else if (o.installedIds.count(s.name) == 0) {
                std::vector<Item> left;
                for (const Item& it : s.items) {
                    if (it.comment) left.push_back(it);
                    else rep.removedPlugins++;
                }
                s.items.swap(left);
                if (s.items.empty()) {
                    secs.erase(secs.begin() + (long)i);
                    rep.removedSections++;
                    if (cur == (int)i) cur = -1;
                    else if (cur > (int)i) --cur;
                    continue;
                }
            }
        }
        ++i;
    }

    std::string out;
    for (size_t i = 0; i < secs.size(); ++i) {
        const Sec& s = secs[i];
        if (i > 0) out += "\n";
        out += "[" + s.name + "]\n";
        for (const Item& it : s.items) {
            out += it.text;
            if (!it.comment) {
                if (it.value == 0) out += "=false";
                else if (it.value == 1) out += "=true";
            }
            out += "\n";
        }
    }
    return out;
}

}   // namespace PluginIniRepair
