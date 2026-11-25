#include "QuickWindow.h"

#include "AppSettings.h"
#include "BwClient.h"
#include "I18n.h"
#include "ItemDetailDialog.h"
#include "SettingsDialog.h"
#include "TotpGenerator.h"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QCryptographicHash>
#include <QCursor>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QGuiApplication>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMessageAuthenticationCode>
#include <QMimeData>
#include <QMouseEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPixmap>
#include <QPushButton>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollArea>
#include <QShortcut>
#include <QSizePolicy>
#include <QSignalBlocker>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QtGlobal>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <utility>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace
{
constexpr int ItemIdRole = Qt::UserRole;
constexpr int UsernameRole = Qt::UserRole + 1;
constexpr int FaviconHostRole = Qt::UserRole + 2;
constexpr int MaxRecentItems = 30;

QString uiText(const char* ko, const char* en)
{
    return I18n::translate("QuickWindow", en, ko);
}

bool isLightTheme()
{
    return AppSettings::theme() == QStringLiteral("light");
}

QIcon makeSearchIcon()
{
    QPixmap pixmap(20, 20);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    const QColor strokeColor = isLightTheme() ? QColor("#57606a") : QColor("#aeb4bf");
    QPen pen(strokeColor, 1.75, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QRectF(3.5, 3.5, 9.5, 9.5));
    painter.drawLine(QPointF(11.8, 11.8), QPointF(16.2, 16.2));

    return QIcon(pixmap);
}

QString quickWindowStyle(int radius)
{
    QString style = QStringLiteral(R"(
        #quickWindow {
            background: #202124;
            border: 1px solid #0b0c0f;
            border-radius: %1px;
            color: #e8eaed;
        }
        #searchEdit {
            background: #111317;
            color: #f1f3f4;
            border: 2px solid #1f8cff;
            border-radius: 9px;
            font-size: 15px;
            font-weight: 600;
            padding: 9px 10px;
            min-height: 26px;
            selection-background-color: #1f8cff;
            selection-color: #ffffff;
            placeholder-text-color: #aeb4bf;
        }
        #resultList {
            background: #15171a;
            color: #f1f3f4;
            border: 1px solid #3a3f46;
            border-radius: 8px;
            font-size: 14px;
            outline: 0;
            padding: 2px;
            selection-background-color: #1f8cff;
            selection-color: #ffffff;
        }
        #resultList::item {
            color: #f1f3f4;
            min-height: 34px;
            padding: 5px 9px;
            border-radius: 5px;
        }
        #resultList::item:selected {
            background: #1f8cff;
            color: #ffffff;
            border-radius: 5px;
        }
        #busyBar {
            background: rgba(255, 255, 255, 35);
            border: 0;
            border-radius: 1px;
        }
        #busyBar::chunk {
            background: #49a0ff;
            border-radius: 1px;
        }
        #authPanel {
            background: transparent;
        }
        #authTitle {
            color: #f1f3f4;
            font-size: 13px;
            font-weight: 700;
        }
        #authNote {
            color: #c5cbd3;
            font-size: 12px;
        }
        #authFieldLabel {
            color: #e8eaed;
            font-size: 12px;
            font-weight: 600;
        }
        #authAccountText {
            color: #f1f3f4;
            font-size: 13px;
            font-weight: 700;
            padding: 4px 0;
        }
        #authHintBar {
            background: transparent;
        }
        #authBusyLabel {
            color: #f0c7d2;
            font-size: 12px;
        }
        #authPanel QLineEdit, #authPanel QComboBox {
            background: #111317;
            color: #f1f3f4;
            border: 1px solid #3a3f46;
            border-radius: 6px;
            padding: 6px 8px;
            selection-background-color: #1f8cff;
            selection-color: #ffffff;
            min-height: 24px;
        }
        #authPanel QComboBox {
            padding-right: 26px;
        }
        #authPanel QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 22px;
            border-left: 1px solid #3a3f46;
            border-top-right-radius: 6px;
            border-bottom-right-radius: 6px;
            background: transparent;
        }
        #authPanel QComboBox::down-arrow {
            image: url(:/icons/chevron-down-dark.png);
            width: 12px;
            height: 8px;
        }
        #authPanel QComboBox QAbstractItemView {
            background: #15171a;
            color: #f1f3f4;
            border: 1px solid #3a3f46;
            selection-background-color: #1f8cff;
            selection-color: #ffffff;
            outline: 0;
        }
        #authPanel QLineEdit:focus, #authPanel QComboBox:focus {
            border: 1px solid #1f8cff;
        }
        #authPanel QPushButton {
            background: #2f343a;
            color: #f1f3f4;
            border: 1px solid #4b5563;
            border-radius: 6px;
            padding: 6px 12px;
        }
        #authPanel QPushButton:hover {
            background: #3a4048;
        }
        #authPanel QPushButton#authSubmitButton {
            background: #1f72c7;
            color: #ffffff;
            border-color: #49a0ff;
            font-weight: 700;
        }
        #authPanel QPushButton#authSubmitButton:hover {
            background: #2484e7;
        }
        #statusLabel {
            color: #f0c7d2;
            font-size: 12px;
            min-height: 16px;
        }
        #footerBar {
            background: transparent;
            min-height: 28px;
        }
        #footerText {
            color: #e8eaed;
            font-size: 12px;
            font-weight: 600;
            padding-left: 2px;
            padding-right: 12px;
        }
        #footerShortcut {
            color: #e8eaed;
            font-size: 12px;
            padding: 0;
            margin: 0;
        }
        #keycapLabel {
            background: #2f343a;
            border: 1px solid #4b5563;
            border-radius: 5px;
            color: #e8eaed;
            font-size: 11px;
            font-weight: 700;
            min-height: 19px;
            padding: 1px 7px;
        }
    )")
                        .arg(radius);

    if (!isLightTheme())
        return style;

    style.replace(QStringLiteral(":/icons/chevron-down-dark.png"), QStringLiteral(":/icons/chevron-down-light.png"));
    style.replace(QStringLiteral("rgba(255, 255, 255, 35)"), QStringLiteral("rgba(0, 0, 0, 35)"));
    style.replace(QStringLiteral("#202124"), QStringLiteral("#f8f9fa"));
    style.replace(QStringLiteral("#0b0c0f"), QStringLiteral("#d0d7de"));
    style.replace(QStringLiteral("#111317"), QStringLiteral("#ffffff"));
    style.replace(QStringLiteral("#15171a"), QStringLiteral("#ffffff"));
    style.replace(QStringLiteral("#1f8cff"), QStringLiteral("#0969da"));
    style.replace(QStringLiteral("#49a0ff"), QStringLiteral("#218bff"));
    style.replace(QStringLiteral("#1f72c7"), QStringLiteral("#0969da"));
    style.replace(QStringLiteral("#2484e7"), QStringLiteral("#0757b8"));
    style.replace(QStringLiteral("#2f343a"), QStringLiteral("#f3f4f6"));
    style.replace(QStringLiteral("#3a4048"), QStringLiteral("#eaeef2"));
    style.replace(QStringLiteral("#3a3f46"), QStringLiteral("#d0d7de"));
    style.replace(QStringLiteral("#4b5563"), QStringLiteral("#d0d7de"));
    style.replace(QStringLiteral("#f1f3f4"), QStringLiteral("#24292f"));
    style.replace(QStringLiteral("#e8eaed"), QStringLiteral("#24292f"));
    style.replace(QStringLiteral("#c5cbd3"), QStringLiteral("#57606a"));
    style.replace(QStringLiteral("#aeb4bf"), QStringLiteral("#57606a"));
    style.replace(QStringLiteral("#f0c7d2"), QStringLiteral("#b42318"));
    return style;
}

QString menuStyle()
{
    if (isLightTheme()) {
        return QStringLiteral(R"(
            QMenu {
                background: #ffffff;
                color: #24292f;
                border: 1px solid #d0d7de;
                padding: 4px;
            }
            QMenu::item {
                padding: 7px 28px 7px 12px;
                border-radius: 4px;
            }
            QMenu::item:selected {
                background: #0969da;
                color: #ffffff;
            }
            QMenu::separator {
                height: 1px;
                background: #d0d7de;
                margin: 4px 6px;
            }
        )");
    }

    return QStringLiteral(R"(
        QMenu {
            background: #15171a;
            color: #f1f3f4;
            border: 1px solid #3a3f46;
            padding: 4px;
        }
        QMenu::item {
            padding: 7px 28px 7px 12px;
            border-radius: 4px;
        }
        QMenu::item:selected {
            background: #1f8cff;
            color: #ffffff;
        }
        QMenu::separator {
            height: 1px;
            background: #3a3f46;
            margin: 4px 6px;
        }
    )");
}

QString stripTerminalControls(const QString& value)
{
    QString output;
    output.reserve(value.size());

    for (qsizetype index = 0; index < value.size(); ++index) {
        const QChar ch = value.at(index);

        if (ch.unicode() == 0x1b && index + 1 < value.size() && value.at(index + 1) == QLatin1Char('[')) {
            index += 2;
            while (index < value.size()) {
                const ushort code = value.at(index).unicode();
                if (code >= 0x40 && code <= 0x7e)
                    break;
                ++index;
            }
            continue;
        }

        if (ch.unicode() == 0x9b) {
            while (index < value.size()) {
                const ushort code = value.at(index).unicode();
                if (code >= 0x40 && code <= 0x7e)
                    break;
                ++index;
            }
            continue;
        }

        if ((ch.unicode() == 0xfffd || ch.unicode() == 0x25a1) && index + 1 < value.size()
            && value.at(index + 1) == QLatin1Char('[')) {
            ++index;
            while (index < value.size()) {
                const ushort code = value.at(index).unicode();
                if (code >= 0x40 && code <= 0x7e)
                    break;
                ++index;
            }
            continue;
        }

        if (ch == QLatin1Char('\r')) {
            output.append(QLatin1Char('\n'));
            continue;
        }

        if (ch.unicode() < 0x20 && ch != QLatin1Char('\n') && ch != QLatin1Char('\t'))
            continue;

        output.append(ch);
    }

    return output;
}

QString cleanStatusText(const QString& message)
{
    QString text = stripTerminalControls(message);
    text.remove(QRegularExpression(
        QStringLiteral("\\?\\s*Master password:\\s*\\[(?:input is hidden|hidden)\\]\\s*"),
        QRegularExpression::CaseInsensitiveOption));
    text.replace(QRegularExpression(QStringLiteral("[ \\t]*\\n[ \\t]*")), QStringLiteral(" "));
    text.replace(QRegularExpression(QStringLiteral("\\s{2,}")), QStringLiteral(" "));
    text = text.trimmed();

    return text;
}

QString compactStatusText(const QString& message)
{
    QString text = cleanStatusText(message);

    constexpr qsizetype MaxStatusLength = 180;
    if (text.size() > MaxStatusLength)
        text = text.left(MaxStatusLength - 3).trimmed() + QStringLiteral("...");

    return text;
}

QString defaultServerUrlForMode(const QString& mode)
{
    if (mode == "eu")
        return "https://vault.bitwarden.eu";
    if (mode == "custom")
        return AppSettings::customBitwardenServerUrl();

    return "https://vault.bitwarden.com";
}

QString displayName(const VaultItem& item)
{
    const QString name = item.name.isEmpty() ? uiText("(이름 없는 항목)", "(unnamed item)") : item.name;
    QStringList fields;

    if (AppSettings::showResultUsername() && !item.username.isEmpty())
        fields.push_back(item.username);

    if (AppSettings::showResultUri() && !item.uris.isEmpty()) {
        QUrl url(item.uris.constFirst());
        QString host = url.host();
        if (host.isEmpty())
            host = item.uris.constFirst();
        fields.push_back(host);
    }

    if (AppSettings::showResultType() && !item.type.isEmpty())
        fields.push_back(item.type);

    if (fields.isEmpty())
        return name;

    return QString("%1 - %2").arg(name, fields.join(QStringLiteral(" · ")));
}

QUrl normalizedItemUrl(const QString& value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty())
        return {};

    static const QRegularExpression schemeRegex(QStringLiteral("^[a-zA-Z][a-zA-Z0-9+.-]*:"));
    if (schemeRegex.match(trimmed).hasMatch())
        return QUrl(trimmed);

    return QUrl(QStringLiteral("https://") + trimmed);
}

QUrl webUrlForItemUri(const QString& value)
{
    const QUrl url = normalizedItemUrl(value);
    const QString scheme = url.scheme().toLower();
    if ((scheme != QStringLiteral("http") && scheme != QStringLiteral("https")) || url.host().isEmpty())
        return {};

    return url;
}

QString faviconHostForItem(const VaultItem& item)
{
    for (const QString& uri : item.uris) {
        const QUrl url = webUrlForItemUri(uri);
        const QString host = url.host().trimmed().toLower();
        if (!host.isEmpty())
            return host;
    }

    return {};
}

QUrl faviconUrlForItem(const VaultItem& item)
{
    for (const QString& uri : item.uris) {
        const QUrl sourceUrl = webUrlForItemUri(uri);
        const QString host = sourceUrl.host().trimmed().toLower();
        if (host.isEmpty())
            continue;

        QUrl faviconUrl;
        faviconUrl.setScheme(QStringLiteral("https"));
        faviconUrl.setHost(host);
        faviconUrl.setPath(QStringLiteral("/favicon.ico"));
        return faviconUrl;
    }

    return {};
}

QIcon makeResultIcon(const VaultItem& item)
{
    QPixmap pixmap(22, 22);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF box(2.0, 2.0, 18.0, 18.0);
    QColor background("#315b9f");
    if (item.typeCode == 2)
        background = QColor("#6b6075");
    else if (item.typeCode == 3)
        background = QColor("#3d7d73");
    else if (item.typeCode == 4)
        background = QColor("#5f6f8b");
    else if (item.typeCode == 5)
        background = QColor("#8a5b3a");

    painter.setPen(Qt::NoPen);
    painter.setBrush(background);
    painter.drawRoundedRect(box, 5, 5);

    QPen pen(QColor("#ffffff"), 1.45, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    if (item.typeCode == 2) {
        painter.drawRoundedRect(QRectF(7, 5.5, 8, 11), 1.5, 1.5);
        painter.drawLine(QPointF(9, 9), QPointF(13, 9));
        painter.drawLine(QPointF(9, 12), QPointF(13, 12));
    } else if (item.typeCode == 3) {
        painter.drawRoundedRect(QRectF(5.5, 7, 11, 8), 1.5, 1.5);
        painter.drawLine(QPointF(5.8, 10), QPointF(16.2, 10));
    } else if (item.typeCode == 4) {
        painter.drawEllipse(QRectF(8, 5, 6, 6));
        painter.drawArc(QRectF(5.5, 10, 11, 8), 25 * 16, 130 * 16);
    } else if (item.typeCode == 5) {
        painter.drawEllipse(QRectF(5.5, 7, 5, 5));
        painter.drawLine(QPointF(10.5, 9.5), QPointF(16.5, 9.5));
        painter.drawLine(QPointF(14.5, 9.5), QPointF(14.5, 12));
        painter.drawLine(QPointF(16.5, 9.5), QPointF(16.5, 11.5));
    } else {
        painter.drawEllipse(QRectF(5, 5, 12, 12));
        painter.drawLine(QPointF(5, 11), QPointF(17, 11));
        painter.drawArc(QRectF(8, 5, 6, 12), 90 * 16, 180 * 16);
        painter.drawArc(QRectF(8, 5, 6, 12), -90 * 16, 180 * 16);
    }

    if (!item.totp.isEmpty()) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#5bd6ff"));
        painter.drawEllipse(QRectF(14.2, 14.2, 5.2, 5.2));
    }

    return QIcon(pixmap);
}

