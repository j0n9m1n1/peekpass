#include "ProtonPassClient.h"

#include "AppSettings.h"
#include "I18n.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMutexLocker>
#include <QProcess>
#include <QStandardPaths>
#include <QtGlobal>

#include <utility>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace
{
constexpr QChar ItemKeySeparator(0x1f);

QString uiText(const char* english, const char* korean)
{
    return I18n::translate("ProtonPassClient", english, korean);
}

QString trimmedUtf8(const QByteArray& value)
{
    return QString::fromUtf8(value).trimmed();
}

void setError(QString* target, const QString& message)
{
    if (target)
        *target = message;
}

QString valueAsText(const QJsonValue& value)
{
    if (value.isString())
        return value.toString();
    if (value.isDouble())
        return QString::number(value.toDouble(), 'f', 0);
    if (value.isBool())
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    return {};
}

QString objectField(const QJsonObject& object, const QString& name);

QString displayValue(const QJsonValue& value)
{
    const QString scalar = valueAsText(value);
    if (!scalar.isNull())
        return scalar;
    if (value.isArray()) {
        QStringList values;
        bool scalarArray = true;
        for (const QJsonValue& entry : value.toArray()) {
            const QString entryText = valueAsText(entry);
            if (entryText.isNull()) {
                scalarArray = false;
                break;
            }
            if (!entryText.isEmpty())
                values.push_back(entryText);
        }
        if (scalarArray)
            return values.join(QLatin1Char('\n'));
        return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Indented)).trimmed();
    }
    if (value.isObject())
        return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Indented)).trimmed();
    return {};
}

QString fieldLabel(QString name)
{
    name.replace(QLatin1Char('_'), QLatin1Char(' '));
    if (!name.isEmpty())
        name[0] = name[0].toUpper();
    return name;
}

bool isTotpField(const QString& name)
{
    return name.contains(QStringLiteral("totp"), Qt::CaseInsensitive);
}

bool isWebsiteField(const QString& name)
{
    const QString key = name.trimmed().toLower();
    return key == QStringLiteral("url")
        || key == QStringLiteral("website")
        || key.endsWith(QStringLiteral("_url"))
        || key.endsWith(QStringLiteral("_website"));
}

bool isSensitiveField(const QString& name)
{
    const QString key = name.toLower();
    return key.contains(QStringLiteral("password"))
        || key.contains(QStringLiteral("secret"))
        || key.contains(QStringLiteral("private_key"))
        || key.contains(QStringLiteral("access_token"))
        || key.contains(QStringLiteral("refresh_token"))
        || key == QStringLiteral("number")
        || key == QStringLiteral("pin")
        || key.contains(QStringLiteral("verification_number"))
        || key.contains(QStringLiteral("security_code"))
        || key.contains(QStringLiteral("social_security"))
        || key.contains(QStringLiteral("passport_number"))
        || key.contains(QStringLiteral("license_number"))
        || key.contains(QStringLiteral("credential_id"))
        || key == QStringLiteral("content")
        || key == QStringLiteral("passkeys")
        || isTotpField(key);
}

void appendDetailField(VaultDetailSection* section,
    const QString& name,
    const QJsonValue& value,
    bool forceSensitive = false,
    bool forceTotp = false)
{
    if (!section || value.isNull() || value.isUndefined())
        return;

    const QString text = displayValue(value);
    if (text.isEmpty())
        return;

    VaultDetailField field;
    field.name = fieldLabel(name);
    field.value = text;
    field.totpSecret = forceTotp || isTotpField(name);
    field.sensitive = forceSensitive || field.totpSecret || isSensitiveField(name);
    field.website = !field.sensitive && isWebsiteField(name);
    section->fields.push_back(std::move(field));
}

void appendObjectFields(const QJsonObject& object,
    VaultDetailSection* section,
    const QStringList& skippedKeys = {})
{
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        if (skippedKeys.contains(it.key()))
            continue;
        appendDetailField(section, it.key(), it.value());
    }
}

