#include "AppSettings.h"
#include "ItemDetailDialog.h"
#include "QuickWindow.h"
#include "SettingsDialog.h"
#include "SingleInstance.h"
#include "TotpGenerator.h"
#include "UpdateChecker.h"
#include "WinHotkey.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QFile>
#include <QFontDatabase>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QProgressBar>
#include <QSettings>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QThread>
#include <QtConcurrent/QtConcurrentRun>
#include <cstdio>

namespace {
int failures = 0;
void check(bool condition, const char* message)
{
    if (!condition) {
        ++failures;
        std::fprintf(stderr, "%s\n", message);
    }
}

bool until(const std::function<bool()>& condition)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 10000) {
        QCoreApplication::processEvents();
        if (condition())
            return true;
        QThread::msleep(10);
    }
    return false;
}

void snapshot(QWidget& widget, const QString& name)
{
    widget.show();
    QCoreApplication::processEvents();
    const QString directory = QCoreApplication::applicationDirPath() + QStringLiteral("/ui-previews");
    QDir().mkpath(directory);
    check(widget.grab().save(directory + QLatin1Char('/') + name + QStringLiteral(".png")),
        "Could not save UI preview.");
}
}

class QuickWindowTestAccess
{
public:
    static void seed(QuickWindow& window)
    {
        qunsetenv("PEEKPASS_FAKE_PASS_MODE");
        QString error;
        check(window.m_passwordManager->statusInfo(&error).status == PasswordManager::VaultStatus::Unlocked,
            "Could not verify fake session.");
        window.m_cachedItems = window.m_passwordManager->listItems(&error);
        check(error.isEmpty() && !window.m_cachedItems.isEmpty(), "Could not load fixture items.");
        window.m_itemsReady = true;
        window.m_currentUserEmail = QStringLiteral("tester@proton.me");
        window.hideAuthPanel();
        window.m_search->setEnabled(true);
        window.show();
        window.m_search->setText(QStringLiteral("Example"));
        window.startSearch(QStringLiteral("Example"));
    }

