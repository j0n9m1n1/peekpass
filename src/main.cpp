#include "AppSettings.h"
#include "I18n.h"
#include "QuickWindow.h"
#include "SingleInstance.h"
#include "UpdateChecker.h"
#include "WinHotkey.h"

#include <QApplication>
#include <QAction>
#include <QCommandLineParser>
#include <QIcon>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QString>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QTranslator>

namespace
{
QString uiText(const char* sourceText, const char* koreanFallback)
{
    return I18n::translate("Main", sourceText, koreanFallback);
}

QIcon makeAppIcon()
{
    QIcon icon;
    for (int size : {16, 24, 32, 48, 64}) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);

        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);

        const qreal s = size / 32.0;
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#1f72c7"));
        painter.drawRoundedRect(QRectF(2 * s, 2 * s, 28 * s, 28 * s), 7 * s, 7 * s);

        QPen pen(QColor("#ffffff"), qMax(1.6, 2.4 * s), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(QRectF(7.5 * s, 11 * s, 9 * s, 9 * s));
        painter.drawLine(QPointF(16.5 * s, 15.5 * s), QPointF(24.5 * s, 15.5 * s));
        painter.drawLine(QPointF(21 * s, 15.5 * s), QPointF(21 * s, 19.5 * s));
        painter.drawLine(QPointF(24 * s, 15.5 * s), QPointF(24 * s, 18.5 * s));

        icon.addPixmap(pixmap);
    }
    return icon;
}
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("peekpass");
    QApplication::setOrganizationName("peekpass");
    QApplication::setApplicationVersion(QStringLiteral(PEEKPASS_VERSION));
    const QIcon appIcon = makeAppIcon();
    app.setWindowIcon(appIcon);
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(app);
    QApplication::setQuitOnLastWindowClosed(false);
    SingleInstance instance;
    const auto instanceResult = instance.start();
    if (instanceResult == SingleInstance::Result::ActivatedExisting)
        return 0;
    if (instanceResult == SingleInstance::Result::Error) {
        QMessageBox::warning(nullptr, QStringLiteral("peekpass"), instance.errorString());
        return 1;
    }
    AppSettings::migratePreviousSettings();

    QTranslator appTranslator;
    QTranslator qtTranslator;
    const auto reloadTranslator = [&] {
        I18n::installTranslator(app, appTranslator, AppSettings::language());
        I18n::installQtTranslator(app, qtTranslator, AppSettings::language());
    };
    reloadTranslator();

    QString startupError;
    if (!AppSettings::applyStartupPreference(&startupError)) {
        QMessageBox::warning(
            nullptr,
            uiText("Windows startup", "Windows 자동 시작"),
            QString("%1\n\n%2")
                .arg(uiText("Could not validate the startup registration. Configure startup again in Settings.",
                            "자동 시작 등록을 확인하지 못했습니다. 설정에서 자동 시작을 다시 지정하세요."),
                    startupError));
    }

    QuickWindow window;
    instance.setActivationHandler([&window] {
        if (window.isVisible()) {
            window.raise();
            window.activateWindow();
        } else {
            window.showQuick();
        }
    });

    QMenu trayMenu;
    QAction* openAction = trayMenu.addAction(QString());
    QAction* lockAction = trayMenu.addAction(QString());
    trayMenu.addSeparator();
    QAction* settingsAction = trayMenu.addAction(QString());
    QAction* checkUpdateAction = trayMenu.addAction(QString());
    trayMenu.addSeparator();
    QAction* quitAction = trayMenu.addAction(QString());

    const auto updateTrayTexts = [&] {
        openAction->setText(uiText("Open", "열기"));
        if (window.passwordManagerName() == QStringLiteral("Proton Pass"))
            lockAction->setText(uiText("Log out", "로그아웃"));
        else
            lockAction->setText(uiText("Lock", "잠금"));
        settingsAction->setText(uiText("Settings", "설정"));
        checkUpdateAction->setText(uiText("Check for updates...", "업데이트 확인..."));
        quitAction->setText(uiText("Quit", "종료"));
    };
    updateTrayTexts();

    QSystemTrayIcon trayIcon;
    trayIcon.setIcon(appIcon);
    trayIcon.setToolTip("peekpass");
    trayIcon.setContextMenu(&trayMenu);

    QObject::connect(openAction, &QAction::triggered, &window, [&window] {
        window.showQuick();
    });

    QObject::connect(lockAction, &QAction::triggered, &window, [&window, &trayIcon] {
        QString errorMessage;
        if (window.passwordManagerName() == QStringLiteral("Proton Pass")) {
            if (!window.lockVault(&errorMessage) && !errorMessage.isEmpty()) {
                QMessageBox::warning(&window, QStringLiteral("Proton Pass"), errorMessage);
                return;
            }

            trayIcon.showMessage("peekpass", uiText("Proton Pass session signed out.", "Proton Pass 세션에서 로그아웃했습니다."));
            return;
        }

        if (!window.lockVault(&errorMessage) && !errorMessage.isEmpty()) {
            QMessageBox::warning(&window, uiText("Bitwarden Lock", "Bitwarden 잠금"), errorMessage);
            return;
        }

        trayIcon.showMessage(
            "peekpass",
            uiText("Bitwarden vault locked.", "Bitwarden vault를 잠갔습니다."));
    });

    QObject::connect(settingsAction, &QAction::triggered, &window, [&window] {
        window.showSettings();
    });

    QObject::connect(checkUpdateAction, &QAction::triggered, &window, [&window, &trayIcon] {
        UpdateChecker::instance().checkForUpdates(true, &window, &trayIcon);
    });

    QObject::connect(quitAction, &QAction::triggered, &app, [&trayIcon] {
        trayIcon.hide();
        QApplication::quit();
    });

    QObject::connect(&trayIcon, &QSystemTrayIcon::activated, &window,
        [&window](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
                window.showQuick();
        });

    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        trayIcon.show();
    } else {
        QMessageBox::warning(
            nullptr,
            uiText("System Tray", "시스템 트레이"),
            uiText("System tray is not available. peekpass will keep running without a tray icon.",
                   "시스템 트레이를 사용할 수 없습니다. peekpass는 트레이 아이콘 없이 계속 실행됩니다."));
    }