QString shortcutMarkup(const QString& shortcut, const QString& label, const QString& labelColor = QString())
{
    const QString shortcutColor = isLightTheme() ? QStringLiteral("#57606a") : QStringLiteral("#aeb4bf");
    const QString effectiveLabelColor = labelColor.isEmpty()
        ? (isLightTheme() ? QStringLiteral("#24292f") : QStringLiteral("#e8eaed"))
        : labelColor;

    return QStringLiteral("<span style=\"color:#aeb4bf;font-weight:800;\">%1</span><span style=\"color:%3;font-weight:650;\"> %2</span>")
        .arg(shortcut.toHtmlEscaped(), label.toHtmlEscaped(), effectiveLabelColor)
        .replace(QStringLiteral("#aeb4bf"), shortcutColor);
}

QString itemSearchText(const VaultItem& item)
{
    QStringList values;
    values << item.name
           << item.username
           << item.type
           << item.notes
           << item.folderId
           << item.organizationId
           << item.collectionIds
           << item.uris;
    for (const VaultDetailSection& section : item.providerSections) {
        if (!section.searchable)
            continue;
        for (const VaultDetailField& field : section.fields) {
            values.push_back(field.name);
            if (!field.sensitive)
                values.push_back(field.value);
        }
    }
    return values.join(QLatin1Char('\n')).toLower();
}

struct PrimaryCopyValue
{
    QString text;
    bool sensitive = false;
};

PrimaryCopyValue primaryCopyValue(const VaultItem& item)
{
    if (!item.username.isEmpty())
        return {item.username, false};

    if (item.typeCode == 2 && !item.notes.isEmpty())
        return {item.notes, true};
    if (item.typeCode == 3 && !item.card.number.isEmpty())
        return {item.card.number, true};
    if (item.typeCode == 4) {
        if (!item.identity.username.isEmpty())
            return {item.identity.username, false};
        if (!item.identity.email.isEmpty())
            return {item.identity.email, false};
    }
    if (item.typeCode == 5 && !item.sshKey.publicKey.isEmpty())
        return {item.sshKey.publicKey, false};

    return {};
}

int itemMatchGroup(const VaultItem& item, const QString& needle)
{
    if (item.name.compare(needle, Qt::CaseInsensitive) == 0)
        return 0;
    if (item.name.startsWith(needle, Qt::CaseInsensitive))
        return 1;
    if (item.username.startsWith(needle, Qt::CaseInsensitive))
        return 2;
    for (const QString& uri : item.uris) {
        if (uri.contains(needle, Qt::CaseInsensitive))
            return 3;
    }
    return 4;
}

int resultRowHeight()
{
    return qMax(38, AppSettings::quickFontSize() + 28);
}
}

QuickWindow::QuickWindow(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("quickWindow");
    setWindowTitle("peekpass");
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground, false);

    m_search = new QLineEdit(this);
    m_search->setObjectName("searchEdit");
    m_search->setClearButtonEnabled(true);
    m_search->setPlaceholderText(uiText("비밀번호 검색", "Password search"));
    m_searchIconAction = m_search->addAction(makeSearchIcon(), QLineEdit::LeadingPosition);
    m_search->installEventFilter(this);

    m_list = new QListWidget(this);
    m_list->setObjectName("resultList");
    m_list->setVisible(false);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setUniformItemSizes(true);
    m_list->setTextElideMode(Qt::ElideRight);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_list->installEventFilter(this);

    m_faviconManager = new QNetworkAccessManager(this);
    connect(m_faviconManager, &QNetworkAccessManager::finished, this, [this](QNetworkReply* reply) {
        handleFaviconFinished(reply);
    });

    m_status = new QLabel(this);
    m_status->setObjectName("statusLabel");
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);
    m_status->setMaximumHeight(42);
    m_status->setVisible(false);

    m_busyBar = new QProgressBar(this);
    m_busyBar->setObjectName("busyBar");
    m_busyBar->setRange(0, 0);
    m_busyBar->setTextVisible(false);
    m_busyBar->setFixedHeight(3);
    m_busyBar->setVisible(false);

    m_authPanel = new QWidget(this);
    m_authPanel->setObjectName("authPanel");
    m_authPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_authPanel->setVisible(false);

    m_authTitle = new QLabel(m_authPanel);
    m_authTitle->setObjectName("authTitle");

    m_authNote = new QLabel(m_authPanel);
    m_authNote->setObjectName("authNote");
    m_authNote->setTextFormat(Qt::PlainText);
    m_authNote->setWordWrap(true);

    m_authPasswordManagerLabel = new QLabel(m_authPanel);
    m_authPasswordManagerLabel->setObjectName("authFieldLabel");
    m_authAccountLabel = new QLabel(m_authPanel);
    m_authAccountLabel->setObjectName("authFieldLabel");
    m_authServerLabel = new QLabel(m_authPanel);
    m_authServerLabel->setObjectName("authFieldLabel");
    m_authEmailLabel = new QLabel(m_authPanel);
    m_authEmailLabel->setObjectName("authFieldLabel");
    m_authPasswordLabel = new QLabel(m_authPanel);
    m_authPasswordLabel->setObjectName("authFieldLabel");
    m_authMethodLabel = new QLabel(m_authPanel);
    m_authMethodLabel->setObjectName("authFieldLabel");
    m_authCodeLabel = new QLabel(m_authPanel);
    m_authCodeLabel->setObjectName("authFieldLabel");
    m_authExtraPasswordLabel = new QLabel(m_authPanel);
    m_authExtraPasswordLabel->setObjectName("authFieldLabel");

    m_authAccount = new QLabel(m_authPanel);
    m_authAccount->setObjectName("authAccountText");
    m_authAccount->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_authPasswordManager = new QComboBox(m_authPanel);
    m_authPasswordManager->setObjectName("authPasswordManagerCombo");

    m_authServer = new QComboBox(m_authPanel);
    m_authServer->setObjectName("authServerCombo");

    m_authCustomServer = new QLineEdit(m_authPanel);
    m_authCustomServer->setObjectName("authCustomServerEdit");
    m_authCustomServer->setClearButtonEnabled(true);

    m_authEmail = new QLineEdit(m_authPanel);
    m_authEmail->setObjectName("authEmailEdit");
    m_authEmail->setClearButtonEnabled(true);

    m_authPassword = new QLineEdit(m_authPanel);
    m_authPassword->setObjectName("authPasswordEdit");
    m_authPassword->setEchoMode(QLineEdit::Password);

    m_authMethod = new QComboBox(m_authPanel);
    m_authMethod->setObjectName("authMethodCombo");

    m_authCode = new QLineEdit(m_authPanel);
    m_authCode->setObjectName("authCodeEdit");
    m_authCode->setClearButtonEnabled(true);

    m_authExtraPassword = new QLineEdit(m_authPanel);
    m_authExtraPassword->setObjectName("authPasswordEdit");
    m_authExtraPassword->setEchoMode(QLineEdit::Password);

    m_authPanel->installEventFilter(this);
    m_authTitle->installEventFilter(this);
    m_authNote->installEventFilter(this);
    m_authPasswordManagerLabel->installEventFilter(this);
    m_authAccountLabel->installEventFilter(this);
    m_authServerLabel->installEventFilter(this);
    m_authEmailLabel->installEventFilter(this);
    m_authPasswordLabel->installEventFilter(this);
    m_authMethodLabel->installEventFilter(this);
    m_authCodeLabel->installEventFilter(this);
    m_authExtraPasswordLabel->installEventFilter(this);

    m_authSubmit = new QPushButton(m_authPanel);
    m_authSubmit->setObjectName("authSubmitButton");
    m_authCancel = new QPushButton(m_authPanel);
    m_authCancel->setObjectName("authCancelButton");
    m_authBrowserLogin = new QPushButton(m_authPanel);
    m_authBrowserLogin->setObjectName("browserLoginButton");
    connect(m_authBrowserLogin, &QPushButton::clicked, this, [this] {
        QString error;
        if (!m_passwordManager->startExternalLogin(&error)) {
            setStatus(error);
            return;
        }
        setStatus(uiText("열린 CLI 창의 링크로 브라우저 로그인을 마친 뒤 연결 확인을 누르세요.",
            "Complete browser sign-in using the link in the CLI window, then check the connection."));
    });

    auto* authForm = new QGridLayout;
    authForm->setContentsMargins(0, 0, 0, 0);
    authForm->setHorizontalSpacing(10);
    authForm->setVerticalSpacing(7);
    authForm->addWidget(m_authPasswordManagerLabel, 0, 0);
    authForm->addWidget(m_authPasswordManager, 0, 1);
    authForm->addWidget(m_authAccountLabel, 1, 0);
    authForm->addWidget(m_authAccount, 1, 1);
    authForm->addWidget(m_authServerLabel, 2, 0);
    authForm->addWidget(m_authServer, 2, 1);
    authForm->addWidget(m_authCustomServer, 3, 1);
    authForm->addWidget(m_authEmailLabel, 4, 0);
    authForm->addWidget(m_authEmail, 4, 1);
    authForm->addWidget(m_authPasswordLabel, 5, 0);
    authForm->addWidget(m_authPassword, 5, 1);
    authForm->addWidget(m_authMethodLabel, 6, 0);
    authForm->addWidget(m_authMethod, 6, 1);
    authForm->addWidget(m_authCodeLabel, 7, 0);
    authForm->addWidget(m_authCode, 7, 1);
    authForm->addWidget(m_authExtraPasswordLabel, 8, 0);
    authForm->addWidget(m_authExtraPassword, 8, 1);
    authForm->setColumnStretch(1, 1);

    m_authBusyLabel = new QLabel(m_authPanel);
    m_authBusyLabel->setObjectName("authBusyLabel");
    m_authBusyLabel->setVisible(false);

    auto* authButtons = new QHBoxLayout;
    authButtons->setContentsMargins(0, 0, 0, 0);
    authButtons->setSpacing(8);
    authButtons->addWidget(m_authBusyLabel);
    authButtons->addStretch(1);
    authButtons->addWidget(m_authCancel);
    authButtons->addWidget(m_authBrowserLogin);
    authButtons->addWidget(m_authSubmit);

    auto* authLayout = new QVBoxLayout(m_authPanel);
    authLayout->setContentsMargins(0, 0, 0, 0);
    authLayout->setSpacing(6);
    authLayout->addWidget(m_authTitle);
    authLayout->addWidget(m_authNote);
    authLayout->addLayout(authForm);
    authLayout->addLayout(authButtons);

    m_footer = new QWidget(this);
    m_footer->setObjectName("footerBar");
    m_footer->setMinimumHeight(0);
    m_footer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_footerLayout = new QGridLayout(m_footer);
    m_footerLayout->setContentsMargins(0, 0, 0, 0);
    m_footerLayout->setHorizontalSpacing(12);
    m_footerLayout->setVerticalSpacing(2);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(7);
    layout->addWidget(m_search);
    layout->addWidget(m_authPanel, 0, Qt::AlignTop);
    layout->addWidget(m_list);
    layout->addWidget(m_status);
    layout->addWidget(m_footer);

    setStyleSheet(quickWindowStyle(AppSettings::roundedCorners() ? 12 : 0));

    applySettings();

    m_debounce.setSingleShot(true);
    m_debounce.setInterval(60);

    m_refreshTimer.setSingleShot(false);
    connect(&m_refreshTimer, &QTimer::timeout, this, [this] {
        if (m_passwordManager->hasSession())
            startItemLoad(true, true);
        else
            m_refreshTimer.stop();
    });
    updateAutoSyncTimer();

    m_sessionTimer.setInterval(15000);
    connect(&m_sessionTimer, &QTimer::timeout, this, [this] {
        if (m_itemsReady && !m_prepareWatcher.isRunning() && !m_authWatcher.isRunning()
            && !m_itemsWatcher.isRunning())
            prepareVault(false);
    });
    m_sessionTimer.start();

    m_preloadRetryTimer.setSingleShot(true);
    connect(&m_preloadRetryTimer, &QTimer::timeout, this, [this] {
        if (m_resetting || m_intentionallyLocked || m_itemsReady || m_authMode != AuthMode::None)
            return;
        if (m_prepareWatcher.isRunning() || m_authWatcher.isRunning() || m_itemsWatcher.isRunning()) {
            schedulePreloadRetry();
            return;
        }
        m_silentPreloadRetry = true;
        prepareVault(false);
    });

    connect(m_search, &QLineEdit::textEdited, this, [this] {
        clearPreservedSearchState();
    });

    connect(m_search, &QLineEdit::textChanged, this, [this] {
        if (m_itemsReady || m_search->text().trimmed().isEmpty()) {
            m_debounce.stop();
            reload();
            return;
        }

        m_debounce.start();
    });

    connect(&m_debounce, &QTimer::timeout, this, [this] {
        reload();
    });

    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        showItemDetails(item);
    });

    connect(m_list, &QListWidget::currentItemChanged, this, [this] {
        updateFooter();
    });

    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_list, &QListWidget::customContextMenuRequested, this, [this](const QPoint& position) {
        if (m_authMode != AuthMode::None)
            return;

        QListWidgetItem* item = m_list->itemAt(position);
        if (!item)
            return;

        m_list->setCurrentItem(item);

        const VaultItem* cached = cachedItemById(item->data(ItemIdRole).toString());

        QMenu menu(this);
        menu.setStyleSheet(menuStyle());

        QAction* detailsAction = menu.addAction(uiText("상세보기", "Details"));
        menu.addSeparator();
        QAction* copyPasswordAction = menu.addAction(uiText("비밀번호 복사", "Copy password"));
        QAction* copyUsernameAction = menu.addAction(uiText("사용자 이름 복사", "Copy username"));
        QAction* copyTotpAction = menu.addAction(uiText("TOTP 복사", "Copy TOTP"));

        if (cached) {
            copyPasswordAction->setEnabled(!cached->password.isEmpty());
            const PrimaryCopyValue primary = primaryCopyValue(*cached);
            copyUsernameAction->setEnabled(!primary.text.isEmpty() || !cached->username.isEmpty());
            copyTotpAction->setEnabled(!cached->totp.isEmpty());
        }

        QAction* openWebsiteAction = nullptr;
        const bool hasWebsite = cached && std::any_of(cached->uris.cbegin(), cached->uris.cend(), [](const QString& u) {
            return webUrlForItemUri(u).isValid();
        });
        if (hasWebsite) {
            menu.addSeparator();
            openWebsiteAction = menu.addAction(uiText("웹사이트 열기", "Open website"));
        }

        QAction* selectedAction = menu.exec(m_list->viewport()->mapToGlobal(position));
        if (selectedAction == detailsAction)
            showItemDetails(item);
        else if (selectedAction == copyPasswordAction)
            copyPassword(item);
        else if (selectedAction == copyUsernameAction)
            copyUsername(item);
        else if (selectedAction == copyTotpAction)
            copyTotp(item);
        else if (openWebsiteAction && selectedAction == openWebsiteAction)
            openItemWebsite(item);
    });

    auto* escape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(escape, &QShortcut::activated, this, [this] {
        if (m_authMode != AuthMode::None) {
            if (m_authWatcher.isRunning())
                return;
            m_authPassword->clear();
            m_authCode->clear();
            m_authExtraPassword->clear();
        }
        hide();
    });

    auto* copyUsernameShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_C), this);
    connect(copyUsernameShortcut, &QShortcut::activated, this, [this] {
        if (m_authMode != AuthMode::None)
            return;
        if (m_search && m_search->hasFocus() && m_search->hasSelectedText()) {
            m_search->copy();
            return;
        }
        if (AppSettings::shortcutPreset() == QStringLiteral("bitwarden"))
            return;
        copyUsername(currentItem());
    });

    auto* copyPasswordShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C), this);
    connect(copyPasswordShortcut, &QShortcut::activated, this, [this] {
        if (m_authMode == AuthMode::None && AppSettings::shortcutPreset() != QStringLiteral("bitwarden"))
            copyPassword(currentItem());
    });

    auto* copyTotpCtrlShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_C), this);
    connect(copyTotpCtrlShortcut, &QShortcut::activated, this, [this] {
        if (m_authMode == AuthMode::None && AppSettings::shortcutPreset() != QStringLiteral("bitwarden"))
            copyTotp(currentItem());
    });

    auto* detailsShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O), this);
    connect(detailsShortcut, &QShortcut::activated, this, [this] {
        if (m_authMode == AuthMode::None)
            showItemDetails(currentItem());
    });

    auto* copyUsernameReturn = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return), this);
    connect(copyUsernameReturn, &QShortcut::activated, this, [this] {
        if (m_authMode == AuthMode::None && AppSettings::shortcutPreset() == QStringLiteral("bitwarden"))
            copyUsername(currentItem());
    });

    auto* copyUsernameEnter = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Enter), this);
    connect(copyUsernameEnter, &QShortcut::activated, this, [this] {
        if (m_authMode == AuthMode::None && AppSettings::shortcutPreset() == QStringLiteral("bitwarden"))
            copyUsername(currentItem());
    });

    auto* copyTotpShortcut = new QShortcut(QKeySequence(Qt::ALT | Qt::Key_T), this);
    connect(copyTotpShortcut, &QShortcut::activated, this, [this] {
        if (m_authMode == AuthMode::None && AppSettings::shortcutPreset() == QStringLiteral("bitwarden"))
            copyTotp(currentItem());
    });

    auto* settingsShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma), this);
    connect(settingsShortcut, &QShortcut::activated, this, [this] {
        openSettings();
    });

    auto* syncShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_R), this);
    connect(syncShortcut, &QShortcut::activated, this, [this] {
        syncVault();
    });

    connect(m_authSubmit, &QPushButton::clicked, this, [this] {
        submitAuth();
    });

    connect(m_authCancel, &QPushButton::clicked, this, [this] {
        if (m_authWatcher.isRunning())
            return;
        m_authPassword->clear();
        m_authCode->clear();
        m_authExtraPassword->clear();
        hide();
    });

    connect(m_authServer, &QComboBox::currentIndexChanged, this, [this] {
        const bool custom = m_authServer->currentData().toString() == "custom";
        m_authCustomServer->setVisible(custom && m_authMode == AuthMode::Login
            && m_passwordManager->supportsServerSelection());
        updateWindowSize();
    });

    connect(m_authPasswordManager, &QComboBox::currentIndexChanged, this, [this] {
        if (m_authMode != AuthMode::Login || m_authWatcher.isRunning())
            return;

        const QString type = m_authPasswordManager->currentData().toString();
        if (type == AppSettings::passwordManagerType())
            return;

        AppSettings::setPasswordManagerType(type);
        reloadPasswordManager();
        showAuthPanel(AuthMode::Login);

        if (m_settingsChanged)
            m_settingsChanged();

        prepareVault(true);
    });

    connect(m_authEmail, &QLineEdit::returnPressed, this, [this] {
        m_authPassword->setFocus();
    });

    connect(m_authPassword, &QLineEdit::returnPressed, this, [this] {
        submitAuth();
    });

    connect(m_authCode, &QLineEdit::returnPressed, this, [this] {
        submitAuth();
    });

    connect(m_authExtraPassword, &QLineEdit::returnPressed, this, [this] {
        submitAuth();
    });

    connect(&m_prepareWatcher, &QFutureWatcher<PrepareResult>::finished, this, [this] {
        handlePrepareFinished();
        continuePendingPreload();
    });

    connect(&m_authWatcher, &QFutureWatcher<AuthResult>::finished, this, [this] {
        handleAuthFinished();
        continuePendingPreload();
    });

    connect(&m_itemsWatcher, &QFutureWatcher<ItemsResult>::finished, this, [this] {
        handleItemsFinished();
        continuePendingPreload();
    });

    connect(qApp, &QApplication::focusChanged, this, [this](QWidget* old, QWidget* now) {
        const bool focusLeftWindow = old && (old == this || isAncestorOf(old))
            && !(now && (now == this || isAncestorOf(now)));
        if (focusLeftWindow || !now)
            scheduleFocusLossCheck();
    });

    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (state == Qt::ApplicationInactive)
            scheduleFocusLossCheck();
    });
}

