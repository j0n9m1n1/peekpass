#include "UpdateChecker.h"

#include "AppSettings.h"
#include "I18n.h"

#include <QApplication>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScreen>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QVersionNumber>

namespace
{
constexpr qint64 AutoCheckIntervalSeconds = 12 * 60 * 60;
constexpr qint64 OldTempInstallerAgeSeconds = 5 * 60;
const char* DefaultLatestReleaseApiUrl = "https://api.github.com/repos/j0n9m1n1/peekpass/releases/latest";
const char* DefaultReleasesPageUrl = "https://github.com/j0n9m1n1/peekpass/releases/latest";

QString uiText(const char* english, const char* korean)
{
    return I18n::translate("UpdateChecker", english, korean);
}

bool isLightTheme()
{
    return AppSettings::theme() == QStringLiteral("light");
}

QString normalizeVersionString(QString value)
{
    value = value.trimmed();
    while (value.startsWith(QLatin1Char('v')) || value.startsWith(QLatin1Char('V')))
        value.remove(0, 1);
    return value.trimmed();
}

bool isTrustedDownloadUrl(const QUrl& url)
{
    if (!url.isValid())
        return false;

    const QString scheme = url.scheme().toLower();
    const QString host = url.host().toLower();
    if (scheme == QStringLiteral("https")) {
        return host == QStringLiteral("github.com")
            || host.endsWith(QStringLiteral(".github.com"))
            || host == QStringLiteral("objects.githubusercontent.com")
            || host.endsWith(QStringLiteral(".githubusercontent.com"));
    }

    if (!qEnvironmentVariableIsEmpty("PEEKPASS_UPDATE_API_URL")
        && (host == QStringLiteral("127.0.0.1") || host == QStringLiteral("localhost"))) {
        return scheme == QStringLiteral("http") || scheme == QStringLiteral("https");
    }

    return false;
}

bool isTrustedReleasePageUrl(const QUrl& url)
{
    if (!url.isValid())
        return false;

    if (url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) != 0)
        return false;

    const QString host = url.host().toLower();
    return host == QStringLiteral("github.com")
        && url.path().startsWith(QStringLiteral("/j0n9m1n1/peekpass"), Qt::CaseInsensitive);
}

QString formatMegabytes(qint64 bytes)
{
    const double mb = static_cast<double>(qMax<qint64>(0, bytes)) / (1024.0 * 1024.0);
    return QString::number(mb, 'f', 1) + QStringLiteral(" MB");
}

QString computeFileSha256Hex(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file))
        return {};
    return QString::fromLatin1(hash.result().toHex()).toLower();
}

