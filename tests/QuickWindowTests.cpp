#include "AppSettings.h"
#include "QuickWindow.h"
#include "SettingsDialog.h"
#include "SingleInstance.h"

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

        seed(window);
        snapshot(window, QStringLiteral("search-dark"));
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

        AppSettings::setTheme(QStringLiteral("light"));
        AppSettings::setLanguage(QStringLiteral("ko"));
        window.applySettings();
        window.showAuthPanel(QuickWindow::AuthMode::Login);
        snapshot(window, QStringLiteral("proton-connect-light-ko"));

        SettingsDialog settings;
        snapshot(settings, QStringLiteral("settings-light-ko"));
        settings.hide();

        // Explicit construction skips settings reload and keeps real credentials untouched.
        AppSettings::setPasswordManagerType(QStringLiteral("bitwarden"));
        window.m_passwordManager = std::make_shared<PasswordManager>(PasswordManager::Type::Bitwarden);
        window.showAuthPanel(QuickWindow::AuthMode::Login);
        snapshot(window, QStringLiteral("bitwarden-login-light-ko"));
        check(window.m_authPassword->isVisible(), "Bitwarden master-password field was missing.");
        window.hide();
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
        first.setActivationHandler([&] { ++activations; });
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
    }
    SingleInstance replacement(runtimeDirectory.path());
    check(replacement.start() == SingleInstance::Result::Primary, "Instance lock was not released on exit.");
    if (!failures)
        qInfo("Vault invalidation, clipboard, UI, and single-instance tests passed.");
    return failures ? 1 : 0;
}
