#include "AppSettings.h"

#include "I18n.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QSettings>
#include <QStringList>

namespace
{
QString uiText(const char* english, const char* korean)
{
    return I18n::translate("AppSettings", english, korean);
}
constexpr int DefaultWindowWidth = 540;
constexpr int DefaultWindowHeight = 360;
constexpr int DefaultWindowPosition = 0;
constexpr int DefaultSearchResultLimit = 8;
constexpr int DefaultReopenRetentionSeconds = 10;
constexpr int DefaultAutoSyncIntervalMinutes = 5;
constexpr int DefaultQuickFontSize = 13;
constexpr double DefaultWindowOpacity = 0.96;
const char* DefaultTheme = "dark";

const char* ShowHotkeyKey = "hotkeys/showQuick";
const char* WindowWidthKey = "window/width";
const char* WindowHeightKey = "window/height";
const char* SearchResultLimitKey = "search/resultLimit";
const char* ShowShortcutHintsKey = "ui/showShortcutHints";
const char* ShortcutPresetKey = "shortcuts/preset";
const char* ShowResultIconsKey = "ui/showResultIcons";
const char* FetchFaviconsFromWebKey = "ui/fetchFaviconsFromWeb";
const char* ShowResultUsernameKey = "ui/showResultUsername";
const char* ShowResultUriKey = "ui/showResultUri";
const char* ShowResultTypeKey = "ui/showResultType";
const char* QuickFontFamilyKey = "ui/quickFontFamily";
const char* QuickFontSizeKey = "ui/quickFontSize";
const char* QuickFontBoldKey = "ui/quickFontBold";
const char* CustomWindowPositionEnabledKey = "window/customPositionEnabled";
const char* WindowPositionXKey = "window/positionX";
const char* WindowPositionYKey = "window/positionY";
const char* WindowOpacityKey = "window/opacity";
const char* RoundedCornersKey = "window/roundedCorners";
const char* ThemeKey = "ui/theme";
const char* ReopenRetentionSecondsKey = "window/reopenRetentionSeconds";
const char* AutoSyncIntervalMinutesKey = "bitwarden/autoSyncIntervalMinutes";
const char* CloseOnFocusLossKey = "window/closeOnFocusLoss";
const char* CredentialSessionStorageEnabledKey = "security/credentialSessionStorageEnabled";
const char* StartOnLoginKey = "system/startOnLogin";
const char* AutoCheckUpdatesEnabledKey = "updates/autoCheckEnabled";
const char* LastUpdateCheckEpochSecondsKey = "updates/lastCheckEpochSeconds";
const char* StartOnLoginName = "peekpass";
const char* OlderStartOnLoginName = "PeekWarden";
const char* WindowsRunKey = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const char* PreviousSettingsMigratedKey = "system/migratedFromPreviousName";
const char* PasswordManagerTypeKey = "passwordManager/type";
const char* BwProgramOverrideKey = "bitwarden/bwProgram";
const char* ProtonPassProgramOverrideKey = "protonPass/passCliProgram";
const char* BitwardenServerModeKey = "bitwarden/serverMode";
const char* CustomBitwardenServerUrlKey = "bitwarden/customServerUrl";
const char* LanguageKey = "ui/language";

QKeySequence defaultShowHotkey()
{
    return QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Space);
}

QString quotedApplicationPath()
{
    return QString("\"%1\"").arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath()));
}

QString startupExecutableFromCommand(QString command)
{
    command = command.trimmed();
    if (command.startsWith(QLatin1Char('"'))) {
        const int closingQuote = command.indexOf(QLatin1Char('"'), 1);
        if (closingQuote > 1)
            return QDir::fromNativeSeparators(command.mid(1, closingQuote - 1));
    }

    const int firstSpace = command.indexOf(QLatin1Char(' '));
    return QDir::fromNativeSeparators(firstSpace > 0 ? command.left(firstSpace) : command);
}

