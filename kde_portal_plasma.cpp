// SPDX-License-Identifier: LGPL-2.0-or-later

#include "kcolorscheme/kde_palette.h"
#include "portal/kde_portal_settings.h"

#include <array>
#include <functional>
#include <memory>
#include <optional>

#include <qpa/qplatformdialoghelper.h>
#include <qpa/qplatformtheme.h>
#include <qpa/qplatformthemefactory_p.h>
#include <qpa/qplatformthemeplugin.h>
#include <qpa/qwindowsysteminterface.h>

#include <QByteArray>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusVariant>
#include <QFileInfo>
#include <QFont>
#include <QGuiApplication>
#include <QIcon>
#include <QKeySequence>
#include <QList>
#include <QPalette>
#include <QPixmap>
#include <QRegularExpression>
#include <QSettings>
#include <QSizeF>
#include <QStandardPaths>
#include <QStringList>
#include <QVariant>

namespace {

constexpr auto kdePortalThemeKey = "kde-portal-plasma";
constexpr auto xdgDesktopPortalThemeKey = "xdgdesktopportal";

using namespace KdePortal;

std::optional<QVariant> readPortalSetting(const QString &location, const QString &key)
{
    QDBusMessage message = QDBusMessage::createMethodCall(
        QString::fromLatin1(portalService),
        QString::fromLatin1(portalPath),
        QString::fromLatin1(portalSettingsInterface),
        QStringLiteral("Read"));
    message << location << key;

    QDBusReply<QVariant> reply = QDBusConnection::sessionBus().call(message, QDBus::Block, 700);
    if (!reply.isValid())
        return std::nullopt;

    return unwrapDBusVariant(reply.value());
}

PortalSettingsMap readKdeGlobalsPortal()
{
    QDBusMessage message = QDBusMessage::createMethodCall(
        QString::fromLatin1(portalService),
        QString::fromLatin1(portalPath),
        QString::fromLatin1(portalSettingsInterface),
        QStringLiteral("ReadAll"));
    message << QStringList{QStringLiteral("org.kde.kdeglobals.*")};

    PortalSettingsMap settings;
    const QDBusMessage reply = QDBusConnection::sessionBus().call(message, QDBus::Block, 700);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
        return settings;

    const QDBusArgument argument = reply.arguments().at(0).value<QDBusArgument>();
    argument >> settings;
    return settings;
}

Qt::ColorScheme colorSchemeFromPortal(const PortalSettingsMap &settings)
{
    const auto value = readPortalSetting(QString::fromLatin1(freedesktopAppearance), QStringLiteral("color-scheme"));
    if (value) {
        switch (value->toUInt()) {
        case 1:
            return Qt::ColorScheme::Dark;
        case 2:
            return Qt::ColorScheme::Light;
        default:
            break;
        }
    }

    const QString colorSchemeName = readKdeString(settings, QString::fromLatin1(kdeGeneral), QStringLiteral("ColorScheme"));
    if (colorSchemeName.contains(QLatin1String("dark"), Qt::CaseInsensitive))
        return Qt::ColorScheme::Dark;
    if (colorSchemeName.contains(QLatin1String("light"), Qt::CaseInsensitive))
        return Qt::ColorScheme::Light;

    return Qt::ColorScheme::Unknown;
}

std::optional<QString> readableConfigPath(const QString &relativePath)
{
    const QStringList paths = QStandardPaths::locateAll(QStandardPaths::GenericConfigLocation, relativePath);
    for (const QString &path : paths) {
        const QFileInfo info(path);
        if (info.isFile() && info.isReadable())
            return path;
    }

    return std::nullopt;
}

class PortalSettingsWatcher : public QObject
{
    Q_OBJECT

public:
    explicit PortalSettingsWatcher(std::function<void()> onChanged)
        : m_onChanged(std::move(onChanged))
    {
        qRegisterMetaType<QDBusVariant>();
        QDBusConnection::sessionBus().connect(
            QString(),
            QString::fromLatin1(portalPath),
            QString::fromLatin1(portalSettingsInterface),
            QStringLiteral("SettingChanged"),
            this,
            SLOT(onSettingChanged(QString,QString,QDBusVariant)));
    }

private Q_SLOTS:
    void onSettingChanged(const QString &location, const QString &key, const QDBusVariant &)
    {
        if (!isRelevant(location, key))
            return;

        if (m_onChanged)
            m_onChanged();
    }

private:
    static bool isRelevant(const QString &location, const QString &key)
    {
        if (location == QLatin1String(freedesktopAppearance))
            return key == QLatin1String("color-scheme") || key == QLatin1String("accent-color");

        if (location == QLatin1String(kdeGeneral))
            return key == QLatin1String("ColorScheme") || key.endsWith(QLatin1String("Font"), Qt::CaseInsensitive)
                || key == QLatin1String("font") || key == QLatin1String("fixed") || key == QLatin1String("widgetStyle");

        if (location == QLatin1String(kdeKde) || location == QLatin1String(kdeIcons)
            || location == QLatin1String(kdeToolbarStyle) || location == QLatin1String(kdeToolbarIcons))
            return true;

        return location.startsWith(QLatin1String("org.kde.kdeglobals.Colors:"));
    }