QString updateDialogStyleSheet()
{
    QString fontFamily = AppSettings::quickFontFamily();
    fontFamily.replace('\\', "\\\\");
    fontFamily.replace('"', "\\\"");

    if (isLightTheme()) {
        return QStringLiteral(R"(
            QDialog {
                background: #f6f8fa;
                border: 1px solid #d0d7de;
                color: #24292f;
                font-family: "%1";
            }
            QLabel, QPushButton, QPlainTextEdit, QProgressBar {
                font-family: "%1";
            }
            #updateWindowTitle {
                color: #57606a;
                font-size: 13px;
                font-weight: 800;
            }
            #updateCard {
                background: #ffffff;
                border: 1px solid #d0d7de;
                border-radius: 8px;
            }
            #updateHeadline {
                color: #111827;
                font-size: 16px;
                font-weight: 800;
            }
            #updateBadge {
                background: #eef6ff;
                color: #0969da;
                border: 1px solid #b6d7ff;
                border-radius: 5px;
                padding: 3px 9px;
                font-size: 12px;
                font-weight: 800;
            }
            #updateSectionTitle {
                color: #111827;
                font-size: 13px;
                font-weight: 800;
            }
            #updatePortableNote {
                color: #57606a;
                font-size: 11px;
                font-weight: 600;
            }
            #updateStatusLabel {
                color: #57606a;
                font-size: 12px;
                font-weight: 700;
            }
            QPlainTextEdit {
                background: #f6f8fa;
                color: #24292f;
                border: 1px solid #d8dee4;
                border-radius: 6px;
                padding: 8px;
                font-size: 12px;
            }
            QProgressBar {
                background: #eaeef2;
                border: 1px solid #d0d7de;
                border-radius: 5px;
                min-height: 10px;
                max-height: 10px;
                text-align: center;
            }
            QProgressBar::chunk {
                background: #1f72c7;
                border-radius: 4px;
            }
            QPushButton {
                background: #f6f8fa;
                color: #24292f;
                border: 1px solid #d0d7de;
                border-radius: 6px;
                padding: 6px 12px;
                font-size: 12px;
                font-weight: 700;
            }
            QPushButton:hover {
                background: #eef2f6;
                border-color: #1f72c7;
            }
            QPushButton:disabled {
                color: #8c959f;
                background: #eaeef2;
            }
            QPushButton#primaryUpdateButton {
                background: #1f72c7;
                color: #ffffff;
                border: 1px solid #1f72c7;
                padding: 6px 14px;
            }
            QPushButton#primaryUpdateButton:hover {
                background: #195fa8;
                border-color: #195fa8;
            }
            QPushButton#primaryUpdateButton:disabled {
                background: #8cb5df;
                border-color: #8cb5df;
                color: #ffffff;
            }
            QPushButton#updateTitleCloseButton {
                background: transparent;
                color: #57606a;
                border: 0;
                border-radius: 5px;
                padding: 2px 8px;
                font-size: 15px;
                font-weight: 800;
            }
            QPushButton#updateTitleCloseButton:hover {
                background: #eaeef2;
                color: #111827;
            }
        )").arg(fontFamily);
    }

    return QStringLiteral(R"(
        QDialog {
            background: #16181c;
            border: 1px solid #2b3038;
            color: #eef2f6;
            font-family: "%1";
        }
        QLabel, QPushButton, QPlainTextEdit, QProgressBar {
            font-family: "%1";
        }
        #updateWindowTitle {
            color: #9aa4b2;
            font-size: 13px;
            font-weight: 800;
        }
        #updateCard {
            background: #1d2026;
            border: 1px solid #2b3038;
            border-radius: 8px;
        }
        #updateHeadline {
            color: #ffffff;
            font-size: 16px;
            font-weight: 800;
        }
        #updateBadge {
            background: #172a40;
            color: #7cc0ff;
            border: 1px solid #284c73;
            border-radius: 5px;
            padding: 3px 9px;
            font-size: 12px;
            font-weight: 800;
        }
        #updateSectionTitle {
            color: #f3f6fa;
            font-size: 13px;
            font-weight: 800;
        }
        #updatePortableNote {
            color: #9aa4b2;
            font-size: 11px;
            font-weight: 600;
        }
        #updateStatusLabel {
            color: #b7c0cc;
            font-size: 12px;
            font-weight: 700;
        }
        QPlainTextEdit {
            background: #14171b;
            color: #e6edf3;
            border: 1px solid #2b3038;
            border-radius: 6px;
            padding: 8px;
            font-size: 12px;
        }
        QProgressBar {
            background: #252a33;
            border: 1px solid #343b46;
            border-radius: 5px;
            min-height: 10px;
            max-height: 10px;
            text-align: center;
        }
        QProgressBar::chunk {
            background: #2f81f7;
            border-radius: 4px;
        }
        QPushButton {
            background: #262b33;
            color: #eef2f6;
            border: 1px solid #353c47;
            border-radius: 6px;
            padding: 6px 12px;
            font-size: 12px;
            font-weight: 700;
        }
        QPushButton:hover {
            background: #303742;
            border-color: #3d8bfd;
        }
        QPushButton:disabled {
            color: #6e7681;
            background: #1d2026;
        }
        QPushButton#primaryUpdateButton {
            background: #1f72c7;
            color: #ffffff;
            border: 1px solid #2f81f7;
            padding: 6px 14px;
        }
        QPushButton#primaryUpdateButton:hover {
            background: #2a82dc;
            border-color: #58a6ff;
        }
        QPushButton#primaryUpdateButton:disabled {
            background: #1b3d63;
            border-color: #244d7a;
            color: #9aa4b2;
        }
        QPushButton#updateTitleCloseButton {
            background: transparent;
            color: #9aa4b2;
            border: 0;
            border-radius: 5px;
            padding: 2px 8px;
            font-size: 15px;
            font-weight: 800;
        }
        QPushButton#updateTitleCloseButton:hover {
            background: #262b33;
            color: #ffffff;
        }
    )").arg(fontFamily);
}
}

