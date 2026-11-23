#pragma once

#include <QByteArray>
#include <QMutex>
#include <QProcessEnvironment>
#include "VaultTypes.h"

class BwClient
{
public:
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

    using ItemDetails = VaultItemDetails;

    BwClient();

    bool hasSession() const;
    QString program() const;
    void reloadSettings();
    void clearSession();
    static void clearStoredSessionData();

    bool isAvailable(QString* errorMessage = nullptr) const;
    StatusInfo statusInfo(QString* errorMessage = nullptr);
    VaultStatus status(QString* errorMessage = nullptr);
    bool configureServer(const QString& serverUrl, QString* errorMessage = nullptr);
    bool loginWithCredentials(const QString& serverUrl,
        const QString& email,
        const QString& password,
        const QString& method,
        const QString& code,
        QString* errorMessage = nullptr);
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
    struct CommandResult
    {
        bool started = false;
        bool timedOut = false;
        bool crashed = false;
        int exitCode = -1;
        QByteArray standardOutput;
        QByteArray standardError;
        QString errorString;

        bool ok() const;
    };

    static QString findBwProgram();
    static QString describeCommandError(const QString& program,
        const QStringList& arguments,
        const CommandResult& result);
    static QString describeCommandError(const QString& program,
        const QString& displayCommand,
        const CommandResult& result);
    CommandResult run(const QStringList& arguments,
        const QByteArray& standardInput = {},
        int timeoutMs = 15000,
        const QProcessEnvironment& extraEnvironment = QProcessEnvironment()) const;

    mutable QRecursiveMutex m_commandMutex;
    mutable QMutex m_stateMutex;
    QString m_program;
    QString m_session;
};
