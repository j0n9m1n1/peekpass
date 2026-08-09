#include <QCoreApplication>
#include <QFile>
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

QString readSecretFile(const QProcessEnvironment& environment, const QString& key)
{
    const QString path = environment.value(key);
    QFile file(path);
    if (path.isEmpty() || !file.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(file.readAll());
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
        output << "pass-cli 1.0.0-test" << Qt::endl;
        return 0;
    }

    if (arguments == QStringList{QStringLiteral("info"), QStringLiteral("--output"), QStringLiteral("json")}) {
        if (environment.value(QStringLiteral("PEEKPASS_FAKE_PASS_MODE")) == QStringLiteral("unauthenticated")) {
            QTextStream(stderr) << "not logged in" << Qt::endl;
            return 1;
        }
        output << R"json({"email":"tester@proton.me"})json" << Qt::endl;
        return 0;
    }

    if (arguments == QStringList{QStringLiteral("login"), QStringLiteral("--interactive"), QStringLiteral("tester@proton.me")}) {
        if (hasDirectCredential(environment))
            return fail(QStringLiteral("direct credential environment variable leaked to pass-cli"));
        if (readSecretFile(environment, QStringLiteral("PROTON_PASS_PASSWORD_FILE")) != QStringLiteral("fixture-master"))
            return fail(QStringLiteral("password file was missing or incorrect"));
        if (readSecretFile(environment, QStringLiteral("PROTON_PASS_TOTP_FILE")) != QStringLiteral("123456"))
            return fail(QStringLiteral("TOTP file was missing or incorrect"));
        if (readSecretFile(environment, QStringLiteral("PROTON_PASS_EXTRA_PASSWORD_FILE")) != QStringLiteral("fixture-extra"))
            return fail(QStringLiteral("extra-password file was missing or incorrect"));
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