UpdateDialog::UpdateDialog(const UpdateReleaseInfo& info, QWidget* parent)
    : QDialog(parent)
    , m_info(info)
    , m_network(new QNetworkAccessManager(this))
{
    setWindowTitle(uiText("Software Update", "소프트웨어 업데이트"));
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setWindowModality(Qt::ApplicationModal);
    resize(510, 410);
    setMinimumSize(460, 360);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(16, 12, 16, 14);
    rootLayout->setSpacing(10);

    auto* titleBar = new QHBoxLayout;
    titleBar->setContentsMargins(2, 0, 0, 2);
    auto* windowTitleLabel = new QLabel(windowTitle(), this);
    windowTitleLabel->setObjectName(QStringLiteral("updateWindowTitle"));
    windowTitleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    auto* titleCloseButton = new QPushButton(QStringLiteral("\u00D7"), this);
    titleCloseButton->setObjectName(QStringLiteral("updateTitleCloseButton"));
    titleCloseButton->setCursor(Qt::PointingHandCursor);
    connect(titleCloseButton, &QPushButton::clicked, this, &QDialog::reject);

    titleBar->addWidget(windowTitleLabel);
    titleBar->addStretch(1);
    titleBar->addWidget(titleCloseButton);
    rootLayout->addLayout(titleBar);

    auto* headerCard = new QFrame(this);
    headerCard->setObjectName(QStringLiteral("updateCard"));
    auto* headerLayout = new QHBoxLayout(headerCard);
    headerLayout->setContentsMargins(14, 12, 14, 12);
    headerLayout->setSpacing(10);

    auto* headlineLabel = new QLabel(
        uiText("A new version of peekpass is available", "새로운 peekpass 버전이 출시되었습니다"),
        headerCard);
    headlineLabel->setObjectName(QStringLiteral("updateHeadline"));
    headlineLabel->setWordWrap(true);

    auto* badgeLabel = new QLabel(
        QStringLiteral("v%1 \u2192 v%2").arg(m_info.currentVersion, m_info.latestVersion),
        headerCard);
    badgeLabel->setObjectName(QStringLiteral("updateBadge"));
    badgeLabel->setAlignment(Qt::AlignCenter);

    headerLayout->addWidget(headlineLabel, 1);
    headerLayout->addWidget(badgeLabel, 0, Qt::AlignRight | Qt::AlignVCenter);
    rootLayout->addWidget(headerCard);

    auto* notesCard = new QFrame(this);
    notesCard->setObjectName(QStringLiteral("updateCard"));
    auto* notesLayout = new QVBoxLayout(notesCard);
    notesLayout->setContentsMargins(14, 12, 14, 12);
    notesLayout->setSpacing(8);

    auto* notesTitle = new QLabel(uiText("Release notes", "변경 사항"), notesCard);
    notesTitle->setObjectName(QStringLiteral("updateSectionTitle"));

    auto* notesEdit = new QPlainTextEdit(notesCard);
    notesEdit->setReadOnly(true);
    const QString notesText = m_info.releaseNotes.trimmed().isEmpty()
        ? uiText("Includes bug fixes and stability improvements.",
                 "버그 수정 및 안정성 개선 사항이 포함되어 있습니다.")
        : m_info.releaseNotes.trimmed();
    notesEdit->setPlainText(notesText);

    notesLayout->addWidget(notesTitle);
    notesLayout->addWidget(notesEdit, 1);
    rootLayout->addWidget(notesCard, 1);

    if (!UpdateChecker::isInstalledBuild()) {
        auto* portableNote = new QLabel(
            uiText("Portable mode detected: 'Update now' installs to the standard user folder (%LOCALAPPDATA%\\Programs\\peekpass). Use 'Release page' for the portable ZIP.",
                   "포터블 모드로 실행 중입니다. '지금 업데이트'를 누르면 표준 사용자 폴더(%LOCALAPPDATA%\\Programs\\peekpass)에 설치됩니다. 포터블 ZIP은 '릴리스 페이지'를 이용하세요."),
            this);
        portableNote->setObjectName(QStringLiteral("updatePortableNote"));
        portableNote->setWordWrap(true);
        rootLayout->addWidget(portableNote);
    }

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(false);
    m_progressBar->setVisible(false);
    rootLayout->addWidget(m_progressBar);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("updateStatusLabel"));
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setVisible(false);
    rootLayout->addWidget(m_statusLabel);

    auto* buttonLayout = new QHBoxLayout;
    buttonLayout->setContentsMargins(0, 2, 0, 0);
    buttonLayout->setSpacing(6);

    m_releasePageButton = new QPushButton(uiText("Release page", "릴리스 페이지"), this);
    m_releasePageButton->setCursor(Qt::PointingHandCursor);
    connect(m_releasePageButton, &QPushButton::clicked, this, [this] {
        const QUrl candidate(m_info.releaseUrl);
        const QUrl url = isTrustedReleasePageUrl(candidate)
            ? candidate
            : QUrl(QString::fromLatin1(DefaultReleasesPageUrl));
        if (url.isValid())
            QDesktopServices::openUrl(url);
    });

    m_skipButton = new QPushButton(uiText("Skip version", "이 버전 건너뛰기"), this);
    m_skipButton->setCursor(Qt::PointingHandCursor);
    connect(m_skipButton, &QPushButton::clicked, this, [this] {
        AppSettings::setSkippedUpdateVersion(m_info.latestVersion);
        reject();
    });

    m_laterButton = new QPushButton(uiText("Later", "나중에"), this);
    m_laterButton->setCursor(Qt::PointingHandCursor);
    connect(m_laterButton, &QPushButton::clicked, this, &QDialog::reject);

    m_updateButton = new QPushButton(uiText("Update now", "지금 업데이트"), this);
    m_updateButton->setObjectName(QStringLiteral("primaryUpdateButton"));
    m_updateButton->setCursor(Qt::PointingHandCursor);
    m_updateButton->setDefault(true);
    connect(m_updateButton, &QPushButton::clicked, this, &UpdateDialog::startInstallerDownload);

    buttonLayout->addWidget(m_releasePageButton);
    buttonLayout->addWidget(m_skipButton);
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(m_laterButton);
    buttonLayout->addWidget(m_updateButton);
    rootLayout->addLayout(buttonLayout);

    applyDialogStyle();
    updateWindowMask();
}

