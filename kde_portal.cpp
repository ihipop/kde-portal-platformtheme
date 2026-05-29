// SPDX-License-Identifier: LGPL-2.0-or-later

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

#include <QBrush>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusVariant>
#include <QFileInfo>
#include <QFont>
#include <QIcon>
#include <QKeySequence>
#include <QList>
#include <QPalette>
#include <QPixmap>
#include <QRegularExpression>
#include <QSizeF>
#include <QStringList>
#include <QVariant>

namespace {

constexpr auto kdePortalThemeKey = "kde-portal";
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

bool setColorIfPresent(QPalette *palette,
    QPalette::ColorRole role,
    const PortalSettingsMap &settings,
    const QString &location,
    const QString &key)
{
    if (const auto color = readKdeColor(settings, location, key)) {
        palette->setColor(role, *color);
        return true;
    }

    return false;
}

void applyKdeDisabledColors(QPalette *palette)
{
    const QColor button = palette->color(QPalette::Button);
    int h = 0;
    int s = 0;
    int v = 0;
    button.getHsv(&h, &s, &v);

    const QBrush whiteBrush(Qt::white);
    const QBrush buttonBrush(button);
    const QBrush buttonBrushDark(button.darker(v > 128 ? 200 : 50));
    const QBrush buttonBrushDark150(button.darker(v > 128 ? 150 : 75));
    const QBrush buttonBrushLight150(button.lighter(v > 128 ? 150 : 75));
    const QBrush buttonBrushLight(button.lighter(v > 128 ? 200 : 50));

    palette->setBrush(QPalette::Disabled, QPalette::WindowText, buttonBrushDark);
    palette->setBrush(QPalette::Disabled, QPalette::ButtonText, buttonBrushDark);
    palette->setBrush(QPalette::Disabled, QPalette::Button, buttonBrush);
    palette->setBrush(QPalette::Disabled, QPalette::Text, buttonBrushDark);
    palette->setBrush(QPalette::Disabled, QPalette::BrightText, whiteBrush);
    palette->setBrush(QPalette::Disabled, QPalette::Base, buttonBrush);
    palette->setBrush(QPalette::Disabled, QPalette::Window, buttonBrush);
    palette->setBrush(QPalette::Disabled, QPalette::Highlight, buttonBrushDark150);
    palette->setBrush(QPalette::Disabled, QPalette::HighlightedText, buttonBrushLight150);

    palette->setBrush(QPalette::Light, buttonBrushLight);
    palette->setBrush(QPalette::Midlight, buttonBrushLight150);
    palette->setBrush(QPalette::Mid, buttonBrushDark150);
    palette->setBrush(QPalette::Dark, buttonBrushDark);
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
        if (const auto *basePalette = m_portalTheme->palette(type))
            return basePalette;
        return &m_palettes[SystemPalette];
    }

