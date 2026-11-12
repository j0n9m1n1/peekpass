#include <QCoreApplication>
#include <QProcessEnvironment>
#include <QStringList>
#include <QTextStream>

namespace
{
int fail(const QString& message)
{
    QTextStream(stderr) << message << Qt::endl;
    return 64;
}

bool hasDirectCredential(const QProcessEnvironment& environment)
{
    return environment.contains(QStringLiteral("PROTON_PASS_PASSWORD"))
        || environment.contains(QStringLiteral("PROTON_PASS_TOTP"))
        || environment.contains(QStringLiteral("PROTON_PASS_EXTRA_PASSWORD"));
}
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList arguments = app.arguments().mid(1);
    QTextStream output(stdout);
    const QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();

    if (arguments == QStringList{QStringLiteral("--version")}) {
        output << "pass-cli 2.7.0-test" << Qt::endl;
        return 0;
    }

    if (arguments == QStringList{QStringLiteral("info"), QStringLiteral("--output"), QStringLiteral("json")}) {
        if (hasDirectCredential(environment)
            || environment.contains(QStringLiteral("PROTON_PASS_PASSWORD_FILE"))
            || environment.contains(QStringLiteral("PROTON_PASS_PERSONAL_ACCESS_TOKEN")))
            return fail(QStringLiteral("inherited credentials leaked to pass-cli"));
        if (environment.value(QStringLiteral("PEEKPASS_FAKE_PASS_MODE")) == QStringLiteral("unauthenticated")) {
            QTextStream(stderr) << "not logged in" << Qt::endl;
            return 1;
        }
        if (environment.value(QStringLiteral("PEEKPASS_FAKE_PASS_MODE")) == QStringLiteral("locked")) {
            QTextStream(stderr) << "session is locked" << Qt::endl;
            return 1;
        }
        if (environment.value(QStringLiteral("PEEKPASS_FAKE_PASS_MODE")) == QStringLiteral("offline")) {
            QTextStream(stderr) << "network unavailable" << Qt::endl;
            return 1;
        }
        output << R"json({"email":"tester@proton.me"})json" << Qt::endl;
        return 0;
    }

    if (arguments == QStringList{QStringLiteral("login")}) {
        return 0;
    }

    if (arguments == QStringList{QStringLiteral("logout")})
        return 0;

    if (arguments == QStringList{QStringLiteral("vault"), QStringLiteral("list"), QStringLiteral("--output"), QStringLiteral("json")}) {
        output << R"json({"vaults":[{"name":"Personal","vault_id":"vault-1","share_id":"share-1"}]})json" << Qt::endl;
        return 0;
    }

    const QStringList itemListArguments = {
        QStringLiteral("item"),
        QStringLiteral("list"),
        QStringLiteral("--share-id"),
        QStringLiteral("share-1"),
        QStringLiteral("--show-secrets"),
        QStringLiteral("--output"),
        QStringLiteral("json")
    };
    if (arguments == itemListArguments) {
        if (environment.value(QStringLiteral("PEEKPASS_FAKE_PASS_MODE")) == QStringLiteral("list-error"))
            return fail(QStringLiteral("vault request failed"));
        output << R"json(
{
  "items": [
    {
      "id": "item-1",
      "share_id": "share-1",
      "vault_id": "vault-1",
      "state": "Active",
      "flags": [],
      "create_time": "2026-08-01T12:00:00",
      "modify_time": "2026-08-02T13:30:00",
      "folder_id": "folder-1",
      "content": {
        "title": "Example Login",
        "note": "Fixture note",
        "item_uuid": "uuid-1",
        "content": {
          "Login": {
            "email": "mail@example.com",
            "username": "fixture-user",
            "password": "fixture-password",
            "urls": ["https://example.com"],
            "totp_uri": "otpauth://totp/Example?secret=JBSWY3DPEHPK3PXP",
            "passkeys": []
          }
        },
        "extra_fields": [
          {"name":"API token","content":{"Hidden":"fixture-token"}},
          {"name":"Secondary OTP","content":{"Totp":"otpauth://totp/Secondary?secret=AAAA"}}
        ]
      }
    }
  ]
}
)json" << Qt::endl;
        return 0;
    }

    const QStringList itemViewArguments = {
        QStringLiteral("item"),
        QStringLiteral("view"),
        QStringLiteral("--share-id"),
        QStringLiteral("share-1"),
        QStringLiteral("--item-id"),
        QStringLiteral("item-1"),
        QStringLiteral("--output"),
        QStringLiteral("json")
    };
    if (arguments == itemViewArguments) {
        output << R"json(
{
  "item": {
    "id": "item-1",
    "share_id": "share-1",
    "vault_id": "vault-1",
    "state": "Active",
    "flags": [],
    "create_time": "2026-08-01T12:00:00",
    "modify_time": "2026-08-02T13:30:00",
    "folder_id": "folder-1",
    "content": {
      "title": "Example Login",
      "note": "Fixture note",
      "item_uuid": "uuid-1",
      "content": {
        "Login": {
          "email": "mail@example.com",
          "username": "fixture-user",
          "password": "fixture-password",
          "urls": ["https://example.com"],
          "totp_uri": "otpauth://totp/Example?secret=JBSWY3DPEHPK3PXP",
          "passkeys": []
        }
      },
      "extra_fields": [
        {"name":"API token","content":{"Hidden":"fixture-token"}}
      ]
    }
  },
  "attachments": []
}
)json" << Qt::endl;
        return 0;
    }

    const QStringList itemTotpArguments = {
        QStringLiteral("item"),
        QStringLiteral("totp"),
        QStringLiteral("--share-id"),
        QStringLiteral("share-1"),
        QStringLiteral("--item-id"),
        QStringLiteral("item-1"),
        QStringLiteral("--output"),
        QStringLiteral("json")
    };
    if (arguments == itemTotpArguments) {
        output << R"json({"Backup":"111111","TOTP":"654321"})json" << Qt::endl;
        return 0;
    }

    return fail(QStringLiteral("unexpected pass-cli arguments: %1").arg(arguments.join(QLatin1Char(' '))));
}