UpdateDialog::~UpdateDialog()
{
    cancelDownload();
}

void UpdateDialog::reject()
{
    cancelDownload();
    QDialog::reject();
}

void UpdateDialog::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
        return;
    }

    QDialog::mousePressEvent(event);
}

void UpdateDialog::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging && event->buttons().testFlag(Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragOffset);
        event->accept();
        return;
    }

    QDialog::mouseMoveEvent(event);
}

void UpdateDialog::mouseReleaseEvent(QMouseEvent* event)
{
    m_dragging = false;
    QDialog::mouseReleaseEvent(event);
}

void UpdateDialog::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    updateWindowMask();
}

void UpdateDialog::applyDialogStyle()
{
    setWindowOpacity(AppSettings::windowOpacity());
    setStyleSheet(updateDialogStyleSheet());
}

void UpdateDialog::updateWindowMask()
{
    if (!AppSettings::roundedCorners()) {
        clearMask();
        return;
    }

    QPainterPath path;
    path.addRoundedRect(rect(), 12, 12);
    setMask(QRegion(path.toFillPolygon().toPolygon()));
}

void UpdateDialog::startInstallerDownload()
{
    if (m_downloadReply || m_checksumReply)
        return;

    const auto openFallbackReleasePage = [this] {
        const QUrl candidate(m_info.releaseUrl);
        const QUrl fallbackUrl = isTrustedReleasePageUrl(candidate)
            ? candidate
            : QUrl(QString::fromLatin1(DefaultReleasesPageUrl));
        if (fallbackUrl.isValid())
            QDesktopServices::openUrl(fallbackUrl);
        accept();
    };

#ifndef Q_OS_WIN
    openFallbackReleasePage();
    return;
#endif

    const QUrl installerUrl(m_info.installerUrl);
    const QUrl checksumUrl(m_info.checksumsUrl);
    if (!isTrustedDownloadUrl(installerUrl) || !isTrustedDownloadUrl(checksumUrl)) {
        openFallbackReleasePage();
        return;
    }

    QString fileName = QFileInfo(m_info.installerName).fileName();
    if (fileName.isEmpty() || !fileName.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive))
        fileName = QStringLiteral("peekpass-%1-win64-setup.exe").arg(m_info.latestVersion);

    const QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QDir().mkpath(tempDir);
    m_finalInstallerPath = QDir(tempDir).filePath(fileName);
    m_partFilePath = m_finalInstallerPath + QStringLiteral(".part");

    QFile::remove(m_partFilePath);
    m_partFile = new QFile(m_partFilePath, this);
    if (!m_partFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        delete m_partFile;
        m_partFile = nullptr;
        m_statusLabel->setVisible(true);
        m_statusLabel->setText(
            uiText("Could not create temporary installer file.",
                   "임시 설치 파일을 생성하지 못했습니다."));
        return;
    }

    m_updateButton->setEnabled(false);
    m_skipButton->setEnabled(false);
    m_laterButton->setText(uiText("Cancel", "취소"));
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_statusLabel->setVisible(true);
    m_statusLabel->setText(
        uiText("Downloading update installer...",
               "업데이트 설치 파일을 다운로드하는 중입니다..."));

    QNetworkRequest request(installerUrl);
    request.setHeader(
        QNetworkRequest::UserAgentHeader,
        QStringLiteral("peekpass/%1").arg(QCoreApplication::applicationVersion()));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(60000);

    m_downloadReply = m_network->get(request);
    QNetworkReply* reply = m_downloadReply.data();

    connect(reply, &QIODevice::readyRead, this, [this, reply] {
        if (m_partFile && m_partFile->isOpen())
            m_partFile->write(reply->readAll());
    });

    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        const qint64 effectiveTotal = total > 0 ? total : m_info.installerSize;
        if (effectiveTotal > 0) {
            const int percent = qBound(0, static_cast<int>((received * 100) / effectiveTotal), 100);
            m_progressBar->setRange(0, 100);
            m_progressBar->setValue(percent);
            m_statusLabel->setText(
                uiText("Downloading update... %1% (%2 / %3)",
                       "업데이트 다운로드 중... %1% (%2 / %3)")
                    .arg(percent)
                    .arg(formatMegabytes(received), formatMegabytes(effectiveTotal)));
        } else {
            m_progressBar->setRange(0, 0);
            m_statusLabel->setText(
                uiText("Downloading update... %1",
                       "업데이트 다운로드 중... %1")
                    .arg(formatMegabytes(received)));
        }
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply, checksumUrl] {
        if (m_partFile && m_partFile->isOpen()) {
            m_partFile->write(reply->readAll());
            m_partFile->flush();
            m_partFile->close();
        }

        const QNetworkReply::NetworkError netError = reply->error();
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QString errorText = reply->errorString();
        const QUrl finalUrl = reply->url();
        reply->deleteLater();
        m_downloadReply.clear();

        if (netError == QNetworkReply::OperationCanceledError) {
            cancelDownload();
            return;
        }

        if (netError != QNetworkReply::NoError || (httpStatus != 200 && httpStatus != 0)
            || !isTrustedDownloadUrl(finalUrl)) {
            cancelDownload();
            m_updateButton->setEnabled(true);
            m_skipButton->setEnabled(true);
            m_laterButton->setText(uiText("Later", "나중에"));
            m_progressBar->setVisible(false);
            m_statusLabel->setVisible(true);
            m_statusLabel->setText(
                !isTrustedDownloadUrl(finalUrl)
                    ? uiText("Download redirected to an untrusted host.",
                             "다운로드가 신뢰할 수 없는 호스트로 리디렉션되었습니다.")
                    : uiText("Download failed: %1", "다운로드에 실패했습니다: %1").arg(errorText));
            return;
        }

        m_statusLabel->setText(
            uiText("Verifying installer checksum (SHA-256)...",
                   "설치 파일 무결성(SHA-256)을 검증하는 중입니다..."));
        QNetworkRequest checksumRequest(checksumUrl);
        checksumRequest.setHeader(
            QNetworkRequest::UserAgentHeader,
            QStringLiteral("peekpass/%1").arg(QCoreApplication::applicationVersion()));
        checksumRequest.setAttribute(
            QNetworkRequest::RedirectPolicyAttribute,
            QNetworkRequest::NoLessSafeRedirectPolicy);
        checksumRequest.setTransferTimeout(15000);

        m_checksumReply = m_network->get(checksumRequest);
        QNetworkReply* hashReply = m_checksumReply.data();
        connect(hashReply, &QNetworkReply::finished, this, [this, hashReply] {
            const QByteArray payload = hashReply->readAll();
            const QNetworkReply::NetworkError hashError = hashReply->error();
            const int hashHttpStatus = hashReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QUrl hashFinalUrl = hashReply->url();
            hashReply->deleteLater();
            m_checksumReply.clear();

            if (hashError == QNetworkReply::OperationCanceledError) {
                cancelDownload();
                return;
            }

            if (hashError != QNetworkReply::NoError || (hashHttpStatus != 200 && hashHttpStatus != 0)
                || !isTrustedDownloadUrl(hashFinalUrl)) {
                cancelDownload();
                m_updateButton->setEnabled(true);
                m_skipButton->setEnabled(true);
                m_laterButton->setText(uiText("Later", "나중에"));
                m_progressBar->setVisible(false);
                m_statusLabel->setVisible(true);
                m_statusLabel->setText(
                    uiText("Could not download release checksums for verification.",
                           "무결성 검증용 체크섬 파일을 다운로드하지 못했습니다."));
                return;
            }

            const QString expectedHash = UpdateChecker::parseSha256SumForFile(
                payload,
                QFileInfo(m_finalInstallerPath).fileName());
            if (expectedHash.isEmpty()) {
                cancelDownload();
                m_updateButton->setEnabled(true);
                m_skipButton->setEnabled(true);
                m_laterButton->setText(uiText("Later", "나중에"));
                m_progressBar->setVisible(false);
                m_statusLabel->setVisible(true);
                m_statusLabel->setText(
                    uiText("Installer entry was not found in SHA256SUMS.txt.",
                           "SHA256SUMS.txt에서 설치 파일 해시를 찾지 못했습니다."));
                return;
            }

            verifyAndLaunchInstaller(expectedHash);
        });
    });
}