QString startupRuntimeError(const QString& executablePath)
{
#ifdef Q_OS_WIN
    const QFileInfo executable(executablePath);
    if (!executable.exists() || !executable.isFile()) {
        return uiText("The startup executable does not exist: %1",
                      "자동 시작 실행 파일이 존재하지 않습니다: %1")
            .arg(QDir::toNativeSeparators(executablePath));
    }

    const QDir directory = executable.dir();
    QStringList missingFiles;
    const auto requireOneOf = [&directory, &missingFiles](const QString& displayName, const QStringList& candidates) {
        for (const QString& candidate : candidates) {
            if (QFileInfo::exists(directory.filePath(candidate)))
                return;
        }
        missingFiles.push_back(displayName);
    };

    requireOneOf(QStringLiteral("Qt6Core.dll"), {QStringLiteral("Qt6Core.dll"), QStringLiteral("Qt6Cored.dll")});
    requireOneOf(QStringLiteral("Qt6Gui.dll"), {QStringLiteral("Qt6Gui.dll"), QStringLiteral("Qt6Guid.dll")});
    requireOneOf(QStringLiteral("Qt6Network.dll"), {QStringLiteral("Qt6Network.dll"), QStringLiteral("Qt6Networkd.dll")});
    requireOneOf(QStringLiteral("Qt6Widgets.dll"), {QStringLiteral("Qt6Widgets.dll"), QStringLiteral("Qt6Widgetsd.dll")});
    requireOneOf(QStringLiteral("platforms\\qwindows.dll"),
        {QStringLiteral("platforms/qwindows.dll"), QStringLiteral("platforms/qwindowsd.dll")});

#ifdef __MINGW32__
    requireOneOf(QStringLiteral("libgcc_s_seh-1.dll"), {QStringLiteral("libgcc_s_seh-1.dll")});
    requireOneOf(QStringLiteral("libstdc++-6.dll"), {QStringLiteral("libstdc++-6.dll")});
    requireOneOf(QStringLiteral("libwinpthread-1.dll"), {QStringLiteral("libwinpthread-1.dll")});
#endif

    if (!missingFiles.isEmpty()) {
        return uiText("Required runtime files are missing beside the application: %1. Use the packaged release created with windeployqt.",
                      "실행 파일 옆에 필요한 런타임 파일이 없습니다: %1. windeployqt로 패키징된 릴리스를 사용하세요.")
            .arg(missingFiles.join(QStringLiteral(", ")));
    }
#else
    Q_UNUSED(executablePath)
#endif

    return {};
}

void removeStartupEntryValues(QSettings& runKey)
{
    runKey.remove(StartOnLoginName);
    runKey.remove(OlderStartOnLoginName);
}

bool hasStartupEntry(const QSettings& runKey)
{
    return runKey.contains(StartOnLoginName)
        || runKey.contains(OlderStartOnLoginName);
}

QString startupEntryValue(const QSettings& runKey)
{
    // Windows registry value names are case-insensitive, so an existing
    // "PeekPass" entry is already found through StartOnLoginName ("peekpass").
    for (const char* name : {StartOnLoginName, OlderStartOnLoginName}) {
        if (runKey.contains(name))
            return runKey.value(name).toString();
    }
    return {};
}

QString settingsError(QSettings::Status status)
{
    switch (status) {
    case QSettings::AccessError:
        return uiText("Windows denied access to the current user's startup registry key.",
                      "Windows에서 현재 사용자의 자동 시작 레지스트리 키 접근을 거부했습니다.");
    case QSettings::FormatError:
        return uiText("Windows returned an invalid startup registry value.",
                      "Windows에서 잘못된 자동 시작 레지스트리 값을 반환했습니다.");
    case QSettings::NoError:
        break;
    }
    return uiText("The Windows startup registry entry could not be updated.",
                  "Windows 자동 시작 레지스트리 항목을 업데이트하지 못했습니다.");
}

void setError(QString* target, const QString& message)
{
    if (target)
        *target = message;
}

