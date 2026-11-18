#pragma once

#include <QChar>
#include <QString>
#include <QStringList>
#include <QVector>

inline void scrubSecretString(QString& value)
{
    if (!value.isEmpty()) {
        value.fill(QChar('\0'));
        value.clear();
    }
}

struct VaultCardData
{
    QString cardholderName;
    QString brand;
    QString number;
    QString expirationMonth;
    QString expirationYear;
    QString securityCode;

    void scrub()
    {
        scrubSecretString(cardholderName);
        scrubSecretString(brand);
        scrubSecretString(number);
        scrubSecretString(expirationMonth);
        scrubSecretString(expirationYear);
        scrubSecretString(securityCode);
    }
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

    void scrub()
    {
        scrubSecretString(socialSecurityNumber);
        scrubSecretString(passportNumber);
        scrubSecretString(licenseNumber);
        scrubSecretString(email);
        scrubSecretString(phone);
        scrubSecretString(username);
        scrubSecretString(address1);
        scrubSecretString(address2);
        scrubSecretString(address3);
    }
};

struct VaultSshKeyData
{
    QString privateKey;
    QString publicKey;
    QString fingerprint;

    void scrub()
    {
        scrubSecretString(privateKey);
        scrubSecretString(publicKey);
        scrubSecretString(fingerprint);
    }
};

struct VaultCustomField
{
    QString name;
    QString value;
    int typeCode = 0;

    void scrub()
    {
        scrubSecretString(name);
        scrubSecretString(value);
    }
};

struct VaultDetailField
{
    QString name;
    QString value;
    bool sensitive = false;
    bool totpSecret = false;
    bool website = false;

    void scrub()
    {
        scrubSecretString(name);
        scrubSecretString(value);
    }
};

struct VaultDetailSection
{
    QString title;
    QVector<VaultDetailField> fields;
    bool searchable = true;

    void scrub()
    {
        for (VaultDetailField& field : fields)
            field.scrub();
        fields.clear();
    }
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

    void scrub()
    {
        scrubSecretString(username);
        scrubSecretString(password);
        scrubSecretString(totp);
        scrubSecretString(notes);
        card.scrub();
        identity.scrub();
        sshKey.scrub();
        for (VaultCustomField& field : customFields)
            field.scrub();
        customFields.clear();
        for (VaultDetailSection& section : providerSections)
            section.scrub();
        providerSections.clear();
    }
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

    void scrub()
    {
        scrubSecretString(username);
        scrubSecretString(password);
        scrubSecretString(totp);
        scrubSecretString(notes);
        card.scrub();
        identity.scrub();
        sshKey.scrub();
        for (VaultCustomField& field : customFields)
            field.scrub();
        customFields.clear();
        for (VaultDetailSection& section : providerSections)
            section.scrub();
        providerSections.clear();
    }
};
