#pragma once

#include "VaultTypes.h"

#include <QByteArray>
#include <QJsonObject>
#include <QMutex>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>
#include <QVector>

class ProtonPassClient
{
public:
    enum class Status
    {
        Unknown,
        Unauthenticated,
        Locked,
        Unlocked
    };

    struct StatusInfo
    {
        Status status = Status::Unknown;
        QString userEmail;
    };

    using ItemDetails = VaultItemDetails;

    ProtonPassClient();

    bool hasSession() const;
    QString program() const;
    void reloadSettings();
    void clearSession();

    bool isAvailable(QString* errorMessage = nullptr) const;
    StatusInfo statusInfo(QString* errorMessage = nullptr);
    bool loginWithCredentials(const QString& email,
        const QString& password,
        const QString& totp,
        const QString& extraPassword,
        QString* errorMessage = nullptr);
    bool lock(QString* errorMessage = nullptr);
    bool logout(QString* errorMessage = nullptr);
    bool sync(QString* errorMessage = nullptr) const;

    QVector<VaultItem> listItems(QString* errorMessage = nullptr) const;
    QString getPassword(const QString& itemId, QString* errorMessage = nullptr) const;
    QString getUsername(const QString& itemId, QString* errorMessage = nullptr) const;
    QString getTotp(const QString& itemId, QString* errorMessage = nullptr) const;
    ItemDetails getItemDetails(const QString& itemId, QString* errorMessage = nullptr) const;

    static QString makeItemKey(const QString& shareId, const QString& itemId);
    static bool splitItemKey(const QString& key, QString* shareId, QString* itemId);

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

    static QString findProgram();
    static VaultItem vaultItemFromObject(const QJsonObject& object, const QString& fallbackShareId);
    static ItemDetails detailsFromObject(const QJsonObject& object, const QString& fallbackShareId);
    static QString describeCommandError(const QString& program,
        const QStringList& arguments,
        const CommandResult& result);
    CommandResult run(const QStringList& arguments,
        int timeoutMs = 30000,
        const QProcessEnvironment& extraEnvironment = QProcessEnvironment()) const;

    mutable QRecursiveMutex m_commandMutex;
    mutable QMutex m_stateMutex;
    QString m_program;
    bool m_sessionActive = false;
    bool m_sessionLocked = false;
};
