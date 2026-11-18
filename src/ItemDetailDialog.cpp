#include "ItemDetailDialog.h"

#include "AppSettings.h"
#include "I18n.h"
#include "TotpGenerator.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QShortcut>
#include <QStyle>
#include <QUrl>
#include <QVBoxLayout>
#include <QtGlobal>

namespace
{
QString uiText(const char* ko, const char* en)
{
    return I18n::translate("QuickWindow", en, ko);
}

bool isLightTheme()
{
    return AppSettings::theme() == QStringLiteral("light");
}

QString detailDialogStyleSheet()
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
            QLabel, QPushButton {
                font-family: "%1";
            }
            QLabel {
                color: #24292f;
            }
            #detailWindowTitle {
                color: #57606a;
                font-size: 13px;
                font-weight: 800;
            }
            #detailHeader {
                color: #111827;
                font-size: 18px;
                font-weight: 800;
            }
            #detailBadge {
                background: #eaeef2;
                color: #57606a;
                border: 1px solid #d0d7de;
                border-radius: 5px;
                padding: 2px 8px;
                font-size: 11px;
                font-weight: 700;
            }
            #detailSubtle {
                color: #57606a;
                font-size: 12px;
            }
            #detailCard {
                background: #ffffff;
                border: 1px solid #d0d7de;
                border-radius: 8px;
            }
            #detailSectionTitle {
                color: #111827;
                font-size: 13px;
                font-weight: 800;
            }
            #detailLabel {
                color: #57606a;
                font-size: 12px;
                font-weight: 700;
                min-width: 92px;
            }
            #detailValue {
                color: #111827;
                font-size: 13px;
                font-weight: 700;
            }
            #detailMonoValue {
                color: #111827;
                font-family: Consolas, "Cascadia Mono", monospace;
                font-size: 13px;
                font-weight: 700;
            }
            #detailTotpCode {
                color: #0969da;
                font-family: Consolas, "Cascadia Mono", monospace;
                font-size: 16px;
                font-weight: 800;
                letter-spacing: 1px;
            }
            #detailTotpTimer {
                background: #eef6ff;
                color: #0969da;
                border: 1px solid #b6d7ff;
                border-radius: 5px;
                padding: 2px 7px;
                font-size: 11px;
                font-weight: 700;
            }
            #detailTotpTimerWarning {
                background: #fff1f1;
                color: #b42318;
                border: 1px solid #f0b8b8;
                border-radius: 5px;
                padding: 2px 7px;
                font-size: 11px;
                font-weight: 700;
            }
            #detailLoading {
                color: #b42318;
                font-size: 12px;
            }
            QScrollArea {
                background: transparent;
                border: 0;
            }
            QScrollArea > QWidget > QWidget {
                background: transparent;
            }
            QScrollBar:vertical {
                background: transparent;
                width: 8px;
                margin: 2px;
            }
            QScrollBar::handle:vertical {
                background: #c5cbd3;
                min-height: 24px;
                border-radius: 2px;
            }
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
                height: 0;
            }
            QPlainTextEdit {
                background: #f8f9fa;
                color: #24292f;
                border: 1px solid #d0d7de;
                border-radius: 6px;
                padding: 8px;
                selection-background-color: #0969da;
                selection-color: #ffffff;
                font-family: Consolas, "Cascadia Mono", monospace;
                font-size: 12px;
            }
            QProgressBar {
                background: rgba(0, 0, 0, 35);
                border: 0;
                border-radius: 1px;
                max-height: 3px;
            }
            QProgressBar::chunk {
                background: #218bff;
                border-radius: 1px;
            }
            QPushButton {
                background: #0969da;
                color: #ffffff;
                border: 1px solid #218bff;
                border-radius: 6px;
                padding: 5px 10px;
                font-size: 12px;
                font-weight: 700;
            }
            QPushButton:hover {
                background: #0757b8;
            }
            QPushButton#detailSecondaryButton, QPushButton#detailCloseButton {
                background: #f3f4f6;
                color: #24292f;
                border: 1px solid #d0d7de;
            }
            QPushButton#detailSecondaryButton:hover, QPushButton#detailCloseButton:hover {
                background: #eaeef2;
            }
            QPushButton#detailCloseIconButton {
                background: transparent;
                color: #57606a;
                border: 1px solid transparent;
                border-radius: 6px;
                padding: 2px 8px;
                font-size: 14px;
                font-weight: 800;
            }
            QPushButton#detailCloseIconButton:hover {
                background: #eaeef2;
                color: #111827;
            }
        )").arg(fontFamily);
    }

    return QStringLiteral(R"(
        QDialog {
            background: #202124;
            border: 1px solid #0b0c0f;
            color: #e8eaed;
            font-family: "%1";
        }
        QLabel, QPushButton {
            font-family: "%1";
        }
        QLabel {
            color: #e8eaed;
        }
        #detailWindowTitle {
            color: #aeb4bf;
            font-size: 13px;
            font-weight: 800;
        }
        #detailHeader {
            color: #ffffff;
            font-size: 18px;
            font-weight: 800;
        }
        #detailBadge {
            background: #252a30;
            color: #c5cbd3;
            border: 1px solid #3a3f46;
            border-radius: 5px;
            padding: 2px 8px;
            font-size: 11px;
            font-weight: 700;
        }
        #detailSubtle {
            color: #aeb4bf;
            font-size: 12px;
        }
        #detailCard {
            background: #15171a;
            border: 1px solid #3a3f46;
            border-radius: 8px;
        }
        #detailSectionTitle {
            color: #ffffff;
            font-size: 13px;
            font-weight: 800;
        }
        #detailLabel {
            color: #aeb4bf;
            font-size: 12px;
            font-weight: 700;
            min-width: 92px;
        }
        #detailValue {
            color: #ffffff;
            font-size: 13px;
            font-weight: 700;
        }
        #detailMonoValue {
            color: #ffffff;
            font-family: Consolas, "Cascadia Mono", monospace;
            font-size: 13px;
            font-weight: 700;
        }
        #detailTotpCode {
            color: #5bd6ff;
            font-family: Consolas, "Cascadia Mono", monospace;
            font-size: 16px;
            font-weight: 800;
            letter-spacing: 1px;
        }
        #detailTotpTimer {
            background: #192638;
            color: #6bb6ff;
            border: 1px solid #2b4c7e;
            border-radius: 5px;
            padding: 2px 7px;
            font-size: 11px;
            font-weight: 700;
        }
        #detailTotpTimerWarning {
            background: #3a1d22;
            color: #f0c7d2;
            border: 1px solid #8c3b4a;
            border-radius: 5px;
            padding: 2px 7px;
            font-size: 11px;
            font-weight: 700;
        }
        #detailLoading {
            color: #f0c7d2;
            font-size: 12px;
        }
        QScrollArea {
            background: transparent;
            border: 0;
        }
        QScrollArea > QWidget > QWidget {
            background: transparent;
        }
        QScrollBar:vertical {
            background: transparent;
            width: 8px;
            margin: 2px;
        }
        QScrollBar::handle:vertical {
            background: #3a3f46;
            min-height: 24px;
            border-radius: 2px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
        QPlainTextEdit {
            background: #101214;
            color: #f1f3f4;
            border: 1px solid #3a3f46;
            border-radius: 6px;
            padding: 8px;
            selection-background-color: #1f8cff;
            selection-color: #ffffff;
            font-family: Consolas, "Cascadia Mono", monospace;
            font-size: 12px;
        }
        QProgressBar {
            background: rgba(255, 255, 255, 35);
            border: 0;
            border-radius: 1px;
            max-height: 3px;
        }
        QProgressBar::chunk {
            background: #49a0ff;
            border-radius: 1px;
        }
        QPushButton {
            background: #1f72c7;
            color: #ffffff;
            border: 1px solid #49a0ff;
            border-radius: 6px;
            padding: 5px 10px;
            font-size: 12px;
            font-weight: 700;
        }
        QPushButton:hover {
            background: #2484e7;
        }
        QPushButton#detailSecondaryButton, QPushButton#detailCloseButton {
            background: #2f343a;
            color: #f1f3f4;
            border: 1px solid #4b5563;
        }
        QPushButton#detailSecondaryButton:hover, QPushButton#detailCloseButton:hover {
            background: #3a4048;
        }
        QPushButton#detailCloseIconButton {
            background: transparent;
            color: #aeb4bf;
            border: 1px solid transparent;
            border-radius: 6px;
            padding: 2px 8px;
            font-size: 14px;
            font-weight: 800;
        }
        QPushButton#detailCloseIconButton:hover {
            background: #2f343a;
            color: #ffffff;
        }
    )").arg(fontFamily);
}