void removeStartupEntries()
{
#ifdef Q_OS_WIN
    QSettings runKey(QString::fromLatin1(WindowsRunKey), QSettings::NativeFormat);
    removeStartupEntryValues(runKey);
    runKey.sync();
#endif
}

void clearSettingsForApplication(const QString& applicationName)
{
    QSettings settings(
        QSettings::NativeFormat,
        QSettings::UserScope,
        applicationName,
        applicationName);
    settings.clear();
    settings.sync();
}
}

void AppSettings::migratePreviousSettings()
{
    QSettings current;
    if (current.value(PreviousSettingsMigratedKey, false).toBool())
        return;

    const auto copyMissingValues = [&current](const QString& applicationName) {
        QSettings previous(
            QSettings::NativeFormat,
            QSettings::UserScope,
            applicationName,
            applicationName);
        for (const QString& key : previous.allKeys()) {
            if (!current.contains(key))
                current.setValue(key, previous.value(key));
        }
    };

    copyMissingValues(QStringLiteral("PeekPass"));
    copyMissingValues(QStringLiteral("PeekWarden"));

    current.setValue(PreviousSettingsMigratedKey, true);
    current.sync();
}

void AppSettings::resetUserData()
{
    removeStartupEntries();
    clearSettingsForApplication(QStringLiteral("PeekPass"));
    clearSettingsForApplication(QStringLiteral("PeekWarden"));

    QSettings current;
    current.clear();
    // Prevent cleared previous-name settings from being imported again on restart.
    current.setValue(PreviousSettingsMigratedKey, true);
    current.sync();
}

QKeySequence AppSettings::showHotkey()
{
    const QString value = QSettings().value(ShowHotkeyKey, defaultHotkeyText()).toString();
    const QKeySequence sequence(value, QKeySequence::PortableText);

    if (!sequence.isEmpty())
        return sequence;

    return defaultShowHotkey();
}

void AppSettings::setShowHotkey(const QKeySequence& sequence)
{
    QSettings().setValue(ShowHotkeyKey, sequence.toString(QKeySequence::PortableText));
}

QString AppSettings::showHotkeyText()
{
    return showHotkey().toString(QKeySequence::NativeText);
}

int AppSettings::windowWidth()
{
    return QSettings().value(WindowWidthKey, DefaultWindowWidth).toInt();
}

void AppSettings::setWindowWidth(int width)
{
    QSettings().setValue(WindowWidthKey, width);
}

int AppSettings::windowHeight()
{
    return QSettings().value(WindowHeightKey, DefaultWindowHeight).toInt();
}

void AppSettings::setWindowHeight(int height)
{
    QSettings().setValue(WindowHeightKey, height);
}

int AppSettings::searchResultLimit()
{
    return qBound(1, QSettings().value(SearchResultLimitKey, DefaultSearchResultLimit).toInt(), 20);
}

void AppSettings::setSearchResultLimit(int limit)
{
    QSettings().setValue(SearchResultLimitKey, qBound(1, limit, 20));
}

bool AppSettings::showShortcutHints()
{
    return QSettings().value(ShowShortcutHintsKey, true).toBool();
}

void AppSettings::setShowShortcutHints(bool enabled)
{
    QSettings().setValue(ShowShortcutHintsKey, enabled);
}

QString AppSettings::shortcutPreset()
{
    const QString value = QSettings().value(ShortcutPresetKey, "1password").toString().trimmed().toLower();
    if (value == QStringLiteral("bitwarden"))
        return QStringLiteral("bitwarden");

    return QStringLiteral("1password");
}

void AppSettings::setShortcutPreset(const QString& preset)
{
    const QString normalized = preset.trimmed().toLower();
    QSettings().setValue(ShortcutPresetKey,
        normalized == QStringLiteral("bitwarden") ? QStringLiteral("bitwarden") : QStringLiteral("1password"));
}

bool AppSettings::showResultIcons()
{
    return QSettings().value(ShowResultIconsKey, true).toBool();
}

void AppSettings::setShowResultIcons(bool enabled)
{
    QSettings().setValue(ShowResultIconsKey, enabled);
}