void UpdateDialog::verifyAndLaunchInstaller(const QString& expectedSha256)
{
    QFile verifyFile(m_partFilePath);
    const qint64 fileSize = verifyFile.size();
    QByteArray header;
    if (verifyFile.open(QIODevice::ReadOnly)) {
        header = verifyFile.read(2);
        verifyFile.close();
    }

    if (header != QByteArrayLiteral("MZ")
        || fileSize < 4096
        || (m_info.installerSize > 0 && fileSize != m_info.installerSize)) {
        cancelDownload();
        m_updateButton->setEnabled(true);
        m_skipButton->setEnabled(true);
        m_laterButton->setText(uiText("Later", "나중에"));
        m_progressBar->setVisible(false);
        m_statusLabel->setVisible(true);
        m_statusLabel->setText(
            uiText("Downloaded installer file could not be verified.",
                   "다운로드된 설치 파일을 검증하지 못했습니다."));
        return;
    }

    if (expectedSha256.isEmpty()) {
        cancelDownload();
        m_updateButton->setEnabled(true);
        m_skipButton->setEnabled(true);
        m_laterButton->setText(uiText("Later", "나중에"));
        m_progressBar->setVisible(false);
        m_statusLabel->setVisible(true);
        m_statusLabel->setText(
            uiText("SHA-256 verification failed. The downloaded file may be corrupted.",
                   "SHA-256 무결성 검증에 실패했습니다. 다운로드된 파일이 손상되었을 수 있습니다."));
        return;
    }

    const QString actualSha256 = computeFileSha256Hex(m_partFilePath);
    if (actualSha256.isEmpty() || actualSha256.compare(expectedSha256, Qt::CaseInsensitive) != 0) {
        cancelDownload();
        m_updateButton->setEnabled(true);
        m_skipButton->setEnabled(true);
        m_laterButton->setText(uiText("Later", "나중에"));
        m_progressBar->setVisible(false);
        m_statusLabel->setVisible(true);
        m_statusLabel->setText(
            uiText("SHA-256 verification failed. The downloaded file may be corrupted.",
                   "SHA-256 무결성 검증에 실패했습니다. 다운로드된 파일이 손상되었을 수 있습니다."));
        return;
    }

    if (m_partFile) {
        delete m_partFile;
        m_partFile = nullptr;
    }

    QFile::remove(m_finalInstallerPath);
    if (!QFile::rename(m_partFilePath, m_finalInstallerPath)) {
        cancelDownload();
        m_updateButton->setEnabled(true);
        m_skipButton->setEnabled(true);
        m_laterButton->setText(uiText("Later", "나중에"));
        m_progressBar->setVisible(false);
        m_statusLabel->setVisible(true);
        m_statusLabel->setText(
            uiText("Could not prepare the installer executable.",
                   "설치 실행 파일을 준비하지 못했습니다."));
        return;
    }

    AppSettings::setSkippedUpdateVersion({});
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(100);
    m_statusLabel->setText(
        uiText("Launching installer and restarting peekpass...",
               "설치 프로그램을 실행하고 peekpass를 다시 시작합니다..."));

    const bool started = QProcess::startDetached(
        m_finalInstallerPath,
        {QStringLiteral("/SILENT"), QStringLiteral("/CLOSEAPPLICATIONS")});

    if (!started) {
        m_updateButton->setEnabled(true);
        m_skipButton->setEnabled(true);
        m_laterButton->setText(uiText("Later", "나중에"));
        m_statusLabel->setText(
            uiText("Could not start the installer automatically.",
                   "설치 프로그램을 자동으로 실행하지 못했습니다."));
        return;
    }

    accept();
    QTimer::singleShot(100, qApp, [] {
        QApplication::closeAllWindows();
        QCoreApplication::quit();
    });
}