QGridLayout* makeDetailForm()
{
    auto* form = new QGridLayout;
    form->setHorizontalSpacing(10);
    form->setVerticalSpacing(9);
    form->setColumnStretch(1, 1);
    return form;
}

QString maskedSecretValue(const QString& value, int visibleSuffixLength = 0)
{
    if (value.isEmpty())
        return {};

    if (value.size() <= visibleSuffixLength)
        visibleSuffixLength = 0;

    const int hiddenLength = value.size() - visibleSuffixLength;
    const int maskLength = qBound(4, hiddenLength, 16);
    QString masked(maskLength, QChar(0x2022));
    if (visibleSuffixLength > 0)
        masked += QStringLiteral(" ") + value.right(visibleSuffixLength);
    return masked;
}

QUrl normalizedItemUrl(const QString& value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty())
        return {};

    QUrl url(trimmed);
    if (url.scheme().isEmpty() || url.host().isEmpty())
        url = QUrl(QStringLiteral("https://") + trimmed);

    return url;
}

QUrl webUrlForItemUri(const QString& value)
{
    const QUrl url = normalizedItemUrl(value);
    const QString scheme = url.scheme().toLower();
    if ((scheme != QStringLiteral("http") && scheme != QStringLiteral("https")) || url.host().isEmpty())
        return {};

    return url;
}

QString linkedSingleWebsiteMarkup(const QString& uri)
{
    const QString displayText = uri.trimmed();
    if (displayText.isEmpty())
        return {};

    const QUrl url = webUrlForItemUri(displayText);
    if (!url.isValid())
        return displayText.toHtmlEscaped();

    const QString linkColor = isLightTheme() ? QStringLiteral("#0969da") : QStringLiteral("#6bb6ff");
    const QString href = QString::fromUtf8(url.toEncoded()).toHtmlEscaped();
    return QStringLiteral("<a style=\"color:%1; text-decoration:none;\" href=\"%2\">%3</a>")
        .arg(linkColor, href, displayText.toHtmlEscaped());
}

