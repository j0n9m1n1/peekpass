#pragma once

#include <QByteArray>
#include <QDialog>
#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QString>

class QFile;
class QLabel;
class QMouseEvent;
class QNetworkAccessManager;
class QNetworkReply;
class QProgressBar;
class QPushButton;
class QResizeEvent;
class QSystemTrayIcon;

struct UpdateReleaseInfo
{
    QString currentVersion;
    QString latestVersion;
    QString tagName;
    QString releaseUrl;
    QString installerUrl;
    QString installerName;
    qint64 installerSize = 0;
    QString checksumsUrl;
    QString portableZipUrl;
    QString releaseNotes;
    bool updateAvailable = false;
};

class UpdateDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UpdateDialog(const UpdateReleaseInfo& info, QWidget* parent = nullptr);
    ~UpdateDialog() override;

    void reject() override;

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void applyDialogStyle();
    void updateWindowMask();
    void startInstallerDownload();
    void verifyAndLaunchInstaller(const QString& expectedSha256);
    void cancelDownload();

    UpdateReleaseInfo m_info;
    QNetworkAccessManager* m_network = nullptr;
    QPointer<QNetworkReply> m_downloadReply;
    QPointer<QNetworkReply> m_checksumReply;
    QFile* m_partFile = nullptr;
    QString m_partFilePath;
    QString m_finalInstallerPath;
    QLabel* m_statusLabel = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QPushButton* m_updateButton = nullptr;
    QPushButton* m_releasePageButton = nullptr;
    QPushButton* m_skipButton = nullptr;
    QPushButton* m_laterButton = nullptr;
    bool m_dragging = false;
    QPoint m_dragOffset;
};

class UpdateChecker : public QObject
{
    Q_OBJECT

public:
    static UpdateChecker& instance();

    explicit UpdateChecker(QObject* parent = nullptr);

    void checkForUpdates(bool interactive, QWidget* parentWidget = nullptr, QSystemTrayIcon* trayIcon = nullptr);
    void checkOnStartupIfDue(QWidget* parentWidget = nullptr, QSystemTrayIcon* trayIcon = nullptr);
    void showUpdateDialog(const UpdateReleaseInfo& info, QWidget* parentWidget = nullptr);

    static bool isInstalledBuild();
    static void cleanupOldInstallers();
    static QString parseSha256SumForFile(const QByteArray& checksumsText, const QString& fileName);

    static UpdateReleaseInfo parseLatestReleaseJson(
        const QByteArray& jsonPayload,
        const QString& currentVersion,
        QString* errorMessage = nullptr);

private:
    QNetworkAccessManager* m_network = nullptr;
    QPointer<QNetworkReply> m_activeReply;
    QPointer<UpdateDialog> m_activeDialog;
    UpdateReleaseInfo m_lastAvailableInfo;
};