QuickWindow::~QuickWindow()
{
    clearVaultCache();
}

void QuickWindow::clearVaultCache()
{
    ++m_vaultGeneration;
    m_pendingVaultActions.clear();
    closeDetailWindows();
    m_refreshTimer.stop();
    m_itemsReady = false;
    if (!m_itemsWatcher.isRunning()) {
        const QSignalBlocker blocker(m_itemsWatcher);
        m_itemsWatcher.setFuture(QFuture<ItemsResult>{});
    }
    for (VaultItem& item : m_cachedItems)
        item.scrub();
    m_cachedItems.clear();
    m_recentItemIds.clear();
    m_faviconCache.clear();
    m_pendingFavicons.clear();
    m_queuedSearchText.clear();
    clearPreservedSearchState();
    m_list->clear();
    m_list->setVisible(false);
    m_list->setEnabled(true);
    ++m_clipboardGeneration;
    if (auto* clipboard = QApplication::clipboard()) {
        const bool owns = clipboard->ownsClipboard();
        const bool matchesHash = !m_lastCopiedClipboardHash.isEmpty()
            && QCryptographicHash::hash(clipboard->text().toUtf8(), QCryptographicHash::Sha256)
                == m_lastCopiedClipboardHash;
        if (owns || matchesHash)
            clipboard->clear();
    }
    m_lastCopiedClipboardHash.clear();
}

void QuickWindow::verifyVaultAction(std::function<void()> action)
{
    if (m_resetting || m_authMode != AuthMode::None || m_authWatcher.isRunning())
        return;

    if (!AppSettings::verifySessionBeforeCopyEnabled() && m_itemsReady && m_passwordManager->hasSession()) {
        if (action) {
            m_executingVerifiedAction = true;
            action();
            m_executingVerifiedAction = false;
        }
        if (!m_prepareWatcher.isRunning())
            prepareVault(false);
        return;
    }

    m_pendingVaultActions.push_back(std::move(action));
    prepareVault(true);
}

void QuickWindow::applySettings()
{
    reloadPasswordManager();
    applyStyle();
    updateAuthTexts();
    if (m_authMode == AuthMode::None)
        m_search->setPlaceholderText(uiText("비밀번호 검색", "Password search"));
    m_search->setVisible(m_authMode == AuthMode::None);
    m_footer->setVisible(m_authMode == AuthMode::None && AppSettings::showShortcutHints());
    setWindowOpacity(m_authMode == AuthMode::None ? AppSettings::windowOpacity() : 1.0);
    if (!AppSettings::fetchFaviconsFromWeb())
        m_faviconCache.clear();
    if (!AppSettings::showResultIcons() || !AppSettings::fetchFaviconsFromWeb())
        m_pendingFavicons.clear();
    if (m_itemsReady && m_authMode == AuthMode::None && m_search && !m_search->text().trimmed().isEmpty()) {
        startSearch(m_search->text());
    } else if (m_list) {
        for (int rowIndex = 0; rowIndex < m_list->count(); ++rowIndex) {
            QListWidgetItem* row = m_list->item(rowIndex);
            if (!row)
                continue;

            const VaultItem* item = cachedItemById(row->data(ItemIdRole).toString());
            row->setSizeHint(QSize(0, resultRowHeight()));
            if (item)
                row->setText(displayName(*item));
            if (!AppSettings::showResultIcons() || !item) {
                row->setIcon(QIcon());
                continue;
            }

            row->setIcon(makeResultIcon(*item));
            if (AppSettings::fetchFaviconsFromWeb())
                requestFaviconForRow(row, *item);
        }
    }
    updateAutoSyncTimer();
    updateFooter();
    updateWindowSize();
    if (isVisible())
        moveToConfiguredPosition();
}

void QuickWindow::reloadPasswordManager()
{
    if (!AppSettings::credentialSessionStorageEnabled())
        BwClient::clearStoredSessionData();

    const PasswordManager::Type configuredType = AppSettings::passwordManagerType() == QStringLiteral("protonpass")
        ? PasswordManager::Type::ProtonPass
        : PasswordManager::Type::Bitwarden;

    if (m_passwordManager && m_passwordManager->type() == configuredType) {
        m_passwordManager->reloadSettings();
        return;
    }

    const bool wasInAuthMode = m_authMode != AuthMode::None;
    clearVaultCache();
    m_passwordManager = std::make_shared<PasswordManager>();
    m_refreshTimer.stop();
    m_itemsReady = false;
    m_cachedItems.clear();
    m_recentItemIds.clear();
    m_currentUserEmail.clear();
    m_currentServerUrl.clear();
    if (m_list) {
        m_list->clear();
        m_list->setVisible(false);
    }
    if (wasInAuthMode) {
        hideAuthPanel();
        if (isVisible())
            showAuthPanel(AuthMode::Login);
    }
}

void QuickWindow::setSettingsChangedCallback(std::function<void()> callback)
{
    m_settingsChanged = std::move(callback);
}

void QuickWindow::setSettingsDialogStateCallback(std::function<void(bool)> callback)
{
    m_settingsDialogStateCallback = std::move(callback);
}

void QuickWindow::setNotificationCallback(std::function<void(const QString&)> callback)
{
    m_notificationCallback = std::move(callback);
}

bool QuickWindow::eventFilter(QObject* watched, QEvent* event)
{
    const bool authDragSurface = watched == m_authPanel
        || watched == m_authTitle
        || watched == m_authNote
        || watched == m_authPasswordManagerLabel
        || watched == m_authAccountLabel
        || watched == m_authServerLabel
        || watched == m_authEmailLabel
        || watched == m_authPasswordLabel
        || watched == m_authMethodLabel
        || watched == m_authCodeLabel
        || watched == m_authExtraPasswordLabel;
    if (authDragSurface) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                beginWindowDrag(mouseEvent);
                return true;
            }
        } else if (event->type() == QEvent::MouseMove) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (updateWindowDrag(mouseEvent))
                return true;
        } else if (event->type() == QEvent::MouseButtonRelease) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton && m_dragging) {
                finishWindowDrag();
                return true;
            }
        }
    }

    if ((watched == m_search || watched == m_list) && event->type() == QEvent::KeyPress && m_authMode == AuthMode::None) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        const Qt::KeyboardModifiers modifiers = keyEvent->modifiers();
        const bool bitwardenPreset = AppSettings::shortcutPreset() == QStringLiteral("bitwarden");

        if (bitwardenPreset) {
            if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                if (modifiers.testFlag(Qt::ControlModifier))
                    copyUsername(currentItem());
                else if (modifiers == Qt::NoModifier || modifiers == Qt::KeypadModifier)
                    copyPassword(currentItem());
                else
                    return false;
                return true;
            }
            if (keyEvent->key() == Qt::Key_T && modifiers.testFlag(Qt::AltModifier)) {
                copyTotp(currentItem());
                return true;
            }
        } else {
            if (keyEvent->key() == Qt::Key_C && modifiers.testFlag(Qt::ControlModifier)) {
                if (modifiers.testFlag(Qt::ShiftModifier)) {
                    copyPassword(currentItem());
                } else if (modifiers.testFlag(Qt::AltModifier)) {
                    copyTotp(currentItem());
                } else {
                    if (watched == m_search && m_search->hasSelectedText())
                        return false;
                    copyUsername(currentItem());
                }
                return true;
            }
        }

        if (keyEvent->key() == Qt::Key_O
            && modifiers.testFlag(Qt::ControlModifier)
            && modifiers.testFlag(Qt::ShiftModifier)) {
            showItemDetails(currentItem());
            return true;
        }

        if (keyEvent->key() == Qt::Key_Down) {
            moveSelection(1);
            return true;
        }
        if (keyEvent->key() == Qt::Key_Up) {
            moveSelection(-1);
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void QuickWindow::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);

    if (event->type() == QEvent::WindowDeactivate || event->type() == QEvent::ActivationChange)
        scheduleFocusLossCheck();
}

void QuickWindow::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        beginWindowDrag(event);
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void QuickWindow::mouseMoveEvent(QMouseEvent* event)
{
    if (updateWindowDrag(event)) {
        event->accept();
        return;
    }

    QWidget::mouseMoveEvent(event);
}

void QuickWindow::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_dragging) {
        finishWindowDrag();
        event->accept();
        return;
    }

    QWidget::mouseReleaseEvent(event);
}