void UpdateDialog::cancelDownload()
{
    if (m_downloadReply) {
        QNetworkReply* reply = m_downloadReply.data();
        m_downloadReply.clear();
        reply->abort();
        reply->deleteLater();
    }

    if (m_checksumReply) {
        QNetworkReply* reply = m_checksumReply.data();
        m_checksumReply.clear();
        reply->abort();
        reply->deleteLater();
    }

    if (m_partFile) {
        if (m_partFile->isOpen())
            m_partFile->close();
        delete m_partFile;
        m_partFile = nullptr;
    }

    if (!m_partFilePath.isEmpty()) {
        QFile::remove(m_partFilePath);
        m_partFilePath.clear();
    }
}

UpdateChecker& UpdateChecker::instance()
{
    static QPointer<UpdateChecker> s_instance;
    if (!s_instance)
        s_instance = new UpdateChecker(qApp);
    return *s_instance;
}

UpdateChecker::UpdateChecker(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

bool UpdateChecker::isInstalledBuild()
{
#ifdef Q_OS_WIN
    const QDir appDir(QCoreApplication::applicationDirPath());
    return QFileInfo::exists(appDir.filePath(QStringLiteral("unins000.exe")));
#else
    return false;
#endif
}

void UpdateChecker::cleanupOldInstallers()
{
    const QString tempDirPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (tempDirPath.isEmpty())
        return;

    const QDir tempDir(tempDirPath);
    const QFileInfoList candidates = tempDir.entryInfoList(
        {QStringLiteral("peekpass-*-setup.exe"), QStringLiteral("peekpass-*-setup.exe.part")},
        QDir::Files);
    const QDateTime now = QDateTime::currentDateTimeUtc();
    for (const QFileInfo& entry : candidates) {
        const qint64 ageSeconds = entry.lastModified().toUTC().secsTo(now);
        if (ageSeconds >= OldTempInstallerAgeSeconds)
            QFile::remove(entry.absoluteFilePath());
    }
}

QString UpdateChecker::parseSha256SumForFile(const QByteArray& checksumsText, const QString& fileName)
{
    const QString targetName = QFileInfo(fileName.trimmed()).fileName();
    if (targetName.isEmpty())
        return {};

    static const QRegularExpression lineRegex(
        QStringLiteral("^\\s*([0-9a-fA-F]{64})\\s+\\*?(.+?)\\s*$"));
    const QStringList lines = QString::fromUtf8(checksumsText).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString& rawLine : lines) {
        const QRegularExpressionMatch match = lineRegex.match(rawLine.trimmed());
        if (!match.hasMatch())
            continue;

        const QString digest = match.captured(1).toLower();
        const QString entryName = QFileInfo(match.captured(2).trimmed()).fileName();
        if (entryName.compare(targetName, Qt::CaseInsensitive) == 0)
            return digest;
    }
    return {};
}