    const QFont *font(Font type = SystemFont) const override
    {
        if (type >= 0 && type < NFonts && m_fontSet[static_cast<size_t>(type)])
            return &m_fonts[static_cast<size_t>(type)];
        if (const auto *baseFont = m_portalTheme->font(type))
            return baseFont;
        if (type == FixedFont || type == EditorFont)
            return &m_defaultFixedFont;
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
            if (m_iconThemeName.isEmpty())
                return m_portalTheme->themeHint(hint);
            return m_iconThemeName;
        case SystemIconFallbackThemeName:
            return m_portalTheme->themeHint(hint);
        case KeyboardScheme:
            return m_portalTheme->themeHint(hint);
        case DialogButtonBoxLayout:
            return m_portalTheme->themeHint(hint);
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
        case UiEffects:
            return m_portalTheme->themeHint(hint);
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

    bool applyKdePaletteOverrides(QPalette *palette) const
    {
        bool changed = false;
        const QString colorsButton = QString::fromLatin1(kdeColorsButton);
        if (const auto button = readKdeColor(m_kdeGlobalsPortal, colorsButton, QStringLiteral("BackgroundNormal"))) {
            palette->setColor(QPalette::Button, *button);
            changed = true;
        }

        changed = setColorIfPresent(palette, QPalette::Window, m_kdeGlobalsPortal, QString::fromLatin1(kdeColorsWindow), QStringLiteral("BackgroundNormal")) || changed;
        changed = setColorIfPresent(palette, QPalette::Text, m_kdeGlobalsPortal, QString::fromLatin1(kdeColorsView), QStringLiteral("ForegroundNormal")) || changed;
        changed = setColorIfPresent(palette, QPalette::WindowText, m_kdeGlobalsPortal, QString::fromLatin1(kdeColorsWindow), QStringLiteral("ForegroundNormal")) || changed;
        changed = setColorIfPresent(palette, QPalette::Base, m_kdeGlobalsPortal, QString::fromLatin1(kdeColorsView), QStringLiteral("BackgroundNormal")) || changed;
        changed = setColorIfPresent(palette, QPalette::Highlight, m_kdeGlobalsPortal, QString::fromLatin1(kdeColorsSelection), QStringLiteral("BackgroundNormal")) || changed;
        changed = setColorIfPresent(palette, QPalette::HighlightedText, m_kdeGlobalsPortal, QString::fromLatin1(kdeColorsSelection), QStringLiteral("ForegroundNormal")) || changed;
        changed = setColorIfPresent(palette, QPalette::AlternateBase, m_kdeGlobalsPortal, QString::fromLatin1(kdeColorsView), QStringLiteral("BackgroundAlternate")) || changed;
        changed = setColorIfPresent(palette, QPalette::ButtonText, m_kdeGlobalsPortal, colorsButton, QStringLiteral("ForegroundNormal")) || changed;
        changed = setColorIfPresent(palette, QPalette::Link, m_kdeGlobalsPortal, QString::fromLatin1(kdeColorsView), QStringLiteral("ForegroundLink")) || changed;
        changed = setColorIfPresent(palette, QPalette::LinkVisited, m_kdeGlobalsPortal, QString::fromLatin1(kdeColorsView), QStringLiteral("ForegroundVisited")) || changed;
        changed = setColorIfPresent(palette, QPalette::ToolTipBase, m_kdeGlobalsPortal, QString::fromLatin1(kdeColorsTooltip), QStringLiteral("BackgroundNormal")) || changed;
        changed = setColorIfPresent(palette, QPalette::ToolTipText, m_kdeGlobalsPortal, QString::fromLatin1(kdeColorsTooltip), QStringLiteral("ForegroundNormal")) || changed;

        if (changed)
            applyKdeDisabledColors(palette);

        return changed;
    }

    void reloadPalette()
    {
        const QPalette *baseSystemPalette = m_portalTheme->palette(SystemPalette);

        for (size_t i = 0; i < m_palettes.size(); ++i) {
            const auto type = static_cast<Palette>(i);
            if (const QPalette *basePalette = m_portalTheme->palette(type))
                m_palettes[i] = *basePalette;
            else if (baseSystemPalette)
                m_palettes[i] = *baseSystemPalette;
            else
                m_palettes[i] = QPalette();

            applyKdePaletteOverrides(&m_palettes[i]);
        }
    }

    void reloadFonts()
    {
        m_fontSet.fill(false);

        if (auto font = readKdeFont(m_kdeGlobalsPortal, QStringLiteral("font")))
            setFont(SystemFont, *font);

        if (auto font = readKdeFont(m_kdeGlobalsPortal, QStringLiteral("fixed")))
            setFont(FixedFont, *font);

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
        m_styleNames = m_portalTheme->themeHint(StyleNames).toStringList();

        const QString widgetStyle = readKdeString(m_kdeGlobalsPortal, QString::fromLatin1(kdeKde), QStringLiteral("widgetStyle"),
            readKdeString(m_kdeGlobalsPortal, QString::fromLatin1(kdeGeneral), QStringLiteral("widgetStyle")));
        if (!widgetStyle.isEmpty())
            m_styleNames.prepend(widgetStyle);
        if (m_styleNames.isEmpty())
            m_styleNames << QStringLiteral("Fusion");
        m_styleNames.removeDuplicates();

        m_iconThemeName = readKdeString(m_kdeGlobalsPortal, QString::fromLatin1(kdeIcons), QStringLiteral("Theme"),
            m_portalTheme->themeHint(SystemIconThemeName).toString());
        m_singleClick = readKdeBool(m_kdeGlobalsPortal, QString::fromLatin1(kdeKde), QStringLiteral("SingleClick"),
            baseBoolHint(ItemViewActivateItemOnSingleClick, m_singleClick));
        m_showIconsOnPushButtons = readKdeBool(m_kdeGlobalsPortal, QString::fromLatin1(kdeKde), QStringLiteral("ShowIconsOnPushButtons"),
            baseBoolHint(DialogButtonBoxButtonsHaveIcons, m_showIconsOnPushButtons));

        m_toolButtonStyle = static_cast<Qt::ToolButtonStyle>(baseIntHint(ToolButtonStyle, int(m_toolButtonStyle)));
        const QString toolButtonStyle = readKdeString(m_kdeGlobalsPortal, QString::fromLatin1(kdeToolbarStyle), QStringLiteral("ToolButtonStyle"));
        if (toolButtonStyle == QLatin1String("TextOnly"))
            m_toolButtonStyle = Qt::ToolButtonTextOnly;
        else if (toolButtonStyle == QLatin1String("TextUnderIcon"))
            m_toolButtonStyle = Qt::ToolButtonTextUnderIcon;
        else if (!toolButtonStyle.isEmpty())
            m_toolButtonStyle = Qt::ToolButtonTextBesideIcon;

        m_toolBarIconSize = readKdeInt(m_kdeGlobalsPortal, QString::fromLatin1(kdeToolbarIcons), QStringLiteral("Size"),
            baseIntHint(ToolBarIconSize, m_toolBarIconSize));
        m_wheelScrollLines = readKdeInt(m_kdeGlobalsPortal, QString::fromLatin1(kdeKde), QStringLiteral("WheelScrollLines"),
            baseIntHint(WheelScrollLines, m_wheelScrollLines));
        m_doubleClickInterval = readKdeInt(m_kdeGlobalsPortal, QString::fromLatin1(kdeKde), QStringLiteral("DoubleClickInterval"),
            baseIntHint(MouseDoubleClickInterval, m_doubleClickInterval));
        m_startDragDistance = readKdeInt(m_kdeGlobalsPortal, QString::fromLatin1(kdeKde), QStringLiteral("StartDragDist"),
            baseIntHint(StartDragDistance, m_startDragDistance));
        m_startDragTime = readKdeInt(m_kdeGlobalsPortal, QString::fromLatin1(kdeKde), QStringLiteral("StartDragTime"),
            baseIntHint(StartDragTime, m_startDragTime));

        const int cursorBlinkRate = readKdeInt(m_kdeGlobalsPortal, QString::fromLatin1(kdeKde), QStringLiteral("CursorBlinkRate"),
            baseIntHint(CursorFlashTime, m_cursorFlashTime));
        m_cursorFlashTime = cursorBlinkRate > 0 ? qBound(200, cursorBlinkRate, 2000) : 0;
    }

    int baseIntHint(ThemeHint hint, int fallback) const
    {
        bool ok = false;
        const int value = m_portalTheme->themeHint(hint).toInt(&ok);
        return ok ? value : fallback;
    }

    bool baseBoolHint(ThemeHint hint, bool fallback) const
    {
        const QVariant value = m_portalTheme->themeHint(hint);
        return value.isValid() ? value.toBool() : fallback;
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
    QFont m_defaultFixedFont = QFont(QStringLiteral("monospace"), 10);
};

class KdePortalPlugin : public QPlatformThemePlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QPlatformThemeFactoryInterface_iid FILE "plugin_metadata.json")

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

#include "kde_portal.moc"
