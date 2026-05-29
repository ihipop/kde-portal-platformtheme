// SPDX-License-Identifier: LGPL-2.0-or-later

#include "kde_portal_settings.h"

#include <QDBusVariant>
#include <QMetaType>
#include <QStringList>
#include <QtGlobal>

namespace KdePortal {

const QDBusArgument &operator>>(const QDBusArgument &argument, PortalSettingsMap &map)
{
    argument.beginMap();
    map.clear();

    while (!argument.atEnd()) {
        QString key;
        QVariantMap value;
        argument.beginMapEntry();
        argument >> key >> value;
        argument.endMapEntry();
        map.insert(key, value);
    }

    argument.endMap();
    return argument;
}

QVariant unwrapDBusVariant(const QVariant &value)
{
    if (value.canConvert<QDBusVariant>())
        return qvariant_cast<QDBusVariant>(value).variant();
    return value;
}

std::optional<QVariant> readKdeConfigValue(const PortalSettingsMap &settings, const QString &location, const QString &key)
{
    const auto groupIt = settings.constFind(location);
    if (groupIt == settings.constEnd())
        return std::nullopt;

    const auto valueIt = groupIt->constFind(key);
    if (valueIt == groupIt->constEnd())
        return std::nullopt;

    return unwrapDBusVariant(*valueIt);
}

std::optional<QColor> readKdeColor(const PortalSettingsMap &settings, const QString &location, const QString &key)
{
    const auto value = readKdeConfigValue(settings, location, key);
    if (!value)
        return std::nullopt;

    const QString text = value->toString().trimmed();
    if (text.isEmpty())
        return std::nullopt;

    if (text.startsWith(QLatin1Char('#'))) {
        const QColor color(text);
        if (color.isValid())
            return color;
    }

    const QStringList parts = text.split(QLatin1Char(','));
    if (parts.size() < 3)
        return std::nullopt;

    bool okR = false;
    bool okG = false;
    bool okB = false;
    bool okA = true;
    const int r = parts.at(0).trimmed().toInt(&okR);
    const int g = parts.at(1).trimmed().toInt(&okG);
    const int b = parts.at(2).trimmed().toInt(&okB);
    int a = 255;
    if (parts.size() >= 4)
        a = parts.at(3).trimmed().toInt(&okA);

    if (!okR || !okG || !okB || !okA)
        return std::nullopt;

    return QColor(qBound(0, r, 255), qBound(0, g, 255), qBound(0, b, 255), qBound(0, a, 255));
}

QColor readKdeColor(const PortalSettingsMap &settings, const QString &location, const QString &key, const QColor &fallback)
{
    if (const auto color = readKdeColor(settings, location, key))
        return *color;
    return fallback;
}

std::optional<QFont> readKdeFont(const PortalSettingsMap &settings, const QString &key)
{
    const auto value = readKdeConfigValue(settings, QString::fromLatin1(kdeGeneral), key);
    if (!value)
        return std::nullopt;

    QString fontDescription;
    QString fontFamily;

    if (value->typeId() == QMetaType::QStringList) {
        const QStringList list = value->toStringList();
        if (list.isEmpty())
            return std::nullopt;
        fontFamily = list.first();
        fontDescription = list.join(QLatin1Char(','));
    } else {
        fontDescription = value->toString();
        fontFamily = fontDescription.section(QLatin1Char(','), 0, 0);
    }

    if (fontDescription.isEmpty() || fontFamily.isEmpty())
        return std::nullopt;

    QFont font(fontFamily);
    if (!font.fromString(fontDescription))
        return std::nullopt;

    return font;
}

bool readKdeBool(const PortalSettingsMap &settings, const QString &location, const QString &key, bool fallback)
{
    const auto value = readKdeConfigValue(settings, location, key);
    if (!value)
        return fallback;

    const QString text = value->toString().trimmed().toLower();
    if (text == QLatin1String("true") || text == QLatin1String("1") || text == QLatin1String("yes"))
        return true;
    if (text == QLatin1String("false") || text == QLatin1String("0") || text == QLatin1String("no"))
        return false;

    return value->toBool();
}

int readKdeInt(const PortalSettingsMap &settings, const QString &location, const QString &key, int fallback)
{
    const auto value = readKdeConfigValue(settings, location, key);
    if (!value)
        return fallback;

    bool ok = false;
    const int result = value->toString().toInt(&ok);
    if (ok)
        return result;

    const int numeric = value->toInt(&ok);
    return ok ? numeric : fallback;
}

qreal readKdeReal(const PortalSettingsMap &settings, const QString &location, const QString &key, qreal fallback)
{
    const auto value = readKdeConfigValue(settings, location, key);
    if (!value)
        return fallback;

    bool ok = false;
    const qreal result = value->toString().toDouble(&ok);
    if (ok)
        return result;

    const qreal numeric = value->toDouble(&ok);
    return ok ? numeric : fallback;
}

QString readKdeString(const PortalSettingsMap &settings, const QString &location, const QString &key, const QString &fallback)
{
    const auto value = readKdeConfigValue(settings, location, key);
    if (!value)
        return fallback;

    const QString text = value->toString().trimmed();
    return text.isEmpty() ? fallback : text;
}

} // namespace KdePortal