void appendStructuredFields(const QJsonArray& fields, VaultDetailSection* section)
{
    for (const QJsonValue& fieldValue : fields) {
        const QJsonObject field = fieldValue.toObject();
        const QJsonObject content = field.value(QStringLiteral("content")).toObject();
        if (content.isEmpty())
            continue;

        const QString kind = content.constBegin().key();
        const QString name = objectField(field, QStringLiteral("name")).isEmpty()
            ? fieldLabel(kind)
            : objectField(field, QStringLiteral("name"));
        appendDetailField(section,
            name,
            content.constBegin().value(),
            kind.compare(QStringLiteral("Hidden"), Qt::CaseInsensitive) == 0,
            kind.compare(QStringLiteral("Totp"), Qt::CaseInsensitive) == 0);
    }
}

void appendCustomSections(const QJsonArray& sections, QVector<VaultDetailSection>* target)
{
    if (!target)
        return;

    for (const QJsonValue& sectionValue : sections) {
        const QJsonObject source = sectionValue.toObject();
        VaultDetailSection section;
        section.title = objectField(source, QStringLiteral("section_name"));
        if (section.title.isEmpty())
            section.title = uiText("Section", "섹션");
        appendStructuredFields(source.value(QStringLiteral("section_fields")).toArray(), &section);
        if (!section.fields.isEmpty())
            target->push_back(std::move(section));
    }
}

void appendProviderSections(const QJsonObject& item,
    const QJsonObject& itemData,
    const QJsonObject& payload,
    const QString& typeName,
    QVector<VaultDetailSection>* target)
{
    if (!target)
        return;

    VaultDetailSection mainSection;
    mainSection.title = typeName;
    appendObjectFields(payload,
        &mainSection,
        {QStringLiteral("urls"),
            QStringLiteral("sections"),
            QStringLiteral("extra_sections"),
            QStringLiteral("extra_personal_details"),
            QStringLiteral("extra_address_details"),
            QStringLiteral("extra_contact_details"),
            QStringLiteral("extra_work_details")});
    if (!mainSection.fields.isEmpty())
        target->push_back(std::move(mainSection));

    appendCustomSections(payload.value(QStringLiteral("sections")).toArray(), target);
    appendCustomSections(payload.value(QStringLiteral("extra_sections")).toArray(), target);

    const QStringList structuredGroups = {
        QStringLiteral("extra_personal_details"),
        QStringLiteral("extra_address_details"),
        QStringLiteral("extra_contact_details"),
        QStringLiteral("extra_work_details")
    };
    for (const QString& group : structuredGroups) {
        VaultDetailSection section;
        section.title = fieldLabel(group);
        appendStructuredFields(payload.value(group).toArray(), &section);
        if (!section.fields.isEmpty())
            target->push_back(std::move(section));
    }

    VaultDetailSection extraSection;
    extraSection.title = uiText("Extra fields", "추가 필드");
    appendStructuredFields(itemData.value(QStringLiteral("extra_fields")).toArray(), &extraSection);
    if (!extraSection.fields.isEmpty())
        target->push_back(std::move(extraSection));

    VaultDetailSection itemDataSection;
    itemDataSection.title = uiText("Item data", "항목 데이터");
    itemDataSection.searchable = false;
    appendObjectFields(itemData,
        &itemDataSection,
        {QStringLiteral("title"),
            QStringLiteral("note"),
            QStringLiteral("content"),
            QStringLiteral("extra_fields")});
    if (!itemDataSection.fields.isEmpty())
        target->push_back(std::move(itemDataSection));

    VaultDetailSection metadataSection;
    metadataSection.title = uiText("Proton Pass metadata", "Proton Pass 메타데이터");
    metadataSection.searchable = false;
    appendObjectFields(item, &metadataSection, {QStringLiteral("content")});
    if (!metadataSection.fields.isEmpty())
        target->push_back(std::move(metadataSection));
}

QProcessEnvironment cleanLoginEnvironment()
{
    auto environment = QProcessEnvironment::systemEnvironment();
    for (const QString& key : environment.keys()) {
        if (key.startsWith(QStringLiteral("PROTON_PASS_"), Qt::CaseInsensitive)
            && (key.contains(QStringLiteral("PASSWORD"), Qt::CaseInsensitive)
                || key.contains(QStringLiteral("TOTP"), Qt::CaseInsensitive)
                || key.contains(QStringLiteral("PERSONAL_ACCESS_TOKEN"), Qt::CaseInsensitive))) {
            environment.remove(key);
        }
    }
    environment.insert(QStringLiteral("NO_COLOR"), QStringLiteral("1"));
    return environment;
}