#ifdef Q_OS_WIN
    WinHotkey hotkey(&window);
    app.installNativeEventFilter(&hotkey);

    window.setSettingsChangedCallback([&] {
        reloadTranslator();
        updateTrayTexts();
        hotkey.unregisterHotkey();
        if (!hotkey.registerHotkey()) {
            QMessageBox::warning(
                &window,
                uiText("Hotkey", "단축키"),
                QString("%1\n\n%2\n\n%3")
                    .arg(uiText("Failed to register the configured hotkey.",
                                "설정된 단축키를 등록하지 못했습니다."),
                        hotkey.lastError(),
                        uiText("Another app, such as 1Password, may already be using this shortcut.",
                               "1Password 같은 다른 앱이 이미 이 단축키를 사용 중일 수 있습니다."))
            );
        }
    });

    if (!hotkey.registerHotkey()) {
        QMessageBox::warning(
            nullptr,
            uiText("Hotkey", "단축키"),
            QString("%1\n\n%2\n\n%3")
                .arg(uiText("Failed to register the configured hotkey.",
                            "설정된 단축키를 등록하지 못했습니다."),
                    hotkey.lastError(),
                    uiText("Another app, such as 1Password, may already be using this shortcut.",
                           "1Password 같은 다른 앱이 이미 이 단축키를 사용 중일 수 있습니다.")));
    }
#else
    window.showQuick();
#endif

    if (trayIcon.isVisible()) {
        QTimer::singleShot(700, &trayIcon, [&trayIcon] {
            if (!trayIcon.isVisible())
                return;

            trayIcon.showMessage(
                "peekpass",
                uiText("peekpass is running.\nPress %1 to open quick access.",
                       "peekpass가 실행 중입니다.\n%1 키로 빠른 액세스를 열 수 있습니다.")
                    .arg(AppSettings::showHotkeyText()),
                QSystemTrayIcon::Information,
                3500);
        });
    }

    QTimer::singleShot(0, &window, [&window] {
        window.preloadVault();
    });

    QTimer::singleShot(4000, &window, [&window, &trayIcon] {
        UpdateChecker::instance().checkOnStartupIfDue(&window, &trayIcon);
    });

    return app.exec();
}
