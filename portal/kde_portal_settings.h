// SPDX-License-Identifier: LGPL-2.0-or-later

#pragma once

#include <optional>

#include <QColor>
#include <QDBusArgument>
#include <QFont>
#include <QMap>
#include <QString>
#include <QVariant>

namespace KdePortal {

inline constexpr auto portalService = "org.freedesktop.portal.Desktop";
inline constexpr auto portalPath = "/org/freedesktop/portal/desktop";
inline constexpr auto portalSettingsInterface = "org.freedesktop.portal.Settings";

inline constexpr auto freedesktopAppearance = "org.freedesktop.appearance";
inline constexpr auto kdeGeneral = "org.kde.kdeglobals.General";
inline constexpr auto kdeKde = "org.kde.kdeglobals.KDE";
inline constexpr auto kdeIcons = "org.kde.kdeglobals.Icons";
inline constexpr auto kdeToolbarStyle = "org.kde.kdeglobals.Toolbar style";
inline constexpr auto kdeToolbarIcons = "org.kde.kdeglobals.ToolbarIcons";
inline constexpr auto kdeColorsWindow = "org.kde.kdeglobals.Colors:Window";
inline constexpr auto kdeColorsView = "org.kde.kdeglobals.Colors:View";
inline constexpr auto kdeColorsButton = "org.kde.kdeglobals.Colors:Button";
inline constexpr auto kdeColorsSelection = "org.kde.kdeglobals.Colors:Selection";
inline constexpr auto kdeColorsTooltip = "org.kde.kdeglobals.Colors:Tooltip";
inline constexpr auto kdeColorEffectsDisabled = "org.kde.kdeglobals.ColorEffects:Disabled";
inline constexpr auto kdeColorEffectsInactive = "org.kde.kdeglobals.ColorEffects:Inactive";

using PortalSettingsMap = QMap<QString, QVariantMap>;

const QDBusArgument &operator>>(const QDBusArgument &argument, PortalSettingsMap &map);

QVariant unwrapDBusVariant(const QVariant &value);
std::optional<QVariant> readKdeConfigValue(const PortalSettingsMap &settings, const QString &location, const QString &key);
std::optional<QColor> readKdeColor(const PortalSettingsMap &settings, const QString &location, const QString &key);
QColor readKdeColor(const PortalSettingsMap &settings, const QString &location, const QString &key, const QColor &fallback);
std::optional<QFont> readKdeFont(const PortalSettingsMap &settings, const QString &key);
bool readKdeBool(const PortalSettingsMap &settings, const QString &location, const QString &key, bool fallback);
int readKdeInt(const PortalSettingsMap &settings, const QString &location, const QString &key, int fallback);
qreal readKdeReal(const PortalSettingsMap &settings, const QString &location, const QString &key, qreal fallback);
QString readKdeString(const PortalSettingsMap &settings, const QString &location, const QString &key, const QString &fallback = QString());

} // namespace KdePortal