void QuickWindow::beginWindowDrag(QMouseEvent* event)
{
    if (!event)
        return;

    m_dragging = true;
    m_dragMoved = false;
    m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
    m_dragStartPosition = pos();
}

bool QuickWindow::updateWindowDrag(QMouseEvent* event)
{
    if (!event || !m_dragging || !event->buttons().testFlag(Qt::LeftButton))
        return false;

    move(event->globalPosition().toPoint() - m_dragOffset);
    if ((pos() - m_dragStartPosition).manhattanLength() > 2)
        m_dragMoved = true;
    return true;
}

void QuickWindow::finishWindowDrag()
{
    if (!m_dragging)
        return;

    m_dragging = false;
    if (!m_dragMoved)
        return;

    AppSettings::setCustomWindowPositionEnabled(true);
    AppSettings::setWindowPositionX(x());
    AppSettings::setWindowPositionY(y());
}

void QuickWindow::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateWindowMask();

    if (!m_busyBar)
        return;

    m_busyBar->setGeometry(0, height() - m_busyBar->height(), width(), m_busyBar->height());
    m_busyBar->raise();
}

bool QuickWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG") {
        auto* msg = static_cast<MSG*>(message);
        if (msg && msg->message == WM_POWERBROADCAST
            && (msg->wParam == PBT_APMRESUMEAUTOMATIC || msg->wParam == PBT_APMRESUMESUSPEND)) {
            if (!m_resetting && !m_intentionallyLocked && m_authMode == AuthMode::None) {
                QTimer::singleShot(5000, this, [this] {
                    if (m_resetting || m_intentionallyLocked || m_authMode != AuthMode::None)
                        return;
                    if (m_itemsReady && m_passwordManager->hasSession())
                        startItemLoad(true, true);
                    else
                        schedulePreloadRetry(0);
                });
            }
        }
    }
#else
    Q_UNUSED(eventType)
    Q_UNUSED(message)
    Q_UNUSED(result)
#endif
    return QWidget::nativeEvent(eventType, message, result);
}

void QuickWindow::showSettings()
{
    openSettings();
}

void QuickWindow::syncVaultNow()
{
    if (m_resetting || m_authMode != AuthMode::None)
        return;

    m_intentionallyLocked = false;
    if (m_itemsWatcher.isRunning() || m_authWatcher.isRunning() || m_prepareWatcher.isRunning())
        return;

    if (!m_passwordManager->hasSession() || !m_itemsReady) {
        if (isVisible()) {
            prepareVault();
        } else {
            m_syncing = true;
            m_silentPreloadRetry = false;
            prepareVault(false);
        }
        return;
    }

    m_syncing = true;
    updateFooter();
    startItemLoad(true, !isVisible());
}

QString QuickWindow::passwordManagerName() const
{
    return m_passwordManager ? m_passwordManager->displayName() : QStringLiteral("Bitwarden");
}

bool QuickWindow::lockVault(QString* errorMessage)
{
    m_intentionallyLocked = true;
    m_preloadRetryTimer.stop();
    clearVaultCache();
    const bool locked = m_passwordManager->lock(errorMessage);
    m_refreshTimer.stop();
    m_itemsReady = false;
    m_cachedItems.clear();
    m_faviconCache.clear();
    m_pendingFavicons.clear();
    m_queuedSearchText.clear();
    m_list->clear();
    m_list->setVisible(false);
    m_search->clear();
    hideAuthPanel();
    setStatus({});
    updateFooter();
    hide();
    return locked;
}

bool QuickWindow::logoutVault(QString* errorMessage)
{
    m_intentionallyLocked = true;
    m_preloadRetryTimer.stop();
    clearVaultCache();
    const bool loggedOut = m_passwordManager->logout(errorMessage);
    m_refreshTimer.stop();
    m_itemsReady = false;
    m_cachedItems.clear();
    m_faviconCache.clear();
    m_pendingFavicons.clear();
    m_queuedSearchText.clear();
    m_recentItemIds.clear();
    m_currentUserEmail.clear();
    m_currentServerUrl.clear();
    m_list->clear();
    m_list->setVisible(false);
    m_search->clear();
    m_authEmail->clear();
    hideAuthPanel();
    setBusy(false);
    setStatus({});
    updateFooter();

    if (loggedOut) {
        applySettings();
        showAuthPanel(AuthMode::Login);
        moveToConfiguredPosition();
        show();
        raise();
        activateWindow();
        focusAuthPrimaryInput();
    }

    return loggedOut;
}

bool QuickWindow::logoutPasswordManager(const QString& managerType, QString* errorMessage)
{
    const PasswordManager::Type targetType = managerType == QStringLiteral("protonpass")
        ? PasswordManager::Type::ProtonPass
        : PasswordManager::Type::Bitwarden;
    if (m_passwordManager->type() == targetType)
        return logoutVault(errorMessage);

    PasswordManager target(targetType);
    return target.logout(errorMessage);
}

void QuickWindow::preloadVault()
{
    if (m_resetting || m_authMode != AuthMode::None || m_itemsReady) {
        m_preloadPending = false;
        return;
    }

    if (m_prepareWatcher.isRunning() || m_authWatcher.isRunning() || m_itemsWatcher.isRunning()) {
        m_preloadPending = true;
        return;
    }

    m_preloadPending = false;
    prepareVault(false);
}

void QuickWindow::continuePendingPreload()
{
    if (m_resetting || !m_preloadPending)
        return;

    QTimer::singleShot(0, this, [this] {
        if (m_preloadPending)
            preloadVault();
    });
}

void QuickWindow::schedulePreloadRetry(int delayMs)
{
    if (m_resetting || m_intentionallyLocked || m_itemsReady || m_authMode != AuthMode::None)
        return;
    m_preloadRetryTimer.start(qMax(0, delayMs));
}

void QuickWindow::showQuick()
{
    m_intentionallyLocked = false;
    m_preloadRetryTimer.stop();
    if (isVisible()) {
        const bool focusInside = QApplication::focusWidget()
            && (QApplication::focusWidget() == this || isAncestorOf(QApplication::focusWidget()));
        if (isActiveWindow() || focusInside) {
            if (m_authMode != AuthMode::None && m_authWatcher.isRunning())
                return;
            hide();
            return;
        }

        raise();
        activateWindow();
        if (m_authMode == AuthMode::None)
            m_search->setFocus();
        else
            focusAuthPrimaryInput();
        return;
    }

    if (m_authWatcher.isRunning() && m_authMode != AuthMode::None) {
        updateWindowSize();
        moveToConfiguredPosition();
        show();
        raise();
        activateWindow();
        return;
    }

    const bool restoreSearch = shouldRestorePreservedSearch();
    hideAuthPanel();
    m_search->setVisible(true);
    m_footer->setVisible(AppSettings::showShortcutHints());
    m_search->setEnabled(true);
    if (!restoreSearch) {
        m_search->clear();
        m_list->clear();
        m_list->setVisible(false);
    }
    setStatus({});
    updateWindowSize();
    moveToConfiguredPosition();
    show();
    raise();
    activateWindow();

    if (m_itemsReady && m_passwordManager->hasSession()) {
        setBusy(false);
        m_list->setEnabled(true);
        if (restoreSearch)
            restorePreservedSearch();
        else
            m_search->setFocus();
        return;
    }

    if (m_itemsWatcher.isRunning() && m_passwordManager->hasSession()) {
        m_quietItemLoad = false;
        setBusy(true, uiText("Bitwarden 항목 불러오는 중...", "Loading Bitwarden items..."));
        if (m_passwordManager->type() == PasswordManager::Type::ProtonPass)
            setBusy(true, uiText("Proton Pass 항목 불러오는 중...", "Loading Proton Pass items..."));
        m_search->setEnabled(true);
        m_search->setFocus();
        return;
    }

    prepareVault();
}

void QuickWindow::openSettings()
{
    if (m_settingsOpen)
        return;

    m_settingsOpen = true;
    if (m_settingsDialogStateCallback)
        m_settingsDialogStateCallback(true);

    SettingsDialog dialog(isVisible() ? this : nullptr);
    const int result = dialog.exec();

    m_settingsOpen = false;

    if (result != QDialog::Accepted) {
        if (m_settingsDialogStateCallback)
            m_settingsDialogStateCallback(false);
        return;
    }

    if (dialog.resetRequested()) {
        if (m_settingsDialogStateCallback)
            m_settingsDialogStateCallback(false);
        resetApplicationData();
        return;
    }

    if (dialog.logoutRequested()) {
        if (m_settingsDialogStateCallback)
            m_settingsDialogStateCallback(false);
        QString errorMessage;
        if (!logoutPasswordManager(dialog.logoutPasswordManagerType(), &errorMessage) && !errorMessage.isEmpty()) {
            QMessageBox::warning(
                isVisible() ? this : nullptr,
                dialog.logoutPasswordManagerType() == QStringLiteral("protonpass")
                    ? QStringLiteral("Proton Pass")
                    : uiText("Bitwarden 로그아웃", "Bitwarden Logout"),
                errorMessage);
        }
        return;
    }

    reloadPasswordManager();
    if (m_settingsChanged)
        m_settingsChanged();
    else if (m_settingsDialogStateCallback)
        m_settingsDialogStateCallback(false);

    applySettings();
    preloadVault();
}

void QuickWindow::resetApplicationData()
{
    if (m_resetting)
        return;

    m_resetting = true;
    ++m_vaultGeneration;
    closeDetailWindows();
    m_refreshTimer.stop();
    m_debounce.stop();
    m_prepareWatcher.cancel();
    m_authWatcher.cancel();
    m_itemsWatcher.cancel();

    hideAuthPanel();
    m_search->clear();
    m_search->setVisible(true);
    m_search->setEnabled(false);
    m_search->setPlaceholderText(uiText("peekpass 초기화 중...", "Resetting peekpass..."));
    m_footer->setVisible(false);
    setWindowOpacity(1.0);
    setBusy(true, uiText("로그아웃 및 로컬 데이터 삭제 중...", "Signing out and clearing local data..."));
    moveToConfiguredPosition();
    show();
    raise();

    const auto activeManager = m_passwordManager;
    auto* watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, this, [this, watcher] {
        watcher->deleteLater();
        BwClient::clearStoredSessionData();
        AppSettings::resetUserData();

        ++m_clipboardGeneration;
        if (QClipboard* clipboard = QApplication::clipboard(); clipboard && clipboard->ownsClipboard())
            clipboard->clear();

        setBusy(false);
        hide();
        QTimer::singleShot(0, qApp, [] {
            QApplication::quit();
        });
    });
    watcher->setFuture(QtConcurrent::run([activeManager] {
        QString ignoredError;
        const PasswordManager::Type activeType = activeManager->type();
        activeManager->logout(&ignoredError);

        const PasswordManager::Type otherType = activeType == PasswordManager::Type::Bitwarden
            ? PasswordManager::Type::ProtonPass
            : PasswordManager::Type::Bitwarden;
        PasswordManager otherManager(otherType);
        otherManager.logout(&ignoredError);
    }));
}

void QuickWindow::applyStyle()
{
    const int radius = AppSettings::roundedCorners() ? 12 : 0;
    const int fontSize = AppSettings::quickFontSize();
    const int baseWeight = AppSettings::quickFontBold() ? 700 : 500;
    const int footerFontSize = qMax(9, fontSize - 3);
    const int keycapFontSize = qMax(9, fontSize - 3);
    const int rowMinHeight = qMax(30, fontSize + 20);
    QString fontFamily = AppSettings::quickFontFamily();
    fontFamily.replace('\\', "\\\\");
    fontFamily.replace('"', "\\\"");
    setAttribute(Qt::WA_TranslucentBackground, false);

    QString style = quickWindowStyle(radius);
    style.append(QStringLiteral(R"(
        /* dynamic-style-start */
        #quickWindow, #quickWindow QLabel, #quickWindow QLineEdit, #quickWindow QListWidget, #quickWindow QComboBox, #quickWindow QPushButton {
            font-family: "%1";
        }
        #searchEdit {
            font-size: %2px;
            font-weight: %3;
        }
        #resultList {
            font-size: %4px;
            font-weight: %5;
        }
        #footerText {
            font-size: %6px;
            font-weight: %7;
            padding-left: 1px;
            padding-right: 6px;
        }
        #footerShortcut {
            font-size: %6px;
        }
        #keycapLabel {
            font-size: %8px;
            min-height: 17px;
            padding: 0 5px;
        }
        #resultList::item {
            min-height: %9px;
            padding: 4px 8px;
            border-radius: 5px;
        }
        #authTitle, #detailHeader {
            font-family: "%1";
        }
        /* dynamic-style-end */
    )")
        .arg(fontFamily)
        .arg(fontSize + 1)
        .arg(baseWeight)
        .arg(fontSize)
        .arg(baseWeight)
        .arg(footerFontSize)
        .arg(baseWeight)
        .arg(keycapFontSize)
        .arg(rowMinHeight));
    setStyleSheet(style);
    if (m_searchIconAction)
        m_searchIconAction->setIcon(makeSearchIcon());
    updateWindowMask();

    const QWidgetList topLevelWidgets = QApplication::topLevelWidgets();
    for (QWidget* widget : topLevelWidgets) {
        if (auto* detailDialog = dynamic_cast<ItemDetailDialog*>(widget))
            detailDialog->applyThemeStyle();
    }
}

void QuickWindow::updateWindowMask()
{
    if (!AppSettings::roundedCorners()) {
        clearMask();
        return;
    }

    QPainterPath path;
    path.addRoundedRect(rect(), 12, 12);
    setMask(QRegion(path.toFillPolygon().toPolygon()));
}