QString formatTimestamp(const QString& rawTimestamp)
{
    const QString trimmed = rawTimestamp.trimmed();
    if (trimmed.isEmpty())
        return {};

    QDateTime dt = QDateTime::fromString(trimmed, Qt::ISODateWithMs);
    if (!dt.isValid())
        dt = QDateTime::fromString(trimmed, Qt::ISODate);
    if (!dt.isValid())
        return trimmed;

    return dt.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

QString compactLongToken(const QString& value, int maxLength = 34)
{
    const QString trimmed = value.trimmed();
    if (trimmed.size() <= maxLength || trimmed.contains(QLatin1Char(' ')) || trimmed.contains(QLatin1Char('\n')))
        return value;

    const int head = (maxLength - 1) / 2 + 2;
    const int tail = (maxLength - 1) - head;
    return trimmed.left(head) + QStringLiteral("\u2026") + trimmed.right(tail);
}

bool isRedundantProtonSection(const QString& title, int typeCode)
{
    if (title == uiText("항목 데이터", "Item data")
        || title == uiText("Proton Pass 메타데이터", "Proton Pass metadata")
        || title == QStringLiteral("Item data")
        || title == QStringLiteral("Proton Pass metadata")) {
        return true;
    }

    if (typeCode == 1 && (title == uiText("로그인", "Login") || title == QStringLiteral("Login")))
        return true;
    if (typeCode == 2 && (title == uiText("보안 메모", "Secure note") || title == QStringLiteral("Secure note")))
        return true;
    if (typeCode == 3 && (title == uiText("카드", "Card") || title == QStringLiteral("Card")))
        return true;
    if (typeCode == 4 && (title == uiText("신원", "Identity") || title == QStringLiteral("Identity")))
        return true;
    if (typeCode == 5 && (title == uiText("SSH 키", "SSH key") || title == QStringLiteral("SSH key")))
        return true;

    return false;
}
}

ItemDetailDialog::ItemDetailDialog(const QString& itemId,
    const QString& fallbackTitle,
    const QIcon& itemIcon,
    CopyCallback copyCallback,
    TotpFallbackCallback totpFallback,
    QWidget* parent)
    : QDialog(parent)
    , m_itemId(itemId)
    , m_fallbackTitle(fallbackTitle)
    , m_itemIcon(itemIcon)
    , m_copyCallback(std::move(copyCallback))
    , m_totpFallback(std::move(totpFallback))
{
    setAttribute(Qt::WA_DeleteOnClose);
    setProperty("peekpassDetailWindow", true);
    setProperty("peekpassItemId", m_itemId);
    setWindowTitle(uiText("상세보기", "Details"));
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    resize(560, 660);
    setMinimumSize(460, 420);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(18, 14, 18, 16);
    root->setSpacing(12);

    auto* headerRow = new QHBoxLayout;
    headerRow->setContentsMargins(0, 0, 0, 0);
    auto* windowTitleLabel = new QLabel(uiText("상세보기", "Details"), this);
    windowTitleLabel->setObjectName("detailWindowTitle");
    windowTitleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    headerRow->addWidget(windowTitleLabel);
    headerRow->addStretch(1);

    auto* closeIconButton = new QPushButton(QStringLiteral("\u00D7"), this);
    closeIconButton->setObjectName("detailCloseIconButton");
    closeIconButton->setToolTip(uiText("닫기 (Esc)", "Close (Esc)"));
    connect(closeIconButton, &QPushButton::clicked, this, &QDialog::close);
    headerRow->addWidget(closeIconButton);
    root->addLayout(headerRow);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* content = new QWidget(scrollArea);
    m_contentLayout = new QVBoxLayout(content);
    m_contentLayout->setContentsMargins(0, 0, 4, 0);
    m_contentLayout->setSpacing(12);
    scrollArea->setWidget(content);
    root->addWidget(scrollArea, 1);

    m_loadingLabel = new QLabel(uiText("상세 정보를 불러오는 중...", "Loading item details..."), content);
    m_loadingLabel->setObjectName("detailLoading");
    m_loadingBar = new QProgressBar(content);
    m_loadingBar->setRange(0, 0);
    m_loadingBar->setTextVisible(false);
    m_loadingBar->setFixedHeight(3);
    m_contentLayout->addWidget(m_loadingLabel);
    m_contentLayout->addWidget(m_loadingBar);
    m_contentLayout->addStretch(1);

    auto* footerRow = new QHBoxLayout;
    footerRow->addStretch(1);
    auto* closeButton = new QPushButton(uiText("닫기", "Close"), this);
    closeButton->setObjectName("detailCloseButton");
    connect(closeButton, &QPushButton::clicked, this, &QDialog::close);
    footerRow->addWidget(closeButton);
    root->addLayout(footerRow);

    auto* escapeShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(escapeShortcut, &QShortcut::activated, this, &QDialog::close);

    m_totpTimer.setInterval(1000);
    connect(&m_totpTimer, &QTimer::timeout, this, [this] {
        updateLiveTotpRows();
    });

    applyThemeStyle();
}