    std::function<void()> m_onChanged;
};

class KdePortalTheme : public QPlatformTheme
{
public:
    KdePortalTheme()
    {
        updateWaylandCursorTheme();
        m_portalTheme.reset(QPlatformThemeFactory::create(QString::fromLatin1(xdgDesktopPortalThemeKey)));
        if (!m_portalTheme)
            m_portalTheme = std::make_unique<QPlatformTheme>();

        reload(false);
        m_watcher = std::make_unique<PortalSettingsWatcher>([this]() {
            reload(true);
        });
    }

    QPlatformMenuItem *createPlatformMenuItem() const override
    {
        return m_portalTheme->createPlatformMenuItem();
    }

    QPlatformMenu *createPlatformMenu() const override
    {
        return m_portalTheme->createPlatformMenu();
    }

    QPlatformMenuBar *createPlatformMenuBar() const override
    {
        return m_portalTheme->createPlatformMenuBar();
    }

    void showPlatformMenuBar() override
    {
        m_portalTheme->showPlatformMenuBar();
    }

    bool usePlatformNativeDialog(DialogType type) const override
    {
        return m_portalTheme->usePlatformNativeDialog(type);
    }

    QPlatformDialogHelper *createPlatformDialogHelper(DialogType type) const override
    {
        return m_portalTheme->createPlatformDialogHelper(type);
    }

#ifndef QT_NO_SYSTEMTRAYICON
    QPlatformSystemTrayIcon *createPlatformSystemTrayIcon() const override
    {
        return m_portalTheme->createPlatformSystemTrayIcon();
    }
#endif

    const QPalette *palette(Palette type = SystemPalette) const override
    {
        if (type >= 0 && type < NPalettes)
            return &m_palettes[static_cast<size_t>(type)];
        return &m_palettes[SystemPalette];
    }

    const QFont *font(Font type = SystemFont) const override
    {
        if (type >= 0 && type < NFonts && m_fontSet[static_cast<size_t>(type)])
            return &m_fonts[static_cast<size_t>(type)];
        return &m_fonts[SystemFont];
    }

    Qt::ColorScheme colorScheme() const override
    {
        return m_colorScheme;
    }

    QVariant themeHint(ThemeHint hint) const override
    {
        switch (hint) {
        case StyleNames:
            return m_styleNames;
        case SystemIconThemeName:
            return m_iconThemeName;
        case SystemIconFallbackThemeName:
            return QStringLiteral("breeze");
        case KeyboardScheme:
            return int(KdeKeyboardScheme);
        case DialogButtonBoxLayout:
            return QPlatformDialogHelper::KdeLayout;
        case DialogButtonBoxButtonsHaveIcons:
            return m_showIconsOnPushButtons;
        case ItemViewActivateItemOnSingleClick:
            return m_singleClick;
        case ToolButtonStyle:
            return m_toolButtonStyle;
        case ToolBarIconSize:
            return m_toolBarIconSize;
        case WheelScrollLines:
            return m_wheelScrollLines;
        case MouseDoubleClickInterval:
            return m_doubleClickInterval;
        case StartDragDistance:
            return m_startDragDistance;
        case StartDragTime:
            return m_startDragTime;
        case CursorFlashTime:
            return m_cursorFlashTime;
        case MouseCursorTheme:
            if (m_useWaylandCursorConfig && !m_cursorTheme.isEmpty())
                return m_cursorTheme;
            return m_portalTheme->themeHint(hint);
        case MouseCursorSize:
            if (m_useWaylandCursorConfig && m_cursorSize > 0)
                return m_cursorSize;
            return m_portalTheme->themeHint(hint);
        case UiEffects:
            return int(HoverEffect);
        default:
            return m_portalTheme->themeHint(hint);
        }
    }

    QPixmap standardPixmap(StandardPixmap sp, const QSizeF &size) const override
    {
        return m_portalTheme->standardPixmap(sp, size);
    }