    static void run()
    {
        QuickWindow window;
        window.m_sessionTimer.stop();
        seed(window);
        check(window.m_list->count() == 1, "Fixture search result was missing.");

        QTemporaryDir traceDirectory;
        check(traceDirectory.isValid(), "Could not create an isolated CLI trace directory.");
        const QString tracePath = traceDirectory.filePath(QStringLiteral("commands.txt"));
        qputenv("PEEKPASS_FAKE_PASS_TRACE", tracePath.toUtf8());
        for (int reopen = 0; reopen < 3; ++reopen) {
            window.preserveSearchState();
            window.hide();
            window.showQuick();
            check(window.m_search->isEnabled() && window.m_list->isEnabled(),
                "Cached reopening disabled search or results.");
            check(window.m_list->isVisible() && window.m_list->count() == 1,
                "Cached reopening did not immediately restore the search results.");
            check(!window.m_busyBar->isVisible() && !window.m_prepareWatcher.isRunning(),
                "Cached reopening started a foreground session check.");
        }
        check(!QFileInfo::exists(tracePath), "Cached reopening unexpectedly invoked the provider CLI.");
        qunsetenv("PEEKPASS_FAKE_PASS_TRACE");

        // Default instant-copy mode: copies immediately without blocking on slow CLI checks,
        // and still clears owned clipboard + cache if background verification detects an external lock.
        AppSettings::setVerifySessionBeforeCopyEnabled(false);
        QApplication::clipboard()->setText(QStringLiteral("sentinel"));
        qputenv("PEEKPASS_FAKE_PASS_MODE", "slow-info");
        window.copyPassword(window.m_list->item(0));
        check(QApplication::clipboard()->text() == QStringLiteral("fixture-password"),
            "Default instant-copy mode did not copy immediately from cache.");
        check(!window.m_busyBar->isVisible(),
            "Default instant-copy mode showed the foreground busy bar.");
        check(until([&] { return !window.m_prepareWatcher.isRunning(); }),
            "Post-copy background session verification did not finish.");

        seed(window);
        QApplication::clipboard()->setText(QStringLiteral("sentinel"));
        qputenv("PEEKPASS_FAKE_PASS_MODE", "locked");
        window.copyPassword(window.m_list->item(0));
        check(QApplication::clipboard()->text() == QStringLiteral("fixture-password"),
            "Instant-copy mode did not copy immediately before background lock check.");
        check(until([&] { return !window.m_itemsReady; }),
            "Background verification after instant copy did not invalidate the cache on external lock.");
        check(QApplication::clipboard()->text().isEmpty(),
            "Background lock invalidation after instant copy did not clear the owned clipboard.");
        check(window.m_cachedItems.isEmpty() && window.m_list->count() == 0,
            "Background lock invalidation left cached secrets or result rows behind.");

        // Strict verification mode: waits for live CLI session check before copying.
        AppSettings::setVerifySessionBeforeCopyEnabled(true);
        seed(window);
        QApplication::clipboard()->setText(QStringLiteral("sentinel"));
        qputenv("PEEKPASS_FAKE_PASS_MODE", "slow-info");
        window.prepareVault(false);
        check(window.m_search->isEnabled() && window.m_list->isEnabled() && !window.m_busyBar->isVisible(),
            "Background validation interrupted cached search.");
        window.hide();
        window.showQuick();
        window.m_search->setText(QStringLiteral("fixture"));
        check(window.m_list->count() == 1 && window.m_search->isEnabled() && !window.m_busyBar->isVisible(),
            "Cached reopening waited for an in-flight background check.");
        window.copyPassword(window.m_list->item(0));
        check(until([] { return QApplication::clipboard()->text() == QStringLiteral("fixture-password"); }),
            "Copy during background validation was dropped instead of using the verified result.");

        seed(window);
        qputenv("PEEKPASS_FAKE_PASS_MODE", "slow-list");
        window.startItemLoad(true, true);
        window.preserveSearchState();
        window.hide();
        window.showQuick();
        check(window.m_search->isEnabled() && !window.m_busyBar->isVisible() && window.m_quietItemLoad,
            "Reopening promoted a quiet refresh into a foreground load.");
        check(window.m_list->isVisible() && window.m_list->count() == 1,
            "Reopening during refresh hid cached search results.");
        check(until([&] { return !window.m_itemsWatcher.isRunning(); }), "Background item refresh did not finish.");
        QCoreApplication::processEvents();

        seed(window);
        QApplication::clipboard()->setText(QStringLiteral("sentinel"));
        qputenv("PEEKPASS_FAKE_PASS_MODE", "locked");
        window.copyPassword(window.m_list->item(0));
        check(until([&] { return !window.m_itemsReady; }), "External lock did not invalidate the cache.");
        check(QApplication::clipboard()->text() != QStringLiteral("fixture-password"),
            "An externally locked password reached the clipboard.");
        check(window.m_cachedItems.isEmpty() && window.m_list->count() == 0,
            "External lock left cached secrets or result rows behind.");
        AppSettings::setVerifySessionBeforeCopyEnabled(false);

        seed(window);
        window.hide();
        qputenv("PEEKPASS_FAKE_PASS_MODE", "unauthenticated");
        window.showQuick();
        window.prepareVault(false);
        check(until([&] { return !window.m_itemsReady; }), "Background validation retained a signed-out cache.");
        check(window.m_authPanel->isVisible(), "Signed-out session did not show the connection screen.");

        seed(window);
        qputenv("PEEKPASS_FAKE_PASS_MODE", "locked");
        window.startItemLoad(true, true);
        check(until([&] { return !window.m_itemsReady; }), "Background lock detection retained vault data.");

        seed(window);
        qputenv("PEEKPASS_FAKE_PASS_MODE", "list-error");
        window.startItemLoad(true, true);
        check(until([&] { return !window.m_itemsReady; }), "Failed background refresh retained vault data.");
        check(window.m_preloadRetryTimer.isActive(),
            "Failed background refresh did not schedule automatic preload recovery.");
        qunsetenv("PEEKPASS_FAKE_PASS_MODE");
        window.hide();
        window.schedulePreloadRetry(0);
        check(until([&] { return window.m_itemsReady && !window.m_cachedItems.isEmpty(); }),
            "Automatic background preload recovery did not reload the vault cache.");

        seed(window);
        snapshot(window, QStringLiteral("search-dark"));
        {
            const VaultItem& sampleItem = window.m_cachedItems.constFirst();
            ItemDetailDialog detailDark(
                sampleItem.id,
                sampleItem.name,
                window.m_list->item(0)->icon(),
                {},
                {});
            detailDark.setDetails(PasswordManager::detailsFromVaultItem(sampleItem));
            snapshot(detailDark, QStringLiteral("detail-dark"));
            detailDark.hide();
        }
        window.m_search->clear();
        window.reload();
        snapshot(window, QStringLiteral("empty-search-dark"));
        window.m_search->setText(QStringLiteral("Example"));
        window.startSearch(QStringLiteral("Example"));
        QSet<int> hintRows;
        for (QLabel* hint : window.m_footer->findChildren<QLabel*>(QStringLiteral("footerShortcut"))) {
            if (hint->isVisible()) {
                hintRows.insert(hint->y());
                check(window.m_footer->rect().contains(hint->geometry()), "A shortcut hint is clipped.");
            }
        }
        check(hintRows.size() == 2, "Shortcut hints must use exactly two rows.");
        window.clearVaultCache();
        window.showAuthPanel(QuickWindow::AuthMode::Login);
        snapshot(window, QStringLiteral("proton-connect-dark"));
        check(!window.m_authPassword->isVisible() && !window.m_authEmail->isVisible(),
            "Proton connection screen still requested account credentials.");
        check(window.m_authBrowserLogin->isVisible(), "Browser sign-in button was missing.");
        check(window.rect().contains(window.m_authSubmit->mapTo(&window, window.m_authSubmit->rect().bottomRight())),
            "Connection button is clipped.");

        {
            SettingsDialog darkSettings;
            snapshot(darkSettings, QStringLiteral("settings-dark"));
            darkSettings.hide();
        }
        {
            AppSettings::setPasswordManagerType(QStringLiteral("bitwarden"));
            window.m_passwordManager = std::make_shared<PasswordManager>(PasswordManager::Type::Bitwarden);
            window.showAuthPanel(QuickWindow::AuthMode::Login);
            snapshot(window, QStringLiteral("bitwarden-login-dark"));
            AppSettings::setPasswordManagerType(QStringLiteral("protonpass"));
            window.m_passwordManager = std::make_shared<PasswordManager>(PasswordManager::Type::ProtonPass);
        }

        AppSettings::setTheme(QStringLiteral("light"));
        AppSettings::setLanguage(QStringLiteral("ko"));
        window.applySettings();
        window.showAuthPanel(QuickWindow::AuthMode::Login);
        snapshot(window, QStringLiteral("proton-connect-light-ko"));

        SettingsDialog settings;
        snapshot(settings, QStringLiteral("settings-light-ko"));
        settings.hide();

        {
            seed(window);
            const VaultItem& sampleItem = window.m_cachedItems.constFirst();
            ItemDetailDialog detailLightKo(
                sampleItem.id,
                sampleItem.name,
                window.m_list->item(0)->icon(),
                {},
                {});
            detailLightKo.setDetails(PasswordManager::detailsFromVaultItem(sampleItem));
            snapshot(detailLightKo, QStringLiteral("detail-light-ko"));
            detailLightKo.hide();
        }

        // Explicit construction skips settings reload and keeps real credentials untouched.
        AppSettings::setPasswordManagerType(QStringLiteral("bitwarden"));
        window.m_passwordManager = std::make_shared<PasswordManager>(PasswordManager::Type::Bitwarden);
        window.showAuthPanel(QuickWindow::AuthMode::Login);
        snapshot(window, QStringLiteral("bitwarden-login-light-ko"));
        check(window.m_authPassword->isVisible(), "Bitwarden master-password field was missing.");
        window.hide();

        const QByteArray releasePayload = R"({
            "tag_name": "v0.3.7",
            "html_url": "https://github.com/j0n9m1n1/peekpass/releases/tag/v0.3.7",
            "body": "- Added SHA-256 verification and icon resource\n- Added automatic background preload recovery",
            "assets": [
                {
                    "name": "peekpass-v0.3.7-win64.zip",
                    "browser_download_url": "https://github.com/j0n9m1n1/peekpass/releases/download/v0.3.7/peekpass-v0.3.7-win64.zip",
                    "size": 123456
                },
                {
                    "name": "peekpass-0.3.7-win64-setup.exe",
                    "browser_download_url": "https://github.com/j0n9m1n1/peekpass/releases/download/v0.3.7/peekpass-0.3.7-win64-setup.exe",
                    "size": 654321
                },
                {
                    "name": "SHA256SUMS.txt",
                    "browser_download_url": "https://github.com/j0n9m1n1/peekpass/releases/download/v0.3.7/SHA256SUMS.txt",
                    "size": 256
                }
            ]
        })";

        QString parseError;
        const UpdateReleaseInfo newerInfo = UpdateChecker::parseLatestReleaseJson(
            releasePayload,
            QStringLiteral("0.3.6"),
            &parseError);
        check(parseError.isEmpty(), "Valid GitHub release JSON returned a parse error.");
        check(newerInfo.updateAvailable, "Newer release was not detected as an available update.");
        check(newerInfo.latestVersion == QStringLiteral("0.3.7"), "Latest version string was not normalized.");
        check(newerInfo.installerName == QStringLiteral("peekpass-0.3.7-win64-setup.exe"),
            "Installer asset name was not selected.");
        check(newerInfo.installerSize == 654321, "Installer asset size did not match.");
        check(newerInfo.checksumsUrl.endsWith(QStringLiteral("/SHA256SUMS.txt")),
            "Checksums asset URL was not extracted.");

        const QByteArray sampleChecksums =
            "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef  peekpass-0.3.7-win64-setup.exe\n"
            "fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210  peekpass-v0.3.7-win64.zip\n";
        check(UpdateChecker::parseSha256SumForFile(sampleChecksums, QStringLiteral("peekpass-0.3.7-win64-setup.exe"))
                == QStringLiteral("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"),
            "SHA-256 checksum parser did not match the installer entry.");

        const UpdateReleaseInfo sameInfo = UpdateChecker::parseLatestReleaseJson(
            releasePayload,
            QStringLiteral("0.3.7"),
            &parseError);
        check(!sameInfo.updateAvailable, "Current version was falsely marked as needing an update.");

        // Regression: untrusted html_url rejected & SHA256SUMS.txt preferred over secondary .sha256 assets
        const QByteArray trickyReleasePayload = R"({
            "tag_name": "v0.3.8",
            "html_url": "file:///C:/Windows/System32/calc.exe",
            "body": "Test",
            "assets": [
                {
                    "name": "peekpass-0.3.8-win64-setup.exe",
                    "browser_download_url": "https://github.com/j0n9m1n1/peekpass/releases/download/v0.3.8/peekpass-0.3.8-win64-setup.exe",
                    "size": 654321
                },
                {
                    "name": "SHA256SUMS.txt",
                    "browser_download_url": "https://github.com/j0n9m1n1/peekpass/releases/download/v0.3.8/SHA256SUMS.txt",
                    "size": 256
                },
                {
                    "name": "peekpass-v0.3.8-win64.zip.sha256",
                    "browser_download_url": "https://github.com/j0n9m1n1/peekpass/releases/download/v0.3.8/peekpass-v0.3.8-win64.zip.sha256",
                    "size": 128
                }
            ]
        })";
        const UpdateReleaseInfo trickyInfo = UpdateChecker::parseLatestReleaseJson(
            trickyReleasePayload,
            QStringLiteral("0.3.7"),
            &parseError);
        check(trickyInfo.releaseUrl == QStringLiteral("https://github.com/j0n9m1n1/peekpass/releases/latest"),
            "Untrusted html_url was not rejected.");
        check(trickyInfo.checksumsUrl.endsWith(QStringLiteral("/SHA256SUMS.txt")),
            "Secondary .sha256 asset overwrote SHA256SUMS.txt.");

        // Regression: WinHotkey shifted-key support & modifier-less rejection
        check(WinHotkey::isSupportedHotkey(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Space)),
            "Standard Ctrl+Shift+Space hotkey was rejected.");
        check(WinHotkey::isSupportedHotkey(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Question)),
            "Shifted Ctrl+Shift+? hotkey was rejected.");
        check(WinHotkey::isSupportedHotkey(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Bar)),
            "Shifted Ctrl+Shift+| hotkey was rejected.");
        check(!WinHotkey::isSupportedHotkey(QKeySequence(Qt::Key_Space)),
            "Modifier-less Space hotkey was accepted.");

        // Regression: TotpGenerator rejects hotp, handles invalid params, and supports 10-digit codes without overflow
        QString totpErr;
        check(TotpGenerator::generateTotpCode(
                QStringLiteral("otpauth://hotp/Test?secret=JBSWY3DPEHPK3PXP&counter=1"),
                &totpErr).isEmpty(),
            "TotpGenerator erroneously accepted an HOTP URI.");
        const TotpCodeInfo defaultFallbackTotp = TotpGenerator::generateTotpInfo(
            QStringLiteral("otpauth://totp/Test?secret=JBSWY3DPEHPK3PXP&digits=&period=abc"),
            &totpErr);
        check(defaultFallbackTotp.code.size() == 6 && defaultFallbackTotp.period == 30,
            "Invalid digits/period query params did not fall back to defaults 6/30.");
        const TotpCodeInfo tenDigitTotp = TotpGenerator::generateTotpInfo(
            QStringLiteral("otpauth://totp/Test?secret=JBSWY3DPEHPK3PXP&digits=10"),
            &totpErr);
        check(tenDigitTotp.code.size() == 10,
            "10-digit TOTP code was not generated cleanly.");

        // Regression: restorePreservedSearch consumes preserved state once restored
        AppSettings::setPasswordManagerType(QStringLiteral("protonpass"));
        window.m_passwordManager = std::make_shared<PasswordManager>(PasswordManager::Type::ProtonPass);
        seed(window);
        window.m_search->setText(QStringLiteral("Example"));
        window.preserveSearchState();
        check(window.shouldRestorePreservedSearch(), "Preserved search state was not active.");
        window.restorePreservedSearch();
        check(!window.shouldRestorePreservedSearch(), "Preserved search state remained active after restore.");
        window.hide();

        UpdateDialog updateDialog(newerInfo);
        snapshot(updateDialog, QStringLiteral("update-dialog-light-ko"));
        updateDialog.hide();
    }
};

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    qInstallMessageHandler([](QtMsgType type, const QMessageLogContext&, const QString& message) {
        if (type == QtCriticalMsg || type == QtFatalMsg)
            std::fprintf(stderr, "%s\n", qPrintable(message));
    });
