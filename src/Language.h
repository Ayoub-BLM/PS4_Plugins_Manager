#pragma once
// Everything about the app language in one file:
// the language list and saved preference, the Portuguese dictionary,
// the localized UI strings, and the text helpers used by the screens.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>

// All user-facing UI strings in one place.
// Language order: 0 English, 1 French, 2 Spanish, 3 German, 4 Arabic.
// Use with languageText(kName) from main.cpp.

struct LocalizedText {
    const char* en;
    const char* fr;
    const char* es;
    const char* de;
    const char* ar;
};

// Localized UI strings. Order: English, French, Spanish, German, Arabic.
// Arabic is optional: when it is missing, the English text is used.

static const LocalizedText kWaitAnAppIsBeing = {"Wait: an app is being installed", "Attendez : une application est en cours d'installation", "Espera: se está instalando una app", "Warten: eine App wird gerade installiert", "انتظر: يجري تثبيت تطبيق الآن"};
static const LocalizedText kWaitAPluginIsDownloading = {"Wait: a plugin is downloading", "Attendez : un plugin se télécharge", "Espera: se está descargando un plugin", "Warten: ein Plugin wird heruntergeladen", "انتظر: تُنزَّل إضافة الآن"};
static const LocalizedText kStartingInstallation = {"Starting installation...", "Démarrage de l'installation...", "Iniciando la instalación...", "Installation wird gestartet...", "جاري بدء التثبيت..."};
static const LocalizedText kAnInstallIsAlreadyBeing = {"An install is already being sent", "Un envoi est déjà en cours", "Ya hay un envío en curso", "Ein Sendevorgang läuft bereits", "هناك إرسال قيد التنفيذ بالفعل"};
static const LocalizedText kCouldNotReadPluginsIni = {"Could not read plugins.ini", "Impossible de lire plugins.ini", "No se pudo leer plugins.ini", "plugins.ini konnte nicht gelesen werden", "تعذر قراءة plugins.ini"};
static const LocalizedText kPluginsIniIsEmpty = {"plugins.ini is empty", "plugins.ini est vide", "plugins.ini está vacío", "plugins.ini ist leer", "plugins.ini فارغ"};
static const LocalizedText kNoBackupFound = {"No backup found", "Aucune sauvegarde trouvée", "No se encontró ningún backup", "Keine Sicherung gefunden", "لا توجد نسخة احتياطية"};
static const LocalizedText kCouldNotRestoreBackup = {"Could not restore backup", "Impossible de restaurer la sauvegarde", "No se pudo restaurar el backup", "Sicherung konnte nicht wiederhergestellt werden", "تعذر استعادة النسخة الاحتياطية"};
static const LocalizedText kPluginsIniRestoredFromBackup = {"plugins.ini restored from backup", "plugins.ini restauré depuis la sauvegarde", "plugins.ini restaurado desde el backup", "plugins.ini aus Sicherung wiederhergestellt", "تمت استعادة plugins.ini من النسخة الاحتياطية"};
static const LocalizedText kCouldNotResetPluginsIni = {"Could not reset plugins.ini", "Impossible de réinitialiser plugins.ini", "No se pudo restablecer plugins.ini", "plugins.ini konnte nicht zurückgesetzt werden", "تعذر إعادة ضبط plugins.ini"};
static const LocalizedText kPluginsIniResetBackupBak = {"plugins.ini reset — backup .bak created", "plugins.ini réinitialisé — sauvegarde .bak créée", "plugins.ini restablecido — backup .bak creado", "plugins.ini zurückgesetzt — .bak-Sicherung erstellt", "تمت إعادة ضبط plugins.ini — أُنشئت نسخة احتياطية .bak"};
static const LocalizedText kTokenSaved = {"Token saved", "Jeton enregistré", "Token guardado", "Token gespeichert", "تم حفظ التوكن"};
static const LocalizedText kCouldNotSaveTheToken = {"Could not save the token file", "Impossible d'enregistrer le fichier du jeton", "No se pudo guardar el archivo del token", "Token-Datei konnte nicht gespeichert werden", "تعذر حفظ ملف التوكن"};
static const LocalizedText kResetCancelled = {"Reset cancelled", "Réinitialisation annulée", "Restablecimiento cancelado", "Zurücksetzen abgebrochen", "تم إلغاء إعادة الضبط"};
static const LocalizedText kLanguageSaved = {"Language saved", "Langue enregistrée", "Idioma guardado", "Sprache gespeichert", "تم حفظ اللغة"};
static const LocalizedText kCouldNotSaveLanguagePreference = {"Could not save language preference", "Impossible d’enregistrer la langue", "No se pudo guardar el idioma", "Sprache konnte nicht gespeichert werden", "تعذر حفظ اللغة"};
static const LocalizedText kLanguageUnchanged = {"Language unchanged", "Langue inchangée", "Idioma sin cambios", "Sprache unverändert", "لم تتغير اللغة"};
static const LocalizedText kCanceled = {"Canceled", "Annulé", "Cancelado", "Abgebrochen", "تم الإلغاء"};
static const LocalizedText kReplacingTheOldVersion = {"Replacing the old version...", "Remplacement de l'ancienne version...", "Reemplazando la versión anterior...", "Alte Version wird ersetzt...", "جاري استبدال النسخة القديمة..."};
static const LocalizedText kInstallingThePrxOnlyNo = {"Installing the .prx only (no ZIP)...", "Installation du .prx seul (sans ZIP)...", "Instalando solo el .prx (sin ZIP)...", "Nur die .prx wird installiert (ohne ZIP)...", "جاري تثبيت ملف prx فقط (بدون ZIP)..."};
static const LocalizedText kCheckingConnection = {"Checking connection...", "Vérification de la connexion...", "Comprobando la conexión...", "Verbindung wird geprüft...", "جاري فحص الاتصال..."};
static const LocalizedText kDownloadCanceled = {"Download canceled", "Téléchargement annulé", "Descarga cancelada", "Download abgebrochen", "تم إلغاء التحميل"};
static const LocalizedText kNoChangesToSave = {"No changes to save", "Aucune modification à enregistrer", "No hay cambios que guardar", "Keine Änderungen zu speichern", "لا توجد تغييرات للحفظ"};
static const LocalizedText kCanceled2 = {"Canceled", "Annul\u00e9", "Cancelado", "Abgebrochen", "\u062a\u0645 \u0627\u0644\u0625\u0644\u063a\u0627\u0621"};
static const LocalizedText kClose = {"Close", "Fermer", "Cerrar", "Schließen", "إغلاق"};
static const LocalizedText kAboutTheApp = {"About the app", "À propos", "Acerca de", "Über die App", "حول البرنامج"};
static const LocalizedText kNoInternetConnection = {"No internet connection", "Pas de connexion Internet", "Sin conexión a Internet", "Keine Internetverbindung", "لا يوجد اتصال بالإنترنت"};
static const LocalizedText kCheckYourConnectionAndTry = {"Check your connection and try again", "Vérifiez votre connexion et réessayez", "Revisa tu conexión e inténtalo de nuevo", "Prüfe deine Verbindung und versuche es erneut", "افحص اتصالك وحاول مرة أخرى"};
static const LocalizedText kPressXToRetryO = {"Press X to retry   -   O to go back", "X pour réessayer   -   O pour revenir", "X para reintentar   -   O para volver", "X = erneut versuchen   -   O = zurück", "اضغط X لإعادة المحاولة   -   O للرجوع"};
static const LocalizedText kNoPluginWithThisName = {"No plugin with this name", "Aucun plugin portant ce nom", "Ningún plugin con ese nombre", "Kein Plugin mit diesem Namen", "لا توجد إضافة بهذا الاسم"};
static const LocalizedText kOtherVersion = {"Other version", "Autre version", "Otra versión", "Andere Version", "\u0646\u0633\u062e\u0629 \u0645\u062e\u062a\u0644\u0641\u0629"};
static const LocalizedText kInstalled = {"Installed", "Installé", "Instalado", "Installiert", "\u0645\u062b\u0628\u0651\u062a"};
static const LocalizedText kFetchingPluginInfo = {"Fetching plugin info...", "Récupération des infos...", "Obteniendo información...", "Informationen werden geladen...", "جاري جلب معلومات الإضافة..."};
static const LocalizedText kLoadingSupportQr = {"Loading support QR", "Chargement du QR", "Cargando QR", "QR wird geladen", "\u062c\u0627\u0631\u0020\u062a\u062d\u0645\u064a\u0644\u0020\u0631\u0645\u0632\u0020QR"};
static const LocalizedText kQrImageUnavailable = {"QR image unavailable", "QR indisponible", "QR no disponible", "QR nicht verfügbar", "\u0635\u0648\u0631\u0629\u0020QR\u0020\u063a\u064a\u0631\u0020\u0645\u062a\u0627\u062d\u0629"};
static const LocalizedText kLoadingSupportQr2 = {"Loading support QR", "Chargement du QR", "Cargando QR", "QR wird geladen", "\u062c\u0627\u0631\u0020\u062a\u062d\u0645\u064a\u0644\u0020QR"};
static const LocalizedText kInstalled2 = {"Installed", "Installé", "Instalado", "Installiert", "\u062a\u0645 \u0627\u0644\u062a\u062b\u0628\u064a\u062a"};
static const LocalizedText kDownloadingPkg = {"Downloading PKG...", "Téléchargement du PKG...", "Descargando PKG...", "PKG wird heruntergeladen...", "جاري تحميل الملف"};
static const LocalizedText kPreparing = {"Preparing...", "Préparation...", "Preparando...", "Vorbereitung...", "جاري التحضير..."};
static const LocalizedText kInstallingApp = {"Installing app...", "Installation de l'application...", "Instalando la aplicación...", "App wird installiert...", "جاري تثبيت التطبيق"};
static const LocalizedText kAppIsAlreadyInstalled = {"App is already installed", "L'application est déjà installée", "La aplicación ya está instalada", "Die App ist bereits installiert", "التطبيق موجود بالفعل"};
static const LocalizedText kDownload = {"Download", "Télécharger", "Descargar", "Herunterladen", "تحميل الإضافة"};
static const LocalizedText kInstall = {"Install", "Installer", "Instalar", "Installieren", "تثبيت التطبيق"};
static const LocalizedText kInstallApp = {"Install app", "Installer l'application", "Instalar aplicación", "App installieren", "تثبيت التطبيق"};
static const LocalizedText kEditToken = {"Edit token", "Modifier le jeton", "Editar token", "Token bearbeiten", "تعديل التوكن"};
static const LocalizedText kEnableTheFtpServerIn = {"Enable the FTP server in GoldHEN", "Activez le serveur FTP dans GoldHEN", "Activa el servidor FTP en GoldHEN", "Aktiviere den FTP-Server in GoldHEN", "فعّل خادم FTP في GoldHEN"};
static const LocalizedText kTransferServerIsNotConnected = {"Transfer server is not connected", "Serveur de transfert non connecté", "Servidor de transferencia no conectado", "Übertragungsserver nicht verbunden", "\u062e\u0627\u062f\u0645 \u0627\u0644\u0646\u0642\u0644 \u063a\u064a\u0631 \u0645\u062a\u0635\u0644"};
static const LocalizedText kOpenStore = {"Open Store", "Ouvrir", "Abrir", "Öffnen", "افتح المتجر"};
static const LocalizedText kBrowseAndDownloadPluginsDirectly = {"Browse and download plugins directly from the Store.", "Parcourez et téléchargez des extensions depuis la boutique.", "Explora y descarga complementos desde la tienda.", "Durchsuche und lade Erweiterungen im Store herunter.", "تصفح وتنزيل الإضافات من المتجر."};
static const LocalizedText kTheFileAlreadyExists = {"The file already exists", "Le fichier existe déjà", "El archivo ya existe", "Die Datei ist bereits vorhanden", "الملف موجود مسبقاً"};
static const LocalizedText kDownloadThisVersionTheOld = {"Download this version? The old one will be deleted", "Télécharger cette version ? L'ancienne sera supprimée", "¿Descargar esta versión? Se eliminará la anterior", "Diese Version laden? Die alte wird gelöscht", "هل تريد تحميل هذه النسخة؟ سيتم حذف القديمة"};
static const LocalizedText kAlreadyOnTheConsole = {"Already on the console: ", "Déjà sur la console : ", "Ya está en la consola: ", "Bereits auf der Konsole: ", "موجود على الجهاز: "};
static const LocalizedText kReplaceAllDeletesTheOld = {"Replace all: deletes the old .prx and its ZIP files, installs the new ones", "Tout remplacer : supprime l'ancien .prx et ses fichiers ZIP, installe les nouveaux", "Reemplazar todo: borra el .prx y los archivos ZIP anteriores e instala los nuevos", "Alles ersetzen: löscht die alte .prx und ZIP-Dateien und installiert die neuen", "استبدال الكل: يحذف ملف prx القديم وملفات الـ ZIP ويثبّت الجديد"};
static const LocalizedText kPrxOnlyReplacesJustThe = {".prx only: replaces just the plugin and keeps your existing ZIP files", ".prx seul : remplace seulement le plugin et garde vos fichiers ZIP", "Solo .prx: reemplaza solo el plugin y conserva tus archivos ZIP", "Nur .prx: ersetzt nur das Plugin und behält deine ZIP-Dateien", "prx فقط: يستبدل البلاجن فقط ويبقي ملفات الـ ZIP الموجودة"};
static const LocalizedText kReplaceAll = {"Replace all", "Tout remplacer", "Reemplazar todo", "Alles ersetzen", "استبدال الكل"};
static const LocalizedText kDownload2 = {"Download", "Télécharger", "Descargar", "Laden", "تحميل"};
static const LocalizedText kPrxOnlyNoZip = {".prx only (no ZIP)", ".prx seul (sans ZIP)", "Solo .prx (sin ZIP)", "Nur .prx (ohne ZIP)", "prx فقط بدون ZIP"};
static const LocalizedText kCancel = {"Cancel", "Annuler", "Cancelar", "Abbrechen", "إلغاء"};
static const LocalizedText kInstallThisAppOnThe = {"Install this app on the console?", "Installer cette application sur la console ?", "¿Instalar esta aplicación en la consola?", "Diese App auf der Konsole installieren?", "تثبيت هذا التطبيق على الجهاز؟"};
static const LocalizedText kTheAppWillBeInstalled = {"The app will be installed on this console.", "L'application sera installée sur cette console.", "La aplicación se instalará en esta consola.", "Die App wird auf dieser Konsole installiert.", "سيتم تثبيت التطبيق على هذا الجهاز."};
static const LocalizedText kOk = {"OK", "OK", "OK", "OK", "موافق"};
static const LocalizedText kNo = {"No", "Non", "No", "Nein", "لا"};
static const LocalizedText kRestorePluginsIniFromBackup = {"Restore plugins.ini from backup?", "Restaurer plugins.ini depuis la sauvegarde ?", "¿Restaurar plugins.ini desde el backup?", "plugins.ini aus Sicherung wiederherstellen?", "استعادة plugins.ini من النسخة الاحتياطية؟"};
static const LocalizedText kResetPluginsIni = {"Reset plugins.ini?", "Réinitialiser plugins.ini ?", "¿Restablecer plugins.ini?", "plugins.ini zurücksetzen?", "إعادة ضبط plugins.ini؟"};
static const LocalizedText kTheCurrentFileWillBe = {"The current file will be replaced by the backup. The current file will become the new .bak.", "Le fichier actuel sera remplacé par la sauvegarde. Le fichier actuel deviendra le nouveau .bak.", "El archivo actual será reemplazado por el backup. El actual pasará a ser el nuevo .bak.", "Die aktuelle Datei wird durch die Sicherung ersetzt. Die aktuelle wird zur neuen .bak-Datei.", "سيُستبدل الملف الحالي بالنسخة الاحتياطية، ويصبح الحالي هو النسخة .bak الجديدة"};
static const LocalizedText kTheFileWillBeEmptied = {"The file will be emptied. A .bak backup will be created.", "Le fichier sera vidé. Une sauvegarde .bak sera créée.", "El archivo quedará vacío. Se creará una copia .bak.", "Die Datei wird geleert. Eine .bak-Sicherung wird erstellt.", "سيُفرَّغ الملف، وستُنشأ نسخة احتياطية .bak"};
static const LocalizedText kSettings = {"Settings", "Paramètres", "Ajustes", "Einstellungen", "الإعدادات"};
static const LocalizedText kLanguage = {"Language", "Langue", "Idioma", "Sprache", "اللغة"};
static const LocalizedText kRepairPluginsIni = {"Repair plugins.ini", "Réparer plugins.ini", "Reparar plugins.ini", "plugins.ini reparieren", "إصلاح plugins.ini"};
static const LocalizedText kAboutTheApp2 = {"About the app", "À propos", "Acerca de", "Über die App", "عن التطبيق"};
static const LocalizedText kBrowsePluginsIni = {"Browse plugins.ini", "Parcourir plugins.ini", "Ver plugins.ini", "plugins.ini anzeigen", "تصفح plugins.ini"};
static const LocalizedText kResetPluginsIni2 = {"Reset plugins.ini", "Réinitialiser plugins.ini", "Restablecer plugins.ini", "plugins.ini zurücksetzen", "إعادة ضبط plugins.ini"};
static const LocalizedText kRestoreBackup = {"Restore backup", "Restaurer la sauvegarde", "Restaurar copia", "Sicherung wiederherstellen", "استعادة النسخة الاحتياطية"};
static const LocalizedText kIssues = {"issues", "problèmes", "problemas", "Probleme", "مشاكل"};
static const LocalizedText kOk2 = {"OK", "OK", "OK", "OK", "سليم"};
static const LocalizedText kView = {"View", "Voir", "Ver", "Anzeigen", "عرض"};
static const LocalizedText kReset = {"Reset", "Réinitialiser", "Restablecer", "Zurücksetzen", "إعادة ضبط"};
static const LocalizedText kRestore = {"Restore", "Restaurer", "Restaurar", "Wiederherstellen", "استعادة"};
static const LocalizedText kChooseLanguage = {"Choose language", "Choisir la langue", "Elegir idioma", "Sprache auswählen", "اختر لغة البرنامج"};
static const LocalizedText kCurrent = {"Current", "Actuelle", "Actual", "Aktuell", "الحالية"};
static const LocalizedText kDiscordServer = {"Discord server", "Serveur Discord", "Servidor de Discord", "Discord-Server", "سيرفر الدسكورد"};
static const LocalizedText kDeveloper = {"Developer", "Développeur", "Desarrollador", "Entwickler", "المطوّر"};
static const LocalizedText kPluginsInTheStore = {"Plugins in the store", "Plugins dans la boutique", "Plugins en la tienda", "Plugins im Store", "عدد الإضافات"};
static const LocalizedText kCatalogLastUpdated = {"Catalog last updated", "Dernière mise à jour du catalogue", "Última actualización del catálogo", "Katalog zuletzt aktualisiert", "آخر تحديث للقائمة"};
static const LocalizedText kOpenTheStoreOnceTo = {"Open the Store once to load the catalog", "Ouvrez la boutique pour charger le catalogue", "Abre la tienda para cargar el catálogo", "Öffne den Store, um den Katalog zu laden", "افتح المتجر مرة لتحميل القائمة"};
static const LocalizedText kInstallStarted = {"Install started", "Installation lancée", "Instalación iniciada", "Installation gestartet", "بدأ التثبيت"};
static const LocalizedText kAppIsAlreadyInstalled2 = {"App is already installed", "L'application est déjà installée", "La aplicación ya está instalada", "Die App ist bereits installiert", "التطبيق مثبت بالفعل"};
static const LocalizedText kCouldNotStartTheInstallation = {"Could not start the installation. Try again.", "Impossible de lancer l'installation. Réessayez.", "No se pudo iniciar la instalación. Inténtalo de nuevo.", "Installation konnte nicht gestartet werden. Erneut versuchen.", "تعذّر بدء التثبيت، حاول مرة أخرى."};
static const LocalizedText kInstallFailed = {"Install failed", "Échec de l'installation", "Error de instalación", "Installation fehlgeschlagen", "فشل التثبيت"};
static const LocalizedText kInstallationComplete = {"Installation complete", "Installation terminée", "Instalación completada", "Installation abgeschlossen", "اكتمل التثبيت"};
static const LocalizedText kInstallationFailed = {"Installation failed", "Échec de l'installation", "Error de instalación", "Installation fehlgeschlagen", "فشل التثبيت"};
static const LocalizedText kBack = {"Back", "Retour", "Volver", "Zurück", "رجوع"};
static const LocalizedText kChangeSelection = {"Change selection", "Changer", "Cambiar", "Auswahl ändern", "تبديل الاختيار"};
static const LocalizedText kConfirm = {"Confirm", "Confirmer", "Confirmar", "Bestätigen", "تأكيد"};
static const LocalizedText kRetry = {"Retry", "Réessayer", "Reintentar", "Erneut", "إعادة المحاولة"};
static const LocalizedText kSearch = {"Search", "Rechercher", "Buscar", "Suchen", "بحث"};
static const LocalizedText kNavigate = {"Navigate", "Naviguer", "Navegar", "Navigieren", "التنقل"};
static const LocalizedText kNavigate2 = {"Navigate", "Naviguer", "Navegar", "Navigieren", "\u0627\u0644\u062a\u0646\u0642\u0644"};
static const LocalizedText kSelect = {"Select", "Choisir", "Seleccionar", "Auswählen", "\u0627\u062e\u062a\u064a\u0627\u0631"};
static const LocalizedText kStore = {"Store", "Boutique", "Tienda", "Shop", "\u0627\u0644\u0645\u062a\u062c\u0631"};
static const LocalizedText kRescan = {"Rescan", "Relancer", "Volver a buscar", "Erneut suchen", "\u0625\u0639\u0627\u062f\u0629 \u0627\u0644\u0641\u062d\u0635"};
static const LocalizedText kCloseSettings = {"Close settings", "Fermer les paramètres", "Cerrar ajustes", "Einstellungen schließen", "إغلاق الإعدادات"};

