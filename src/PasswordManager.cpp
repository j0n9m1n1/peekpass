#include "PasswordManager.h"

#include "AppSettings.h"
#include "BwClient.h"
#include "I18n.h"
#include "ProtonPassClient.h"

namespace
{
PasswordManager::VaultStatus toVaultStatus(BwClient::VaultStatus status)
{
    switch (status) {
    case BwClient::VaultStatus::Unauthenticated:
        return PasswordManager::VaultStatus::Unauthenticated;
    case BwClient::VaultStatus::Locked:
        return PasswordManager::VaultStatus::Locked;
    case BwClient::VaultStatus::Unlocked:
        return PasswordManager::VaultStatus::Unlocked;
    case BwClient::VaultStatus::Unknown:
        return PasswordManager::VaultStatus::Unknown;
    }

    return PasswordManager::VaultStatus::Unknown;
}

PasswordManager::VaultStatus toVaultStatus(ProtonPassClient::Status status)
{
    switch (status) {
    case ProtonPassClient::Status::Unauthenticated:
        return PasswordManager::VaultStatus::Unauthenticated;
    case ProtonPassClient::Status::Locked:
        return PasswordManager::VaultStatus::Locked;
    case ProtonPassClient::Status::Unlocked:
        return PasswordManager::VaultStatus::Unlocked;
    case ProtonPassClient::Status::Unknown:
        return PasswordManager::VaultStatus::Unknown;
    }

    return PasswordManager::VaultStatus::Unknown;
}
}

PasswordManager::PasswordManager()
{
    reloadSettings();
}

PasswordManager::PasswordManager(Type type)
    : m_type(type)
{
    createSelectedBackend();
}

PasswordManager::~PasswordManager() = default;

PasswordManager::Type PasswordManager::type() const
{
    return m_type;
}

QString PasswordManager::displayName() const
{
    return m_type == Type::ProtonPass ? QStringLiteral("Proton Pass") : QStringLiteral("Bitwarden");
}

bool PasswordManager::supportsServerSelection() const
{
    return m_type == Type::Bitwarden;
}

bool PasswordManager::supportsTwoFactorMethodSelection() const
{
    return m_type == Type::Bitwarden;
}

bool PasswordManager::supportsExtraPassword() const
{
    return m_type == Type::ProtonPass;
}

bool PasswordManager::usesExternalSessionStorage() const
{
    return m_type == Type::ProtonPass;
}

bool PasswordManager::hasSession() const
{
    return m_type == Type::ProtonPass ? m_protonPass->hasSession() : m_bitwarden->hasSession();
}

QString PasswordManager::program() const
{
    return m_type == Type::ProtonPass ? m_protonPass->program() : m_bitwarden->program();
}

void PasswordManager::reloadSettings()
{
    const Type selectedType = AppSettings::passwordManagerType() == QStringLiteral("protonpass")
        ? Type::ProtonPass
        : Type::Bitwarden;

    if (selectedType != m_type || (!m_bitwarden && !m_protonPass)) {
        m_type = selectedType;
        createSelectedBackend();
    }

    if (m_type == Type::ProtonPass)
        m_protonPass->reloadSettings();
    else
        m_bitwarden->reloadSettings();
}

void PasswordManager::clearSession()
{
    if (m_type == Type::ProtonPass)
        m_protonPass->clearSession();
    else
        m_bitwarden->clearSession();
}

bool PasswordManager::isAvailable(QString* errorMessage) const
{
    return m_type == Type::ProtonPass
        ? m_protonPass->isAvailable(errorMessage)
        : m_bitwarden->isAvailable(errorMessage);
}

PasswordManager::StatusInfo PasswordManager::statusInfo(QString* errorMessage) const
{
    StatusInfo result;
    if (m_type == Type::ProtonPass) {
        const ProtonPassClient::StatusInfo protonStatus = m_protonPass->statusInfo(errorMessage);
        result.status = toVaultStatus(protonStatus.status);
        result.userEmail = protonStatus.userEmail;
        return result;
    }

    const BwClient::StatusInfo bitwardenStatus = m_bitwarden->statusInfo(errorMessage);
    result.status = toVaultStatus(bitwardenStatus.status);
    result.userEmail = bitwardenStatus.userEmail;
    result.serverUrl = bitwardenStatus.serverUrl;
    return result;
}