void QuickWindow::updateAuthTexts()
{
    if (!m_authPanel)
        return;

    const bool protonPass = m_passwordManager->type() == PasswordManager::Type::ProtonPass;
    m_authBrowserLogin->setText(uiText("브라우저 로그인", "Browser sign-in"));
    m_authBrowserLogin->setVisible(protonPass);

    {
        const QSignalBlocker blocker(m_authPasswordManager);
        m_authPasswordManager->clear();
        m_authPasswordManager->addItem(QStringLiteral("Bitwarden"), QStringLiteral("bitwarden"));
        m_authPasswordManager->addItem(QStringLiteral("Proton Pass"), QStringLiteral("protonpass"));
        const int managerIndex = m_authPasswordManager->findData(AppSettings::passwordManagerType());
        m_authPasswordManager->setCurrentIndex(managerIndex >= 0 ? managerIndex : 0);
    }

    const QString currentServer = m_authServer->currentData().toString();
    m_authServer->clear();
    m_authServer->addItem("bitwarden.com", "com");
    m_authServer->addItem("bitwarden.eu", "eu");
    m_authServer->addItem(uiText("사용자 지정", "Custom"), "custom");

    QString serverMode = currentServer.isEmpty() ? AppSettings::bitwardenServerMode() : currentServer;
    int serverIndex = m_authServer->findData(serverMode);
    if (serverIndex < 0)
        serverIndex = 0;
    m_authServer->setCurrentIndex(serverIndex);

    const QString currentMethod = m_authMethod->currentData().toString();
    m_authMethod->clear();
    m_authMethod->addItem(uiText("없음", "None"), QString());
    m_authMethod->addItem(uiText("인증 앱", "Authenticator app"), "0");
    m_authMethod->addItem(uiText("이메일", "Email"), "1");
    m_authMethod->addItem(uiText("YubiKey OTP", "YubiKey OTP"), "3");

    const int methodIndex = m_authMethod->findData(currentMethod);
    m_authMethod->setCurrentIndex(methodIndex >= 0 ? methodIndex : 0);

    m_authPasswordManagerLabel->setText(uiText(u8"\uBE44\uBC00\uBC88\uD638 \uAD00\uB9AC\uC790", "Password manager"));
    m_authAccountLabel->setText(uiText("계정", "Account"));
    m_authServerLabel->setText(uiText("서버", "Server"));
    m_authEmailLabel->setText(uiText("이메일", "Email"));
    m_authPasswordLabel->setText(protonPass ? uiText("비밀번호", "Password") : uiText("마스터 비밀번호", "Master password"));
    m_authMethodLabel->setText(uiText("2단계 방식", "Two-step method"));
    m_authCodeLabel->setText(uiText("2단계 코드", "Two-factor code"));
    m_authExtraPasswordLabel->setText(uiText("Pass 추가 비밀번호", "Pass extra password"));
    m_authCustomServer->setPlaceholderText("https://your.bitwarden.example.com");
    if (!protonPass && m_authCustomServer->text().trimmed().isEmpty())
        m_authCustomServer->setText(AppSettings::customBitwardenServerUrl());
    m_authEmail->setPlaceholderText("name@example.com");
    m_authCode->setPlaceholderText(uiText("필요한 경우에만 입력", "Only if required"));
    m_authExtraPassword->setPlaceholderText(uiText("설정한 경우에만 입력", "Only if configured"));
    m_authCancel->setText(uiText("취소", "Cancel"));

    if (m_authMode == AuthMode::Unlock && !protonPass) {
        m_authTitle->setText(uiText("Bitwarden 잠금 해제", "Unlock Bitwarden"));
        m_authNote->setText(uiText("마스터 비밀번호를 입력하면 이 창에서 바로 검색을 시작할 수 있습니다.",
                                   "Enter your master password to start searching in this window."));
        m_authSubmit->setText(uiText("잠금 해제", "Unlock"));
        return;
    }

    if (m_authMode == AuthMode::Unlock && protonPass) {
        m_authTitle->setText(uiText("Proton Pass 잠금 해제", "Unlock Proton Pass"));
        m_authNote->setText(uiText("Proton Pass CLI 세션이 잠겨 있습니다. CLI에서 잠금을 해제하거나 브라우저 로그인을 다시 수행한 뒤 연결 확인을 누르세요.",
                                   "The Proton Pass CLI session is locked. Unlock it in the CLI or sign in again, then check the connection."));
        m_authSubmit->setText(uiText("연결 확인", "Check connection"));
        return;
    }

    m_authTitle->setText(protonPass ? uiText("Proton Pass 로그인", "Proton Pass Login")
                                    : uiText("Bitwarden 로그인", "Bitwarden Login"));
    m_authNote->setText(protonPass
            ? uiText("브라우저 로그인을 누르면 CLI 창이 열립니다. 표시된 링크로 로그인을 마친 뒤 연결 확인을 누르세요. 이미 로그인되어 있다면 연결 확인만 누르면 됩니다. 잠긴 세션은 CLI에서 잠금 해제하세요.",
                     "Browser sign-in opens a CLI window. Follow its link, finish signing in, then check the connection. Already signed in? Just check the connection. Unlock locked sessions in the CLI.")
            : uiText("처음 사용하는 경우 Bitwarden CLI 계정 로그인이 먼저 필요합니다.",
                     "First use requires logging in to your Bitwarden CLI account."));
    m_authSubmit->setText(protonPass ? uiText("연결 확인", "Check connection") : uiText("로그인", "Log in"));
}

void QuickWindow::updateFooter()
{
    if (!m_footerLayout)
        return;

    const auto hideFooter = [this] {
        if (m_footer) {
            m_footer->setVisible(false);
            m_footer->setFixedHeight(0);
            m_footer->setMinimumHeight(0);
            m_footer->setMaximumHeight(0);
            m_footer->updateGeometry();
        }
        updateWindowSize();
    };

    while (QLayoutItem* item = m_footerLayout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }

    if (!AppSettings::showShortcutHints()) {
        hideFooter();
        return;
    }

    if (m_authMode != AuthMode::None) {
        hideFooter();
        return;
    }

    if (m_footer) {
        m_footer->setMinimumHeight(0);
        m_footer->setMaximumHeight(QWIDGETSIZE_MAX);
        m_footer->setVisible(true);
    }

    const auto addShortcut = [this](int row,
                                 int column,
                                 const QString& shortcut,
                                 const QString& label,
                                 const QString& labelColor = QString()) {
        auto* widget = new QLabel(m_footer);
        widget->setObjectName("footerShortcut");
        widget->setTextFormat(Qt::RichText);
        widget->setText(shortcutMarkup(shortcut, label, labelColor));
        widget->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        widget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        widget->ensurePolished();
        widget->setFixedHeight(qMax(20, widget->sizeHint().height()));
        m_footerLayout->setRowMinimumHeight(row, widget->height());
        m_footerLayout->addWidget(widget, row, column);
        widget->show();
        return column + 1;
    };

    const bool bitwardenStyle = AppSettings::shortcutPreset() == QStringLiteral("bitwarden");
    const QString usernameShortcut = bitwardenStyle ? QStringLiteral("Ctrl+Enter") : QStringLiteral("Ctrl+C");
    const QString passwordShortcut = bitwardenStyle ? QStringLiteral("Enter") : QStringLiteral("Ctrl+Shift+C");
    const QString totpShortcut = bitwardenStyle ? QStringLiteral("Alt+T") : QStringLiteral("Ctrl+Alt+C");

    int column = 0;
    column = addShortcut(0, column, usernameShortcut, uiText("아이디", "Username"));
    column = addShortcut(0, column, passwordShortcut, uiText("비밀번호", "Password"));
    column = addShortcut(0, column, totpShortcut, QStringLiteral("OTP"));
    addShortcut(0, column, "Ctrl+Shift+O", uiText("상세", "Details"));

    column = 0;
    column = addShortcut(1, column, "Esc", uiText("숨기기", "Hide"));
    column = addShortcut(1, column, "Ctrl+,", uiText("설정", "Settings"));
    addShortcut(1,
        column,
        "Ctrl+R",
        m_syncing ? uiText("동기화 중...", "Syncing...") : uiText("동기화", "Sync"),
        m_syncing
            ? (isLightTheme() ? QStringLiteral("#57606a") : QStringLiteral("#aeb4bf"))
            : QString());

    m_footerLayout->setColumnStretch(12, 1);
    if (m_footer) {
        m_footerLayout->activate();
        const int footerHeight = qMax(44, m_footerLayout->sizeHint().height() + 4);
        m_footer->setFixedHeight(footerHeight);
        m_footer->updateGeometry();
    }
    updateWindowSize();
}

void QuickWindow::updateWindowSize()
{
    const bool expanded = m_list && m_list->isVisible();
    int width = AppSettings::windowWidth();
    if (m_authPanel && m_authPanel->isVisible()) {
        m_authPanel->ensurePolished();
        if (QLayout* authLayout = m_authPanel->layout()) {
            authLayout->invalidate();
            authLayout->activate();
            const int authHeight = qMax(authLayout->sizeHint().height(), authLayout->minimumSize().height());
            if (authHeight > 0)
                m_authPanel->setFixedHeight(authHeight);
        }

        const int minimumAuthWidth = m_authMode == AuthMode::Unlock ? 420 : 500;
        const int maximumAuthWidth = qMax(AppSettings::windowWidth(), minimumAuthWidth);
        const int authContentWidth = m_authPanel->sizeHint().width() + 20;
        width = qMin(maximumAuthWidth, qMax(minimumAuthWidth, authContentWidth));
    } else if (m_authPanel) {
        m_authPanel->setMinimumHeight(0);
        m_authPanel->setMaximumHeight(QWIDGETSIZE_MAX);
    }

    if (m_footer && m_footer->isVisible()) {
        m_footerLayout->activate();
        const int footerFixedHeight = qMax(44, m_footerLayout->sizeHint().height() + 4);
        m_footer->setFixedHeight(footerFixedHeight);
        m_footer->adjustSize();
    } else if (m_footer) {
        m_footer->setFixedHeight(0);
    }

    QLayout* rootLayout = layout();
    const QMargins margins = rootLayout ? rootLayout->contentsMargins() : QMargins(10, 10, 10, 10);
    const int spacing = rootLayout ? qMax(0, rootLayout->spacing()) : 7;
    const int marginHeight = margins.top() + margins.bottom();
    auto visibleHeight = [](const QWidget* widget) {
        if (!widget || !widget->isVisible())
            return 0;

        return qMax(widget->sizeHint().height(), widget->minimumHeight());
    };

    const int searchHeight = visibleHeight(m_search);
    const int authHeight = visibleHeight(m_authPanel);
    const int statusHeight = visibleHeight(m_status);
    const int footerHeight = visibleHeight(m_footer);
    const int fixedWidgetCount =
        (m_search && m_search->isVisible() ? 1 : 0)
        + (m_authPanel && m_authPanel->isVisible() ? 1 : 0)
        + (m_status && m_status->isVisible() ? 1 : 0)
        + (m_footer && m_footer->isVisible() ? 1 : 0);
    const int visibleWidgetCount = fixedWidgetCount + (expanded ? 1 : 0);
    const int spacingHeight = qMax(0, visibleWidgetCount - 1) * spacing;
    const int fixedNonListHeight = marginHeight
        + spacingHeight
        + searchHeight
        + authHeight
        + statusHeight
        + footerHeight;
    const int minimumHeight = m_authPanel && m_authPanel->isVisible() ? 88 : 108;
    int listHeight = 0;
    if (expanded) {
        const int rowHeight = qMax(resultRowHeight(), m_list->sizeHintForRow(0));
        const int availableListHeight = qMax(0, AppSettings::windowHeight() - fixedNonListHeight);
        const int naturalRows = qMin(m_list->count(), AppSettings::searchResultLimit());
        const int naturalListHeight = rowHeight * naturalRows + 10;
        listHeight = qMin(naturalListHeight, availableListHeight);
        if (listHeight > 0 && listHeight < 44)
            listHeight = qMin(44, availableListHeight);
        m_list->setFixedHeight(listHeight);
        m_list->setMinimumHeight(listHeight);
        m_list->setMaximumHeight(listHeight);
    } else if (m_list) {
        m_list->setFixedHeight(0);
        m_list->setMinimumHeight(0);
        m_list->setMaximumHeight(0);
    }
    const int compactHeight = searchHeight
        + authHeight
        + listHeight
        + statusHeight
        + footerHeight
        + marginHeight
        + spacingHeight;
    const int desiredHeight = qMax(minimumHeight, compactHeight);
    int height = desiredHeight;
    if (!(m_authPanel && m_authPanel->isVisible()))
        height = qMin(AppSettings::windowHeight(), desiredHeight);
    height = qMax(height, compactHeight);
    if (m_authPanel && m_authPanel->isVisible() && rootLayout) {
        rootLayout->activate();
        height = qMax(height, rootLayout->sizeHint().height());
    }

    setMinimumSize(0, 0);
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    resize(width, height);
    setFixedSize(width, height);
}

void QuickWindow::reload()
{
    if (m_authMode != AuthMode::None || !m_search->isEnabled())
        return;

    const QString keyword = m_search->text().trimmed();
    if (keyword.isEmpty()) {
        m_queuedSearchText.clear();
        m_list->setVisible(false);
        m_list->clear();
        m_list->setFixedHeight(0);
        m_list->setMinimumHeight(0);
        m_list->setMaximumHeight(0);
        setStatus({});
        updateFooter();
        if (!m_prepareWatcher.isRunning() && !m_authWatcher.isRunning())
            setBusy(false);
        updateWindowSize();
        return;
    }

    startSearch(keyword);
}

void QuickWindow::startSearch(const QString& keyword)
{
    if (!m_itemsReady) {
        m_queuedSearchText = keyword;
        startItemLoad();
        return;
    }

    m_queuedSearchText.clear();
    setBusy(false);

    const QVector<VaultItem> items = filterCachedItems(keyword);
    const int resultLimit = AppSettings::searchResultLimit();
    const QString selectedItemId = currentItem()
        ? currentItem()->data(ItemIdRole).toString()
        : QString();
    const QString preferredItemId = selectedItemId.isEmpty() ? m_preservedItemId : selectedItemId;
    int displayed = 0;

    m_list->setUpdatesEnabled(false);
    if (m_list->viewport())
        m_list->viewport()->setUpdatesEnabled(false);

    {
        const QSignalBlocker blocker(m_list);
        m_list->clear();

        for (const VaultItem& item : items) {
            if (displayed >= resultLimit)
                break;

            auto* row = new QListWidgetItem(displayName(item));
            row->setSizeHint(QSize(0, resultRowHeight()));
            row->setData(ItemIdRole, item.id);
            row->setData(UsernameRole, item.username);
            row->setData(FaviconHostRole, faviconHostForItem(item));
            if (AppSettings::showResultIcons()) {
                row->setIcon(makeResultIcon(item));
                if (AppSettings::fetchFaviconsFromWeb())
                    requestFaviconForRow(row, item);
            }
            m_list->addItem(row);
            ++displayed;
        }
    }

    if (m_list->count() > 0) {
        m_list->setVisible(true);
        m_list->setCurrentRow(0);
        selectItemById(preferredItemId);
    } else {
        m_list->setVisible(false);
    }

    if (m_list->viewport())
        m_list->viewport()->setUpdatesEnabled(true);
    m_list->setUpdatesEnabled(true);
    if (m_list->viewport())
        m_list->viewport()->update();

    if (m_list->count() > 0)
        setStatus({});
    else
        setStatus(uiText("검색 결과 없음", "No matches"));

    updateFooter();
    updateWindowSize();
}