    QIcon fileIcon(const QFileInfo &fileInfo, QPlatformTheme::IconOptions iconOptions = { }) const override
    {
        return m_portalTheme->fileIcon(fileInfo, iconOptions);
    }

    QIconEngine *createIconEngine(const QString &iconName) const override
    {
        return m_portalTheme->createIconEngine(iconName);
    }

#if QT_CONFIG(shortcut)
    QList<QKeySequence> keyBindings(QKeySequence::StandardKey key) const override
    {
        return m_portalTheme->keyBindings(key);
    }
#endif

    QString standardButtonText(int button) const override
    {
        return m_portalTheme->standardButtonText(button);
    }

#if QT_CONFIG(shortcut)
    QKeySequence standardButtonShortcut(int button) const override
    {
        return m_portalTheme->standardButtonShortcut(button);
    }
#endif

private:
    void reload(bool notify)
    {
        m_kdeGlobalsPortal = readKdeGlobalsPortal();
        m_colorScheme = colorSchemeFromPortal(m_kdeGlobalsPortal);
        reloadPalette();
        reloadFonts();
        reloadHints();

        if (m_colorScheme == Qt::ColorScheme::Unknown) {
            const int bgLightness = m_palettes[SystemPalette].color(QPalette::Window).lightness();
            m_colorScheme = bgLightness < 128 ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light;
        }

        if (notify)
            QWindowSystemInterface::handleThemeChange();
    }

    void reloadPalette()
    {
        const QPalette palette = createKdeApplicationPalette(m_kdeGlobalsPortal);

        for (auto &entry : m_palettes)
            entry = palette;
    }

    void reloadFonts()
    {
        m_fontSet.fill(false);

        setFont(SystemFont, readKdeFont(m_kdeGlobalsPortal, QStringLiteral("font")).value_or(QFont(QStringLiteral("Noto Sans"), 10)));
        setFont(FixedFont, readKdeFont(m_kdeGlobalsPortal, QStringLiteral("fixed")).value_or(QFont(QStringLiteral("monospace"), 10)));

        if (auto font = readKdeFont(m_kdeGlobalsPortal, QStringLiteral("menuFont"))) {
            setFont(MenuFont, *font);
            setFont(MenuBarFont, *font);
            setFont(MenuItemFont, *font);
        }

        if (auto font = readKdeFont(m_kdeGlobalsPortal, QStringLiteral("toolBarFont")))
            setFont(ToolButtonFont, *font);

        if (auto font = readKdeFont(m_kdeGlobalsPortal, QStringLiteral("smallestReadableFont"))) {
            setFont(SmallFont, *font);
            setFont(MiniFont, *font);
            setFont(TipLabelFont, *font);
        }
    }

    void reloadHints()
    {
        const QString widgetStyle = readKdeString(m_kdeGlobalsPortal, QString::fromLatin1(kdeKde), QStringLiteral("widgetStyle"),
            readKdeString(m_kdeGlobalsPortal, QString::fromLatin1(kdeGeneral), QStringLiteral("widgetStyle")));

        m_styleNames.clear();
        if (!widgetStyle.isEmpty())
            m_styleNames << widgetStyle;
        m_styleNames << QStringLiteral("breeze") << QStringLiteral("Fusion") << QStringLiteral("windows");
        m_styleNames.removeDuplicates();

        m_iconThemeName = readKdeString(m_kdeGlobalsPortal, QString::fromLatin1(kdeIcons), QStringLiteral("Theme"), QStringLiteral("breeze"));
        m_singleClick = readKdeBool(m_kdeGlobalsPortal, QString::fromLatin1(kdeKde), QStringLiteral("SingleClick"), m_singleClick);
        m_showIconsOnPushButtons = readKdeBool(m_kdeGlobalsPortal, QString::fromLatin1(kdeKde), QStringLiteral("ShowIconsOnPushButtons"), m_showIconsOnPushButtons);

        const QString toolButtonStyle = readKdeString(m_kdeGlobalsPortal, QString::fromLatin1(kdeToolbarStyle), QStringLiteral("ToolButtonStyle"));
        if (toolButtonStyle == QLatin1String("TextOnly"))
            m_toolButtonStyle = Qt::ToolButtonTextOnly;
        else if (toolButtonStyle == QLatin1String("TextUnderIcon"))
            m_toolButtonStyle = Qt::ToolButtonTextUnderIcon;
        else
            m_toolButtonStyle = Qt::ToolButtonTextBesideIcon;

        m_toolBarIconSize = readKdeInt(m_kdeGlobalsPortal, QString::fromLatin1(kdeToolbarIcons), QStringLiteral("Size"), m_toolBarIconSize);
        m_wheelScrollLines = readKdeInt(m_kdeGlobalsPortal, QString::fromLatin1(kdeKde), QStringLiteral("WheelScrollLines"), m_wheelScrollLines);
        m_doubleClickInterval = readKdeInt(m_kdeGlobalsPortal, QString::fromLatin1(kdeKde), QStringLiteral("DoubleClickInterval"), m_doubleClickInterval);
        m_startDragDistance = readKdeInt(m_kdeGlobalsPortal, QString::fromLatin1(kdeKde), QStringLiteral("StartDragDist"), m_startDragDistance);
        m_startDragTime = readKdeInt(m_kdeGlobalsPortal, QString::fromLatin1(kdeKde), QStringLiteral("StartDragTime"), m_startDragTime);

        const int cursorBlinkRate = readKdeInt(m_kdeGlobalsPortal, QString::fromLatin1(kdeKde), QStringLiteral("CursorBlinkRate"), m_cursorFlashTime);
        m_cursorFlashTime = cursorBlinkRate > 0 ? qBound(200, cursorBlinkRate, 2000) : 0;
    }