bool AppSettings::fetchFaviconsFromWeb()
{
    return QSettings().value(FetchFaviconsFromWebKey, true).toBool();
}

void AppSettings::setFetchFaviconsFromWeb(bool enabled)
{
    QSettings().setValue(FetchFaviconsFromWebKey, enabled);
}

bool AppSettings::showResultUsername()
{
    return QSettings().value(ShowResultUsernameKey, true).toBool();
}

void AppSettings::setShowResultUsername(bool enabled)
{
    QSettings().setValue(ShowResultUsernameKey, enabled);
}

bool AppSettings::showResultUri()
{
    return QSettings().value(ShowResultUriKey, false).toBool();
}

void AppSettings::setShowResultUri(bool enabled)
{
    QSettings().setValue(ShowResultUriKey, enabled);
}

bool AppSettings::showResultType()
{
    return QSettings().value(ShowResultTypeKey, false).toBool();
}

void AppSettings::setShowResultType(bool enabled)
{
    QSettings().setValue(ShowResultTypeKey, enabled);
}

QString AppSettings::quickFontFamily()
{
    const QString family = QSettings().value(QuickFontFamilyKey).toString().trimmed();
    if (!family.isEmpty())
        return family;

    return QFontDatabase::systemFont(QFontDatabase::GeneralFont).family();
}

void AppSettings::setQuickFontFamily(const QString& family)
{
    QSettings().setValue(QuickFontFamilyKey, family.trimmed());
}

int AppSettings::quickFontSize()
{
    return qBound(9, QSettings().value(QuickFontSizeKey, DefaultQuickFontSize).toInt(), 22);
}

void AppSettings::setQuickFontSize(int size)
{
    QSettings().setValue(QuickFontSizeKey, qBound(9, size, 22));
}

bool AppSettings::quickFontBold()
{
    return QSettings().value(QuickFontBoldKey, true).toBool();
}

void AppSettings::setQuickFontBold(bool enabled)
{
    QSettings().setValue(QuickFontBoldKey, enabled);
}

bool AppSettings::customWindowPositionEnabled()
{
    return QSettings().value(CustomWindowPositionEnabledKey, false).toBool();
}

void AppSettings::setCustomWindowPositionEnabled(bool enabled)
{
    QSettings().setValue(CustomWindowPositionEnabledKey, enabled);
}

int AppSettings::windowPositionX()
{
    return qBound(-20000, QSettings().value(WindowPositionXKey, DefaultWindowPosition).toInt(), 20000);
}

void AppSettings::setWindowPositionX(int x)
{
    QSettings().setValue(WindowPositionXKey, qBound(-20000, x, 20000));
}

int AppSettings::windowPositionY()
{
    return qBound(-20000, QSettings().value(WindowPositionYKey, DefaultWindowPosition).toInt(), 20000);
}

void AppSettings::setWindowPositionY(int y)
{
    QSettings().setValue(WindowPositionYKey, qBound(-20000, y, 20000));
}

double AppSettings::windowOpacity()
{
    return QSettings().value(WindowOpacityKey, DefaultWindowOpacity).toDouble();
}

void AppSettings::setWindowOpacity(double opacity)
{
    QSettings().setValue(WindowOpacityKey, opacity);
}

bool AppSettings::roundedCorners()
{
    return QSettings().value(RoundedCornersKey, true).toBool();
}

void AppSettings::setRoundedCorners(bool enabled)
{
    QSettings().setValue(RoundedCornersKey, enabled);
}

QString AppSettings::theme()
{
    const QString value = QSettings().value(ThemeKey, DefaultTheme).toString().trimmed().toLower();
    return value == QStringLiteral("light") ? QStringLiteral("light") : QStringLiteral("dark");
}

void AppSettings::setTheme(const QString& theme)
{
    const QString value = theme.trimmed().toLower();
    QSettings().setValue(ThemeKey, value == QStringLiteral("light") ? QStringLiteral("light") : QStringLiteral("dark"));
}

