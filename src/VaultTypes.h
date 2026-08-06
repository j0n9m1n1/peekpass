#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

struct VaultCardData
{
    QString cardholderName;
    QString brand;
    QString number;
    QString expirationMonth;
    QString expirationYear;
    QString securityCode;
};

struct VaultIdentityData
{
    QString title;
    QString firstName;
    QString middleName;
    QString lastName;
    QString company;
    QString email;
    QString phone;
    QString username;
    QString address1;
    QString address2;
    QString address3;
    QString city;
    QString state;
    QString postalCode;
    QString country;
    QString socialSecurityNumber;
    QString passportNumber;
    QString licenseNumber;
};

struct VaultSshKeyData
{
    QString privateKey;
    QString publicKey;
    QString fingerprint;
};

struct VaultCustomField
{
    QString name;
    QString value;
    int typeCode = 0;
};

struct VaultDetailField
{
    QString name;
    QString value;
    bool sensitive = false;
    bool totpSecret = false;
    bool website = false;
};

struct VaultDetailSection
{
    QString title;
    QVector<VaultDetailField> fields;
    bool searchable = true;
};

struct VaultItem
{
    QString providerId = QStringLiteral("bitwarden");
    QString id;
    QString name;
    int typeCode = 0;
    QString type;
    QString username;
    QString password;
    QString totp;
    VaultCardData card;
    VaultIdentityData identity;
    VaultSshKeyData sshKey;
    QVector<VaultCustomField> customFields;
    QString notes;
    QString folderId;
    QString organizationId;
    QStringList collectionIds;
    QString revisionDate;
    QString creationDate;
    QString deletedDate;
    QStringList uris;
    QVector<VaultDetailSection> providerSections;
};

struct VaultItemDetails
{
    QString providerId = QStringLiteral("bitwarden");
    QString id;
    QString name;
    int typeCode = 0;
    QString type;
    QString username;
    QString password;
    QString totp;
    VaultCardData card;
    VaultIdentityData identity;
    VaultSshKeyData sshKey;
    QVector<VaultCustomField> customFields;
    QString notes;
    QString folderId;
    QString organizationId;
    QString collectionIds;
    QString revisionDate;
    QString creationDate;
    QString deletedDate;
    QStringList uris;
    QVector<VaultDetailSection> providerSections;
};