QString objectField(const QJsonObject& object, const QString& name)
{
    return valueAsText(object.value(name));
}

QJsonObject variantPayload(const QJsonObject& object, QString* name = nullptr)
{
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        if (it.value().isObject()) {
            if (name)
                *name = it.key();
            return it.value().toObject();
        }
    }

    return {};
}

QString itemTypeName(const QString& variant)
{
    if (variant.compare(QStringLiteral("Login"), Qt::CaseInsensitive) == 0)
        return uiText("Login", "로그인");
    if (variant.compare(QStringLiteral("Note"), Qt::CaseInsensitive) == 0)
        return uiText("Secure note", "보안 메모");
    if (variant.compare(QStringLiteral("CreditCard"), Qt::CaseInsensitive) == 0)
        return uiText("Card", "카드");
    if (variant.compare(QStringLiteral("Identity"), Qt::CaseInsensitive) == 0)
        return uiText("Identity", "신원");
    if (variant.compare(QStringLiteral("SshKey"), Qt::CaseInsensitive) == 0)
        return uiText("SSH key", "SSH 키");
    if (variant.compare(QStringLiteral("Wifi"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("Wi-Fi");
    if (variant.compare(QStringLiteral("Alias"), Qt::CaseInsensitive) == 0)
        return uiText("Alias", "별칭");
    if (variant.compare(QStringLiteral("Custom"), Qt::CaseInsensitive) == 0)
        return uiText("Custom", "사용자 지정");
    return variant;
}

int itemTypeCode(const QString& variant)
{
    if (variant.compare(QStringLiteral("Login"), Qt::CaseInsensitive) == 0)
        return 1;
    if (variant.compare(QStringLiteral("Note"), Qt::CaseInsensitive) == 0)
        return 2;
    if (variant.compare(QStringLiteral("CreditCard"), Qt::CaseInsensitive) == 0)
        return 3;
    if (variant.compare(QStringLiteral("Identity"), Qt::CaseInsensitive) == 0)
        return 4;
    if (variant.compare(QStringLiteral("SshKey"), Qt::CaseInsensitive) == 0)
        return 5;
    return 0;
}

void appendCustomFields(const QJsonArray& fields, QVector<VaultCustomField>* target)
{
    for (const QJsonValue& fieldValue : fields) {
        const QJsonObject field = fieldValue.toObject();
        const QJsonObject content = field.value(QStringLiteral("content")).toObject();
        if (field.isEmpty() || content.isEmpty())
            continue;

        const QString fieldKind = content.constBegin().key();
        if (fieldKind.compare(QStringLiteral("Totp"), Qt::CaseInsensitive) == 0)
            continue;

        VaultCustomField customField;
        customField.name = objectField(field, QStringLiteral("name"));
        const QJsonObject payload = variantPayload(content);
        customField.typeCode = fieldKind.compare(QStringLiteral("Hidden"), Qt::CaseInsensitive) == 0 ? 1 : 0;
        customField.value = payload.isEmpty() ? valueAsText(content.begin().value()) : valueAsText(payload.begin().value());
        if (!customField.name.isEmpty())
            target->push_back(customField);
    }
}

VaultItem itemFromPayload(const QJsonObject& source, const QString& fallbackShareId)
{
    const QJsonObject item = source.contains(QStringLiteral("item"))
        ? source.value(QStringLiteral("item")).toObject()
        : source;
    const QJsonObject itemData = item.value(QStringLiteral("content")).toObject();
    const QJsonObject itemContent = itemData.value(QStringLiteral("content")).toObject();
    QString variant;
    const QJsonObject payload = variantPayload(itemContent, &variant);

    VaultItem result;
    result.providerId = QStringLiteral("protonpass");
    const QString shareId = objectField(item, QStringLiteral("share_id")).isEmpty()
        ? fallbackShareId
        : objectField(item, QStringLiteral("share_id"));
    const QString rawItemId = objectField(item, QStringLiteral("id"));
    result.id = rawItemId.isEmpty() ? QString() : ProtonPassClient::makeItemKey(shareId, rawItemId);
    result.name = objectField(itemData, QStringLiteral("title"));
    result.type = itemTypeName(variant);
    result.typeCode = itemTypeCode(variant);
    result.folderId = objectField(item, QStringLiteral("folder_id"));
    result.creationDate = objectField(item, QStringLiteral("create_time"));
    result.revisionDate = objectField(item, QStringLiteral("modify_time"));
    const QString state = objectField(item, QStringLiteral("state")).trimmed();
    if (!state.isEmpty() && state.compare(QStringLiteral("Active"), Qt::CaseInsensitive) != 0)
        result.deletedDate = state;
    result.notes = objectField(itemData, QStringLiteral("note"));

    if (variant.compare(QStringLiteral("Login"), Qt::CaseInsensitive) == 0) {
        result.username = objectField(payload, QStringLiteral("username"));
        if (result.username.isEmpty())
            result.username = objectField(payload, QStringLiteral("email"));
        result.password = objectField(payload, QStringLiteral("password"));
        result.totp = objectField(payload, QStringLiteral("totp_uri"));
        for (const QJsonValue& url : payload.value(QStringLiteral("urls")).toArray()) {
            const QString urlText = valueAsText(url);
            if (!urlText.isEmpty())
                result.uris.push_back(urlText);
        }
    } else if (variant.compare(QStringLiteral("CreditCard"), Qt::CaseInsensitive) == 0) {
        result.card.cardholderName = objectField(payload, QStringLiteral("cardholder_name"));
        result.card.brand = objectField(payload, QStringLiteral("card_type"));
        result.card.number = objectField(payload, QStringLiteral("number"));
        result.card.securityCode = objectField(payload, QStringLiteral("verification_number"));
        const QStringList expiration = objectField(payload, QStringLiteral("expiration_date")).split(
            objectField(payload, QStringLiteral("expiration_date")).contains(QLatin1Char('/'))
                ? QLatin1Char('/')
                : QLatin1Char('-'));
        if (expiration.size() == 2) {
            if (expiration.first().size() == 4) {
                result.card.expirationYear = expiration.first();
                result.card.expirationMonth = expiration.last();
            } else {
                result.card.expirationMonth = expiration.first();
                result.card.expirationYear = expiration.last();
            }
        }
    } else if (variant.compare(QStringLiteral("Identity"), Qt::CaseInsensitive) == 0) {
        result.identity.firstName = objectField(payload, QStringLiteral("first_name"));
        result.identity.middleName = objectField(payload, QStringLiteral("middle_name"));
        result.identity.lastName = objectField(payload, QStringLiteral("last_name"));
        result.identity.company = objectField(payload, QStringLiteral("company"));
        result.identity.email = objectField(payload, QStringLiteral("email"));
        result.identity.phone = objectField(payload, QStringLiteral("phone_number"));
        result.identity.address1 = objectField(payload, QStringLiteral("street_address"));
        result.identity.city = objectField(payload, QStringLiteral("city"));
        result.identity.state = objectField(payload, QStringLiteral("state_or_province"));
        result.identity.postalCode = objectField(payload, QStringLiteral("zip_or_postal_code"));
        result.identity.country = objectField(payload, QStringLiteral("country_or_region"));
        result.identity.passportNumber = objectField(payload, QStringLiteral("passport_number"));
        result.identity.licenseNumber = objectField(payload, QStringLiteral("license_number"));
        result.identity.socialSecurityNumber = objectField(payload, QStringLiteral("social_security_number"));
        for (const QString& key : {QStringLiteral("website"), QStringLiteral("personal_website")}) {
            const QString website = objectField(payload, key);
            if (!website.isEmpty())
                result.uris.push_back(website);
        }
    } else if (variant.compare(QStringLiteral("SshKey"), Qt::CaseInsensitive) == 0) {
        result.sshKey.privateKey = objectField(payload, QStringLiteral("private_key"));
        result.sshKey.publicKey = objectField(payload, QStringLiteral("public_key"));
    } else if (variant.compare(QStringLiteral("Wifi"), Qt::CaseInsensitive) == 0) {
        result.username = objectField(payload, QStringLiteral("ssid"));
        result.password = objectField(payload, QStringLiteral("password"));
    }

    const QJsonArray extraFields = itemData.value(QStringLiteral("extra_fields")).toArray();
    for (const QJsonValue& fieldValue : extraFields) {
        const QJsonObject content = fieldValue.toObject().value(QStringLiteral("content")).toObject();
        if (result.totp.isEmpty())
            result.totp = valueAsText(content.value(QStringLiteral("Totp")));
    }
    appendCustomFields(extraFields, &result.customFields);
    appendProviderSections(item, itemData, payload, result.type, &result.providerSections);
    return result;
}
}

bool ProtonPassClient::CommandResult::ok() const
{
    return started && !timedOut && !crashed && exitCode == 0;
}

ProtonPassClient::ProtonPassClient()
{
    reloadSettings();
}

bool ProtonPassClient::hasSession() const
{
    QMutexLocker<QMutex> locker(&m_stateMutex);
    return m_sessionActive && !m_sessionLocked;
}

QString ProtonPassClient::program() const
{
    QMutexLocker<QMutex> locker(&m_stateMutex);
    return m_program;
}

void ProtonPassClient::reloadSettings()
{
    QMutexLocker<QMutex> locker(&m_stateMutex);
    m_program = findProgram();
}

void ProtonPassClient::clearSession()
{
    QMutexLocker<QMutex> locker(&m_stateMutex);
    m_sessionActive = false;
    m_sessionLocked = false;
}

bool ProtonPassClient::isAvailable(QString* errorMessage) const
{
    const CommandResult result = run({QStringLiteral("--version")}, 5000);
    if (result.ok()) {
        setError(errorMessage, {});
        return true;
    }

    setError(errorMessage,
        uiText("Proton Pass CLI was not found or could not run.\n\npeekpass checked the app folder, the standard Proton Pass install location, and PATH. Set the pass-cli.exe location in Settings if needed.",
               "Proton Pass CLI를 찾지 못했거나 실행하지 못했습니다.\n\npeekpass는 앱 폴더, Proton Pass 기본 설치 경로, PATH 순서로 확인했습니다. 필요하면 설정에서 pass-cli.exe 위치를 지정하세요."));
    return false;
}

ProtonPassClient::StatusInfo ProtonPassClient::statusInfo(QString* errorMessage)
{
    QMutexLocker<QRecursiveMutex> commandLocker(&m_commandMutex);
    StatusInfo info;
    const CommandResult result = run({QStringLiteral("info"), QStringLiteral("--output"), QStringLiteral("json")}, 15000);
    if (!result.ok()) {
        const QString diagnostic = (trimmedUtf8(result.standardOutput) + QLatin1Char('\n')
                                       + trimmedUtf8(result.standardError) + QLatin1Char('\n') + result.errorString)
                                       .toLower();
        if (diagnostic.contains(QStringLiteral("session is locked"))
            || diagnostic.contains(QStringLiteral("session locked"))
            || diagnostic.contains(QStringLiteral("session needs to be unlocked"))) {
            clearSession();
            info.status = Status::Locked;
            setError(errorMessage, {});
            return info;
        }
        if (diagnostic.contains(QStringLiteral("not logged"))
            || diagnostic.contains(QStringLiteral("not authenticated"))
            || diagnostic.contains(QStringLiteral("no active session"))
            || diagnostic.contains(QStringLiteral("not logged in"))
            || diagnostic.contains(QStringLiteral("requires an authenticated client"))
            || diagnostic.contains(QStringLiteral("session expired"))
            || diagnostic.contains(QStringLiteral("session invalidated"))) {
            info.status = Status::Unauthenticated;
            clearSession();
            setError(errorMessage, {});
            return info;
        }

        setError(errorMessage, describeCommandError(program(), {QStringLiteral("info")}, result));
        return info;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(result.standardOutput, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(errorMessage,
            uiText("Could not parse Proton Pass CLI status: %1",
                   "Proton Pass CLI 상태를 해석하지 못했습니다: %1")
                .arg(parseError.errorString()));
        return info;
    }

    info.status = Status::Unlocked;
    info.userEmail = objectField(document.object(), QStringLiteral("email"));
    {
        QMutexLocker<QMutex> locker(&m_stateMutex);
        m_sessionActive = true;
        m_sessionLocked = false;
    }
    setError(errorMessage, {});
    return info;
}

bool ProtonPassClient::startLogin(QString* errorMessage) const
{
    const QString executable = QStandardPaths::findExecutable(program());
    if (executable.isEmpty()) {
        setError(errorMessage, uiText("Proton Pass CLI was not found. Set its location in Settings.",
            "Proton Pass CLI를 찾지 못했습니다. 설정에서 실행 파일 위치를 지정하세요."));
        return false;
    }
    QProcess process;
    process.setProgram(executable);
    process.setArguments({QStringLiteral("login")});
    process.setProcessEnvironment(cleanLoginEnvironment());
#ifdef Q_OS_WIN
    // The user completes browser authentication and any extra-password prompt.
    process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* arguments) {
        arguments->flags &= ~DWORD(DETACHED_PROCESS | CREATE_NO_WINDOW);
        arguments->flags |= CREATE_NEW_CONSOLE;
        arguments->startupInfo->dwFlags &= ~DWORD(STARTF_USESTDHANDLES);
    });
#else
    setError(errorMessage, uiText("Run pass-cli login in a terminal, then check the connection here.",
        "터미널에서 pass-cli login을 실행한 뒤 여기서 연결 확인을 누르세요."));
    return false;
#endif
    const bool started = process.startDetached();
    setError(errorMessage, started ? QString() : process.errorString());
    return started;
}

bool ProtonPassClient::lock(QString* errorMessage)
{
    // Proton Pass CLI session locks use a separate user-configured lock code.
    // Logging out is the safe, predictable tray action until that flow has its own UI.
    return logout(errorMessage);
}

bool ProtonPassClient::logout(QString* errorMessage)
{
    QMutexLocker<QRecursiveMutex> commandLocker(&m_commandMutex);
    const CommandResult result = run({QStringLiteral("logout")});
    clearSession();
    if (!result.ok()) {
        setError(errorMessage, describeCommandError(program(), {QStringLiteral("logout")}, result));
        return false;
    }

    setError(errorMessage, {});
    return true;
}

bool ProtonPassClient::sync(QString* errorMessage) const
{
    // Proton Pass has no separate sync command; item list requests current vault contents.
    setError(errorMessage, {});
    return true;
}

QVector<VaultItem> ProtonPassClient::listItems(QString* errorMessage) const
{
    QVector<VaultItem> items;
    const CommandResult vaultResult = run({QStringLiteral("vault"), QStringLiteral("list"), QStringLiteral("--output"), QStringLiteral("json")});
    if (!vaultResult.ok()) {
        setError(errorMessage,
            describeCommandError(program(), {QStringLiteral("vault"), QStringLiteral("list")}, vaultResult));
        return items;
    }

    QJsonParseError parseError;
    const QJsonDocument vaultDocument = QJsonDocument::fromJson(vaultResult.standardOutput, &parseError);
    if (parseError.error != QJsonParseError::NoError || !vaultDocument.isObject()) {
        setError(errorMessage,
            uiText("Could not parse Proton Pass vault list: %1",
                   "Proton Pass 보관함 목록을 해석하지 못했습니다: %1")
                .arg(parseError.errorString()));
        return items;
    }

    for (const QJsonValue& vaultValue : vaultDocument.object().value(QStringLiteral("vaults")).toArray()) {
        const QString shareId = objectField(vaultValue.toObject(), QStringLiteral("share_id"));
        if (shareId.isEmpty())
            continue;

        const CommandResult itemResult = run({QStringLiteral("item"),
            QStringLiteral("list"),
            QStringLiteral("--share-id"),
            shareId,
            QStringLiteral("--show-secrets"),
            QStringLiteral("--output"),
            QStringLiteral("json")},
            30000);
        if (!itemResult.ok()) {
            setError(errorMessage,
                describeCommandError(program(), {QStringLiteral("item"), QStringLiteral("list"), QStringLiteral("--share-id"), shareId}, itemResult));
            return {};
        }

        const QJsonDocument itemDocument = QJsonDocument::fromJson(itemResult.standardOutput, &parseError);
        if (parseError.error != QJsonParseError::NoError || !itemDocument.isObject()) {
            setError(errorMessage,
                uiText("Could not parse Proton Pass item list: %1",
                       "Proton Pass 항목 목록을 해석하지 못했습니다: %1")
                    .arg(parseError.errorString()));
            return {};
        }

        for (const QJsonValue& itemValue : itemDocument.object().value(QStringLiteral("items")).toArray()) {
            VaultItem item = vaultItemFromObject(itemValue.toObject(), shareId);
            if (!item.id.isEmpty() && item.deletedDate.isEmpty())
                items.push_back(std::move(item));
        }
    }

    setError(errorMessage, {});
    return items;
}

QString ProtonPassClient::getPassword(const QString& itemId, QString* errorMessage) const
{
    return getItemDetails(itemId, errorMessage).password;
}

QString ProtonPassClient::getUsername(const QString& itemId, QString* errorMessage) const
{
    return getItemDetails(itemId, errorMessage).username;
}

QString ProtonPassClient::getTotp(const QString& itemId, QString* errorMessage) const
{
    QString shareId;
    QString rawItemId;
    if (!splitItemKey(itemId, &shareId, &rawItemId)) {
        setError(errorMessage, uiText("Invalid Proton Pass item identifier.", "Proton Pass 항목 식별자가 올바르지 않습니다."));
        return {};
    }

    const CommandResult result = run({QStringLiteral("item"),
        QStringLiteral("totp"),
        QStringLiteral("--share-id"),
        shareId,
        QStringLiteral("--item-id"),
        rawItemId,
        QStringLiteral("--output"),
        QStringLiteral("json")});
    if (!result.ok()) {
        setError(errorMessage,
            describeCommandError(program(), {QStringLiteral("item"), QStringLiteral("totp"), QStringLiteral("--item-id"), rawItemId}, result));
        return {};
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(result.standardOutput, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject() || document.object().isEmpty()) {
        setError(errorMessage,
            uiText("Could not parse the Proton Pass OTP code.", "Proton Pass OTP 코드를 해석하지 못했습니다."));
        return {};
    }

    const QJsonObject codes = document.object();
    if (codes.size() == 1) {
        setError(errorMessage, {});
        return valueAsText(codes.constBegin().value());
    }

    const QStringList preferredNames = {
        QStringLiteral("TOTP"),
        QStringLiteral("totp_uri"),
        QStringLiteral("OTP"),
        QStringLiteral("Authenticator")
    };
    for (const QString& preferredName : preferredNames) {
        for (auto it = codes.constBegin(); it != codes.constEnd(); ++it) {
            if (it.key().compare(preferredName, Qt::CaseInsensitive) == 0) {
                setError(errorMessage, {});
                return valueAsText(it.value());
            }
        }
    }

    setError(errorMessage,
        uiText("This item has multiple OTP fields. Open Details and copy the required code.",
               "이 항목에는 OTP 필드가 여러 개 있습니다. 상세보기에서 필요한 코드를 복사하세요."));
    return {};

}

ProtonPassClient::ItemDetails ProtonPassClient::getItemDetails(const QString& itemId, QString* errorMessage) const
{
    ItemDetails details;
    QString shareId;
    QString rawItemId;
    if (!splitItemKey(itemId, &shareId, &rawItemId)) {
        setError(errorMessage, uiText("Invalid Proton Pass item identifier.", "Proton Pass 항목 식별자가 올바르지 않습니다."));
        return details;
    }

    const CommandResult result = run({QStringLiteral("item"),
        QStringLiteral("view"),
        QStringLiteral("--share-id"),
        shareId,
        QStringLiteral("--item-id"),
        rawItemId,
        QStringLiteral("--output"),
        QStringLiteral("json")});
    if (!result.ok()) {
        setError(errorMessage,
            describeCommandError(program(), {QStringLiteral("item"), QStringLiteral("view"), QStringLiteral("--item-id"), rawItemId}, result));
        return details;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(result.standardOutput, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(errorMessage,
            uiText("Could not parse Proton Pass item: %1", "Proton Pass 항목을 해석하지 못했습니다: %1")
                .arg(parseError.errorString()));
        return details;
    }

    details = detailsFromObject(document.object(), shareId);
    setError(errorMessage, {});
    return details;
}

QString ProtonPassClient::findProgram()
{
    const QString configuredProgram = AppSettings::protonPassProgramOverride();
    if (!configuredProgram.isEmpty())
        return configuredProgram;

#ifdef Q_OS_WIN
    const QString localProgram = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("pass-cli.exe"));
    const QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
    const QString installedProgram = localAppData.isEmpty()
        ? QString()
        : QDir(localAppData).filePath(QStringLiteral("Programs/ProtonPass/pass-cli.exe"));
#else
    const QString localProgram = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("pass-cli"));
#endif
    if (QFileInfo::exists(localProgram) && QFileInfo(localProgram).isFile())
        return localProgram;

#ifdef Q_OS_WIN
    if (QFileInfo::exists(installedProgram) && QFileInfo(installedProgram).isFile())
        return installedProgram;
#endif

    return QStringLiteral("pass-cli");
}

QString ProtonPassClient::makeItemKey(const QString& shareId, const QString& itemId)
{
    return shareId + ItemKeySeparator + itemId;
}

bool ProtonPassClient::splitItemKey(const QString& key, QString* shareId, QString* itemId)
{
    const int separator = key.indexOf(ItemKeySeparator);
    if (separator <= 0 || separator + 1 >= key.size())
        return false;

    if (shareId)
        *shareId = key.left(separator);
    if (itemId)
        *itemId = key.mid(separator + 1);
    return true;
}

VaultItem ProtonPassClient::vaultItemFromObject(const QJsonObject& object, const QString& fallbackShareId)
{
    return itemFromPayload(object, fallbackShareId);
}

ProtonPassClient::ItemDetails ProtonPassClient::detailsFromObject(const QJsonObject& object, const QString& fallbackShareId)
{
    const VaultItem item = itemFromPayload(object, fallbackShareId);
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
    details.revisionDate = item.revisionDate;
    details.creationDate = item.creationDate;
    details.deletedDate = item.deletedDate;
    details.uris = item.uris;
    details.providerSections = item.providerSections;
    return details;
}

QString ProtonPassClient::describeCommandError(const QString& program,
    const QStringList& arguments,
    const CommandResult& result)
{
    const QString command = program + QLatin1Char(' ') + arguments.join(QLatin1Char(' '));
    if (!result.started) {
        return !result.errorString.isEmpty()
            ? result.errorString
            : uiText("Could not start %1.", "%1을(를) 시작하지 못했습니다.").arg(command);
    }
    if (result.timedOut)
        return uiText("Timed out while running %1.", "%1 실행 시간이 초과되었습니다.").arg(command);
    if (result.crashed)
        return uiText("%1 crashed.", "%1 실행이 비정상 종료되었습니다.").arg(command);
    const QString standardError = trimmedUtf8(result.standardError);
    if (!standardError.isEmpty())
        return standardError;
    const QString standardOutput = trimmedUtf8(result.standardOutput);
    if (!standardOutput.isEmpty())
        return standardOutput;
    return uiText("%1 failed with exit code %2.", "%1 실행에 실패했습니다. 종료 코드: %2")
        .arg(command)
        .arg(result.exitCode);
}

ProtonPassClient::CommandResult ProtonPassClient::run(const QStringList& arguments,
    int timeoutMs,
    const QProcessEnvironment& extraEnvironment) const
{
    QMutexLocker<QRecursiveMutex> commandLocker(&m_commandMutex);
    const QString programSnapshot = program();

    QProcess process;
    process.setProgram(programSnapshot);
    process.setArguments(arguments);
    process.setProcessChannelMode(QProcess::SeparateChannels);

    QProcessEnvironment environment = cleanLoginEnvironment();
    for (const QString& key : extraEnvironment.keys())
        environment.insert(key, extraEnvironment.value(key));
    environment.insert(QStringLiteral("NO_COLOR"), QStringLiteral("1"));
    process.setProcessEnvironment(environment);
    process.start();

    CommandResult result;
    result.started = process.waitForStarted(3000);
    if (!result.started) {
        result.errorString = process.errorString();
        if (process.state() != QProcess::NotRunning) {
            process.kill();
            process.waitForFinished(1000);
        }
        return result;
    }

    process.closeWriteChannel();

    if (!process.waitForFinished(timeoutMs)) {
        result.timedOut = true;
        process.kill();
        process.waitForFinished(1000);
    }

    result.crashed = process.exitStatus() == QProcess::CrashExit;
    result.exitCode = process.exitCode();
    result.standardOutput = process.readAllStandardOutput();
    result.standardError = process.readAllStandardError();
    result.errorString = process.errorString();
    return result;
}