// تفضيل اللغة محفوظ في /data/PluginManager/language.ini.
static int gLanguage = 0; // 0 English, 1 Français, 2 Español, 3 Deutsch, 4 العربية
static const char* LANGUAGE_NAMES[] = {"English", "Français", "Español", "Deutsch", "العربية", "Português"};
// الترجمة البرتغالية: مفتاح كل نص هو نصه الإنجليزي الأصلي
static const char* ptText(const char* en) {
    struct PtHit { std::string en; const char* pt; };
    static std::unordered_map<const char*, PtHit> hits;
    auto hit = hits.find(en);
    if (hit != hits.end() && hit->second.en == en) return hit->second.pt;
    static const std::map<std::string, std::string> pt = {
        {"Restore backup", "Restaurar backup"},
        {"Restore", "Restaurar"},
        {"Restore plugins.ini from backup?", "Restaurar plugins.ini do backup?"},
        {"The current file will be replaced by the backup. The current file will become the new .bak.", "O arquivo atual será substituído pelo backup. O atual passará a ser o novo .bak."},
        {"plugins.ini restored from backup", "plugins.ini restaurado do backup"},
        {"Could not restore backup", "Não foi possível restaurar o backup"},
        {"No backup found", "Nenhum backup encontrado"},
        {"Reset plugins.ini", "Redefinir plugins.ini"},
        {"Reset plugins.ini?", "Redefinir plugins.ini?"},
        {"Reset", "Redefinir"},
        {"The file will be emptied. A .bak backup will be created.", "O arquivo será esvaziado. Um backup .bak será criado."},
        {"plugins.ini reset — backup .bak created", "plugins.ini redefinido — backup .bak criado"},
        {"Reset cancelled", "Redefinição cancelada"},
        {"Could not reset plugins.ini", "Não foi possível redefinir o plugins.ini"},
        {"Browse plugins.ini", "Ver plugins.ini"},
        {"View", "Ver"},
        {"Could not read plugins.ini", "Não foi possível ler o plugins.ini"},
        {"plugins.ini is empty", "O plugins.ini está vazio"},
        {" attempts — files received:", "tentativas — arquivos recebidos:"},
        {" of ", "de "},
        {" — found ", "— encontrados "},
        {".prx only (no ZIP)", "Somente .prx (sem ZIP)"},
        {".prx only: replaces just the plugin and keeps your existing ZIP files", "Somente .prx: substitui só o plugin e mantém seus arquivos ZIP"},
        {"About the app", "Sobre o aplicativo"},
        {"All games", "Todos os jogos"},
        {"Already on the console: ", "Já está no console: "},
        {"An install is already being sent", "Uma instalação já está sendo enviada"},
        {"App is already installed", "O aplicativo já está instalado"},
        {"Author", "Autor"},
        {"Back", "Voltar"},
        {"Browse and download plugins directly from the Store.", "Navegue e baixe plugins diretamente da loja."},
        {"Cancel", "Cancelar"},
        {"Cancel download", "Cancelar download"},
        {"Canceled", "Cancelado"},
        {"Catalog last updated", "Catálogo atualizado por último em"},
        {"Catalog ready", "Catálogo pronto"},
        {"Change selection", "Alterar seleção"},
        {"Changes are not saved; press Triangle to save", "As alterações não foram salvas; pressione Triângulo para salvar"},
        {"Check your connection and try again", "Verifique sua conexão e tente novamente"},
        {"Checking connection...", "Verificando conexão..."},
        {"Choose a game, then manage plugins", "Escolha um jogo e gerencie os plugins"},
        {"Choose another game", "Escolher outro jogo"},
        {"Choose language", "Escolher idioma"},
        {"Close", "Fechar"},
        {"Close settings", "Fechar configurações"},
        {"Compatibility", "Compatibilidade"},
        {"Confirm", "Confirmar"},
        {"Confirm deletion", "Confirmar exclusão"},
        {"Connected", "Conectado"},
        {"Could not connect on port 2121", "Não foi possível conectar na porta 2121"},
        {"Could not decode this game's icon", "Não foi possível decodificar o ícone deste jogo"},
        {"Could not delete plugin", "Não foi possível excluir o plugin"},
        {"Could not load catalog", "Não foi possível carregar o catálogo"},
        {"Could not save icon to /data/PluginManager", "Não foi possível salvar o ícone em /data/PluginManager"},
        {"Could not save language preference", "Não foi possível salvar o idioma"},
        {"Could not save the token file", "Não foi possível salvar o arquivo de token"},
        {"Could not start scan", "Não foi possível iniciar a busca"},
        {"Could not start the installation. Try again.", "Não foi possível iniciar a instalação. Tente novamente."},
        {"Current", "Atual"},
        {"D-pad", "Direcional"},
        {"Default settings for all games", "Configurações padrão para todos os jogos"},
        {"Delete", "Excluir"},
        {"Delete plugin", "Excluir plugin"},
        {"Delete this plugin permanently?", "Excluir este plugin permanentemente?"},
        {"Deletion cancelled", "Exclusão cancelada"},
        {"Description", "Descrição"},
        {"Developer", "Desenvolvedor"},
        {"Disabled", "Desativado"},
        {"Disconnected", "Desconectado"},
        {"Discord server", "Servidor do Discord"},
        {"Download", "Baixar"},
        {"Download canceled", "Download cancelado"},
        {"Download this version? The old one will be deleted", "Baixar esta versão? A antiga será excluída"},
        {"Downloading PKG...", "Baixando PKG..."},
        {"Downloading plugin", "Baixando plugin"},
        {"Edit plugins for this game", "Editar plugins deste jogo"},
        {"Edit settings for all games", "Editar configurações de todos os jogos"},
        {"Edit token", "Editar token"},
        {"Enable / disable", "Ativar / desativar"},
        {"Enable the FTP server in GoldHEN", "Ative o servidor FTP no GoldHEN"},
        {"Enabled", "Ativado"},
        {"Enabled plugins:", "Plugins ativados:"},
        {"Entries scanned:", "Itens verificados:"},
        {"FTP login failed", "Falha no login FTP"},
        {"FTP:", "FTP:"},
        {"Failed to receive server response", "Falha ao receber a resposta do servidor"},
        {"Fetching game icons", "Buscando ícones dos jogos"},
        {"Fetching plugin info...", "Buscando informações do plugin..."},
        {"Folders ", "Pastas "},
        {"Games", "Jogos"},
        {"Games found:", "Jogos encontrados:"},
        {"Games:", "Jogos:"},
        {"Icons available:", "Ícones disponíveis:"},
        {"Icons available: ", "Ícones disponíveis: "},
        {"Icons fetched over FTP:", "Ícones baixados via FTP:"},
        {"Install", "Instalar"},
        {"Install failed", "Falha na instalação"},
        {"Install started", "Instalação iniciada"},
        {"Install this app on the console?", "Instalar este aplicativo no console?"},
        {"Installation complete", "Instalação concluída"},
        {"Installation failed", "Falha na instalação"},
        {"Installed", "Instalado"},
        {"Installing app...", "Instalando aplicativo..."},
        {"Installing the .prx only (no ZIP)...", "Instalando somente o .prx (sem ZIP)..."},
        {"Language", "Idioma"},
        {"Language saved", "Idioma salvo"},
        {"Language unchanged", "Idioma não alterado"},
        {"Last server reply:", "Última resposta do servidor:"},
        {"Load the catalog to browse plugins", "Carregue o catálogo para navegar pelos plugins"},
        {"Loading catalog", "Carregando catálogo"},
        {"Loading support QR", "Carregando QR de suporte"},
        {"Manage game plugins", "Gerenciar plugins de jogos"},
        {"Manage plugins", "Gerenciar plugins"},
        {"Navigate", "Navegar"},
        {"No", "Não"},
        {"No changes to save", "Nenhuma alteração para salvar"},
        {"No games found", "Nenhum jogo encontrado"},
        {"No icon file found for this game", "Nenhum arquivo de ícone encontrado para este jogo"},
        {"No installed games found", "Nenhum jogo instalado encontrado"},
        {"No internet connection", "Sem conexão com a internet"},
        {"No plugin with this name", "Nenhum plugin com este nome"},
        {"No plugins in the GoldHEN folder", "Nenhum plugin na pasta do GoldHEN"},
        {"No plugins in the store yet", "Ainda não há plugins na loja"},
        {"No plugins to delete", "Nenhum plugin para excluir"},
        {"Not used", "Não usado"},
        {"OK", "OK"},
        {"Offline cache", "Cache offline"},
        {"Open Store", "Abrir loja"},
        {"Open the Store once to load the catalog", "Abra a loja uma vez para carregar o catálogo"},
        {"Place PRX or SPRX files in: /data/GoldHEN/plugins", "Coloque arquivos PRX ou SPRX em: /data/GoldHEN/plugins"},
        {"Plugin Manager", "Gerenciador de plugins"},
        {"Plugin Store", "Loja de plugins"},
        {"Plugin deleted", "Plugin excluído"},
        {"Plugin deleted and plugins.ini updated", "Plugin excluído e plugins.ini atualizado"},
        {"Plugin deleted — press Triangle to save", "Plugin excluído — pressione Triângulo para salvar"},
        {"Plugin deleted; press Triangle to save plugins.ini", "Plugin excluído; pressione Triângulo para salvar o plugins.ini"},
        {"Plugin files in PRX and SPRX format", "Arquivos de plugin nos formatos PRX e SPRX"},
        {"Plugins in the store", "Plugins na loja"},
        {"Plugins:", "Plugins:"},
        {"Preparing...", "Preparando..."},
        {"Press Triangle to save", "Pressione Triângulo para salvar"},
        {"Press X to retry   -   O to go back", "Pressione X para tentar de novo   -   O para voltar"},
        {"QR image unavailable", "Imagem do QR indisponível"},
        {"Readable folders:", "Pastas legíveis:"},
        {"Reading plugins", "Lendo plugins"},
        {"Refresh catalog", "Atualizar catálogo"},
        {"Repair plugins.ini", "Reparar plugins.ini"},
        {"Replace all", "Substituir tudo"},
        {"Replace all: deletes the old .prx and its ZIP files, installs the new ones", "Substituir tudo: exclui o .prx antigo e seus ZIPs e instala os novos"},
        {"Replacing the old version...", "Substituindo a versão antiga..."},
        {"Rescan", "Buscar novamente"},
        {"Rescan started", "Nova busca iniciada"},
        {"Results appear as the scan runs", "Os resultados aparecem durante a busca"},
        {"Retry", "Tentar novamente"},
        {"SHA-256 verification failed", "Falha na verificação SHA-256"},
        {"Save", "Salvar"},
        {"Save changes", "Salvar alterações"},
        {"Save changes first; rescanning will discard them", "Salve as alterações primeiro; buscar novamente as descartará"},
        {"Save failed; check GoldHEN folder permissions", "Falha ao salvar; verifique as permissões da pasta do GoldHEN"},
        {"Scan already in progress", "A busca já está em andamento"},
        {"Scan complete", "Busca concluída"},
        {"Scan complete — games found:", "Busca concluída — jogos encontrados:"},
        {"Scan timed out; stopping", "Tempo da busca esgotado; parando"},
        {"Scanning game folders", "Verificando pastas de jogos"},
        {"Scanning games", "Verificando jogos"},
        {"Scanning games and plugins...", "Verificando jogos e plugins..."},
        {"Scanning over FTP", "Verificando via FTP"},
        {"Scanning...", "Verificando..."},
        {"Search", "Pesquisar"},
        {"Select", "Selecionar"},
        {"Settings", "Configurações"},
        {"Install app", "Instalar aplicativo"},
        {"Wait: an app is being installed", "Aguarde: um aplicativo está sendo instalado"},
        {"Wait: a plugin is downloading", "Aguarde: um plugin está sendo baixado"},
        {"Other version", "Outra versão"},
        {"Settings are saved in /data/GoldHEN/plugins.ini", "As configurações são salvas em /data/GoldHEN/plugins.ini"},
        {"Starting installation...", "Iniciando instalação..."},
        {"Store", "Loja"},
        {"The app will be installed on this console.", "O aplicativo será instalado neste console."},
        {"The file already exists", "O arquivo já existe"},
        {"The file will be deleted permanently", "O arquivo será excluído permanentemente"},
        {"Token saved", "Token salvo"},
        {"Transfer server is not connected", "O servidor de transferência não está conectado"},
        {"Unsaved changes", "Alterações não salvas"},
        {"Verifying file integrity", "Verificando a integridade do arquivo"},
        {"Version", "Versão"},
        {"Wait for plugins to load", "Aguarde o carregamento dos plugins"},
        {"backup .bak created", "backup .bak criado"},
        {"issues", "problemas"},
        {"plugins.ini saved", "plugins.ini salvo"},
        {"plugins.ini saved — backup .bak created", "plugins.ini salvo — backup .bak criado"},
    };
    auto it = pt.find(en);
    const char* out = it != pt.end() ? it->second.c_str() : en;
    if (hits.size() < 4096) hits[en] = PtHit{std::string(en), out};
    return out;
}
static const char* languageText(const LocalizedText& text) {
    if (gLanguage == 5) return ptText(text.en);
    const char* values[] = {text.en, text.fr, text.es, text.de, text.ar ? text.ar : text.en};
    return values[gLanguage >= 0 && gLanguage < 6 ? gLanguage : 0];
}
static std::string ltr(const std::vector<std::string>& e, int lang) { return lang == 5 ? std::string(ptText(e[0].c_str())) : e[lang]; }
static std::string tr(const std::string& s) {
    // لا تعِد البحث في قاموس الترجمة لكل نص في كل إطار؛ خزّن الترجمات الشائعة.
    static std::map<std::string, std::string> translated[6];
    static int cachedLanguage = -1;
    if (cachedLanguage != gLanguage) {
        for (auto& cache : translated) cache.clear();
        cachedLanguage = gLanguage;
    }
    const int lang = gLanguage >= 0 && gLanguage < 6 ? gLanguage : 0;
    auto cached = translated[lang].find(s);
    if (cached != translated[lang].end()) return cached->second;
    if (lang == 4) return s; // النصوص المصدرية عربية بالفعل.
    static const std::map<std::string, std::vector<std::string>> words = {
        {"مدير الإضافات", {"Plugin Manager", "Gestionnaire de plugins", "Gestor de plugins", "Plugin-Manager", "مدير الإضافات"}},
        {"إدارة إضافات الألعاب", {"Manage game plugins", "Gérer les plugins des jeux", "Gestionar plugins de juegos", "Spiel-Plugins verwalten", "إدارة إضافات الألعاب"}},
        {"جاري الفحص...", {"Scanning...", "Analyse en cours...", "Buscando...", "Suche läuft...", "جاري الفحص..."}},
        {"الألعاب", {"Games", "Jeux", "Juegos", "Spiele", "الألعاب"}},
        {"كل الألعاب", {"All games", "Tous les jeux", "Todos los juegos", "Alle Spiele", "كل الألعاب"}},
        {"الإعداد الافتراضي لكل الألعاب", {"Default settings for all games", "Paramètres par défaut pour tous les jeux", "Ajustes predeterminados para todos", "Standardeinstellungen für alle Spiele", "الإعداد الافتراضي لكل الألعاب"}},
        {"إدارة الإضافات", {"Manage plugins", "Gérer les plugins", "Gestionar plugins", "Plugins verwalten", "إدارة الإضافات"}},
        {"لا توجد إضافات في مجلد GoldHEN", {"No plugins in the GoldHEN folder", "Aucun plugin dans le dossier GoldHEN", "No hay plugins en la carpeta GoldHEN", "Keine Plugins im GoldHEN-Ordner", "لا توجد إضافات في مجلد GoldHEN"}},
        {"الإضافات المفعّلة:", {"Enabled plugins:", "Plugins activés :", "Plugins activados:", "Aktive Plugins:", "الإضافات المفعّلة:"}},
        {"ملفات الإضافات بصيغتي PRX و SPRX", {"Plugin files in PRX and SPRX format", "Fichiers plugin au format PRX et SPRX", "Archivos plugin en formato PRX y SPRX", "Plugin-Dateien im PRX- und SPRX-Format", "ملفات الإضافات بصيغتي PRX و SPRX"}},
        {"تغييرات غير محفوظة", {"Unsaved changes", "Modifications non enregistrées", "Cambios sin guardar", "Nicht gespeicherte Änderungen", "تغييرات غير محفوظة"}},
        {"جاري فحص الألعاب والإضافات...", {"Scanning games and plugins...", "Analyse des jeux et plugins...", "Buscando juegos y plugins...", "Spiele und Plugins werden gesucht...", "جاري فحص الألعاب والإضافات..."}},
        {"النتائج تظهر تدريجياً أثناء الفحص", {"Results appear as the scan runs", "Les résultats apparaissent pendant l’analyse", "Los resultados aparecen durante el análisis", "Ergebnisse erscheinen während der Suche", "النتائج تظهر تدريجياً أثناء الفحص"}},
        {"لم يتم العثور على ألعاب مثبتة", {"No installed games found", "Aucun jeu installé trouvé", "No se encontraron juegos instalados", "Keine installierten Spiele gefunden", "لم يتم العثور على ألعاب مثبتة"}},
        {"ضع ملفات PRX أو SPRX في المجلد: /data/GoldHEN/plugins", {"Place PRX or SPRX files in: /data/GoldHEN/plugins", "Placez les fichiers PRX ou SPRX dans : /data/GoldHEN/plugins", "Coloca archivos PRX o SPRX en: /data/GoldHEN/plugins", "Lege PRX- oder SPRX-Dateien hier ab: /data/GoldHEN/plugins", "ضع ملفات PRX أو SPRX في المجلد: /data/GoldHEN/plugins"}},
        {"مفعّل", {"Enabled", "Activé", "Activado", "Aktiviert", "مفعّل"}}, {"متوقف", {"Disabled", "Désactivé", "Desactivado", "Deaktiviert", "متوقف"}},
        {"حفظ التغييرات", {"Save changes", "Enregistrer", "Guardar cambios", "Änderungen speichern", "حفظ التغييرات"}},
        {"حذف الإضافة", {"Delete plugin", "Supprimer le plugin", "Eliminar plugin", "Plugin löschen", "حذف الإضافة"}},
        {"تأكيد الحذف", {"Confirm deletion", "Confirmer la suppression", "Confirmar eliminación", "Löschen bestätigen", "تأكيد الحذف"}},
        {"هل أنت متأكد من حذف هذه الإضافة نهائياً؟", {"Delete this plugin permanently?", "Supprimer définitivement ce plugin ?", "¿Eliminar este plugin permanentemente?", "Dieses Plugin dauerhaft löschen?", "هل أنت متأكد من حذف هذه الإضافة نهائياً؟"}},
        {"سيُحذف الملف من الجهاز ولا يمكن التراجع عن هذا الإجراء", {"The file will be deleted permanently", "Le fichier sera supprimé définitivement", "El archivo se eliminará permanentemente", "Die Datei wird dauerhaft gelöscht", "سيُحذف الملف من الجهاز ولا يمكن التراجع عن هذا الإجراء"}},
        {"حذف", {"Delete", "Supprimer", "Eliminar", "Löschen", "حذف"}}, {"إلغاء", {"Cancel", "Annuler", "Cancelar", "Abbrechen", "إلغاء"}},
        {"التنقل", {"Navigate", "Naviguer", "Navegar", "Navigieren", "التنقل"}}, {"اختيار", {"Select", "Choisir", "Seleccionar", "Auswählen", "اختيار"}},
        {"إعادة الفحص", {"Rescan", "Relancer", "Volver a buscar", "Erneut suchen", "إعادة الفحص"}}, {"تفعيل وتعطيل", {"Enable / disable", "Activer / désactiver", "Activar / desactivar", "Aktivieren / deaktivieren", "تفعيل وتعطيل"}},
        {"حفظ", {"Save", "Enregistrer", "Guardar", "Speichern", "حفظ"}}, {"رجوع", {"Back", "Retour", "Atrás", "Zurück", "رجوع"}},
        {"تبديل الاختيار", {"Change selection", "Changer la sélection", "Cambiar selección", "Auswahl ändern", "تبديل الاختيار"}},
        {"تأكيد", {"Confirm", "Confirmer", "Confirmar", "Bestätigen", "تأكيد"}},
        {"مدير الإضافات", {"Plugin Manager", "Gestionnaire de plugins", "Gestor de plugins", "Plugin-Manager", "مدير الإضافات"}}
        ,{"قراءة الإضافات", {"Reading plugins", "Lecture des plugins", "Leyendo plugins", "Plugins werden gelesen", "قراءة الإضافات"}}
        ,{"الفحص عبر خادم النقل", {"Scanning over FTP", "Analyse via FTP", "Búsqueda por FTP", "Suche über FTP", "الفحص عبر خادم النقل"}}
        ,{"جلب أيقونات الألعاب", {"Fetching game icons", "Récupération des icônes", "Obteniendo iconos", "Spielicons werden geladen", "جلب أيقونات الألعاب"}}
        ,{"فحص مجلدات الألعاب", {"Scanning game folders", "Analyse des dossiers de jeux", "Buscando carpetas de juegos", "Spielordner werden durchsucht", "فحص مجلدات الألعاب"}}
        ,{"أيقونات متوفرة:", {"Icons available:", "Icônes disponibles :", "Iconos disponibles:", "Verfügbare Icons:", "أيقونات متوفرة:"}}
        ,{"لم يتم العثور على ملف أيقونة لهذه اللعبة", {"No icon file found for this game", "Aucune icône trouvée pour ce jeu", "No se encontró un icono para este juego", "Kein Icon für dieses Spiel gefunden", "لم يتم العثور على ملف أيقونة لهذه اللعبة"}}
        ,{"تعذر فك ترميز أيقونة هذه اللعبة", {"Could not decode this game's icon", "Impossible de décoder l’icône", "No se pudo decodificar el icono", "Spielicon konnte nicht decodiert werden", "تعذر فك ترميز أيقونة هذه اللعبة"}}
        ,{"خادم النقل:", {"FTP:", "FTP :", "FTP:", "FTP:", "خادم النقل:"}}
        ,{"متصل", {"Connected", "Connecté", "Conectado", "Verbunden", "متصل"}}
        ,{"غير متصل", {"Disconnected", "Déconnecté", "Desconectado", "Getrennt", "غير متصل"}}
        ,{"اختر لعبة ثم عدّل الإضافات", {"Choose a game, then manage plugins", "Choisissez un jeu puis gérez ses plugins", "Elige un juego y gestiona sus plugins", "Spiel wählen und Plugins verwalten", "اختر لعبة ثم عدّل الإضافات"}}
        ,{"انتظر حتى تُقرأ الإضافات", {"Wait for plugins to load", "Attendez le chargement des plugins", "Espera a que se carguen los plugins", "Warte, bis Plugins geladen sind", "انتظر حتى تُقرأ الإضافات"}}
        ,{"تعديل إعداد كل الألعاب", {"Edit settings for all games", "Modifier les paramètres de tous les jeux", "Editar ajustes de todos los juegos", "Einstellungen für alle Spiele bearbeiten", "تعديل إعداد كل الألعاب"}}
        ,{"تعديل إضافات اللعبة المحددة", {"Edit plugins for this game", "Modifier les plugins de ce jeu", "Editar plugins de este juego", "Plugins für dieses Spiel bearbeiten", "تعديل إضافات اللعبة المحددة"}}
        ,{"احفظ التغييرات أولاً؛ إعادة الفحص ستلغي التعديلات", {"Save changes first; rescanning will discard them", "Enregistrez d’abord ; l’analyse annulera les modifications", "Guarda primero; volver a buscar descartará los cambios", "Speichere zuerst; erneutes Suchen verwirft Änderungen", "احفظ التغييرات أولاً؛ إعادة الفحص ستلغي التعديلات"}}
        ,{"الفحص جارٍ بالفعل", {"Scan already in progress", "Analyse déjà en cours", "La búsqueda ya está en curso", "Suche läuft bereits", "الفحص جارٍ بالفعل"}}
        ,{"بدأت إعادة الفحص", {"Rescan started", "Nouvelle analyse démarrée", "Búsqueda iniciada", "Erneute Suche gestartet", "بدأت إعادة الفحص"}}
        ,{"تعذر بدء الفحص", {"Could not start scan", "Impossible de démarrer l’analyse", "No se pudo iniciar la búsqueda", "Suche konnte nicht gestartet werden", "تعذر بدء الفحص"}}
        ,{"تم حفظ plugins.ini", {"plugins.ini saved", "plugins.ini enregistré", "plugins.ini guardado", "plugins.ini gespeichert", "تم حفظ plugins.ini"}}
        ,{"تعذر الحفظ؛ تحقق من صلاحيات مجلد GoldHEN", {"Save failed; check GoldHEN folder permissions", "Échec ; vérifiez les droits du dossier GoldHEN", "Error al guardar; revisa los permisos de GoldHEN", "Speichern fehlgeschlagen; GoldHEN-Berechtigungen prüfen", "تعذر الحفظ؛ تحقق من صلاحيات مجلد GoldHEN"}}
        ,{"تم التفعيل", {"Enabled", "Activé", "Activado", "Aktiviert", "تم التفعيل"}}
        ,{"تم التعطيل", {"Disabled", "Désactivé", "Desactivado", "Deaktiviert", "تم التعطيل"}}
        ,{"اضغط مثلث للحفظ", {"Press Triangle to save", "Appuyez sur Triangle pour enregistrer", "Pulsa Triángulo para guardar", "Dreieck zum Speichern drücken", "اضغط مثلث للحفظ"}}
        ,{"تم إلغاء الحذف", {"Deletion cancelled", "Suppression annulée", "Eliminación cancelada", "Löschen abgebrochen", "تم إلغاء الحذف"}}
        ,{"اختر لعبة أخرى", {"Choose another game", "Choisissez un autre jeu", "Elige otro juego", "Anderes Spiel wählen", "اختر لعبة أخرى"}}
        ,{"لا توجد إضافات للحذف", {"No plugins to delete", "Aucun plugin à supprimer", "No hay plugins que eliminar", "Keine Plugins zum Löschen", "لا توجد إضافات للحذف"}}
        ,{"تم حذف الإضافة", {"Plugin deleted", "Plugin supprimé", "Plugin eliminado", "Plugin gelöscht", "تم حذف الإضافة"}}
        ,{"تعذر حذف الإضافة", {"Could not delete plugin", "Impossible de supprimer le plugin", "No se pudo eliminar el plugin", "Plugin konnte nicht gelöscht werden", "تعذر حذف الإضافة"}}
        ,{"ألعاب:", {"Games:", "Jeux :", "Juegos:", "Spiele:", "ألعاب:"}}
        ,{"إضافات:", {"Plugins:", "Plugins :", "Plugins:", "Plugins:", "إضافات:"}}
        ,{"جاري فحص الألعاب", {"Scanning games", "Analyse des jeux", "Buscando juegos", "Spiele werden gesucht", "جاري فحص الألعاب"}}
        ,{"الأسهم", {"D-pad", "Croix directionnelle", "Cruceta", "Steuerkreuz", "الأسهم"}}
        ,{"الإعدادات محفوظة في /data/GoldHEN/plugins.ini", {"Settings are saved in /data/GoldHEN/plugins.ini", "Paramètres enregistrés dans /data/GoldHEN/plugins.ini", "Ajustes guardados en /data/GoldHEN/plugins.ini", "Einstellungen gespeichert unter /data/GoldHEN/plugins.ini", "الإعدادات محفوظة في /data/GoldHEN/plugins.ini"}}
        ,{"أيقونات جُلبت عبر النقل:", {"Icons fetched over FTP:", "Icônes récupérées via FTP :", "Iconos obtenidos por FTP:", "Über FTP geladene Icons:", "أيقونات جُلبت عبر النقل:"}}
        ,{"فشل الاتصال بالمنفذ 2121", {"Could not connect on port 2121", "Connexion impossible au port 2121", "No se pudo conectar al puerto 2121", "Verbindung über Port 2121 fehlgeschlagen", "فشل الاتصال بالمنفذ 2121"}}
        ,{"فعّل FTP من إعدادات GoldHEN", {"Enable the FTP server in GoldHEN", "Activez FTP dans les paramètres GoldHEN", "Activa FTP en los ajustes de GoldHEN", "Aktiviere FTP in den GoldHEN-Einstellungen", "فعّل FTP من إعدادات GoldHEN"}}
        ,{"عدد الألعاب:", {"Games found:", "Jeux trouvés :", "Juegos encontrados:", "Gefundene Spiele:", "عدد الألعاب:"}}
        ,{"استغرق الفحص وقتاً طويلاً؛ جاري إيقافه", {"Scan timed out; stopping", "Délai dépassé ; arrêt de l’analyse", "La búsqueda tardó demasiado; deteniendo", "Suche dauerte zu lange; wird beendet", "استغرق الفحص وقتاً طويلاً؛ جاري إيقافه"}}
        ,{"اكتمل الفحص", {"Scan complete", "Analyse terminée", "Búsqueda completada", "Suche abgeschlossen", "اكتمل الفحص"}}
        ,{"تم حذف الإضافة وتحديث plugins.ini", {"Plugin deleted and plugins.ini updated", "Plugin supprimé et plugins.ini mis à jour", "Plugin eliminado y plugins.ini actualizado", "Plugin gelöscht und plugins.ini aktualisiert", "تم حذف الإضافة وتحديث plugins.ini"}}
        ,{"حُذفت الإضافة؛ اضغط مثلث لحفظ plugins.ini", {"Plugin deleted; press Triangle to save plugins.ini", "Plugin supprimé ; Triangle pour enregistrer plugins.ini", "Plugin eliminado; pulsa Triángulo para guardar plugins.ini", "Plugin gelöscht; Dreieck speichert plugins.ini", "حُذفت الإضافة؛ اضغط مثلث لحفظ plugins.ini"}}
        ,{"حُذفت الإضافة — اضغط مثلث لحفظ التغييرات", {"Plugin deleted — press Triangle to save", "Plugin supprimé — Triangle pour enregistrer", "Plugin eliminado — pulsa Triángulo para guardar", "Plugin gelöscht — Dreieck zum Speichern drücken", "حُذفت الإضافة — اضغط مثلث لحفظ التغييرات"}}
        ,{"التغييرات لم تحفظ بعد؛ اضغط مثلث للحفظ", {"Changes are not saved; press Triangle to save", "Modifications non enregistrées ; Triangle pour sauvegarder", "Cambios sin guardar; pulsa Triángulo para guardar", "Änderungen nicht gespeichert; Dreieck drücken", "التغييرات لم تحفظ بعد؛ اضغط مثلث للحفظ"}}
        ,{" من ", {" of ", " sur ", " de ", " von ", " من "}}
        ,{" محاولة — ملفات وصلت:", {" attempts — files received:", " essais — fichiers reçus :", " intentos — archivos recibidos:", " Versuche — Dateien empfangen:", " محاولة — ملفات وصلت:"}}
        ,{"مجلدات مقروءة:", {"Readable folders:", "Dossiers lisibles :", "Carpetas legibles:", "Lesbare Ordner:", "مجلدات مقروءة:"}}
        ,{"عناصر تم فحصها:", {"Entries scanned:", "Éléments analysés :", "Elementos analizados:", "Durchsuchte Einträge:", "عناصر تم فحصها:"}}
        ,{"اكتمل الفحص — عدد الألعاب:", {"Scan complete — games found:", "Analyse terminée — jeux trouvés :", "Búsqueda completa — juegos encontrados:", "Suche abgeschlossen — Spiele gefunden:", "اكتمل الفحص — عدد الألعاب:"}}
        ,{"تم حفظ plugins.ini — أُنشئت نسخة احتياطية .bak", {"plugins.ini saved — backup .bak created", "plugins.ini enregistré — sauvegarde .bak créée", "plugins.ini guardado — copia .bak creada", "plugins.ini gespeichert — Sicherung .bak erstellt", "تم حفظ plugins.ini — أُنشئت نسخة احتياطية .bak"}}
        ,{"مجلدات ", {"Folders ", "Dossiers ", "Carpetas ", "Ordner ", "مجلدات "}}
        ,{"آخر رد من الخادم:", {"Last server reply:", "Dernière réponse du serveur :", "Última respuesta del servidor:", "Letzte Serverantwort:", "آخر رد من الخادم:"}}
        ,{"فشل حفظ الأيقونة في /data/PluginManager", {"Could not save icon to /data/PluginManager", "Impossible d’enregistrer l’icône dans /data/PluginManager", "No se pudo guardar el icono en /data/PluginManager", "Icon konnte nicht in /data/PluginManager gespeichert werden", "فشل حفظ الأيقونة في /data/PluginManager"}}
        ,{"تم حفظ plugins.ini", {"plugins.ini saved", "plugins.ini enregistré", "plugins.ini guardado", "plugins.ini gespeichert", "تم حفظ plugins.ini"}}
        ,{"أُنشئت نسخة احتياطية .bak", {"backup .bak created", "sauvegarde .bak créée", "copia .bak creada", "Sicherung .bak erstellt", "أُنشئت نسخة احتياطية .bak"}}
        ,{" — تم العثور على ", {" — found ", " — trouvés : ", " — encontrados: ", " — gefunden: ", " — تم العثور على "}}
        ,{"فشل استقبال رد الخادم", {"Failed to receive server response", "Échec de réception de la réponse du serveur", "No se recibió respuesta del servidor", "Serverantwort konnte nicht empfangen werden", "فشل استقبال رد الخادم"}}
        ,{"فشل تسجيل الدخول", {"FTP login failed", "Échec de connexion FTP", "Error de inicio de sesión FTP", "FTP-Anmeldung fehlgeschlagen", "فشل تسجيل الدخول"}}
        ,{"لم يُستخدم", {"Not used", "Non utilisé", "No usado", "Nicht verwendet", "لم يُستخدم"}}
        ,{"لم يُعثر على ألعاب", {"No games found", "Aucun jeu trouvé", "No se encontraron juegos", "Keine Spiele gefunden", "لم يُعثر على ألعاب"}}
        ,{"أيقونات متوفرة: ", {"Icons available: ", "Icônes disponibles : ", "Iconos disponibles: ", "Verfügbare Icons: ", "أيقونات متوفرة: "}}
        ,{"متجر الإضافات", {"Plugin Store", "Boutique de plugins", "Tienda de plugins", "Plugin-Shop", "متجر الإضافات"}}
        ,{"المتجر", {"Store", "Boutique", "Tienda", "Shop", "المتجر"}}
        ,{"الوصف", {"Description", "Description", "Descripción", "Beschreibung", "الوصف"}}
        ,{"المطوّر", {"Author", "Auteur", "Autor", "Autor", "المطوّر"}}
        ,{"الدعم", {"Compatibility", "Compatibilité", "Compatibilidad", "Kompatibilität", "الدعم"}}
        ,{"الإصدار", {"Version", "Version", "Versión", "Version", "الإصدار"}}
        ,{"ابدأ التصفح بعد تحميل القائمة", {"Load the catalog to browse plugins", "Chargez le catalogue pour parcourir les plugins", "Carga el catálogo para ver plugins", "Katalog laden, um Plugins zu sehen", "ابدأ التصفح بعد تحميل القائمة"}}
        ,{"لا توجد إضافات في المتجر", {"No plugins in the store yet", "Aucun plugin dans la boutique", "Aún no hay plugins", "Noch keine Plugins im Shop", "لا توجد إضافات في المتجر"}}
        ,{"تحميل", {"Download", "Télécharger", "Descargar", "Herunterladen", "تحميل"}}
        ,{"تحديث القائمة", {"Refresh catalog", "Actualiser", "Actualizar", "Aktualisieren", "تحديث القائمة"}}
        ,{"إلغاء التحميل", {"Cancel download", "Annuler", "Cancelar descarga", "Download abbrechen", "إلغاء التحميل"}}
        ,{"جارٍ تحميل القائمة", {"Loading catalog", "Chargement du catalogue", "Cargando catálogo", "Katalog wird geladen", "جارٍ تحميل القائمة"}}
        ,{"اكتملت القائمة", {"Catalog ready", "Catalogue prêt", "Catálogo listo", "Katalog bereit", "اكتملت القائمة"}}
        ,{"القائمة المحفوظة دون اتصال", {"Offline cache", "Copie hors ligne", "Caché sin conexión", "Offline-Kopie", "القائمة المحفوظة دون اتصال"}}
        ,{"جارٍ تنزيل الإضافة", {"Downloading plugin", "Téléchargement du plugin", "Descargando plugin", "Plugin wird geladen", "جارٍ تنزيل الإضافة"}}
        ,{"جارٍ التحقق من سلامة الملف", {"Verifying file integrity", "Vérification du fichier", "Verificando el archivo", "Datei wird geprüft", "جارٍ التحقق من سلامة الملف"}}
        ,{"تم التثبيت", {"Installed", "Installé", "Instalado", "Installiert", "تم التثبيت"}}
        ,{"تعذر تحميل القائمة", {"Could not load catalog", "Impossible de charger le catalogue", "No se pudo cargar el catálogo", "Katalog konnte nicht geladen werden", "تعذر تحميل القائمة"}}
        ,{"فشل التحقق من SHA-256", {"SHA-256 verification failed", "Échec de vérification SHA-256", "Falló la verificación SHA-256", "SHA-256-Prüfung fehlgeschlagen", "فشل التحقق من SHA-256"}}
        ,{"تم الإلغاء", {"Canceled", "Annulé", "Cancelado", "Abgebrochen", "تم الإلغاء"}}
    };
    static const std::vector<std::string> keys = []() {
        std::vector<std::string> k;
        for (const auto& item : words) k.push_back(item.first);
        std::sort(k.begin(), k.end(), [](const std::string& a, const std::string& b) { return a.size() > b.size(); });
        return k;
    }();
    std::string out = s;
    for (const auto& key : keys) {
        const auto& entry = words.at(key);
        size_t at = 0;
        while ((at = out.find(key, at)) != std::string::npos) {
            const std::string rep = ltr(entry, lang);
            out.replace(at, key.size(), rep);
            at += rep.size();
        }
    }
    if (translated[lang].size() < 512) translated[lang].emplace(s, out);
    return out;
}
static void loadLanguagePreference() {
    mkdir(APP_DATA_DIR, 0777);
    std::string path = std::string(APP_DATA_DIR) + "/language.ini";
    FILE* f = std::fopen(path.c_str(), "r");
    int lang = 0;
    if (f) {
        char line[64] = {};
        if (!std::fgets(line, sizeof(line), f) || std::sscanf(line, "language=%d", &lang) != 1) {
            // يقبل أيضاً ملفاً رقمياً قديماً لتسهيل الترقية.
            if (std::sscanf(line, "%d", &lang) != 1) lang = 0;
        }
        std::fclose(f);
    }
    gLanguage = lang >= 0 && lang < 6 ? lang : 0;
}
static bool saveLanguagePreference(int lang) {
    if (lang < 0 || lang >= 5 || (mkdir(APP_DATA_DIR, 0777) != 0 && errno != EEXIST)) return false;
    std::string path = std::string(APP_DATA_DIR) + "/language.ini";
    std::string temp = path + ".tmp";
    // حفظ ذري قدر الإمكان لتفادي ملف إعدادات جزئي عند انقطاع التطبيق.
    FILE* f = std::fopen(temp.c_str(), "w");
    if (!f) return false;
    bool ok = std::fprintf(f, "language=%d\n", lang) > 0;
    if (std::fclose(f) != 0) ok = false;
    if (!ok || rename(temp.c_str(), path.c_str()) != 0) { unlink(temp.c_str()); return false; }
    return true;
}