void QuickWindow::startItemLoad(bool forceRefresh, bool quiet)
{
    if ((!forceRefresh && m_itemsReady) || m_itemsWatcher.isRunning())
        return;

    m_quietItemLoad = quiet;
    if (!quiet)
        setBusy(true, forceRefresh
                ? uiText("Bitwarden 동기화 중...", "Syncing Bitwarden...")
                : uiText("Bitwarden 항목 불러오는 중...", "Loading Bitwarden items..."));

    if (!quiet && m_passwordManager->type() == PasswordManager::Type::ProtonPass) {
        setBusy(true, forceRefresh
                ? uiText("Proton Pass 동기화 중...", "Syncing Proton Pass...")
                : uiText("Proton Pass 항목 불러오는 중...", "Loading Proton Pass items..."));
    }

    const quint64 generation = m_vaultGeneration;
    const auto manager = m_passwordManager;
    m_itemsWatcher.setFuture(QtConcurrent::run([manager, forceRefresh, generation] {
        ItemsResult result;
        result.generation = generation;
        if (manager->statusInfo(&result.errorMessage).status != PasswordManager::VaultStatus::Unlocked) {
            if (result.errorMessage.isEmpty())
                result.errorMessage = uiText("보관함 세션이 잠겨 있거나 로그아웃되었습니다.", "Vault session is locked or signed out.");
            return result;
        }
        if (forceRefresh) {
            QString syncError;
            if (!manager->sync(&syncError)) {
                result.errorMessage = syncError;
                return result;
            }
        }
        result.items = manager->listItems(&result.errorMessage);
        return result;
    }));
}

void QuickWindow::handleItemsFinished()
{
    if (m_resetting) {
        const QSignalBlocker blocker(m_itemsWatcher);
        m_itemsWatcher.setFuture(QFuture<ItemsResult>{});
        return;
    }

    if (m_itemsWatcher.future().resultCount() == 0)
        return;

    ItemsResult result = m_itemsWatcher.result();
    {
        const QSignalBlocker blocker(m_itemsWatcher);
        m_itemsWatcher.setFuture(QFuture<ItemsResult>{});
    }
    const bool quiet = m_quietItemLoad;
    m_quietItemLoad = false;
    const bool wasSyncing = m_syncing;
    m_syncing = false;
    if (!quiet)
        setBusy(false);

    if (result.generation != m_vaultGeneration) {
        for (VaultItem& item : result.items)
            item.scrub();
        updateFooter();
        return;
    }

    if (!m_passwordManager->hasSession() || !result.errorMessage.isEmpty()) {
        for (VaultItem& item : result.items)
            item.scrub();
        clearVaultCache();
        if (quiet && !m_intentionallyLocked && m_authMode == AuthMode::None)
            schedulePreloadRetry(30000);
        if (!quiet) {
            setStatus(result.errorMessage);
            updateWindowSize();
        } else if (wasSyncing && m_notificationCallback && !result.errorMessage.isEmpty()) {
            m_notificationCallback(result.errorMessage);
        }
        updateFooter();
        return;
    }

    m_preloadRetryTimer.stop();
    m_silentPreloadRetry = false;
    for (VaultItem& item : m_cachedItems)
        item.scrub();
    m_cachedItems = std::move(result.items);
    m_itemsReady = true;
    updateAutoSyncTimer();

    const QString keyword = m_search ? m_search->text().trimmed() : QString();
    if (!keyword.isEmpty() && (isVisible() || !quiet))
        startSearch(keyword);
    else if (!quiet)
        setStatus({});

    if (wasSyncing) {
        if (isVisible()) {
            const QString syncCompleteText = uiText("동기화 완료", "Sync complete");
            setStatus(syncCompleteText);
            QTimer::singleShot(1400, this, [this, syncCompleteText] {
                if (!m_itemsWatcher.isRunning()
                    && ((m_status && m_status->text() == syncCompleteText)
                        || (m_search && m_search->text().trimmed().isEmpty()
                            && m_search->placeholderText() == syncCompleteText))) {
                    setStatus({});
                }
            });
        } else if (m_notificationCallback) {
            m_notificationCallback(uiText("보관함 동기화를 완료했습니다.", "Vault sync complete."));
        }
    }
    updateFooter();
}

QVector<VaultItem> QuickWindow::filterCachedItems(const QString& keyword) const
{
    QVector<VaultItem> filtered;
    const QString needle = keyword.trimmed().toLower();
    if (needle.isEmpty())
        return filtered;

    const QStringList parts = needle.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    for (const VaultItem& item : m_cachedItems) {
        const QString haystack = itemSearchText(item);
        bool matched = true;
        for (const QString& part : parts) {
            if (!haystack.contains(part)) {
                matched = false;
                break;
            }
        }

        if (matched)
            filtered.push_back(item);
    }

    std::stable_sort(filtered.begin(), filtered.end(), [this, needle](const VaultItem& left, const VaultItem& right) {
        const int leftGroup = itemMatchGroup(left, needle);
        const int rightGroup = itemMatchGroup(right, needle);
        if (leftGroup != rightGroup)
            return leftGroup < rightGroup;

        const int leftRecent = recentItemRank(left.id);
        const int rightRecent = recentItemRank(right.id);
        if (leftRecent != rightRecent)
            return leftRecent < rightRecent;

        return QString::localeAwareCompare(left.name, right.name) < 0;
    });

    return filtered;
}

void QuickWindow::restorePreservedSearch()
{
    if (!shouldRestorePreservedSearch())
        return;

    const QString preservedText = m_preservedSearchText;
    const QString preservedItemId = m_preservedItemId;
    clearPreservedSearchState();

    m_search->setText(preservedText);
    m_search->setFocus();

    if (m_itemsReady) {
        startSearch(preservedText);
        selectItemById(preservedItemId);
    }
}

void QuickWindow::clearPreservedSearchState()
{
    m_preservedSearchText.clear();
    m_preservedItemId.clear();
    m_preservedSearchTimer.invalidate();
}

bool QuickWindow::shouldRestorePreservedSearch() const
{
    const int seconds = AppSettings::reopenRetentionSeconds();
    return seconds > 0
        && m_preservedSearchTimer.isValid()
        && m_preservedSearchTimer.elapsed() <= seconds * 1000
        && !m_preservedSearchText.trimmed().isEmpty();
}

void QuickWindow::preserveSearchState()
{
    m_preservedSearchText = m_search ? m_search->text() : QString();
    m_preservedItemId = currentItem() ? currentItem()->data(ItemIdRole).toString() : QString();

    if (m_preservedSearchText.trimmed().isEmpty()) {
        m_preservedSearchTimer.invalidate();
        return;
    }

    m_preservedSearchTimer.restart();
}

void QuickWindow::recordRecentItem(const QString& itemId)
{
    if (itemId.isEmpty())
        return;

    m_recentItemIds.removeAll(itemId);
    m_recentItemIds.prepend(itemId);
    while (m_recentItemIds.size() > MaxRecentItems)
        m_recentItemIds.removeLast();
}

int QuickWindow::recentItemRank(const QString& itemId) const
{
    const int index = m_recentItemIds.indexOf(itemId);
    return index < 0 ? 100000 : index;
}

const VaultItem* QuickWindow::cachedItemById(const QString& itemId) const
{
    for (const VaultItem& item : m_cachedItems) {
        if (item.id == itemId)
            return &item;
    }
    return nullptr;
}

QString QuickWindow::totpCodeForItem(const QString& itemId, QString* errorMessage) const
{
    const VaultItem* cached = cachedItemById(itemId);
    if (cached) {
        if (cached->totp.isEmpty()) {
            if (errorMessage)
                *errorMessage = uiText("이 항목에는 TOTP가 없습니다.", "This item has no TOTP.");
            return {};
        }
        const QString code = TotpGenerator::generateTotpCode(cached->totp, errorMessage);
        if (!code.isEmpty())
            return code;
    }

    return m_passwordManager->getTotp(itemId, errorMessage);
}

void QuickWindow::selectItemById(const QString& itemId)
{
    if (itemId.isEmpty() || !m_list)
        return;

    for (int row = 0; row < m_list->count(); ++row) {
        QListWidgetItem* item = m_list->item(row);
        if (item && item->data(ItemIdRole).toString() == itemId) {
            m_list->setCurrentRow(row);
            m_list->scrollToItem(item);
            return;
        }
    }
}

void QuickWindow::moveSelection(int delta)
{
    if (!m_list || !m_list->isVisible() || m_list->count() == 0)
        return;

    int row = m_list->currentRow();
    if (row < 0)
        row = delta > 0 ? -1 : 0;

    row += delta;
    if (row < 0)
        row = m_list->count() - 1;
    else if (row >= m_list->count())
        row = 0;

    m_list->setCurrentRow(row);
    m_list->scrollToItem(m_list->currentItem());
}

void QuickWindow::prepareVault(bool interactive)
{
    const auto preparingText = [this]() {
        return m_passwordManager->type() == PasswordManager::Type::ProtonPass
            ? uiText("Proton Pass 준비 중...", "Preparing Proton Pass...")
            : uiText("Bitwarden 준비 중...", "Preparing Bitwarden...");
    };

    if (m_prepareWatcher.isRunning() || m_authWatcher.isRunning()) {
        if (interactive) {
            m_preloadingVault = false;
            m_retryInteractivePrepare = m_prepareWatcher.isRunning();
            setBusy(true, preparingText());
            moveToConfiguredPosition();
            m_search->setEnabled(false);
        }
        return;
    }

    m_retryInteractivePrepare = false;
    m_backgroundSessionCheck = !interactive && m_itemsReady;
    m_preloadingVault = !interactive;
    if (!m_backgroundSessionCheck)
        m_list->setEnabled(false);
    if (interactive) {
        setBusy(true, preparingText());
        moveToConfiguredPosition();
        m_search->setEnabled(false);
    }

    const quint64 generation = m_vaultGeneration;
    const auto manager = m_passwordManager;
    const bool skipAvailabilityCheck = m_itemsReady;
    m_prepareWatcher.setFuture(QtConcurrent::run([manager, generation, skipAvailabilityCheck] {
        PrepareResult result;
        result.generation = generation;
        QString errorMessage;

        if (skipAvailabilityCheck) {
            result.available = true;
        } else {
            result.available = manager->isAvailable(&errorMessage);
            if (!result.available) {
                result.errorMessage = errorMessage;
                return result;
            }
        }

        result.statusInfo = manager->statusInfo(&errorMessage);
        result.errorMessage = errorMessage;
        return result;
    }));
}

void QuickWindow::handlePrepareFinished()
{
    if (m_resetting)
        return;

    const PrepareResult result = m_prepareWatcher.result();
    const bool preloading = m_preloadingVault;
    const bool background = m_backgroundSessionCheck;
    const bool silentRetry = m_silentPreloadRetry;
    const bool retryInteractive = m_retryInteractivePrepare;
    m_preloadingVault = false;
    m_silentPreloadRetry = false;
    m_retryInteractivePrepare = false;

    if (!preloading)
        setBusy(false);

    if (result.generation != m_vaultGeneration) {
        if (retryInteractive)
            prepareVault(true);
        return;
    }
    m_list->setEnabled(true);

    if (!result.available || result.statusInfo.status != PasswordManager::VaultStatus::Unlocked) {
        clearVaultCache();
        if (result.statusInfo.status == PasswordManager::VaultStatus::Unauthenticated
            || result.statusInfo.status == PasswordManager::VaultStatus::Locked) {
            m_passwordManager->clearSession();
        }
    }

    if (!result.available) {
        m_syncing = false;
        m_currentUserEmail.clear();
        m_currentServerUrl.clear();
        if (silentRetry && !isVisible()) {
            schedulePreloadRetry(30000);
            return;
        }
        if (background && !isVisible())
            return;
        if (preloading && !background) {
            showStartupAuthPanel(AuthMode::Login);
        } else {
            showAuthPanel(AuthMode::Login);
        }
        if (!result.errorMessage.isEmpty())
            m_authNote->setText(m_authNote->text() + QStringLiteral("\n\n") + result.errorMessage);
        setStatus({});
        updateWindowSize();
        return;
    }

    if ((!m_currentUserEmail.isEmpty() && m_currentUserEmail != result.statusInfo.userEmail)
        || (!m_currentServerUrl.isEmpty() && m_currentServerUrl != result.statusInfo.serverUrl))
        clearVaultCache();
    m_currentUserEmail = result.statusInfo.userEmail;
    m_currentServerUrl = result.statusInfo.serverUrl;

    switch (result.statusInfo.status) {
    case PasswordManager::VaultStatus::Unauthenticated:
        m_syncing = false;
        if ((silentRetry || background) && !isVisible())
            return;
        if (preloading && !background) {
            showStartupAuthPanel(AuthMode::Login);
            return;
        }

        showAuthPanel(AuthMode::Login);
        return;
    case PasswordManager::VaultStatus::Locked:
        m_syncing = false;
        if ((silentRetry || background) && !isVisible())
            return;
        if (preloading && !background) {
            showStartupAuthPanel(AuthMode::Unlock);
            return;
        }

        showAuthPanel(AuthMode::Unlock);
        return;
    case PasswordManager::VaultStatus::Unlocked:
        m_preloadRetryTimer.stop();
        if (m_itemsReady) {
            const bool detailWindowActive = QApplication::activeWindow()
                && QApplication::activeWindow()->property("peekpassDetailWindow").toBool();
            if (!preloading) {
                hideAuthPanel();
                m_search->setEnabled(true);
                if (shouldRestorePreservedSearch())
                    restorePreservedSearch();
                else if (!m_search->text().trimmed().isEmpty())
                    startSearch(m_search->text());
                if (!detailWindowActive)
                    m_search->setFocus();
            }
            QVector<std::function<void()>> actions = std::move(m_pendingVaultActions);
            m_pendingVaultActions.clear();
            for (const auto& action : actions) {
                if (action) {
                    m_executingVerifiedAction = true;
                    action();
                    m_executingVerifiedAction = false;
                }
            }
            return;
        }
        m_pendingVaultActions.clear();
        if (preloading) {
            startItemLoad(m_syncing, true);
            return;
        }

        hideAuthPanel();
        m_search->setEnabled(true);
        m_search->setPlaceholderText(uiText("비밀번호 검색", "Password search"));
        setStatus({});
        m_search->setFocus();
        startItemLoad(m_syncing, false);
        return;
    case PasswordManager::VaultStatus::Unknown:
        m_syncing = false;
        if (preloading) {
            schedulePreloadRetry(30000);
            return;
        }

        hideAuthPanel();
        m_search->setEnabled(false);
        setStatus(result.errorMessage.isEmpty()
                ? (m_passwordManager->type() == PasswordManager::Type::ProtonPass
                        ? uiText("Proton Pass 상태를 확인하지 못했습니다.", "Could not check Proton Pass status.")
                        : uiText("Bitwarden 상태를 확인하지 못했습니다.", "Could not check Bitwarden status."))
                : result.errorMessage);
        return;
    }
}