    void updateWaylandCursorTheme()
    {
        m_useWaylandCursorConfig = QGuiApplication::platformName() == QLatin1String("wayland");
        if (!m_useWaylandCursorConfig)
            return;

        const auto inputConfigPath = readableConfigPath(QStringLiteral("kcminputrc"));
        if (!inputConfigPath)
            return;

        QSettings inputConfig(*inputConfigPath, QSettings::IniFormat);
        inputConfig.beginGroup(QStringLiteral("Mouse"));
        const bool hasCursorTheme = inputConfig.contains(QStringLiteral("cursorTheme"));
        const QString cursorTheme = inputConfig.value(QStringLiteral("cursorTheme")).toString().trimmed();
        const bool hasCursorSize = inputConfig.contains(QStringLiteral("cursorSize"));
        bool ok = false;
        const int configuredSize = inputConfig.value(QStringLiteral("cursorSize")).toInt(&ok);
        inputConfig.endGroup();

        if (hasCursorTheme && !cursorTheme.isEmpty()) {
            m_cursorTheme = cursorTheme;
            qputenv("XCURSOR_THEME", cursorTheme.toUtf8());
        }

        if (hasCursorSize && ok && configuredSize > 0) {
            m_cursorSize = configuredSize;
            qputenv("XCURSOR_SIZE", QByteArray::number(configuredSize));
        }
    }

    void setFont(Font type, const QFont &font)
    {
        m_fonts[static_cast<size_t>(type)] = font;
        m_fontSet[static_cast<size_t>(type)] = true;
    }

    std::array<QPalette, NPalettes> m_palettes;
    std::array<QFont, NFonts> m_fonts;
    std::array<bool, NFonts> m_fontSet = {};
    mutable std::unique_ptr<QPlatformTheme> m_portalTheme;
    std::unique_ptr<PortalSettingsWatcher> m_watcher;
    PortalSettingsMap m_kdeGlobalsPortal;

    Qt::ColorScheme m_colorScheme = Qt::ColorScheme::Unknown;
    QStringList m_styleNames = { QStringLiteral("breeze"), QStringLiteral("Fusion"), QStringLiteral("windows") };
    QString m_iconThemeName = QStringLiteral("breeze");

    bool m_singleClick = true;
    bool m_showIconsOnPushButtons = false;
    Qt::ToolButtonStyle m_toolButtonStyle = Qt::ToolButtonTextBesideIcon;
    int m_toolBarIconSize = 0;
    int m_wheelScrollLines = 3;
    int m_doubleClickInterval = 400;
    int m_startDragDistance = 10;
    int m_startDragTime = 500;
    int m_cursorFlashTime = 1000;
    bool m_useWaylandCursorConfig = false;
    QString m_cursorTheme;
    int m_cursorSize = 0;
};

class KdePortalPlugin : public QPlatformThemePlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QPlatformThemeFactoryInterface_iid FILE "plugin_metadata_plasma.json")

public:
    QPlatformTheme *create(const QString &key, const QStringList &params) override
    {
        Q_UNUSED(params);
        if (key.compare(QLatin1String(kdePortalThemeKey), Qt::CaseInsensitive) == 0)
            return new KdePortalTheme;

        return nullptr;
    }
};

} // namespace

#include "kde_portal_plasma.moc"