UpdateReleaseInfo UpdateChecker::parseLatestReleaseJson(
    const QByteArray& jsonPayload,
    const QString& currentVersion,
    QString* errorMessage)
{
    UpdateReleaseInfo info;
    info.currentVersion = normalizeVersionString(currentVersion);
    info.releaseUrl = QString::fromLatin1(DefaultReleasesPageUrl);

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(jsonPayload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorMessage) {
            *errorMessage = uiText(
                "Could not parse release information from GitHub.",
                "GitHub에서 릴리스 정보를 해석하지 못했습니다.");
        }
        return info;
    }

    const QJsonObject root = doc.object();
    info.tagName = root.value(QStringLiteral("tag_name")).toString().trimmed();
    info.latestVersion = normalizeVersionString(info.tagName);

    const QString htmlUrl = root.value(QStringLiteral("html_url")).toString().trimmed();
    if (!htmlUrl.isEmpty() && isTrustedReleasePageUrl(QUrl(htmlUrl)))
        info.releaseUrl = htmlUrl;

    info.releaseNotes = root.value(QStringLiteral("body")).toString().trimmed();

    bool foundCanonicalChecksums = false;
    const QJsonArray assets = root.value(QStringLiteral("assets")).toArray();
    for (const QJsonValue& assetVal : assets) {
        const QJsonObject asset = assetVal.toObject();
        const QString name = asset.value(QStringLiteral("name")).toString().trimmed();
        const QString downloadUrl = asset.value(QStringLiteral("browser_download_url")).toString().trimmed();
        const qint64 size = asset.value(QStringLiteral("size")).toInteger(0);

        if (name.isEmpty() || downloadUrl.isEmpty())
            continue;

        if (name.endsWith(QStringLiteral("-setup.exe"), Qt::CaseInsensitive)
            && isTrustedDownloadUrl(QUrl(downloadUrl))) {
            if (info.installerUrl.isEmpty() || name.contains(QStringLiteral("win64"), Qt::CaseInsensitive)) {
                info.installerName = name;
                info.installerUrl = downloadUrl;
                info.installerSize = size;
            }
        } else if (name.compare(QStringLiteral("SHA256SUMS.txt"), Qt::CaseInsensitive) == 0
                   && isTrustedDownloadUrl(QUrl(downloadUrl))) {
            info.checksumsUrl = downloadUrl;
            foundCanonicalChecksums = true;
        } else if (!foundCanonicalChecksums
                   && name.endsWith(QStringLiteral(".sha256"), Qt::CaseInsensitive)
                   && isTrustedDownloadUrl(QUrl(downloadUrl))) {
            if (info.checksumsUrl.isEmpty() || name.contains(QStringLiteral("setup"), Qt::CaseInsensitive))
                info.checksumsUrl = downloadUrl;
        } else if (name.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive)
                   && isTrustedDownloadUrl(QUrl(downloadUrl))) {
            info.portableZipUrl = downloadUrl;
        }
    }

    const QVersionNumber currentVer = QVersionNumber::fromString(info.currentVersion);
    const QVersionNumber latestVer = QVersionNumber::fromString(info.latestVersion);
    if (currentVer.isNull() || latestVer.isNull()) {
        if (errorMessage) {
            *errorMessage = uiText(
                "Received an invalid version number from the release server.",
                "릴리스 서버에서 올바르지 않은 버전 번호를 받았습니다.");
        }
        return info;
    }

    info.updateAvailable = QVersionNumber::compare(latestVer, currentVer) > 0;
    return info;
}

