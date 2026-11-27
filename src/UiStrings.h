#pragma once
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
static const LocalizedText kClose = {"Close", "Fermer", "Cerrar", "Schließen", "إغلاق"};
static const LocalizedText kAboutTheApp = {"About the app", "À propos", "Acerca de", "Über die App", "حول البرنامج"};
static const LocalizedText kNoInternetConnection = {"No internet connection", "Pas de connexion Internet", "Sin conexión a Internet", "Keine Internetverbindung", "لا يوجد اتصال بالانترنت"};
static const LocalizedText kCheckYourConnectionAndTry = {"Check your connection and try again", "Vérifiez votre connexion et réessayez", "Revisa tu conexión e inténtalo de nuevo", "Prüfe deine Verbindung und versuche es erneut", "افحص اتصالك وحاول مرة أخرى"};
static const LocalizedText kPressXToRetryO = {"Press X to retry   -   O to go back", "X pour réessayer   -   O pour revenir", "X para reintentar   -   O para volver", "X = erneut versuchen   -   O = zurück", "اضغط X لإعادة المحاولة   -   O للرجوع"};
static const LocalizedText kNoPluginWithThisName = {"No plugin with this name", "Aucun plugin portant ce nom", "Ningún plugin con ese nombre", "Kein Plugin mit diesem Namen", "لا توجد إضافة بهذا الاسم"};
static const LocalizedText kOtherVersion = {"Other version", "Autre version", "Otra versión", "Andere Version", "\u0646\u0633\u062e\u0629 \u0645\u062e\u062a\u0644\u0641\u0629"};
static const LocalizedText kInstalled = {"Installed", "Installé", "Instalado", "Installiert", "\u0645\u062b\u0628\u0651\u062a"};
static const LocalizedText kFetchingPluginInfo = {"Fetching plugin info...", "Récupération des infos...", "Obteniendo información...", "Informationen werden geladen...", "جاري جلب معلومات الإضافة..."};
static const LocalizedText kLoadingSupportQr = {"Loading support QR", "Chargement du QR", "Cargando QR", "QR wird geladen", "\u062c\u0627\u0631\u0020\u062a\u062d\u0645\u064a\u0644\u0020\u0631\u0645\u0632\u0020QR"};
static const LocalizedText kQrImageUnavailable = {"QR image unavailable", "QR indisponible", "QR no disponible", "QR nicht verfuegbar", "\u0635\u0648\u0631\u0629\u0020QR\u0020\u063a\u064a\u0631\u0020\u0645\u062a\u0627\u062d\u0629"};
static const LocalizedText kDownloadingPkg = {"Downloading PKG...", "Téléchargement du PKG...", "Descargando PKG...", "PKG wird heruntergeladen...", "جاري تحميل الملف"};
static const LocalizedText kPreparing = {"Preparing...", "Préparation...", "Preparando...", "Vorbereitung...", "جاري التحضير..."};
static const LocalizedText kInstallingApp = {"Installing app...", "Installation de l'application...", "Instalando la aplicación...", "App wird installiert...", "جاري تثبيت التطبيق"};
static const LocalizedText kAppIsAlreadyInstalled = {"App is already installed", "L'application est déjà installée", "La aplicación ya está instalada", "Die App ist bereits installiert", "التطبيق موجود بالفعل"};
static const LocalizedText kDownload = {"Download", "Télécharger", "Descargar", "Herunterladen", "تحميل الإضافة"};
static const LocalizedText kInstall = {"Install", "Installer", "Instalar", "Installieren", "تثبيت التطبيق"};
static const LocalizedText kEditToken = {"Edit token", "Modifier le jeton", "Editar token", "Token bearbeiten", "تعديل التوكن"};
static const LocalizedText kEnableTheFtpServerIn = {"Enable the FTP server in GoldHEN", "Activez le serveur FTP dans GoldHEN", "Activa el servidor FTP en GoldHEN", "Aktiviere den FTP-Server in GoldHEN", "\u0641\u0639\u0644 \u062e\u0627\u062f\u0645 FTP \u0641\u064a GoldHEN"};
static const LocalizedText kTransferServerIsNotConnected = {"Transfer server is not connected", "Serveur de transfert non connecte", "Servidor de transferencia no conectado", "Uebertragungsserver nicht verbunden", "\u062e\u0627\u062f\u0645 \u0627\u0644\u0646\u0642\u0644 \u063a\u064a\u0631 \u0645\u062a\u0635\u0644"};
static const LocalizedText kOpenStore = {"Open Store", "Ouvrir", "Abrir", "Öffnen", "افتح المتجر"};
static const LocalizedText kBrowseAndDownloadPluginsDirectly = {"Browse and download plugins directly from the Store.", "Parcourez et telechargez des extensions depuis la boutique.", "Explora y descarga complementos desde la tienda.", "Durchsuche und lade Erweiterungen im Store herunter.", "تصفح وتنزيل الاضافات من المتجر."};
static const LocalizedText kTheFileAlreadyExists = {"The file already exists", "Le fichier existe déjà", "El archivo ya existe", "Die Datei ist bereits vorhanden", "الملف موجود مسبقاً"};
static const LocalizedText kDownloadThisVersionTheOld = {"Download this version? The old one will be deleted", "Télécharger cette version ? L'ancienne sera supprimée", "¿Descargar esta versión? Se eliminará la anterior", "Diese Version laden? Die alte wird gelöscht", "هل تريد تحميل هذه النسخة؟ سيتم حذف القديمة"};
static const LocalizedText kAlreadyOnTheConsole = {"Already on the console: ", "Déjà sur la console : ", "Ya está en la consola: ", "Bereits auf der Konsole: ", "موجود على الجهاز: "};
static const LocalizedText kReplaceAllDeletesTheOld = {"Replace all: deletes the old .prx and its ZIP files, installs the new ones", "Tout remplacer : supprime l'ancien .prx et ses fichiers ZIP, installe les nouveaux", "Reemplazar todo: borra el .prx y los archivos ZIP anteriores e instala los nuevos", "Alles ersetzen: löscht die alte .prx und ZIP-Dateien und installiert die neuen", "استبدال الكل: يحذف ملف prx القديم وملفات الـ ZIP ويثبّت الجديد"};
static const LocalizedText kPrxOnlyReplacesJustThe = {".prx only: replaces just the plugin and keeps your existing ZIP files", ".prx seul : remplace seulement le plugin et garde vos fichiers ZIP", "Solo .prx: reemplaza solo el plugin y conserva tus archivos ZIP", "Nur .prx: ersetzt nur das Plugin und behält deine ZIP-Dateien", "prx فقط: يستبدل البلاجن فقط ويبقي ملفات الـ ZIP الموجودة"};
static const LocalizedText kReplaceAll = {"Replace all", "Tout remplacer", "Reemplazar todo", "Alles ersetzen", "استبدال الكل"};
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
static const LocalizedText kBrowsePluginsIni = {"Browse plugins.ini", "Parcourir plugins.ini", "Ver plugins.ini", "plugins.ini anzeigen", "تصفح plugins.ini"};
static const LocalizedText kResetPluginsIni2 = {"Reset plugins.ini", "Réinitialiser plugins.ini", "Restablecer plugins.ini", "plugins.ini zurücksetzen", "إعادة ضبط plugins.ini"};
static const LocalizedText kRestoreBackup = {"Restore backup", "Restaurer la sauvegarde", "Restaurar copia", "Sicherung wiederherstellen", "استعادة النسخة الاحتياطية"};
static const LocalizedText kIssues = {"issues", "problèmes", "problemas", "Probleme", "مشاكل"};
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
static const LocalizedText kSelect = {"Select", "Choisir", "Seleccionar", "Auswaehlen", "\u0627\u062e\u062a\u064a\u0627\u0631"};
static const LocalizedText kStore = {"Store", "Boutique", "Tienda", "Shop", "\u0627\u0644\u0645\u062a\u062c\u0631"};
static const LocalizedText kRescan = {"Rescan", "Relancer", "Volver a buscar", "Erneut suchen", "\u0625\u0639\u0627\u062f\u0629 \u0627\u0644\u0641\u062d\u0635"};
static const LocalizedText kCloseSettings = {"Close settings", "Fermer les paramètres", "Cerrar ajustes", "Einstellungen schließen", "إغلاق الإعدادات"};