bool PasswordManager::loginWithCredentials(const AuthRequest& request, QString* errorMessage)
{
    if (m_type == Type::ProtonPass) {
        const auto status = m_protonPass->statusInfo(errorMessage);
        return status.status == ProtonPassClient::Status::Unlocked;
    }

    return m_bitwarden->loginWithCredentials(request.serverUrl,
        request.email,
        request.password,
        request.twoFactorMethod,
        request.twoFactorCode,
        errorMessage);
}

bool PasswordManager::startExternalLogin(QString* errorMessage) const
{
    return m_type == Type::ProtonPass && m_protonPass->startLogin(errorMessage);
}

bool PasswordManager::unlockWithPassword(const QString& password, QString* errorMessage)
{
    if (m_type == Type::ProtonPass) {
        if (errorMessage) {
            *errorMessage = I18n::translate(
                "PasswordManager",
                "Proton Pass session unlocking is not available in peekpass yet. Log in again instead.",
                "Proton Pass 세션 잠금 해제는 아직 peekpass에서 지원하지 않음. 다시 로그인해야 함.");
        }
        return false;
    }

    return m_bitwarden->unlockWithPassword(password, errorMessage);
}

bool PasswordManager::lock(QString* errorMessage)
{
    return m_type == Type::ProtonPass ? m_protonPass->lock(errorMessage) : m_bitwarden->lock(errorMessage);
}

bool PasswordManager::logout(QString* errorMessage)
{
    return m_type == Type::ProtonPass ? m_protonPass->logout(errorMessage) : m_bitwarden->logout(errorMessage);
}

bool PasswordManager::sync(QString* errorMessage) const
{
    return m_type == Type::ProtonPass ? m_protonPass->sync(errorMessage) : m_bitwarden->sync(errorMessage);
}

QVector<VaultItem> PasswordManager::listItems(QString* errorMessage) const
{
    return m_type == Type::ProtonPass ? m_protonPass->listItems(errorMessage) : m_bitwarden->listItems(errorMessage);
}

QString PasswordManager::getPassword(const QString& itemId, QString* errorMessage) const
{
    return m_type == Type::ProtonPass
        ? m_protonPass->getPassword(itemId, errorMessage)
        : m_bitwarden->getPassword(itemId, errorMessage);
}

QString PasswordManager::getUsername(const QString& itemId, QString* errorMessage) const
{
    return m_type == Type::ProtonPass
        ? m_protonPass->getUsername(itemId, errorMessage)
        : m_bitwarden->getUsername(itemId, errorMessage);
}

QString PasswordManager::getTotp(const QString& itemId, QString* errorMessage) const
{
    return m_type == Type::ProtonPass ? m_protonPass->getTotp(itemId, errorMessage) : m_bitwarden->getTotp(itemId, errorMessage);
}

PasswordManager::ItemDetails PasswordManager::getItemDetails(const QString& itemId, QString* errorMessage) const
{
    return m_type == Type::ProtonPass
        ? m_protonPass->getItemDetails(itemId, errorMessage)
        : m_bitwarden->getItemDetails(itemId, errorMessage);
}

PasswordManager::ItemDetails PasswordManager::detailsFromVaultItem(const VaultItem& item)
{
    ItemDetails details;
    details.providerId = item.providerId;
    details.id = item.id;
    details.name = item.name;
    details.typeCode = item.typeCode;
    details.type = item.type;
    details.username = item.username;
    details.password = item.password;
    details.totp = item.totp;
    details.card = item.card;
    details.identity = item.identity;
    details.sshKey = item.sshKey;
    details.customFields = item.customFields;
    details.notes = item.notes;
    details.folderId = item.folderId;
    details.organizationId = item.organizationId;
    details.collectionIds = item.collectionIds.join(QStringLiteral(", "));
    details.revisionDate = item.revisionDate;
    details.creationDate = item.creationDate;
    details.deletedDate = item.deletedDate;
    details.uris = item.uris;
    details.providerSections = item.providerSections;
    return details;
}

void PasswordManager::createSelectedBackend()
{
    m_bitwarden.reset();
    m_protonPass.reset();

    if (m_type == Type::ProtonPass)
        m_protonPass = std::make_unique<ProtonPassClient>();
    else
        m_bitwarden = std::make_unique<BwClient>();
}