void QuickWindow::focusAuthPrimaryInput()
{
    if (m_authMode == AuthMode::None)
        return;

    if (m_passwordManager->type() == PasswordManager::Type::ProtonPass) {
        if (m_authSubmit)
            m_authSubmit->setFocus();
        return;
    }

    if (m_authMode == AuthMode::Login && m_authEmail && m_authEmail->text().trimmed().isEmpty()) {
        m_authEmail->setFocus();
        return;
    }

    if (m_authPassword) {
        m_authPassword->setFocus();
        m_authPassword->selectAll();
    }
}

void QuickWindow::showStartupAuthPanel(AuthMode mode)
{
    applySettings();
    hideAuthPanel();
    m_search->setVisible(false);
    m_footer->setVisible(false);
    showAuthPanel(mode);

    show();
    updateWindowSize();
    moveToConfiguredPosition();
    raise();
    activateWindow();

    focusAuthPrimaryInput();

    QTimer::singleShot(0, this, [this, mode] {
        if (!isVisible() || m_authMode != mode)
            return;

        updateWindowSize();
        moveToConfiguredPosition();
        focusAuthPrimaryInput();
    });
}

void QuickWindow::showAuthPanel(AuthMode mode)
{
    m_authMode = mode;
    updateAuthTexts();

    const bool loginMode = mode == AuthMode::Login;
    const bool unlockMode = mode == AuthMode::Unlock;
    const bool protonPass = m_passwordManager->type() == PasswordManager::Type::ProtonPass;
    const bool supportsServerSelection = m_passwordManager->supportsServerSelection();
    const bool supportsMethodSelection = m_passwordManager->supportsTwoFactorMethodSelection();
    const bool customServer = m_authServer->currentData().toString() == "custom";
    const QString accountText = m_currentUserEmail.isEmpty()
        ? uiText("로그인된 계정", "Signed-in account")
        : m_currentUserEmail;

    m_authAccount->setText(accountText);
    m_authPasswordManagerLabel->setVisible(loginMode);
    m_authPasswordManager->setVisible(loginMode);
    m_authAccountLabel->setVisible(unlockMode);
    m_authAccount->setVisible(unlockMode);
    m_authServerLabel->setVisible(loginMode && supportsServerSelection);
    m_authServer->setVisible(loginMode && supportsServerSelection);
    m_authCustomServer->setVisible(loginMode && supportsServerSelection && customServer);
    m_authEmailLabel->setVisible(loginMode && !protonPass);
    m_authEmail->setVisible(loginMode && !protonPass);
    m_authPasswordLabel->setVisible(!protonPass);
    m_authPassword->setVisible(!protonPass);
    m_authMethodLabel->setVisible(loginMode && supportsMethodSelection);
    m_authMethod->setVisible(loginMode && supportsMethodSelection);
    m_authCodeLabel->setVisible(loginMode && !protonPass);
    m_authCode->setVisible(loginMode && !protonPass);
    m_authExtraPasswordLabel->setVisible(false);
    m_authExtraPassword->setVisible(false);

    m_list->clear();
    m_list->setVisible(false);
    m_authPanel->setVisible(mode != AuthMode::None);
    setWindowOpacity(1.0);
    m_search->setVisible(false);
    m_search->setEnabled(false);
    m_footer->setVisible(false);
    setStatus({});
    if (QLayout* authLayout = m_authPanel->layout())
        authLayout->invalidate();
    if (QLayout* rootLayout = layout())
        rootLayout->invalidate();
    m_authPanel->updateGeometry();
    updateWindowSize();
    moveToConfiguredPosition();

    if (protonPass) {
        m_authSubmit->setFocus();
    } else if (loginMode && m_authEmail->text().trimmed().isEmpty()) {
        m_authEmail->setFocus();
    } else {
        m_authPassword->setFocus();
        m_authPassword->selectAll();
    }
}

void QuickWindow::hideAuthPanel()
{
    m_authMode = AuthMode::None;
    if (m_authPanel)
        m_authPanel->setVisible(false);
    if (m_authPassword)
        m_authPassword->clear();
    if (m_authCode)
        m_authCode->clear();
    if (m_authExtraPassword)
        m_authExtraPassword->clear();
    if (m_search) {
        m_search->setVisible(true);
        m_search->setPlaceholderText(uiText("비밀번호 검색", "Password search"));
    }
    if (m_footer)
        m_footer->setVisible(AppSettings::showShortcutHints());
    setWindowOpacity(AppSettings::windowOpacity());
    updateAuthTexts();
    updateWindowSize();
}

QString QuickWindow::selectedAuthServerUrl() const
{
    if (!m_passwordManager->supportsServerSelection())
        return {};

    const QString mode = m_authServer->currentData().toString();
    if (mode == "custom")
        return m_authCustomServer->text().trimmed();

    return defaultServerUrlForMode(mode);
}

void QuickWindow::setBusy(bool busy, const QString& message)
{
    const bool authBusy = m_authMode != AuthMode::None && m_authPanel && m_authPanel->isVisible();

    if (m_authBusyLabel) {
        m_authBusyLabel->setText(authBusy ? message : QString());
        m_authBusyLabel->setVisible(busy && authBusy && !message.isEmpty());
    }

    if (m_busyBar) {
        m_busyBar->setVisible(busy);
        m_busyBar->setGeometry(0, height() - m_busyBar->height(), width(), m_busyBar->height());
        m_busyBar->raise();
    }

    if (authBusy) {
        if (!busy)
            setStatus({});
    } else if (!message.isEmpty()) {
        setStatus(message);
    } else if (!busy) {
        setStatus({});
    }

    updateWindowSize();
}

void QuickWindow::setAuthInputsEnabled(bool enabled)
{
    m_authPasswordManager->setEnabled(enabled);
    m_authServer->setEnabled(enabled);
    m_authCustomServer->setEnabled(enabled);
    m_authEmail->setEnabled(enabled);
    m_authPassword->setEnabled(enabled);
    m_authMethod->setEnabled(enabled);
    m_authCode->setEnabled(enabled);
    m_authExtraPassword->setEnabled(enabled);
    m_authCancel->setEnabled(enabled);
    m_authSubmit->setEnabled(enabled);
    m_authBrowserLogin->setEnabled(enabled);
}

void QuickWindow::syncVault()
{
    if (m_authMode != AuthMode::None)
        return;

    if (m_itemsWatcher.isRunning() || m_authWatcher.isRunning() || m_prepareWatcher.isRunning())
        return;

    if (!m_passwordManager->hasSession()) {
        setStatus(uiText("Bitwarden 잠금 해제가 먼저 필요합니다.", "Unlock Bitwarden first."));
        if (m_passwordManager->type() == PasswordManager::Type::ProtonPass)
            setStatus(uiText("Proton Pass 로그인이 먼저 필요합니다.", "Sign in to Proton Pass first."));
        prepareVault();
        return;
    }

    m_syncing = true;
    updateFooter();
    startItemLoad(true, false);
}

void QuickWindow::updateAutoSyncTimer()
{
    const int minutes = AppSettings::autoSyncIntervalMinutes();
    if (minutes <= 0 || !m_passwordManager->hasSession() || !m_itemsReady) {
        m_refreshTimer.stop();
        return;
    }

    m_refreshTimer.setInterval(minutes * 60 * 1000);
    m_refreshTimer.start();
}

void QuickWindow::requestFaviconForRow(QListWidgetItem* row, const VaultItem& item)
{
    if (!row || !m_faviconManager || !AppSettings::showResultIcons() || !AppSettings::fetchFaviconsFromWeb())
        return;

    const QString host = faviconHostForItem(item);
    if (host.isEmpty())
        return;

    row->setData(FaviconHostRole, host);

    if (m_faviconCache.contains(host)) {
        const QIcon cachedIcon = m_faviconCache.value(host);
        if (!cachedIcon.isNull())
            row->setIcon(cachedIcon);
        return;
    }

    if (m_pendingFavicons.contains(host))
        return;

    const QUrl url = faviconUrlForItem(item);
    if (!url.isValid() || url.host().isEmpty())
        return;

    m_pendingFavicons.insert(host);
    fetchFavicon(host, url);
}

void QuickWindow::fetchFavicon(const QString& host, const QUrl& url)
{
    if (!m_faviconManager || host.isEmpty() || !url.isValid())
        return;

    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(2500);
    request.setRawHeader("User-Agent", QByteArrayLiteral("peekpass/" PEEKPASS_VERSION));

    QNetworkReply* reply = m_faviconManager->get(request);
    reply->setProperty("peekpassHost", host);
}

