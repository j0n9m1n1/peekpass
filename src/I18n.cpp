#include "I18n.h"

#include "AppSettings.h"

#include <QCoreApplication>
#include <QDir>
#include <QLibraryInfo>
#include <QLocale>
#include <QSet>
#include <QTranslator>

#include <algorithm>

namespace
{
constexpr const char* TranslationPrefix = "peekpass_";

QString normalizedLanguageCode(const QString& languageCode)
{
    QString normalized = languageCode.trimmed().replace('-', '_');
    if (normalized.isEmpty())
        return QStringLiteral("en");

    const QStringList parts = normalized.split('_', Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return QStringLiteral("en");
    if (parts.size() == 1)
        return parts.at(0).toLower();

    return parts.at(0).toLower() + QLatin1Char('_') + parts.at(1).toUpper();
}

QStringList languageFileVariants(const QString& code)
{
    QStringList variants { code };
    const QString lower = code.toLower();
    if (lower != code)
        variants.push_back(lower);
    const int underscore = code.indexOf(QLatin1Char('_'));
    if (underscore > 0) {
        const QString base = code.left(underscore).toLower();
        if (!variants.contains(base))
            variants.push_back(base);
    }
    return variants;
}

QStringList translationDirectories()
{
    return {
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("translations")),
        QCoreApplication::applicationDirPath(),
        QStringLiteral(":/i18n")
    };
}
}

namespace I18n
{
QString translate(const char* context, const char* sourceText, const char* koreanFallback)
{
    const QString translated = QCoreApplication::translate(context, sourceText);
    if (translated == QLatin1String(sourceText) && koreanFallback && AppSettings::isKorean())
        return QString::fromUtf8(koreanFallback);

    return translated;
}

QStringList availableLanguageCodes()
{
    QSet<QString> codes;
    codes.insert(QStringLiteral("en"));
    codes.insert(QStringLiteral("ko"));

    for (const QString& directoryPath : translationDirectories()) {
        const QDir directory(directoryPath);
        const QStringList files = directory.entryList(
            { QStringLiteral("peekpass_*.qm") },
            QDir::Files | QDir::Readable);
        for (const QString& file : files) {
            QString code = file;
            code.remove(QStringLiteral("peekpass_"));
            code.chop(QStringLiteral(".qm").size());
            if (!code.trimmed().isEmpty())
                codes.insert(normalizedLanguageCode(code));
        }
    }

    QStringList sorted = codes.values();
    std::sort(sorted.begin(), sorted.end());
    if (sorted.removeOne(QStringLiteral("ko")))
        sorted.prepend(QStringLiteral("ko"));
    if (sorted.removeOne(QStringLiteral("en")))
        sorted.prepend(QStringLiteral("en"));
    return sorted;
}

QString languageName(const QString& languageCode)
{
    const QString code = normalizedLanguageCode(languageCode);
    if (code == QStringLiteral("ko"))
        return QStringLiteral("한국어");
    if (code == QStringLiteral("en"))
        return QStringLiteral("English");

    const QLocale locale(code);
    const QString nativeName = locale.nativeLanguageName();
    if (!nativeName.isEmpty())
        return nativeName.left(1).toUpper() + nativeName.mid(1);

    return code;
}

bool installTranslator(QCoreApplication& app, QTranslator& translator, const QString& languageCode)
{
    app.removeTranslator(&translator);

    const QString code = normalizedLanguageCode(languageCode);
    if (code == QStringLiteral("en"))
        return true;

    for (const QString& variant : languageFileVariants(code)) {
        const QString fileBaseName = QStringLiteral("%1%2").arg(QString::fromLatin1(TranslationPrefix), variant);
        for (const QString& directoryPath : translationDirectories()) {
            if (translator.load(fileBaseName, directoryPath)) {
                app.installTranslator(&translator);
                return true;
            }
        }
    }

    return false;
}

bool installQtTranslator(QCoreApplication& app, QTranslator& translator, const QString& languageCode)
{
    app.removeTranslator(&translator);

    const QString code = normalizedLanguageCode(languageCode);
    if (code == QStringLiteral("en"))
        return true;

    QStringList directories = translationDirectories();
    const QString libraryTranslationsPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    if (!libraryTranslationsPath.isEmpty() && !directories.contains(libraryTranslationsPath))
        directories.push_back(libraryTranslationsPath);

    for (const QString& variant : languageFileVariants(code)) {
        const QStringList candidates = {
            QStringLiteral("qtbase_%1").arg(variant),
            QStringLiteral("qt_%1").arg(variant)
        };
        for (const QString& directoryPath : directories) {
            for (const QString& candidate : candidates) {
                if (translator.load(candidate, directoryPath)) {
                    app.installTranslator(&translator);
                    return true;
                }
            }
        }
    }

    return false;
}
}