ItemDetailDialog::~ItemDetailDialog()
{
    m_totpTimer.stop();
    m_details.scrub();
}

QString ItemDetailDialog::itemId() const
{
    return m_itemId;
}

void ItemDetailDialog::setDetails(const PasswordManager::ItemDetails& details)
{
    m_details.scrub();
    m_details = details;
    m_hasDetails = true;
    m_errorMessage.clear();
    rebuildContent();
}

void ItemDetailDialog::setError(const QString& errorMessage)
{
    m_details.scrub();
    m_hasDetails = false;
    m_errorMessage = errorMessage;
    rebuildContent();
}

void ItemDetailDialog::applyThemeStyle()
{
    setStyleSheet(detailDialogStyleSheet());
    updateWindowMask();
    if (m_hasDetails || !m_errorMessage.isEmpty())
        rebuildContent();
}

void ItemDetailDialog::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
        return;
    }

    QDialog::mousePressEvent(event);
}

void ItemDetailDialog::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging && event->buttons().testFlag(Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragOffset);
        event->accept();
        return;
    }

    QDialog::mouseMoveEvent(event);
}

void ItemDetailDialog::mouseReleaseEvent(QMouseEvent* event)
{
    m_dragging = false;
    QDialog::mouseReleaseEvent(event);
}

void ItemDetailDialog::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    updateWindowMask();
}

void ItemDetailDialog::updateWindowMask()
{
    if (!AppSettings::roundedCorners()) {
        clearMask();
        return;
    }

    QPainterPath path;
    path.addRoundedRect(rect(), 12, 12);
    setMask(QRegion(path.toFillPolygon().toPolygon()));
}