void QuickWindow::handleFaviconFinished(QNetworkReply* reply)
{
    if (!reply)
        return;

    const QString host = reply->property("peekpassHost").toString();

    if (!AppSettings::showResultIcons() || !AppSettings::fetchFaviconsFromWeb()) {
        m_pendingFavicons.remove(host);
        reply->deleteLater();
        return;
    }

    const QVariant statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    const bool httpOk = !statusCode.isValid() || statusCode.toInt() < 400;
    const bool ok = reply->error() == QNetworkReply::NoError && httpOk;

    m_pendingFavicons.remove(host);

    if (!m_passwordManager->hasSession()) {
        reply->deleteLater();
        return;
    }

    QIcon icon;
    if (ok) {
        const QByteArray data = reply->read(256 * 1024);
        QPixmap pixmap;
        if (pixmap.loadFromData(data) && !pixmap.isNull()) {
            pixmap = pixmap.scaled(22, 22, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            icon = QIcon(pixmap);
        }
    }

    m_faviconCache.insert(host, icon);
    if (!icon.isNull())
        applyFaviconToVisibleRows(host, icon);

    reply->deleteLater();
}

void QuickWindow::applyFaviconToVisibleRows(const QString& host, const QIcon& icon)
{
    if (!m_list || host.isEmpty() || icon.isNull()
        || !AppSettings::showResultIcons() || !AppSettings::fetchFaviconsFromWeb())
        return;

    for (int row = 0; row < m_list->count(); ++row) {
        QListWidgetItem* item = m_list->item(row);
        if (item && item->data(FaviconHostRole).toString() == host)
            item->setIcon(icon);
    }
}

void QuickWindow::scheduleFocusLossCheck()
{
    if (m_resetting || !AppSettings::closeOnFocusLoss() || !isVisible() || m_authMode != AuthMode::None)
        return;

    QTimer::singleShot(0, this, [this] {
        hideIfFocusLost();
    });
    QTimer::singleShot(150, this, [this] {
        hideIfFocusLost();
    });
}

void QuickWindow::hideIfFocusLost()
{
    if (m_resetting || !AppSettings::closeOnFocusLoss() || !isVisible() || m_authMode != AuthMode::None)
        return;

    if (QApplication::activeModalWidget() || QApplication::activePopupWidget())
        return;

    for (QWidget* widget : QApplication::topLevelWidgets()) {
        if (widget && widget->isVisible() && widget->property("peekpassDetailWindow").toBool())
            return;
    }

    if (QWidget* active = QApplication::activeWindow()) {
        if (active->property("peekpassDetailWindow").toBool())
            return;
    }

    if (isActiveWindow())
        return;

    QWidget* focusWidget = QApplication::focusWidget();
    if (focusWidget && (focusWidget == this || isAncestorOf(focusWidget)))
        return;

    hide();
}

void QuickWindow::submitAuth()
{
    if (m_authMode == AuthMode::None || m_authWatcher.isRunning())
        return;

    if (m_passwordManager->type() == PasswordManager::Type::ProtonPass) {
        prepareVault(true);
        return;
    }

    QString password = m_authPassword->text();
    const bool loginMode = m_authMode == AuthMode::Login;
    const QString serverMode = m_authServer->currentData().toString();
    const QString serverUrl = selectedAuthServerUrl();
    const QString email = m_authEmail->text();
    const QString method = m_authMethod->currentData().toString();
    const QString code = m_authCode->text();
    QString extraPassword = m_authExtraPassword->text();

    if (loginMode && m_passwordManager->supportsServerSelection() && serverUrl.isEmpty()) {
        setStatus(uiText("사용자 지정 서버 주소를 입력하세요.",
                         "Enter the custom server URL."));
        m_authCustomServer->setFocus();
        password.fill(QChar('\0'));
        return;
    }

    if (loginMode && m_passwordManager->supportsServerSelection()) {
        AppSettings::setBitwardenServerMode(serverMode);
        AppSettings::setCustomBitwardenServerUrl(m_authCustomServer->text());
    }

    setAuthInputsEnabled(false);
    const QString managerName = m_passwordManager->displayName();
    setBusy(true, loginMode
            ? uiText("%1 로그인 중...", "Logging in to %1...").arg(managerName)
            : uiText("%1 잠금 해제 중...", "Unlocking %1...").arg(managerName));

    const quint64 generation = m_vaultGeneration;
    const auto manager = m_passwordManager;
    m_authWatcher.setFuture(QtConcurrent::run([manager, loginMode, serverUrl, email, password, method, code, extraPassword, generation] {
        AuthResult result;
        result.generation = generation;
        result.loginMode = loginMode;
        QString errorMessage;

        if (loginMode) {
            PasswordManager::AuthRequest request;
            request.serverUrl = serverUrl;
            request.email = email;
            request.password = password;
            request.twoFactorMethod = method;
            request.twoFactorCode = code;
            request.extraPassword = extraPassword;
            result.ok = manager->loginWithCredentials(request, &errorMessage);
        } else {
            result.ok = manager->unlockWithPassword(password, &errorMessage);
        }

        result.errorMessage = errorMessage;
        return result;
    }));

    password.fill(QChar('\0'));
    extraPassword.fill(QChar('\0'));
}

void QuickWindow::handleAuthFinished()
{
    if (m_resetting)
        return;

    const AuthResult result = m_authWatcher.result();
    setBusy(false);
    setAuthInputsEnabled(true);
    updateAuthTexts();
    m_authPassword->clear();
    m_authExtraPassword->clear();

    if (result.generation != m_vaultGeneration)
        return;

    if (!result.ok) {
        setStatus(result.errorMessage);
        focusAuthPrimaryInput();
        return;
    }

    ++m_vaultGeneration;
    m_intentionallyLocked = false;
    m_preloadRetryTimer.stop();
    m_authCode->clear();
    m_itemsReady = false;
    for (VaultItem& item : m_cachedItems)
        item.scrub();
    m_cachedItems.clear();
    m_refreshTimer.stop();
    m_queuedSearchText.clear();
    hideAuthPanel();
    m_search->setEnabled(true);
    m_search->setPlaceholderText(uiText("비밀번호 검색", "Password search"));
    setStatus({});
    m_search->setFocus();
    startItemLoad();
}

void QuickWindow::copyPassword(QListWidgetItem* item)
{
    if (!item)
        return;

    const QString itemId = item->data(ItemIdRole).toString();
    if (itemId.isEmpty())
        return;

    if (!m_executingVerifiedAction) {
        verifyVaultAction([this, itemId] {
            for (int row = 0; row < m_list->count(); ++row) {
                if (m_list->item(row)->data(ItemIdRole).toString() == itemId) {
                    copyPassword(m_list->item(row));
                    return;
                }
            }
            if (const VaultItem* cachedItem = cachedItemById(itemId)) {
                if (!cachedItem->password.isEmpty()) {
                    recordRecentItem(itemId);
                    copyTextAndHide(
                        cachedItem->password,
                        true,
                        uiText("비밀번호를 복사했습니다. (30초 후 자동 삭제)", "Password copied. (Clears in 30s)"));
                }
            }
        });
        return;
    }

    QString errorMessage;
    QString password;
    if (const VaultItem* cachedItem = cachedItemById(itemId)) {
        password = cachedItem->password;
        if (password.isEmpty()) {
            setStatus(uiText("이 항목에는 비밀번호가 없습니다.", "This item has no password."));
            return;
        }
    } else {
        password = m_passwordManager->getPassword(itemId, &errorMessage);
    }

    if (!errorMessage.isEmpty()) {
        scrubSecretString(password);
        setStatus(errorMessage);
        return;
    }

    if (!password.isEmpty()) {
        recordRecentItem(itemId);
        copyTextAndHide(
            password,
            true,
            uiText("비밀번호를 복사했습니다. (30초 후 자동 삭제)", "Password copied. (Clears in 30s)"));
        scrubSecretString(password);
    }
}

void QuickWindow::copyUsername(QListWidgetItem* item)
{
    if (!item)
        return;

    const QString itemId = item->data(ItemIdRole).toString();
    if (itemId.isEmpty())
        return;

    if (!m_executingVerifiedAction) {
        const QString cachedUsernameFallback = item->data(UsernameRole).toString();
        verifyVaultAction([this, itemId, cachedUsernameFallback] {
            for (int row = 0; row < m_list->count(); ++row) {
                if (m_list->item(row)->data(ItemIdRole).toString() == itemId) {
                    copyUsername(m_list->item(row));
                    return;
                }
            }
            if (const VaultItem* cachedItem = cachedItemById(itemId)) {
                const PrimaryCopyValue primary = primaryCopyValue(*cachedItem);
                if (!primary.text.isEmpty()) {
                    recordRecentItem(itemId);
                    copyTextAndHide(
                        primary.text,
                        primary.sensitive,
                        primary.sensitive
                            ? uiText("항목 값을 복사했습니다. (30초 후 자동 삭제)", "Value copied. (Clears in 30s)")
                            : uiText("사용자 이름을 복사했습니다.", "Username copied."));
                    return;
                }
            }
            if (!cachedUsernameFallback.isEmpty()) {
                recordRecentItem(itemId);
                copyTextAndHide(cachedUsernameFallback, false, uiText("사용자 이름을 복사했습니다.", "Username copied."));
            }
        });
        return;
    }

    if (const VaultItem* cachedItem = cachedItemById(itemId)) {
        const PrimaryCopyValue primary = primaryCopyValue(*cachedItem);
        if (!primary.text.isEmpty()) {
            recordRecentItem(itemId);
            copyTextAndHide(
                primary.text,
                primary.sensitive,
                primary.sensitive
                    ? uiText("항목 값을 복사했습니다. (30초 후 자동 삭제)", "Value copied. (Clears in 30s)")
                    : uiText("사용자 이름을 복사했습니다.", "Username copied."));
            return;
        }
        setStatus(uiText("이 항목에는 사용자 이름이 없습니다.", "This item has no username."));
        return;
    }

    const QString cachedUsername = item->data(UsernameRole).toString();
    if (!cachedUsername.isEmpty()) {
        recordRecentItem(itemId);
        copyTextAndHide(cachedUsername, false, uiText("사용자 이름을 복사했습니다.", "Username copied."));
        return;
    }

    QString errorMessage;
    QString username = m_passwordManager->getUsername(itemId, &errorMessage);

    if (!errorMessage.isEmpty()) {
        setStatus(errorMessage);
        return;
    }

    if (username.isEmpty())
        username = item->data(UsernameRole).toString();

    if (!username.isEmpty()) {
        recordRecentItem(itemId);
        copyTextAndHide(username, false, uiText("사용자 이름을 복사했습니다.", "Username copied."));
    }
}

void QuickWindow::copyTotp(QListWidgetItem* item)
{
    if (!item)
        return;

    const QString itemId = item->data(ItemIdRole).toString();
    if (itemId.isEmpty())
        return;

    if (!m_executingVerifiedAction) {
        verifyVaultAction([this, itemId] {
            for (int row = 0; row < m_list->count(); ++row) {
                if (m_list->item(row)->data(ItemIdRole).toString() == itemId) {
                    copyTotp(m_list->item(row));
                    return;
                }
            }
            QString errorMessage;
            const QString totp = totpCodeForItem(itemId, &errorMessage);
            if (!errorMessage.isEmpty()) {
                setStatus(errorMessage);
                return;
            }
            if (!totp.isEmpty()) {
                recordRecentItem(itemId);
                copyTextAndHide(
                    totp,
                    true,
                    uiText("TOTP 코드를 복사했습니다. (30초 후 자동 삭제)", "TOTP code copied. (Clears in 30s)"));
            }
        });
        return;
    }

    QString errorMessage;
    const QString totp = totpCodeForItem(itemId, &errorMessage);

    if (!errorMessage.isEmpty()) {
        setStatus(errorMessage);
        return;
    }

    if (!totp.isEmpty()) {
        recordRecentItem(itemId);
        copyTextAndHide(
            totp,
            true,
            uiText("TOTP 코드를 복사했습니다. (30초 후 자동 삭제)", "TOTP code copied. (Clears in 30s)"));
    }
}

void QuickWindow::openItemWebsite(QListWidgetItem* item)
{
    if (!item)
        return;

    const QString itemId = item->data(ItemIdRole).toString();
    const VaultItem* cached = cachedItemById(itemId);
    if (!cached)
        return;

    QUrl url;
    for (const QString& candidate : cached->uris) {
        url = webUrlForItemUri(candidate);
        if (url.isValid())
            break;
    }
    if (!url.isValid())
        return;

    recordRecentItem(itemId);
    preserveSearchState();
    QDesktopServices::openUrl(url);
    hide();
}

void QuickWindow::showItemDetails(QListWidgetItem* item)
{
    if (!item)
        return;

    const QString itemId = item->data(ItemIdRole).toString();
    if (itemId.isEmpty())
        return;
    if (!m_executingVerifiedAction) {
        verifyVaultAction([this, itemId] {
            for (int row = 0; row < m_list->count(); ++row) {
                if (m_list->item(row)->data(ItemIdRole).toString() == itemId) {
                    showItemDetails(m_list->item(row));
                    break;
                }
            }
        });
        return;
    }
    recordRecentItem(itemId);

    for (QWidget* widget : QApplication::topLevelWidgets()) {
        if (widget && widget->property("peekpassDetailWindow").toBool()
            && widget->property("peekpassItemId").toString() == itemId) {
            widget->show();
            widget->raise();
            widget->activateWindow();
            return;
        }
    }

    const QString fallbackTitle = item->text();
    const quint64 generation = m_vaultGeneration;
    const VaultItem* cachedItem = cachedItemById(itemId);
    const bool hasCachedDetails = cachedItem != nullptr;
    const PasswordManager::ItemDetails cachedDetails = hasCachedDetails
        ? PasswordManager::detailsFromVaultItem(*cachedItem)
        : PasswordManager::ItemDetails();
    const QIcon itemIcon = !item->icon().isNull()
        ? item->icon()
        : (cachedItem ? makeResultIcon(*cachedItem) : QIcon());

    struct DetailsResult
    {
        quint64 generation = 0;
        PasswordManager::ItemDetails details;
        QString errorMessage;
    };

    auto* dialog = new ItemDetailDialog(
        itemId,
        fallbackTitle,
        itemIcon,
        [this](const QString& text, bool sensitive) {
            copyToClipboard(text, sensitive);
        },
        [this](const QString& id, QString* errorMessage) {
            return totpCodeForItem(id, errorMessage);
        },
        nullptr);

    auto* watcher = new QFutureWatcher<DetailsResult>(dialog);
    QObject::connect(watcher, &QFutureWatcher<DetailsResult>::finished, dialog,
        [watcher, dialog, this] {
            const DetailsResult result = watcher->result();

            if (result.generation != m_vaultGeneration || !m_passwordManager->hasSession()) {
                watcher->deleteLater();
                dialog->close();
                return;
            }

            if (!result.errorMessage.isEmpty()) {
                dialog->setError(result.errorMessage);
                watcher->deleteLater();
                return;
            }

            dialog->setDetails(result.details);
            watcher->deleteLater();
        });

    const auto manager = m_passwordManager;
    watcher->setFuture(QtConcurrent::run([manager, itemId, hasCachedDetails, cachedDetails, generation] {
        DetailsResult result;
        result.generation = generation;
        if (hasCachedDetails) {
            result.details = cachedDetails;
            return result;
        }

        result.details = manager->getItemDetails(itemId, &result.errorMessage);
        return result;
    }));

    QScreen* screen = QGuiApplication::screenAt(frameGeometry().center());
    if (!screen)
        screen = QGuiApplication::primaryScreen();

    if (screen) {
        const QRect available = screen->availableGeometry();
        const QRect quickRect = frameGeometry();
        constexpr int Gap = 12;
        int x = quickRect.right() + Gap;
        if (x + dialog->width() > available.right() + 1)
            x = quickRect.left() - Gap - dialog->width();
        if (x < available.left())
            x = available.center().x() - dialog->width() / 2;

        const int maxY = qMax(available.top(), available.bottom() - dialog->height() + 1);
        const int y = qBound(available.top(), quickRect.top(), maxY);
        dialog->move(qBound(available.left(), x, qMax(available.left(), available.right() - dialog->width() + 1)), y);
    }

    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void QuickWindow::closeDetailWindows()
{
    const QWidgetList topLevelWidgets = QApplication::topLevelWidgets();
    for (QWidget* widget : topLevelWidgets) {
        if (widget && widget->property("peekpassDetailWindow").toBool())
            widget->close();
    }
}

void QuickWindow::copyToClipboard(const QString& text, bool sensitive)
{
    if (text.isEmpty())
        return;
    if (!m_executingVerifiedAction) {
        verifyVaultAction([this, text, sensitive] { copyToClipboard(text, sensitive); });
        return;
    }

    QClipboard* clipboard = QApplication::clipboard();
    if (!clipboard)
        return;

    auto* mimeData = new QMimeData;
    mimeData->setText(text);

#ifdef Q_OS_WIN
    if (sensitive) {
        const QByteArray disabledFlag(sizeof(quint32), '\0');
        mimeData->setData(
            QStringLiteral("application/x-qt-windows-mime;value=\"CanIncludeInClipboardHistory\""),
            disabledFlag);
        mimeData->setData(
            QStringLiteral("application/x-qt-windows-mime;value=\"CanUploadToCloudClipboard\""),
            disabledFlag);
    }
#endif

    clipboard->setMimeData(mimeData);
    const quint64 clipboardGeneration = ++m_clipboardGeneration;
    const QByteArray expectedHash = QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha256);
    m_lastCopiedClipboardHash = expectedHash;

    if (!sensitive)
        return;

    QTimer::singleShot(30000, this, [this, clipboardGeneration, expectedHash] {
        QClipboard* currentClipboard = QApplication::clipboard();
        if (!currentClipboard || clipboardGeneration != m_clipboardGeneration)
            return;

        const QByteArray currentHash = QCryptographicHash::hash(
            currentClipboard->text().toUtf8(),
            QCryptographicHash::Sha256);
        if (currentHash == expectedHash) {
            currentClipboard->clear();
            m_lastCopiedClipboardHash.clear();
        }
    });
}

void QuickWindow::copyTextAndHide(const QString& text, bool sensitive, const QString& notificationMessage)
{
    if (!m_executingVerifiedAction) {
        verifyVaultAction([this, text, sensitive, notificationMessage] {
            copyTextAndHide(text, sensitive, notificationMessage);
        });
        return;
    }
    preserveSearchState();
    copyToClipboard(text, sensitive);
    hide();
    if (AppSettings::notifyOnCopyEnabled() && m_notificationCallback && !notificationMessage.isEmpty())
        m_notificationCallback(notificationMessage);
}

void QuickWindow::setStatus(const QString& message)
{
    const QString compactMessage = compactStatusText(message);
    const QString cleanMessage = cleanStatusText(message);

    if (m_search && m_search->isVisible() && m_authMode == AuthMode::None) {
        if (m_search->text().trimmed().isEmpty()) {
            QString searchMessage = compactMessage;
            constexpr qsizetype MaxSearchPlaceholderLength = 70;
            if (searchMessage.size() > MaxSearchPlaceholderLength)
                searchMessage = searchMessage.left(MaxSearchPlaceholderLength - 3).trimmed() + QStringLiteral("...");
            m_search->setPlaceholderText(compactMessage.isEmpty()
                    ? uiText("비밀번호 검색", "Password search")
                    : searchMessage);
            m_search->setToolTip(cleanMessage == compactMessage ? QString() : cleanMessage);
            m_status->clear();
            m_status->setVisible(false);
            updateWindowSize();
            return;
        }

        m_search->setPlaceholderText(uiText("비밀번호 검색", "Password search"));
        m_search->setToolTip(QString());
    }

    m_status->setText(compactMessage);
    m_status->setToolTip(cleanMessage == compactMessage ? QString() : cleanMessage);
    m_status->setVisible(!compactMessage.isEmpty());
    updateWindowSize();
}

QListWidgetItem* QuickWindow::currentItem() const
{
    return m_list ? m_list->currentItem() : nullptr;
}

void QuickWindow::moveToConfiguredPosition()
{
    QScreen* screen = nullptr;
    if (AppSettings::customWindowPositionEnabled())
        screen = QGuiApplication::screenAt(QPoint(AppSettings::windowPositionX(), AppSettings::windowPositionY()));
    else
        screen = QGuiApplication::screenAt(QCursor::pos());

    if (!screen)
        screen = QGuiApplication::primaryScreen();

    if (!screen)
        return;

    const QRect available = screen->availableGeometry();
    const QPoint centered = available.center() - rect().center();
    int x = centered.x();
    int y = centered.y();

    if (AppSettings::customWindowPositionEnabled()) {
        x = AppSettings::windowPositionX();
        y = AppSettings::windowPositionY();
    }

    const int maxX = qMax(available.left(), available.right() - width() + 1);
    const int maxY = qMax(available.top(), available.bottom() - height() + 1);
    move(qBound(available.left(), x, maxX), qBound(available.top(), y, maxY));
}
