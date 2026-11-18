#pragma once

#include "PasswordManager.h"

#include <functional>

#include <QDialog>
#include <QIcon>
#include <QPoint>
#include <QString>
#include <QTimer>
#include <QVector>

class QGridLayout;
class QLabel;
class QMouseEvent;
class QProgressBar;
class QPushButton;
class QResizeEvent;
class QVBoxLayout;

class ItemDetailDialog : public QDialog
{
public:
    using CopyCallback = std::function<void(const QString& text, bool sensitive)>;
    using TotpFallbackCallback = std::function<QString(const QString& itemId, QString* errorMessage)>;

    ItemDetailDialog(const QString& itemId,
        const QString& fallbackTitle,
        const QIcon& itemIcon,
        CopyCallback copyCallback,
        TotpFallbackCallback totpFallback,
        QWidget* parent = nullptr);
    ~ItemDetailDialog() override;

    QString itemId() const;
    void setDetails(const PasswordManager::ItemDetails& details);
    void setError(const QString& errorMessage);
    void applyThemeStyle();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    struct LiveTotpBinding
    {
        QString secretOrUri;
        bool useItemFallback = false;
        QLabel* codeLabel = nullptr;
        QLabel* timerLabel = nullptr;
    };

    void updateWindowMask();
    void rebuildContent();
    void updateLiveTotpRows();

    QString m_itemId;
    QString m_fallbackTitle;
    QIcon m_itemIcon;
    CopyCallback m_copyCallback;
    TotpFallbackCallback m_totpFallback;
    PasswordManager::ItemDetails m_details;
    bool m_hasDetails = false;
    QString m_errorMessage;

    QVBoxLayout* m_contentLayout = nullptr;
    QLabel* m_loadingLabel = nullptr;
    QProgressBar* m_loadingBar = nullptr;
    QVector<LiveTotpBinding> m_liveTotpBindings;
    QTimer m_totpTimer;

    bool m_dragging = false;
    QPoint m_dragOffset;
};
