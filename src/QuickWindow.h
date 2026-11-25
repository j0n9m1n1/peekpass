#pragma once

#include "PasswordManager.h"

#include <functional>
#include <memory>

#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QHash>
#include <QIcon>
#include <QPoint>
#include <QSet>
#include <QTimer>
#include <QUrl>
#include <QWidget>

class QAction;
class QLabel;
class QComboBox;
class QGridLayout;
class QHBoxLayout;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QMouseEvent;
class QNetworkAccessManager;
class QNetworkReply;
class QProgressBar;
class QPushButton;
class QResizeEvent;

class QuickWindow : public QWidget
{
public:
    explicit QuickWindow(QWidget* parent = nullptr);
    ~QuickWindow() override;

    void preloadVault();
    void showQuick();
    void showSettings();
    void syncVaultNow();
    QString passwordManagerName() const;
    bool lockVault(QString* errorMessage = nullptr);
    bool logoutVault(QString* errorMessage = nullptr);
    void applySettings();
    void setSettingsChangedCallback(std::function<void()> callback);
    void setSettingsDialogStateCallback(std::function<void(bool)> callback);
    void setNotificationCallback(std::function<void(const QString&)> callback);

private:
    friend class QuickWindowTestAccess;
    void clearVaultCache();
    void verifyVaultAction(std::function<void()> action);
    enum class AuthMode
    {
        None,
        Login,
        Unlock
    };

    struct PrepareResult
    {
        quint64 generation = 0;
        bool available = false;
        PasswordManager::StatusInfo statusInfo;
        QString errorMessage;
    };

    struct AuthResult
    {
        quint64 generation = 0;
        bool ok = false;
        bool loginMode = false;
        QString errorMessage;
    };

    struct ItemsResult
    {
        quint64 generation = 0;
        QVector<VaultItem> items;
        QString errorMessage;
    };

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void changeEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;

private:
    void openSettings();
    void resetApplicationData();
    bool logoutPasswordManager(const QString& managerType, QString* errorMessage = nullptr);
    void applyStyle();
    void updateWindowMask();
    void updateFooter();
    void updateAuthTexts();
    void updateWindowSize();
    void reload();
    void startSearch(const QString& keyword);
    void startItemLoad(bool forceRefresh = false, bool quiet = false);
    void handleItemsFinished();
    QVector<VaultItem> filterCachedItems(const QString& keyword) const;
    void restorePreservedSearch();
    void clearPreservedSearchState();
    bool shouldRestorePreservedSearch() const;
    void preserveSearchState();
    void recordRecentItem(const QString& itemId);
    int recentItemRank(const QString& itemId) const;
    const VaultItem* cachedItemById(const QString& itemId) const;
    QString totpCodeForItem(const QString& itemId, QString* errorMessage = nullptr) const;
    void selectItemById(const QString& itemId);
    void moveSelection(int delta);
    void prepareVault(bool interactive = true);
    void handlePrepareFinished();
    void showStartupAuthPanel(AuthMode mode);
    void showAuthPanel(AuthMode mode);
    void focusAuthPrimaryInput();
    void hideAuthPanel();
    void submitAuth();
    void handleAuthFinished();
    QString selectedAuthServerUrl() const;
    void setBusy(bool busy, const QString& message = {});
    void setAuthInputsEnabled(bool enabled);
    void syncVault();
    void updateAutoSyncTimer();
    void reloadPasswordManager();
    void continuePendingPreload();
    void schedulePreloadRetry(int delayMs = 30000);
    void requestFaviconForRow(QListWidgetItem* row, const VaultItem& item);
    void fetchFavicon(const QString& host, const QUrl& url);
    void handleFaviconFinished(QNetworkReply* reply);
    void applyFaviconToVisibleRows(const QString& host, const QIcon& icon);
    void scheduleFocusLossCheck();
    void hideIfFocusLost();
    void copyPassword(QListWidgetItem* item);
    void copyUsername(QListWidgetItem* item);
    void copyTotp(QListWidgetItem* item);
    void openItemWebsite(QListWidgetItem* item);
    void showItemDetails(QListWidgetItem* item);
    void closeDetailWindows();
    void copyToClipboard(const QString& text, bool sensitive);
    void copyTextAndHide(const QString& text, bool sensitive, const QString& notificationMessage = {});
    void beginWindowDrag(QMouseEvent* event);
    bool updateWindowDrag(QMouseEvent* event);
    void finishWindowDrag();
    void setStatus(const QString& message);
    QListWidgetItem* currentItem() const;
    void moveToConfiguredPosition();