#ifdef Q_OS_WIN
    QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/segoeui.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/malgun.ttf"));
    app.setFont(QFont(QStringLiteral("Segoe UI"), 10));
#endif
    app.setQuitOnLastWindowClosed(false);
    app.setOrganizationName(QStringLiteral("peekpass-tests"));
    app.setApplicationName(QStringLiteral("quick-window-tests"));
    if (app.arguments().size() != 2)
        return 2;
    QTemporaryDir settingsDirectory;
    check(settingsDirectory.isValid(), "Could not create isolated settings.");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory.path());
    AppSettings::setCredentialSessionStorageEnabled(false);
    AppSettings::setPasswordManagerType(QStringLiteral("protonpass"));
    AppSettings::setProtonPassProgramOverride(QFileInfo(app.arguments().at(1)).absoluteFilePath());
    AppSettings::setFetchFaviconsFromWeb(false);
    AppSettings::setAutoSyncIntervalMinutes(0);
    AppSettings::setCloseOnFocusLoss(false);
    AppSettings::setLanguage(QStringLiteral("en"));
    AppSettings::setTheme(QStringLiteral("dark"));
    QuickWindowTestAccess::run();

    QTemporaryDir runtimeDirectory;
    {
        SingleInstance first(runtimeDirectory.path());
        check(first.start() == SingleInstance::Result::Primary, "Primary instance failed to start.");
        if (!first.errorString().isEmpty())
            std::fprintf(stderr, "%s\n", qPrintable(first.errorString()));
        int activations = 0;
        int quitRequests = 0;
        first.setActivationHandler([&] { ++activations; });
        first.setQuitHandler([&] { ++quitRequests; });
        auto secondary = QtConcurrent::run([directory = runtimeDirectory.path()] {
            SingleInstance second(directory);
            const auto result = second.start();
            if (result == SingleInstance::Result::Error)
                std::fprintf(stderr, "%s\n", qPrintable(second.errorString()));
            return result;
        });
        check(until([&] { return secondary.isFinished(); }), "Secondary instance timed out.");
        check(secondary.result() == SingleInstance::Result::ActivatedExisting,
            "Duplicate instance did not forward activation.");
        check(until([&] { return activations == 1; }), "Primary did not receive the activation message.");

        auto quitter = QtConcurrent::run([directory = runtimeDirectory.path()] {
            SingleInstance helper(directory);
            return helper.requestQuit(0);
        });
        check(until([&] { return quitter.isFinished(); }), "Quit request timed out.");
        check(quitter.result(), "Quit helper failed to signal primary instance.");
        check(until([&] { return quitRequests == 1; }), "Primary did not receive the quit request.");
    }
    SingleInstance replacement(runtimeDirectory.path());
    check(replacement.start() == SingleInstance::Result::Primary, "Instance lock was not released on exit.");
    if (!failures)
        qInfo("Vault invalidation, clipboard, UI, and single-instance tests passed.");
    return failures ? 1 : 0;
}