void UpdateChecker::checkForUpdates(bool interactive, QWidget* parentWidget, QSystemTrayIcon* trayIcon)
{
    if (m_activeReply) {
        if (interactive) {
            m_activeInteractive = true;
            if (parentWidget)
                m_activeParent = parentWidget;
            if (trayIcon)
                m_activeTray = trayIcon;
            if (m_activeDialog) {
                m_activeDialog->show();
                m_activeDialog->raise();
                m_activeDialog->activateWindow();
            }
        }
        return;
    }

    const QString envApiUrl = qEnvironmentVariable("PEEKPASS_UPDATE_API_URL").trimmed();
    const QUrl apiUrl(envApiUrl.isEmpty() ? QString::fromLatin1(DefaultLatestReleaseApiUrl) : envApiUrl);

    QNetworkRequest request(apiUrl);
    request.setHeader(
        QNetworkRequest::UserAgentHeader,
        QStringLiteral("peekpass/%1").arg(QCoreApplication::applicationVersion()));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(15000);

    m_activeInteractive = interactive;
    m_activeParent = parentWidget;
    m_activeTray = trayIcon;

    m_activeReply = m_network->get(request);
    QNetworkReply* reply = m_activeReply.data();

    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        const QByteArray payload = reply->readAll();
        const QNetworkReply::NetworkError netError = reply->error();
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QString netErrorText = reply->errorString();
        reply->deleteLater();
        m_activeReply.clear();

        const bool interactiveCheck = m_activeInteractive;
        const QPointer<QWidget> safeParent = m_activeParent;
        const QPointer<QSystemTrayIcon> safeTray = m_activeTray;
        m_activeInteractive = false;
        m_activeParent.clear();
        m_activeTray.clear();

        QWidget* dialogParent = (safeParent && safeParent->isVisible()) ? safeParent.data() : nullptr;

        if (netError != QNetworkReply::NoError || (httpStatus != 200 && httpStatus != 0)) {
            if (interactiveCheck) {
                QMessageBox::warning(
                    dialogParent,
                    uiText("Check for updates", "업데이트 확인"),
                    uiText("Could not check for updates.\n\n%1",
                           "업데이트를 확인하지 못했습니다.\n\n%1")
                        .arg(netErrorText));
            }
            return;
        }

        QString parseError;
        const UpdateReleaseInfo info = parseLatestReleaseJson(
            payload,
            QCoreApplication::applicationVersion(),
            &parseError);

        if (!parseError.isEmpty()) {
            if (interactiveCheck) {
                QMessageBox::warning(
                    dialogParent,
                    uiText("Check for updates", "업데이트 확인"),
                    parseError);
            }
            return;
        }

        AppSettings::setLastUpdateCheckEpochSeconds(QDateTime::currentSecsSinceEpoch());

        if (!info.updateAvailable) {
            if (interactiveCheck) {
                QMessageBox::information(
                    dialogParent,
                    uiText("Check for updates", "업데이트 확인"),
                    uiText("You are using the latest version (v%1).",
                           "현재 최신 버전(v%1)을 사용 중입니다.")
                        .arg(info.currentVersion));
            }
            return;
        }

        if (!interactiveCheck && info.latestVersion == AppSettings::skippedUpdateVersion())
            return;

        m_lastAvailableInfo = info;
        if (!interactiveCheck && safeTray && safeTray->isVisible()) {
            safeTray->showMessage(
                QStringLiteral("peekpass"),
                uiText("A new update (v%1) is available.",
                       "새 업데이트(v%1)가 출시되었습니다.")
                    .arg(info.latestVersion),
                QSystemTrayIcon::Information,
                5000);
        }

        showUpdateDialog(info, dialogParent);
    });
}

void UpdateChecker::checkOnStartupIfDue(QWidget* parentWidget, QSystemTrayIcon* trayIcon)
{
    if (!AppSettings::autoCheckUpdatesEnabled())
        return;

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const qint64 lastCheck = AppSettings::lastUpdateCheckEpochSeconds();
    if (lastCheck > 0 && (now - lastCheck) < AutoCheckIntervalSeconds)
        return;

    checkForUpdates(false, parentWidget, trayIcon);
}

void UpdateChecker::showUpdateDialog(const UpdateReleaseInfo& info, QWidget* parentWidget)
{
    if (m_activeDialog) {
        m_activeDialog->show();
        m_activeDialog->raise();
        m_activeDialog->activateWindow();
        return;
    }

    auto* dialog = new UpdateDialog(info, nullptr);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    if (parentWidget && parentWidget->isVisible()) {
        const QRect parentGeo = parentWidget->frameGeometry();
        dialog->move(
            parentGeo.center().x() - dialog->width() / 2,
            parentGeo.center().y() - dialog->height() / 2);
    } else if (QScreen* screen = QGuiApplication::primaryScreen()) {
        const QRect avail = screen->availableGeometry();
        dialog->move(
            avail.center().x() - dialog->width() / 2,
            avail.center().y() - dialog->height() / 2);
    }
    m_activeDialog = dialog;
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}
