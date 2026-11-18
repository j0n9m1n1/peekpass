#pragma once

#include <QString>

struct TotpCodeInfo
{
    QString code;
    int remainingSeconds = 0;
    int period = 30;
};

namespace TotpGenerator
{
TotpCodeInfo generateTotpInfo(const QString& value, QString* errorMessage = nullptr);
QString generateTotpCode(const QString& value, QString* errorMessage = nullptr);
QString formatTotpCode(const QString& code);
}
