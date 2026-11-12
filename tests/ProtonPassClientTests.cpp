#include "AppSettings.h"
#include "ProtonPassClient.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>

namespace
{
int failures = 0;

void check(bool condition, const QString& message)
{
    if (condition)
        return;

    ++failures;
    qCritical().noquote() << message;
}
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("peekpass-tests"));
    QCoreApplication::setApplicationName(QStringLiteral("proton-pass-client-tests"));

    if (app.arguments().size() != 2) {
        qCritical("Expected the fake pass-cli path as the only argument.");
        return 2;
    }

    QTemporaryDir settingsDirectory;
    check(settingsDirectory.isValid(), QStringLiteral("Could not create isolated test settings directory."));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory.path());

    const QString fakeProgram = QFileInfo(app.arguments().at(1)).absoluteFilePath();
    check(QFileInfo::exists(fakeProgram), QStringLiteral("Fake pass-cli does not exist: %1").arg(fakeProgram));
    AppSettings::setProtonPassProgramOverride(fakeProgram);

    ProtonPassClient client;
    check(QFileInfo(client.program()).absoluteFilePath() == fakeProgram,
        QStringLiteral("Configured pass-cli override was not selected."));

    QString error;
    check(client.isAvailable(&error), QStringLiteral("Fake pass-cli availability check failed: %1").arg(error));

    qputenv("PROTON_PASS_PASSWORD", "inherited-password-must-not-leak");
    qputenv("PROTON_PASS_TOTP", "inherited-totp-must-not-leak");
    qputenv("PROTON_PASS_EXTRA_PASSWORD", "inherited-extra-must-not-leak");
    qputenv("PROTON_PASS_PASSWORD_FILE", "must-not-be-read");
    qputenv("PROTON_PASS_PERSONAL_ACCESS_TOKEN", "must-not-be-used");
    check(!client.hasSession(), QStringLiteral("Session must be verified before use."));

    const ProtonPassClient::StatusInfo status = client.statusInfo(&error);
    check(client.hasSession(), QStringLiteral("Verified session was not marked active."));
    check(status.status == ProtonPassClient::Status::Unlocked,
        QStringLiteral("Authenticated status was not reported as unlocked: %1").arg(error));
    check(status.userEmail == QStringLiteral("tester@proton.me"),
        QStringLiteral("Status email was parsed incorrectly."));

    const QVector<VaultItem> items = client.listItems(&error);
    check(error.isEmpty(), QStringLiteral("Item listing returned an error: %1").arg(error));
    check(items.size() == 1, QStringLiteral("Expected one fixture item, got %1.").arg(items.size()));
    if (items.size() == 1) {
        const VaultItem& item = items.constFirst();
        check(item.providerId == QStringLiteral("protonpass"), QStringLiteral("Provider ID was parsed incorrectly."));
        check(item.name == QStringLiteral("Example Login"), QStringLiteral("Item title was parsed incorrectly."));
        check(item.username == QStringLiteral("fixture-user"), QStringLiteral("Username was parsed incorrectly."));
        check(item.password == QStringLiteral("fixture-password"), QStringLiteral("Password was parsed incorrectly."));
        check(item.totp.startsWith(QStringLiteral("otpauth://")), QStringLiteral("TOTP URI was parsed incorrectly."));
        check(item.uris == QStringList{QStringLiteral("https://example.com")}, QStringLiteral("URLs were parsed incorrectly."));
        check(item.customFields.size() == 1, QStringLiteral("Hidden custom field was not retained."));

        QString shareId;
        QString itemId;
        check(ProtonPassClient::splitItemKey(item.id, &shareId, &itemId), QStringLiteral("Composite item key could not be split."));
        check(shareId == QStringLiteral("share-1") && itemId == QStringLiteral("item-1"),
            QStringLiteral("Composite item key contents were incorrect."));

        const VaultItemDetails details = client.getItemDetails(item.id, &error);
        check(error.isEmpty(), QStringLiteral("Item details returned an error: %1").arg(error));
        check(details.name == item.name && details.password == item.password,
            QStringLiteral("Item details did not preserve login fields."));
        check(client.getUsername(item.id, &error) == QStringLiteral("fixture-user"),
            QStringLiteral("Username lookup returned the wrong value: %1").arg(error));
        check(client.getPassword(item.id, &error) == QStringLiteral("fixture-password"),
            QStringLiteral("Password lookup returned the wrong value: %1").arg(error));
        check(client.getTotp(item.id, &error) == QStringLiteral("654321"),
            QStringLiteral("Preferred TOTP field was not selected: %1").arg(error));
    }

    QString shareId;
    QString itemId;
    check(!ProtonPassClient::splitItemKey(QStringLiteral("invalid"), &shareId, &itemId),
        QStringLiteral("Invalid composite item key was accepted."));

    check(client.logout(&error), QStringLiteral("Logout failed: %1").arg(error));
    check(!client.hasSession(), QStringLiteral("Logout did not clear the session."));

    qputenv("PEEKPASS_FAKE_PASS_MODE", "unauthenticated");
    const ProtonPassClient::StatusInfo unauthenticated = client.statusInfo(&error);
    check(unauthenticated.status == ProtonPassClient::Status::Unauthenticated,
        QStringLiteral("Unauthenticated CLI response was not recognized: %1").arg(error));
    check(error.isEmpty(), QStringLiteral("Expected unauthenticated status to be handled without an error."));
    qunsetenv("PEEKPASS_FAKE_PASS_MODE");

    client.statusInfo(&error);
    qputenv("PEEKPASS_FAKE_PASS_MODE", "locked");
    check(client.statusInfo(&error).status == ProtonPassClient::Status::Locked,
        QStringLiteral("Externally locked session was not recognized."));
    check(!client.hasSession(), QStringLiteral("Externally locked session remained usable."));
    qunsetenv("PEEKPASS_FAKE_PASS_MODE");

    QSettings().clear();
    if (failures == 0)
        qInfo("All Proton Pass client tests passed.");
    return failures == 0 ? 0 : 1;
}
