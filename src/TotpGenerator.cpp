#include "TotpGenerator.h"

#include "I18n.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <QMessageAuthenticationCode>
#include <QUrl>
#include <QUrlQuery>
#include <QtGlobal>

namespace
{
struct TotpSpec
{
    QByteArray secret;
    int digits = 6;
    int period = 30;
    QCryptographicHash::Algorithm algorithm = QCryptographicHash::Sha1;
};

QString uiText(const char* ko, const char* en)
{
    return I18n::translate("QuickWindow", en, ko);
}

QByteArray decodeBase32(const QString& input, bool* ok)
{
    QByteArray output;
    quint32 buffer = 0;
    int bitsLeft = 0;
    bool valid = true;

    for (QChar ch : input.trimmed().toUpper()) {
        if (ch == QLatin1Char('='))
            break;
        if (ch.isSpace() || ch == QLatin1Char('-'))
            continue;

        int value = -1;
        if (ch >= QLatin1Char('A') && ch <= QLatin1Char('Z'))
            value = ch.unicode() - 'A';
        else if (ch >= QLatin1Char('2') && ch <= QLatin1Char('7'))
            value = 26 + ch.unicode() - '2';

        if (value < 0) {
            valid = false;
            break;
        }

        buffer = (buffer << 5) | static_cast<quint32>(value);
        bitsLeft += 5;
        if (bitsLeft >= 8) {
            output.append(static_cast<char>((buffer >> (bitsLeft - 8)) & 0xff));
            bitsLeft -= 8;
            buffer &= bitsLeft == 0 ? 0 : ((1u << bitsLeft) - 1u);
        }
    }

    if (ok)
        *ok = valid && !output.isEmpty();
    return output;
}

QCryptographicHash::Algorithm totpAlgorithm(const QString& value)
{
    const QString normalized = value.trimmed().toUpper();
    if (normalized == QStringLiteral("SHA256"))
        return QCryptographicHash::Sha256;
    if (normalized == QStringLiteral("SHA512"))
        return QCryptographicHash::Sha512;
    return QCryptographicHash::Sha1;
}

TotpSpec parseTotpSpec(const QString& value, QString* errorMessage)
{
    QString secretText = value.trimmed();
    TotpSpec spec;

    if (secretText.startsWith(QStringLiteral("otpauth://"), Qt::CaseInsensitive)) {
        const QUrl url(secretText);
        if (url.host().compare(QStringLiteral("totp"), Qt::CaseInsensitive) != 0) {
            if (errorMessage)
                *errorMessage = uiText("지원하지 않는 OTP 형식입니다.", "Unsupported OTP type.");
            return spec;
        }
        const QUrlQuery query(url);
        secretText = query.queryItemValue(QStringLiteral("secret"));
        if (query.hasQueryItem(QStringLiteral("digits"))) {
            bool digitsOk = false;
            const int parsedDigits = query.queryItemValue(QStringLiteral("digits")).toInt(&digitsOk);
            if (digitsOk)
                spec.digits = qBound(4, parsedDigits, 10);
        }
        if (query.hasQueryItem(QStringLiteral("period"))) {
            bool periodOk = false;
            const int parsedPeriod = query.queryItemValue(QStringLiteral("period")).toInt(&periodOk);
            if (periodOk)
                spec.period = qBound(5, parsedPeriod, 300);
        }
        spec.algorithm = totpAlgorithm(query.queryItemValue(QStringLiteral("algorithm")));
    }

    bool ok = false;
    spec.secret = decodeBase32(secretText, &ok);
    if (!ok && errorMessage)
        *errorMessage = uiText("TOTP secret을 해석하지 못했습니다.", "Could not parse the TOTP secret.");

    return spec;
}
}

namespace TotpGenerator
{
TotpCodeInfo generateTotpInfo(const QString& value, QString* errorMessage)
{
    TotpCodeInfo info;
    if (value.trimmed().isEmpty()) {
        if (errorMessage)
            *errorMessage = uiText("이 항목에는 TOTP가 없습니다.", "This item has no TOTP.");
        return info;
    }

    TotpSpec spec = parseTotpSpec(value, errorMessage);
    if (spec.secret.isEmpty())
        return info;

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const quint64 counter = static_cast<quint64>(now / spec.period);
    info.period = spec.period;
    info.remainingSeconds = spec.period - static_cast<int>(now % spec.period);

    QByteArray message(8, '\0');
    for (int index = 7; index >= 0; --index)
        message[7 - index] = static_cast<char>((counter >> (index * 8)) & 0xff);

    const QByteArray hmac = QMessageAuthenticationCode::hash(message, spec.secret, spec.algorithm);
    spec.secret.fill('\0');

    if (hmac.size() < 20) {
        if (errorMessage)
            *errorMessage = uiText("TOTP 코드를 만들지 못했습니다.", "Could not generate the TOTP code.");
        return info;
    }

    const int offset = static_cast<unsigned char>(hmac.at(hmac.size() - 1)) & 0x0f;
    if (offset + 3 >= hmac.size()) {
        if (errorMessage)
            *errorMessage = uiText("TOTP 코드를 만들지 못했습니다.", "Could not generate the TOTP code.");
        return info;
    }

    const quint32 binary =
        ((static_cast<unsigned char>(hmac.at(offset)) & 0x7f) << 24)
        | ((static_cast<unsigned char>(hmac.at(offset + 1)) & 0xff) << 16)
        | ((static_cast<unsigned char>(hmac.at(offset + 2)) & 0xff) << 8)
        | (static_cast<unsigned char>(hmac.at(offset + 3)) & 0xff);

    quint64 modulo = 1;
    for (int index = 0; index < spec.digits; ++index)
        modulo *= 10;

    if (errorMessage)
        errorMessage->clear();
    info.code = QStringLiteral("%1").arg(static_cast<quint64>(binary) % modulo, spec.digits, 10, QLatin1Char('0'));
    return info;
}

QString generateTotpCode(const QString& value, QString* errorMessage)
{
    return generateTotpInfo(value, errorMessage).code;
}

QString formatTotpCode(const QString& code)
{
    const QString trimmed = code.trimmed();
    if (trimmed.size() == 6)
        return trimmed.left(3) + QLatin1Char(' ') + trimmed.mid(3);
    if (trimmed.size() == 8)
        return trimmed.left(4) + QLatin1Char(' ') + trimmed.mid(4);
    return trimmed;
}
}