void ItemDetailDialog::rebuildContent()
{
    m_totpTimer.stop();
    m_liveTotpBindings.clear();

    while (QLayoutItem* item = m_contentLayout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }

    if (!m_errorMessage.isEmpty()) {
        auto* errorLabel = new QLabel(m_errorMessage, this);
        errorLabel->setObjectName("detailLoading");
        errorLabel->setWordWrap(true);
        m_contentLayout->addWidget(errorLabel);
        m_contentLayout->addStretch(1);
        return;
    }

    if (!m_hasDetails)
        return;

    const auto makeCard = [this](const QString& title, QWidget* headerRightWidget = nullptr) {
        auto* card = new QWidget(this);
        card->setObjectName("detailCard");
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(16, 14, 16, 14);
        cardLayout->setSpacing(10);
        if (!title.isEmpty() || headerRightWidget) {
            auto* headerLayout = new QHBoxLayout;
            headerLayout->setContentsMargins(0, 0, 0, 0);
            headerLayout->setSpacing(8);
            if (!title.isEmpty()) {
                auto* titleLabel = new QLabel(title, card);
                titleLabel->setObjectName("detailSectionTitle");
                headerLayout->addWidget(titleLabel);
            }
            headerLayout->addStretch(1);
            if (headerRightWidget) {
                headerRightWidget->setParent(card);
                headerLayout->addWidget(headerRightWidget);
            }
            cardLayout->addLayout(headerLayout);
        }
        m_contentLayout->addWidget(card);
        return cardLayout;
    };

    const auto triggerCopyFeedback = [](QPushButton* button, const QString& defaultText) {
        if (!button)
            return;
        button->setText(uiText("복사됨", "Copied"));
        QTimer::singleShot(1200, button, [button, defaultText] {
            button->setText(defaultText);
        });
    };

    const auto addDetailRow = [this, triggerCopyFeedback](QGridLayout* form,
                                  int row,
                                  const QString& label,
                                  const QString& value,
                                  const QString& copyText = {},
                                  bool sensitive = false,
                                  int visibleSuffixLength = 0) {
        if (value.isEmpty() && copyText.isEmpty())
            return row;

        auto* labelWidget = new QLabel(label, this);
        labelWidget->setObjectName("detailLabel");

        const QString plainValue = copyText.isEmpty() ? value : copyText;
        const QString visibleText = compactLongToken(value, 36);
        const QString maskedText = sensitive
            ? maskedSecretValue(plainValue, visibleSuffixLength)
            : visibleText;

        auto* valueWidget = new QLabel(maskedText, this);
        valueWidget->setObjectName("detailValue");
        valueWidget->setWordWrap(true);
        valueWidget->setMinimumWidth(0);
        valueWidget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        valueWidget->setTextInteractionFlags(Qt::TextSelectableByMouse);
        if (!sensitive && visibleText != plainValue)
            valueWidget->setToolTip(plainValue);

        auto* actionsLayout = new QHBoxLayout;
        actionsLayout->setContentsMargins(0, 0, 0, 0);
        actionsLayout->setSpacing(6);
        actionsLayout->addStretch(1);

        if (sensitive && !plainValue.isEmpty()) {
            auto* toggleButton = new QPushButton(uiText("보기", "Show"), this);
            toggleButton->setObjectName("detailSecondaryButton");
            const QString revealedDisplay = compactLongToken(plainValue, 36);
            connect(toggleButton, &QPushButton::clicked, valueWidget, [valueWidget, toggleButton, plainValue, revealedDisplay, maskedText] {
                const bool currentlyRevealed = toggleButton->property("peekpassRevealed").toBool();
                const bool nextRevealed = !currentlyRevealed;
                toggleButton->setProperty("peekpassRevealed", nextRevealed);
                toggleButton->setText(nextRevealed ? uiText("숨기기", "Hide") : uiText("보기", "Show"));
                valueWidget->setObjectName(nextRevealed ? "detailMonoValue" : "detailValue");
                valueWidget->setText(nextRevealed ? revealedDisplay : maskedText);
                valueWidget->setToolTip(nextRevealed && revealedDisplay != plainValue ? plainValue : QString());
                valueWidget->style()->unpolish(valueWidget);
                valueWidget->style()->polish(valueWidget);
            });
            actionsLayout->addWidget(toggleButton);
        }

        if (!copyText.isEmpty()) {
            const QString copyLabel = uiText("복사", "Copy");
            auto* copyButton = new QPushButton(copyLabel, this);
            connect(copyButton, &QPushButton::clicked, copyButton, [this, copyButton, copyLabel, copyText, sensitive, triggerCopyFeedback] {
                if (m_copyCallback)
                    m_copyCallback(copyText, sensitive);
                triggerCopyFeedback(copyButton, copyLabel);
            });
            actionsLayout->addWidget(copyButton);
        }

        form->addWidget(labelWidget, row, 0, Qt::AlignVCenter);
        form->addWidget(valueWidget, row, 1, Qt::AlignVCenter);
        if (actionsLayout->count() > 1)
            form->addLayout(actionsLayout, row, 2, Qt::AlignRight | Qt::AlignVCenter);
        return row + 1;
    };

    const auto addLiveTotpRow = [this, triggerCopyFeedback](QGridLayout* form,
                                    int row,
                                    const QString& label,
                                    const QString& totpSecretOrUri,
                                    bool useItemFallback) {
        if (totpSecretOrUri.trimmed().isEmpty())
            return row;

        auto* labelWidget = new QLabel(label, this);
        labelWidget->setObjectName("detailLabel");

        auto* valueRow = new QHBoxLayout;
        valueRow->setContentsMargins(0, 0, 0, 0);
        valueRow->setSpacing(8);

        auto* codeLabel = new QLabel(this);
        codeLabel->setObjectName("detailTotpCode");
        codeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

        auto* timerLabel = new QLabel(this);
        timerLabel->setObjectName("detailTotpTimer");
        timerLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

        valueRow->addWidget(codeLabel);
        valueRow->addWidget(timerLabel);
        valueRow->addStretch(1);

        const QString copyLabel = uiText("코드 복사", "Copy code");
        auto* copyButton = new QPushButton(copyLabel, this);
        connect(copyButton, &QPushButton::clicked, copyButton,
            [this, copyButton, copyLabel, totpSecretOrUri, useItemFallback, triggerCopyFeedback] {
                QString errorMessage;
                QString code = TotpGenerator::generateTotpCode(totpSecretOrUri, &errorMessage);
                if (code.isEmpty() && useItemFallback && m_totpFallback)
                    code = m_totpFallback(m_itemId, &errorMessage);
                if (code.isEmpty()) {
                    copyButton->setToolTip(errorMessage);
                    return;
                }
                if (m_copyCallback)
                    m_copyCallback(code, true);
                triggerCopyFeedback(copyButton, copyLabel);
            });

        form->addWidget(labelWidget, row, 0, Qt::AlignVCenter);
        form->addLayout(valueRow, row, 1, Qt::AlignVCenter);
        form->addWidget(copyButton, row, 2, Qt::AlignRight | Qt::AlignVCenter);

        m_liveTotpBindings.push_back({totpSecretOrUri, useItemFallback, codeLabel, timerLabel});
        return row + 1;
    };

    const auto addWebsiteRow = [this, triggerCopyFeedback](QGridLayout* form,
                                   int row,
                                   const QString& label,
                                   const QString& uri) {
        const QString trimmed = uri.trimmed();
        if (trimmed.isEmpty())
            return row;

        auto* labelWidget = new QLabel(label, this);
        labelWidget->setObjectName("detailLabel");

        auto* valueWidget = new QLabel(linkedSingleWebsiteMarkup(trimmed), this);
        valueWidget->setObjectName("detailValue");
        valueWidget->setTextFormat(Qt::RichText);
        valueWidget->setWordWrap(true);
        valueWidget->setMinimumWidth(0);
        valueWidget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        valueWidget->setTextInteractionFlags(Qt::TextBrowserInteraction | Qt::TextSelectableByMouse);
        valueWidget->setOpenExternalLinks(false);
        if (trimmed.size() > 38)
            valueWidget->setToolTip(trimmed);
        connect(valueWidget, &QLabel::linkActivated, valueWidget, [](const QString& link) {
            QDesktopServices::openUrl(QUrl(link));
        });

        auto* actionsLayout = new QHBoxLayout;
        actionsLayout->setContentsMargins(0, 0, 0, 0);
        actionsLayout->setSpacing(6);
        actionsLayout->addStretch(1);

        const QUrl webUrl = webUrlForItemUri(trimmed);
        if (webUrl.isValid()) {
            auto* openButton = new QPushButton(uiText("열기", "Open"), this);
            openButton->setObjectName("detailSecondaryButton");
            connect(openButton, &QPushButton::clicked, openButton, [webUrl] {
                QDesktopServices::openUrl(webUrl);
            });
            actionsLayout->addWidget(openButton);
        }

        const QString copyLabel = uiText("복사", "Copy");
        auto* copyButton = new QPushButton(copyLabel, this);
        connect(copyButton, &QPushButton::clicked, copyButton, [this, copyButton, copyLabel, trimmed, triggerCopyFeedback] {
            if (m_copyCallback)
                m_copyCallback(trimmed, false);
            triggerCopyFeedback(copyButton, copyLabel);
        });
        actionsLayout->addWidget(copyButton);

        form->addWidget(labelWidget, row, 0, Qt::AlignVCenter);
        form->addWidget(valueWidget, row, 1, Qt::AlignVCenter);
        form->addLayout(actionsLayout, row, 2, Qt::AlignRight | Qt::AlignVCenter);
        return row + 1;
    };

    const QString itemTitle = m_details.name.isEmpty() ? m_fallbackTitle : m_details.name;
    const bool protonPassDetails = m_details.providerId == QStringLiteral("protonpass");
    const QString providerName = protonPassDetails ? QStringLiteral("Proton Pass") : QStringLiteral("Bitwarden");

    // 1. Header Card
    {
        QVBoxLayout* titleCard = makeCard({});
        auto* headerRowLayout = new QHBoxLayout;
        headerRowLayout->setContentsMargins(0, 0, 0, 0);
        headerRowLayout->setSpacing(10);

        if (!m_itemIcon.isNull()) {
            auto* iconLabel = new QLabel(this);
            iconLabel->setPixmap(m_itemIcon.pixmap(28, 28));
            iconLabel->setFixedSize(28, 28);
            headerRowLayout->addWidget(iconLabel, 0, Qt::AlignVCenter);
        }

        auto* titleLabel = new QLabel(itemTitle, this);
        titleLabel->setObjectName("detailHeader");
        titleLabel->setWordWrap(true);
        titleLabel->setMinimumWidth(0);
        titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        titleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        headerRowLayout->addWidget(titleLabel, 1, Qt::AlignVCenter);

        const QString badgeText = m_details.type.isEmpty()
            ? providerName
            : QStringLiteral("%1 · %2").arg(providerName, m_details.type);
        auto* badgeLabel = new QLabel(badgeText, this);
        badgeLabel->setObjectName("detailBadge");
        badgeLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        headerRowLayout->addWidget(badgeLabel, 0, Qt::AlignRight | Qt::AlignVCenter);

        titleCard->addLayout(headerRowLayout);
    }

    // 2. Primary Type-Specific Card (Unified for both Bitwarden & Proton Pass)
    if (m_details.typeCode == 1 || (!m_details.username.isEmpty() || !m_details.password.isEmpty() || !m_details.totp.isEmpty())) {
        QVBoxLayout* credentialsLayout = makeCard(uiText("로그인 정보", "Login credentials"));
        QGridLayout* credentialsForm = makeDetailForm();
        int row = 0;
        row = addDetailRow(credentialsForm, row, uiText("사용자 이름", "Username"), m_details.username, m_details.username);
        row = addDetailRow(credentialsForm, row, uiText("비밀번호", "Password"), m_details.password, m_details.password, true);
        if (!m_details.totp.isEmpty())
            row = addLiveTotpRow(credentialsForm, row, uiText("TOTP", "TOTP"), m_details.totp, true);
        if (row == 0)
            addDetailRow(credentialsForm, row, uiText("상태", "Status"), uiText("로그인 필드 없음", "No login fields"));
        credentialsLayout->addLayout(credentialsForm);
    } else if (m_details.typeCode == 3) {
        QVBoxLayout* cardLayout = makeCard(uiText("카드 정보", "Card details"));
        QGridLayout* cardForm = makeDetailForm();
        int row = 0;
        row = addDetailRow(cardForm, row, uiText("카드 소유자", "Cardholder"), m_details.card.cardholderName, m_details.card.cardholderName);
        row = addDetailRow(cardForm, row, uiText("브랜드", "Brand"), m_details.card.brand, m_details.card.brand);
        row = addDetailRow(cardForm, row, uiText("카드 번호", "Card number"), m_details.card.number, m_details.card.number, true, 4);
        QStringList expirationParts;
        if (!m_details.card.expirationMonth.isEmpty())
            expirationParts.push_back(m_details.card.expirationMonth);
        if (!m_details.card.expirationYear.isEmpty())
            expirationParts.push_back(m_details.card.expirationYear);
        const QString expiration = expirationParts.join(QLatin1Char('/'));
        row = addDetailRow(cardForm, row, uiText("만료일", "Expiration"), expiration, expiration);
        row = addDetailRow(cardForm, row, uiText("보안 코드", "Security code"), m_details.card.securityCode, m_details.card.securityCode, true);
        if (row == 0)
            addDetailRow(cardForm, row, uiText("상태", "Status"), uiText("카드 필드 없음", "No card fields"));
        cardLayout->addLayout(cardForm);
    } else if (m_details.typeCode == 4) {
        QVBoxLayout* identityLayout = makeCard(uiText("신원 정보", "Identity details"));
        QGridLayout* identityForm = makeDetailForm();
        int row = 0;
        row = addDetailRow(identityForm, row, uiText("호칭", "Title"), m_details.identity.title, m_details.identity.title);
        row = addDetailRow(identityForm, row, uiText("이름", "First name"), m_details.identity.firstName, m_details.identity.firstName);
        row = addDetailRow(identityForm, row, uiText("중간 이름", "Middle name"), m_details.identity.middleName, m_details.identity.middleName);
        row = addDetailRow(identityForm, row, uiText("성", "Last name"), m_details.identity.lastName, m_details.identity.lastName);
        row = addDetailRow(identityForm, row, uiText("회사", "Company"), m_details.identity.company, m_details.identity.company);
        row = addDetailRow(identityForm, row, uiText("사용자 이름", "Username"), m_details.identity.username, m_details.identity.username);
        row = addDetailRow(identityForm, row, uiText("이메일", "Email"), m_details.identity.email, m_details.identity.email);
        row = addDetailRow(identityForm, row, uiText("전화번호", "Phone"), m_details.identity.phone, m_details.identity.phone);
        row = addDetailRow(identityForm, row, uiText("주소 1", "Address 1"), m_details.identity.address1, m_details.identity.address1);
        row = addDetailRow(identityForm, row, uiText("주소 2", "Address 2"), m_details.identity.address2, m_details.identity.address2);
        row = addDetailRow(identityForm, row, uiText("주소 3", "Address 3"), m_details.identity.address3, m_details.identity.address3);
        row = addDetailRow(identityForm, row, uiText("도시", "City"), m_details.identity.city, m_details.identity.city);
        row = addDetailRow(identityForm, row, uiText("주/도", "State / Province"), m_details.identity.state, m_details.identity.state);
        row = addDetailRow(identityForm, row, uiText("우편번호", "Postal code"), m_details.identity.postalCode, m_details.identity.postalCode);
        row = addDetailRow(identityForm, row, uiText("국가", "Country"), m_details.identity.country, m_details.identity.country);
        row = addDetailRow(identityForm,
            row,
            uiText("주민/사회보장 번호", "Social security number"),
            m_details.identity.socialSecurityNumber,
            m_details.identity.socialSecurityNumber,
            true,
            4);
        row = addDetailRow(identityForm,
            row,
            uiText("여권 번호", "Passport number"),
            m_details.identity.passportNumber,
            m_details.identity.passportNumber,
            true,
            4);
        row = addDetailRow(identityForm,
            row,
            uiText("면허 번호", "License number"),
            m_details.identity.licenseNumber,
            m_details.identity.licenseNumber,
            true,
            4);
        if (row == 0)
            addDetailRow(identityForm, row, uiText("상태", "Status"), uiText("신원 필드 없음", "No identity fields"));
        identityLayout->addLayout(identityForm);
    } else if (m_details.typeCode == 5) {
        QVBoxLayout* sshLayout = makeCard(uiText("SSH 키", "SSH key"));
        QGridLayout* sshForm = makeDetailForm();
        int row = 0;
        row = addDetailRow(sshForm, row, uiText("공개 키", "Public key"), m_details.sshKey.publicKey, m_details.sshKey.publicKey);
        row = addDetailRow(sshForm, row, uiText("지문", "Fingerprint"), m_details.sshKey.fingerprint, m_details.sshKey.fingerprint);
        row = addDetailRow(sshForm, row, uiText("개인 키", "Private key"), m_details.sshKey.privateKey, m_details.sshKey.privateKey, true);
        if (row == 0)
            addDetailRow(sshForm, row, uiText("상태", "Status"), uiText("SSH 키 필드 없음", "No SSH key fields"));
        sshLayout->addLayout(sshForm);
    }

    // 3. Websites Card (with Open + Copy buttons per URL)
    if (!m_details.uris.isEmpty()) {
        QVBoxLayout* urisLayout = makeCard(uiText("웹 사이트", "Websites"));
        QGridLayout* urisForm = makeDetailForm();
        int uriRow = 0;
        for (int index = 0; index < m_details.uris.size(); ++index) {
            const QString label = m_details.uris.size() == 1
                ? QStringLiteral("URL")
                : QStringLiteral("URL %1").arg(index + 1);
            uriRow = addWebsiteRow(urisForm, uriRow, label, m_details.uris.at(index));
        }
        urisLayout->addLayout(urisForm);
    }

    // 4. Custom Fields / Provider Extra Sections
    if (!protonPassDetails && !m_details.customFields.isEmpty()) {
        QVBoxLayout* customLayout = makeCard(uiText("사용자 지정 필드", "Custom fields"));
        QGridLayout* customForm = makeDetailForm();
        int customRow = 0;
        for (const VaultCustomField& field : m_details.customFields) {
            const bool sensitive = field.typeCode == 1;
            QString displayValue = field.value;
            if (field.typeCode == 2) {
                const bool enabled = field.value.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0;
                displayValue = enabled ? uiText("예", "Yes") : uiText("아니요", "No");
            }
            customRow = addDetailRow(
                customForm,
                customRow,
                field.name.isEmpty() ? uiText("사용자 지정", "Custom") : field.name,
                displayValue,
                field.value,
                sensitive);
        }
        customLayout->addLayout(customForm);
    }

    if (protonPassDetails) {
        for (const VaultDetailSection& section : m_details.providerSections) {
            if (isRedundantProtonSection(section.title, m_details.typeCode))
                continue;

            QGridLayout* sectionForm = makeDetailForm();
            int row = 0;
            for (const VaultDetailField& field : section.fields) {
                if (field.totpSecret) {
                    if (field.value == m_details.totp)
                        continue;
                    row = addLiveTotpRow(sectionForm, row, field.name, field.value, false);
                    continue;
                }
                if (field.website) {
                    row = addWebsiteRow(sectionForm, row, field.name, field.value);
                    continue;
                }
                const int visibleTail = field.name.contains(QStringLiteral("number"), Qt::CaseInsensitive) ? 4 : 0;
                row = addDetailRow(sectionForm,
                    row,
                    field.name,
                    field.value,
                    field.value,
                    field.sensitive,
                    visibleTail);
            }
            if (row > 0) {
                QVBoxLayout* sectionLayout = makeCard(section.title);
                sectionLayout->addLayout(sectionForm);
            } else {
                delete sectionForm;
            }
        }
    }

    // 5. Notes / Secure Note Card (with Copy button in card header)
    if (!m_details.notes.isEmpty()) {
        const QString notesTitle = m_details.typeCode == 2
            ? uiText("보안 메모", "Secure note")
            : uiText("메모", "Notes");
        const QString copyLabel = uiText("복사", "Copy");
        auto* copyNotesButton = new QPushButton(copyLabel, this);
        const QString notesText = m_details.notes;
        const bool sensitiveNote = m_details.typeCode == 2;
        connect(copyNotesButton, &QPushButton::clicked, copyNotesButton,
            [this, copyNotesButton, copyLabel, notesText, sensitiveNote, triggerCopyFeedback] {
                if (m_copyCallback)
                    m_copyCallback(notesText, sensitiveNote);
                triggerCopyFeedback(copyNotesButton, copyLabel);
            });

        QVBoxLayout* notesLayout = makeCard(notesTitle, copyNotesButton);
        auto* notesEdit = new QPlainTextEdit(m_details.notes, this);
        notesEdit->setReadOnly(true);
        notesEdit->setMinimumHeight(m_details.typeCode == 2 ? 160 : 88);
        notesLayout->addWidget(notesEdit);
    }

    // 6. Compact Metadata Card
    {
        QVBoxLayout* metaLayout = makeCard(uiText("메타데이터", "Metadata"));
        QGridLayout* metaForm = makeDetailForm();
        int metaRow = 0;
        metaRow = addDetailRow(metaForm, metaRow, uiText("유형", "Type"), m_details.type);
        metaRow = addDetailRow(metaForm, metaRow, uiText("생성일", "Created"), formatTimestamp(m_details.creationDate));
        metaRow = addDetailRow(metaForm, metaRow, uiText("수정일", "Updated"), formatTimestamp(m_details.revisionDate));
        if (!m_details.id.isEmpty()) {
            QString displayId = m_details.id;
            const int sep = displayId.indexOf(QChar(0x1f));
            if (sep > 0)
                displayId = displayId.mid(sep + 1);
            metaRow = addDetailRow(metaForm, metaRow, QStringLiteral("ID"), displayId, displayId);
        }
        metaRow = addDetailRow(metaForm, metaRow, uiText("폴더 ID", "Folder ID"), m_details.folderId, m_details.folderId);
        metaRow = addDetailRow(metaForm, metaRow, uiText("조직 ID", "Organization ID"), m_details.organizationId, m_details.organizationId);
        addDetailRow(metaForm, metaRow, uiText("컬렉션", "Collections"), m_details.collectionIds, m_details.collectionIds);
        metaLayout->addLayout(metaForm);
    }

    m_contentLayout->addStretch(1);

    if (!m_liveTotpBindings.isEmpty()) {
        updateLiveTotpRows();
        m_totpTimer.start();
    }
}

