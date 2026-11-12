#pragma once

#include "VaultTypes.h"

#include <memory>

class BwClient;
class ProtonPassClient;

class PasswordManager
{
public:
    enum class Type
    {
        Bitwarden,
        ProtonPass
    };

    enum class VaultStatus
    {
        Unknown,
        Unauthenticated,
        Locked,
        Unlocked
    };

    struct StatusInfo
    {
        VaultStatus status = VaultStatus::Unknown;
        QString userEmail;
        QString serverUrl;
    };

    struct AuthRequest
    {
        QString serverUrl;
        QString email;
        QString password;
        QString twoFactorMethod;
        QString twoFactorCode;
        QString extraPassword;
    };

    using ItemDetails = VaultItemDetails;

    PasswordManager();
    explicit PasswordManager(Type type);
    ~PasswordManager();

    Type type() const;
    QString displayName() const;
    bool supportsServerSelection() const;
    bool supportsTwoFactorMethodSelection() const;
    bool supportsExtraPassword() const;
    bool usesExternalSessionStorage() const;

    bool hasSession() const;
    QString program() const;
    void reloadSettings();
    void clearSession();

    bool isAvailable(QString* errorMessage = nullptr) const;
    StatusInfo statusInfo(QString* errorMessage = nullptr) const;
    bool startExternalLogin(QString* errorMessage = nullptr) const;
    bool loginWithCredentials(const AuthRequest& request, QString* errorMessage = nullptr);
    bool unlockWithPassword(const QString& password, QString* errorMessage = nullptr);
    bool lock(QString* errorMessage = nullptr);
    bool logout(QString* errorMessage = nullptr);
    bool sync(QString* errorMessage = nullptr) const;

    QVector<VaultItem> listItems(QString* errorMessage = nullptr) const;
    QString getPassword(const QString& itemId, QString* errorMessage = nullptr) const;
    QString getUsername(const QString& itemId, QString* errorMessage = nullptr) const;
    QString getTotp(const QString& itemId, QString* errorMessage = nullptr) const;
    ItemDetails getItemDetails(const QString& itemId, QString* errorMessage = nullptr) const;
    static ItemDetails detailsFromVaultItem(const VaultItem& item);

private:
    void createSelectedBackend();

    Type m_type = Type::Bitwarden;
    std::unique_ptr<BwClient> m_bitwarden;
    std::unique_ptr<ProtonPassClient> m_protonPass;
};