int AppSettings::reopenRetentionSeconds()
{
    return qBound(0, QSettings().value(ReopenRetentionSecondsKey, DefaultReopenRetentionSeconds).toInt(), 120);
}

void AppSettings::setReopenRetentionSeconds(int seconds)
{
    QSettings().setValue(ReopenRetentionSecondsKey, qBound(0, seconds, 120));
}

int AppSettings::autoSyncIntervalMinutes()
{
    return qBound(0, QSettings().value(AutoSyncIntervalMinutesKey, DefaultAutoSyncIntervalMinutes).toInt(), 1440);
}

void AppSettings::setAutoSyncIntervalMinutes(int minutes)
{
    QSettings().setValue(AutoSyncIntervalMinutesKey, qBound(0, minutes, 1440));
}

bool AppSettings::closeOnFocusLoss()
{
    return QSettings().value(CloseOnFocusLossKey, true).toBool();
}

void AppSettings::setCloseOnFocusLoss(bool enabled)
{
    QSettings().setValue(CloseOnFocusLossKey, enabled);
}

bool AppSettings::credentialSessionStorageEnabled()
{
    return QSettings().value(CredentialSessionStorageEnabledKey, true).toBool();
}

void AppSettings::setCredentialSessionStorageEnabled(bool enabled)
{
    QSettings().setValue(CredentialSessionStorageEnabledKey, enabled);
}

bool AppSettings::startOnLoginEnabled()
{
#ifdef Q_OS_WIN
    QSettings runKey(QString::fromLatin1(WindowsRunKey), QSettings::NativeFormat);
    return hasStartupEntry(runKey);
#else
    return QSettings().value(StartOnLoginKey, false).toBool();
#endif
}

bool AppSettings::setStartOnLoginEnabled(bool enabled, QString* errorMessage)
{
#ifdef Q_OS_WIN
    if (enabled) {
        const QString runtimeError = startupRuntimeError(QCoreApplication::applicationFilePath());
        if (!runtimeError.isEmpty()) {
            setError(errorMessage, runtimeError);
            return false;
        }
    }

    QSettings runKey(QString::fromLatin1(WindowsRunKey), QSettings::NativeFormat);
    if (enabled) {
        runKey.setValue(StartOnLoginName, quotedApplicationPath());
        runKey.remove(OlderStartOnLoginName);
    } else {
        removeStartupEntryValues(runKey);
    }
    runKey.sync();

    if (runKey.status() != QSettings::NoError) {
        setError(errorMessage, settingsError(runKey.status()));
        return false;
    }

    const bool verified = enabled
        ? runKey.value(StartOnLoginName).toString() == quotedApplicationPath()
        : !hasStartupEntry(runKey);
    if (!verified) {
        setError(errorMessage, uiText("Windows did not retain the requested startup registry entry.",
                                      "Windows에 요청한 자동 시작 레지스트리 항목이 유지되지 않았습니다."));
        return false;
    }
#endif

    QSettings settings;
    settings.setValue(StartOnLoginKey, enabled);
    settings.sync();
    setError(errorMessage, {});
    return true;
}

bool AppSettings::applyStartupPreference(QString* errorMessage)
{
#ifdef Q_OS_WIN
    QSettings runKey(QString::fromLatin1(WindowsRunKey), QSettings::NativeFormat);
    if (!hasStartupEntry(runKey)) {
        setError(errorMessage, {});
        return true;
    }

    const QString existingCommand = startupEntryValue(runKey);
    const QString existingExecutable = startupExecutableFromCommand(existingCommand);
    const QString existingRuntimeError = startupRuntimeError(existingExecutable);
    if (existingRuntimeError.isEmpty()) {
        runKey.setValue(StartOnLoginName, existingCommand);
        runKey.remove(OlderStartOnLoginName);
        runKey.sync();
        if (runKey.status() == QSettings::NoError
            && runKey.value(StartOnLoginName).toString() == existingCommand) {
            setError(errorMessage, {});
            return true;
        }

        setError(errorMessage, settingsError(runKey.status()));
        return false;
    }

    const QString currentRuntimeError = startupRuntimeError(QCoreApplication::applicationFilePath());
    if (currentRuntimeError.isEmpty())
        return setStartOnLoginEnabled(true, errorMessage);

    setError(errorMessage,
        uiText("The registered startup entry is not runnable. %1",
               "등록된 자동 시작 항목을 실행할 수 없습니다. %1")
            .arg(existingRuntimeError));
    return false;
#else
    setError(errorMessage, {});
    return true;
#endif
}