void ItemDetailDialog::updateLiveTotpRows()
{
    for (const LiveTotpBinding& binding : m_liveTotpBindings) {
        if (!binding.codeLabel || !binding.timerLabel)
            continue;

        QString errorMessage;
        TotpCodeInfo info = TotpGenerator::generateTotpInfo(binding.secretOrUri, &errorMessage);
        if (info.code.isEmpty() && binding.useItemFallback && m_totpFallback) {
            info.code = m_totpFallback(m_itemId, &errorMessage);
            const qint64 now = QDateTime::currentSecsSinceEpoch();
            info.period = 30;
            info.remainingSeconds = 30 - static_cast<int>(now % 30);
        }

        if (info.code.isEmpty()) {
            binding.codeLabel->setText(QStringLiteral("------"));
            binding.timerLabel->setVisible(false);
            continue;
        }

        binding.codeLabel->setText(TotpGenerator::formatTotpCode(info.code));
        binding.timerLabel->setVisible(true);
        binding.timerLabel->setText(QStringLiteral("%1s").arg(info.remainingSeconds));
        const char* desiredObjectName = info.remainingSeconds <= 5
            ? "detailTotpTimerWarning"
            : "detailTotpTimer";
        if (binding.timerLabel->objectName() != QLatin1String(desiredObjectName)) {
            binding.timerLabel->setObjectName(QString::fromLatin1(desiredObjectName));
            binding.timerLabel->style()->unpolish(binding.timerLabel);
            binding.timerLabel->style()->polish(binding.timerLabel);
        }
    }
}
