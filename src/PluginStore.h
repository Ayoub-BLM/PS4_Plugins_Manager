#pragma once
#include <SDL2/SDL.h>
#include <cstdint>
#include <string>
#include <vector>

namespace PluginStore {
// فلترة البحث في المتجر (فارغ = بلا فلترة): تُطبَّق على القائمة التي تعيدها snapshot()
void setStoreFilter(const std::string& q);
// Envoi d'une URL PKG à Remote Package Installer (même console, 127.0.0.1:12800). Thread séparé, aucun blocage de l'interface.
namespace Rpi {
enum class State { Idle, Sending, Started, NotRunning, Failed, AlreadyInstalled };
bool isValidPkgUrl(const std::string& url);
// Retourne false si un envoi est déjà en cours ou si l'URL est invalide.
bool sendPkg(const std::string& url);
// État courant ; msg contient le texte renvoyé par la console quand State::Failed.
State state(std::string& msg);
void reset();
// حالة تقدم التثبيت (تُستدعى من الواجهة كل إطار، وتُحدَّث كل ثانية)
struct Progress { bool active; int percent; bool done; bool failed; };
Progress progress();
// خطاف يُستدعى قبل تهيئة BGFT لرفع صلاحيات الهروب من الـ sandbox (يرجّع true عند النجاح)
void setEscapeHook(bool (*hook)());
// خطاف يُستدعى بعد انتهاء التثبيت أو فشله لإرجاع الصلاحيات الأصلية (الهروب لا يبقى بعد التثبيت)
void setRestoreHook(void (*hook)());
// إلغاء تحميل الـ PKG الجاري (قبل بدء التثبيت)
void cancelDownload();
}
struct Entry {
    std::string id, name, version, description, descriptionAr, author, support;   // descriptionAr: optional Arabic description (shown when the app language is Arabic)
    std::string filename, filename2, urlDownload, url2download, packageUrl, installPath;
    std::string pkgUrl;   // optional direct PKG URL: sent to the console's Remote Package Installer, never downloaded by the app
    std::string sha256, sha256_2, packageSha256, supportQR, qrText;
    // Optional per-plugin token editor, controlled by the catalog JSON ("tokenEnabled" + "tokenPath").
    bool tokenEnabled = false;
    std::string tokenPath;   // absolute .txt, .json or .ini path the user's token is written to
    unsigned long long sizeBytes = 0;   // optional catalog "size" in bytes (0 = unknown): used to detect updates when no SHA-256 is given
    std::string tokenKey = "token";   // .json: key path (dots for nested objects); .ini: "key" or "Section.key"
};
// قراءة/كتابة التوكن داخل ملف JSON عند مفتاح محدد (مثلاً "token" أو "auth.token")
bool readTokenJson(const std::string& path, const std::string& key, std::string& out);
bool writeTokenJson(const std::string& path, const std::string& key, const std::string& value);
// قراءة/كتابة التوكن داخل ملف INI: المفتاح "token" (قبل أي [قسم]) أو "Section.token" (داخل [Section])
bool readTokenIni(const std::string& path, const std::string& key, std::string& out);
bool writeTokenIni(const std::string& path, const std::string& key, const std::string& value);
enum Phase { Idle, LoadingCatalog, LoadingQr, Ready, Downloading, Verifying, Installed, Failed, Canceled, Offline, Confirm };
struct Snapshot {
    Phase phase = Idle;
    std::vector<Entry> entries;
    uint64_t received = 0, total = 0;
    std::string message;
    bool catalogLoaded = false;
    std::string qrUrl, qrPath;
    std::string installedId;
    std::string catalogUpdated;   // "last update" of the catalog (from the JSON if it has one, else the time it was fetched)
};

class Client {
public:
    Client();
    ~Client();
    bool initialize();
    bool refreshCatalog(bool reset = false);
    // Starts downloading the support QR image straight into memory (nothing is written to the console's storage).
    bool loadSupportQr(const Entry& entry);
    // Moves the downloaded PNG bytes for `url` out of the client (true once, when they are ready).
    bool takeQrBytes(const std::string& url, std::vector<unsigned char>& out);
    bool download(const Entry& entry, const std::string& destinationDirectory, bool replace = false);
    // True if this plugin (its files, or its ZIP package folder) is already on the console.
    static bool exists(const Entry& entry, const std::string& destinationDirectory);
    // 0 = not installed, 1 = installed and identical to the catalog (SHA-256), 2 = installed but different (update), 3 = installed, cannot compare
    static int installedState(const Entry& entry, const std::string& destinationDirectory);
    // Answer to a Phase::Confirm question: 1 = replace everything, 2 = install the plugin file(s) only (no ZIP), 3 = cancel.
    void answerConfirm(int answer);
    void cancel();
    void settle();   // بعد معالجة نتيجة نهائية: يرجّع الحالة لـ Ready حتى تكون الإضافات قابلة للتحميل
    Snapshot snapshot();
    void shutdown();
private:
    struct Job;
    static int worker(void* opaque);
    void run(Job* job);
    void reap();
    void publish(Phase phase, const std::string& message);
    static void onProgress(void* opaque, uint64_t received, uint64_t total);
    SDL_mutex* mutex_ = nullptr;
    SDL_Thread* thread_ = nullptr;
    SDL_atomic_t cancel_{};
    SDL_atomic_t answer_{};
    SDL_atomic_t gen_{};   // رقم طلب الكتالوج: الطلب القديم المعلّق يُتجاهل
    bool done_ = false, catalogLoaded_ = false;
    Phase phase_ = Idle;
    std::vector<Entry> entries_;
    uint64_t received_ = 0, total_ = 0;
    std::string message_;
    std::string qrUrl_, qrPath_;
    std::string qrBytes_;   // PNG bytes of the support QR (memory only)
    std::string installedId_;
    std::string catalogUpdated_;
};
}