bool AppSettings::autoCheckUpdatesEnabled()
{
    return QSettings().value(AutoCheckUpdatesEnabledKey, true).toBool();
}

void AppSettings::setAutoCheckUpdatesEnabled(bool enabled)
{
    QSettings().setValue(AutoCheckUpdatesEnabledKey, enabled);
}

qint64 AppSettings::lastUpdateCheckEpochSeconds()
{
    return QSettings().value(LastUpdateCheckEpochSecondsKey, 0).toLongLong();
}

void AppSettings::setLastUpdateCheckEpochSeconds(qint64 epochSeconds)
{
    QSettings().setValue(LastUpdateCheckEpochSecondsKey, qMax<qint64>(0, epochSeconds));
}

QString AppSettings::passwordManagerType()
{
    const QString type = QSettings().value(PasswordManagerTypeKey, "bitwarden").toString().trimmed().toLower();
    return type == QStringLiteral("protonpass") ? QStringLiteral("protonpass") : QStringLiteral("bitwarden");
}

void AppSettings::setPasswordManagerType(const QString& type)
{
    const QString normalized = type.trimmed().toLower();
    QSettings().setValue(PasswordManagerTypeKey,
        normalized == QStringLiteral("protonpass") ? QStringLiteral("protonpass") : QStringLiteral("bitwarden"));
}

QString AppSettings::bwProgramOverride()
{
    return QSettings().value(BwProgramOverrideKey).toString().trimmed();
}

void AppSettings::setBwProgramOverride(const QString& path)
{
    QSettings().setValue(BwProgramOverrideKey, path.trimmed());
}

QString AppSettings::protonPassProgramOverride()
{
    return QSettings().value(ProtonPassProgramOverrideKey).toString().trimmed();
}

void AppSettings::setProtonPassProgramOverride(const QString& path)
{
    QSettings().setValue(ProtonPassProgramOverrideKey, path.trimmed());
}

QString AppSettings::bitwardenServerMode()
{
    const QString value = QSettings().value(BitwardenServerModeKey, "com").toString().trimmed().toLower();

    if (value == "eu" || value == "custom")
        return value;

    return "com";
}

void AppSettings::setBitwardenServerMode(const QString& mode)
{
    const QString normalized = mode.trimmed().toLower();
    if (normalized == "eu" || normalized == "custom") {
        QSettings().setValue(BitwardenServerModeKey, normalized);
        return;
    }

    QSettings().setValue(BitwardenServerModeKey, "com");
}

QString AppSettings::customBitwardenServerUrl()
{
    return QSettings().value(CustomBitwardenServerUrlKey).toString().trimmed();
}

void AppSettings::setCustomBitwardenServerUrl(const QString& url)
{
    QSettings().setValue(CustomBitwardenServerUrlKey, url.trimmed());
}

QString AppSettings::language()
{
    const QString value = QSettings().value(LanguageKey, "ko").toString().trimmed().toLower();
    return value.isEmpty() ? QStringLiteral("ko") : value;
}

void AppSettings::setLanguage(const QString& language)
{
    const QString normalized = language.trimmed().toLower();
    QSettings().setValue(LanguageKey, normalized.isEmpty() ? QStringLiteral("en") : normalized);
}

bool AppSettings::isKorean()
{
    return language().startsWith(QStringLiteral("ko"));
}

QString AppSettings::defaultHotkeyText()
{
    return defaultShowHotkey().toString(QKeySequence::PortableText);
}