    QLineEdit* m_search = nullptr;
    QAction* m_searchIconAction = nullptr;
    QPushButton* m_authBrowserLogin = nullptr;
    QProgressBar* m_busyBar = nullptr;
    QListWidget* m_list = nullptr;
    QLabel* m_status = nullptr;
    QWidget* m_authPanel = nullptr;
    QLabel* m_authTitle = nullptr;
    QLabel* m_authNote = nullptr;
    QLabel* m_authPasswordManagerLabel = nullptr;
    QLabel* m_authAccountLabel = nullptr;
    QLabel* m_authServerLabel = nullptr;
    QLabel* m_authEmailLabel = nullptr;
    QLabel* m_authPasswordLabel = nullptr;
    QLabel* m_authMethodLabel = nullptr;
    QLabel* m_authCodeLabel = nullptr;
    QLabel* m_authExtraPasswordLabel = nullptr;
    QLabel* m_authAccount = nullptr;
    QComboBox* m_authPasswordManager = nullptr;
    QComboBox* m_authServer = nullptr;
    QLineEdit* m_authCustomServer = nullptr;
    QLineEdit* m_authEmail = nullptr;
    QLineEdit* m_authPassword = nullptr;
    QComboBox* m_authMethod = nullptr;
    QLineEdit* m_authCode = nullptr;
    QLineEdit* m_authExtraPassword = nullptr;
    QLabel* m_authBusyLabel = nullptr;
    QPushButton* m_authSubmit = nullptr;
    QPushButton* m_authCancel = nullptr;
    QWidget* m_footer = nullptr;
    QGridLayout* m_footerLayout = nullptr;
    QNetworkAccessManager* m_faviconManager = nullptr;
    QHash<QString, QIcon> m_faviconCache;
    QSet<QString> m_pendingFavicons;
    QTimer m_debounce;
    QTimer m_refreshTimer;
    QTimer m_sessionTimer;
    QTimer m_preloadRetryTimer;
    std::shared_ptr<PasswordManager> m_passwordManager = std::make_shared<PasswordManager>();
    QFutureWatcher<PrepareResult> m_prepareWatcher;
    QFutureWatcher<AuthResult> m_authWatcher;
    QFutureWatcher<ItemsResult> m_itemsWatcher;
    AuthMode m_authMode = AuthMode::None;
    QString m_queuedSearchText;
    QVector<VaultItem> m_cachedItems;
    QStringList m_recentItemIds;
    bool m_itemsReady = false;
    bool m_preloadingVault = false;
    bool m_quietItemLoad = false;
    bool m_syncing = false;
    bool m_preloadPending = false;
    bool m_retryInteractivePrepare = false;
    bool m_settingsOpen = false;
    bool m_silentPreloadRetry = false;
    bool m_intentionallyLocked = false;
    bool m_resetting = false;
    bool m_backgroundSessionCheck = false;
    bool m_executingVerifiedAction = false;
    QVector<std::function<void()>> m_pendingVaultActions;
    quint64 m_vaultGeneration = 0;
    quint64 m_clipboardGeneration = 0;
    QByteArray m_lastCopiedClipboardHash;
    QElapsedTimer m_preservedSearchTimer;
    QString m_preservedSearchText;
    QString m_preservedItemId;
    QString m_currentUserEmail;
    QString m_currentServerUrl;
    bool m_dragging = false;
    bool m_dragMoved = false;
    QPoint m_dragOffset;
    QPoint m_dragStartPosition;
    std::function<void()> m_settingsChanged;
    std::function<void(bool)> m_settingsDialogStateCallback;
    std::function<void(const QString&)> m_notificationCallback;
};
