// SPDX-License-Identifier: LGPL-2.0-or-later
//
// Reduced KColorScheme-style palette builder adapted from
// kcolorscheme/src/kcolorscheme.cpp for xdg-desktop-portal data.

#include "kde_palette.h"

#include "../kguiaddons/kde_color_utils.h"

namespace KdePortal {
namespace {

struct KdeColorSet {
    QColor normalBackground;
    QColor alternateBackground;
    QColor normalText;
    QColor inactiveText;
    QColor activeText;
    QColor linkText;
    QColor visitedText;
    QColor negativeText;
    QColor neutralText;
    QColor positiveText;
};

const KdeColorSet defaultViewColors {
    QColor(255, 255, 255),
    QColor(247, 247, 247),
    QColor(35, 38, 41),
    QColor(112, 125, 138),
    QColor(61, 174, 233),
    QColor(41, 128, 185),
    QColor(155, 89, 182),
    QColor(218, 68, 83),
    QColor(246, 116, 0),
    QColor(39, 174, 96),
};

const KdeColorSet defaultWindowColors {
    QColor(239, 240, 241),
    QColor(227, 229, 231),
    QColor(35, 38, 41),
    QColor(112, 125, 138),
    QColor(61, 174, 233),
    QColor(41, 128, 185),
    QColor(155, 89, 182),
    QColor(218, 68, 83),
    QColor(246, 116, 0),
    QColor(39, 174, 96),
};

const KdeColorSet defaultButtonColors {
    QColor(252, 252, 252),
    QColor(163, 212, 250),
    QColor(35, 38, 41),
    QColor(112, 125, 138),
    QColor(61, 174, 233),
    QColor(41, 128, 185),
    QColor(155, 89, 182),
    QColor(218, 68, 83),
    QColor(246, 116, 0),
    QColor(39, 174, 96),
};

const KdeColorSet defaultSelectionColors {
    QColor(61, 174, 233),
    QColor(163, 212, 250),
    QColor(255, 255, 255),
    QColor(112, 125, 138),
    QColor(255, 255, 255),
    QColor(253, 188, 75),
    QColor(155, 89, 182),
    QColor(176, 55, 69),
    QColor(198, 92, 0),
    QColor(23, 104, 57),
};

const KdeColorSet defaultTooltipColors {
    QColor(247, 247, 247),
    QColor(239, 240, 241),
    QColor(35, 38, 41),
    QColor(112, 125, 138),
    QColor(61, 174, 233),
    QColor(41, 128, 185),
    QColor(155, 89, 182),
    QColor(218, 68, 83),
    QColor(246, 116, 0),
    QColor(39, 174, 96),
};

KdeColorSet readKdeColorSet(const PortalSettingsMap &settings, const QString &location, const KdeColorSet &fallback)
{
    return {
        readKdeColor(settings, location, QStringLiteral("BackgroundNormal"), fallback.normalBackground),
        readKdeColor(settings, location, QStringLiteral("BackgroundAlternate"), fallback.alternateBackground),
        readKdeColor(settings, location, QStringLiteral("ForegroundNormal"), fallback.normalText),
        readKdeColor(settings, location, QStringLiteral("ForegroundInactive"), fallback.inactiveText),
        readKdeColor(settings, location, QStringLiteral("ForegroundActive"), fallback.activeText),
        readKdeColor(settings, location, QStringLiteral("ForegroundLink"), fallback.linkText),
        readKdeColor(settings, location, QStringLiteral("ForegroundVisited"), fallback.visitedText),
        readKdeColor(settings, location, QStringLiteral("ForegroundNegative"), fallback.negativeText),
        readKdeColor(settings, location, QStringLiteral("ForegroundNeutral"), fallback.neutralText),
        readKdeColor(settings, location, QStringLiteral("ForegroundPositive"), fallback.positiveText),
    };
}

enum ShadeRole {
    LightShade,
    MidlightShade,
    MidShade,
    DarkShade,
    ShadowShade,
};

QColor kdeShade(const QColor &color, ShadeRole role, qreal contrast)
{
    contrast = qBound<qreal>(-1.0, contrast, 1.0);
    const qreal y = ColorUtils::luma(color);
    const qreal yi = 1.0 - y;

    if (y < 0.006) {
        switch (role) {
        case LightShade:
            return ColorUtils::shade(color, 0.05 + 0.95 * contrast);
        case MidShade:
            return ColorUtils::shade(color, 0.01 + 0.20 * contrast);
        case DarkShade:
            return ColorUtils::shade(color, 0.02 + 0.40 * contrast);
        default:
            return ColorUtils::shade(color, 0.03 + 0.60 * contrast);
        }
    }

    if (y > 0.93) {
        switch (role) {
        case MidlightShade:
            return ColorUtils::shade(color, -0.02 - 0.20 * contrast);
        case DarkShade:
            return ColorUtils::shade(color, -0.06 - 0.60 * contrast);
        case ShadowShade:
            return ColorUtils::shade(color, -0.10 - 0.90 * contrast);
        default:
            return ColorUtils::shade(color, -0.04 - 0.40 * contrast);
        }
    }

    const qreal lightAmount = (0.05 + y * 0.55) * (0.25 + contrast * 0.75);
    const qreal darkAmount = (-y) * (0.55 + contrast * 0.35);
    switch (role) {
    case LightShade:
        return ColorUtils::shade(color, lightAmount);
    case MidlightShade:
        return ColorUtils::shade(color, (0.15 + 0.35 * yi) * lightAmount);
    case MidShade:
        return ColorUtils::shade(color, (0.35 + 0.15 * y) * darkAmount);
    case DarkShade:
        return ColorUtils::shade(color, darkAmount);
    case ShadowShade:
        return ColorUtils::darken(ColorUtils::shade(color, darkAmount), 0.5 + 0.3 * y);
    }

    return color;
}

class StateEffects
{
public:
    StateEffects(const PortalSettingsMap &settings, QPalette::ColorGroup state)
    {
        if (state == QPalette::Disabled) {
            const QString group = QString::fromLatin1(kdeColorEffectsDisabled);
            init(settings, group, true, 2, 0, 1, 0.10, 0.0, 0.65, QColor(56, 56, 56));
        } else if (state == QPalette::Inactive) {
            const QString group = QString::fromLatin1(kdeColorEffectsInactive);
            init(settings, group, false, 0, 1, 2, 0.0, -0.9, 0.25, QColor(112, 111, 110));
        }
    }

    QColor background(const QColor &color) const
    {
        QColor result = color;
        switch (m_intensityEffect) {
        case 1:
            result = ColorUtils::shade(result, m_intensityAmount);
            break;
        case 2:
            result = ColorUtils::darken(result, m_intensityAmount);
            break;
        case 3:
            result = ColorUtils::lighten(result, m_intensityAmount);
            break;
        default:
            break;
        }

        switch (m_colorEffect) {
        case 1:
            result = ColorUtils::darken(result, 0.0, 1.0 - m_colorAmount);
            break;
        case 2:
            result = ColorUtils::mix(result, m_color, m_colorAmount);
            break;
        case 3:
            result = ColorUtils::tint(result, m_color, m_colorAmount);
            break;
        default:
            break;
        }

        return result;
    }

    QColor foreground(const QColor &color, const QColor &backgroundColor) const
    {
        QColor result = color;
        switch (m_contrastEffect) {
        case 1:
            result = ColorUtils::mix(result, backgroundColor, m_contrastAmount);
            break;
        case 2:
            result = ColorUtils::tint(result, backgroundColor, m_contrastAmount);
            break;
        default:
            break;
        }

        return background(result);
    }

private:
    void init(const PortalSettingsMap &settings,
        const QString &group,
        bool enabledByDefault,
        int defaultIntensityEffect,
        int defaultColorEffect,
        int defaultContrastEffect,
        qreal defaultIntensityAmount,
        qreal defaultColorAmount,
        qreal defaultContrastAmount,
        const QColor &defaultColor)
    {
        if (!readKdeBool(settings, group, QStringLiteral("Enable"), enabledByDefault))
            return;

        m_intensityEffect = readKdeInt(settings, group, QStringLiteral("IntensityEffect"), defaultIntensityEffect);
        m_colorEffect = readKdeInt(settings, group, QStringLiteral("ColorEffect"), defaultColorEffect);
        m_contrastEffect = readKdeInt(settings, group, QStringLiteral("ContrastEffect"), defaultContrastEffect);
        m_intensityAmount = readKdeReal(settings, group, QStringLiteral("IntensityAmount"), defaultIntensityAmount);
        m_colorAmount = readKdeReal(settings, group, QStringLiteral("ColorAmount"), defaultColorAmount);
        m_contrastAmount = readKdeReal(settings, group, QStringLiteral("ContrastAmount"), defaultContrastAmount);
        m_color = readKdeColor(settings, group, QStringLiteral("Color"), defaultColor);
    }

    int m_intensityEffect = 0;
    int m_colorEffect = 0;
    int m_contrastEffect = 0;
    qreal m_intensityAmount = 0.0;
    qreal m_colorAmount = 0.0;
    qreal m_contrastAmount = 0.0;
    QColor m_color;
};

KdeColorSet applyStateEffects(const KdeColorSet &colors, const StateEffects &effects)
{
    const QColor background = effects.background(colors.normalBackground);
    return {
        background,
        effects.background(colors.alternateBackground),
        effects.foreground(colors.normalText, background),
        effects.foreground(colors.inactiveText, background),
        effects.foreground(colors.activeText, background),
        effects.foreground(colors.linkText, background),
        effects.foreground(colors.visitedText, background),
        effects.foreground(colors.negativeText, background),
        effects.foreground(colors.neutralText, background),
        effects.foreground(colors.positiveText, background),
    };
}

KdeColorSet colorsForState(const PortalSettingsMap &settings, const KdeColorSet &colors, QPalette::ColorGroup state)
{
    if (state == QPalette::Active)
        return colors;

    return applyStateEffects(colors, StateEffects(settings, state));
}

KdeColorSet selectionColorsForState(const PortalSettingsMap &settings,
    const KdeColorSet &window,
    const KdeColorSet &selection,
    QPalette::ColorGroup state)
{
    if (state == QPalette::Active)
        return selection;

    const QString inactiveEffects = QString::fromLatin1(kdeColorEffectsInactive);
    const bool inactiveSelectionEffect = readKdeBool(settings, inactiveEffects, QStringLiteral("ChangeSelectionColor"),
        readKdeBool(settings, inactiveEffects, QStringLiteral("Enable"), true));

    if (state == QPalette::Inactive && !inactiveSelectionEffect)
        return applyStateEffects(selection, StateEffects(settings, state));

    KdeColorSet selectedWindow = window;
    if (state == QPalette::Inactive) {
        selectedWindow.normalBackground = ColorUtils::tint(selectedWindow.normalBackground, selection.normalBackground, 0.4);
        selectedWindow.alternateBackground = ColorUtils::tint(selectedWindow.alternateBackground, selection.normalBackground, 0.4);
    }

    return applyStateEffects(selectedWindow, StateEffects(settings, state));
}

} // namespace

QPalette createKdeApplicationPalette(const PortalSettingsMap &settings)
{
    const KdeColorSet view = readKdeColorSet(settings, QString::fromLatin1(kdeColorsView), defaultViewColors);
    const KdeColorSet window = readKdeColorSet(settings, QString::fromLatin1(kdeColorsWindow), defaultWindowColors);
    const KdeColorSet button = readKdeColorSet(settings, QString::fromLatin1(kdeColorsButton), defaultButtonColors);
    const KdeColorSet selection = readKdeColorSet(settings, QString::fromLatin1(kdeColorsSelection), defaultSelectionColors);
    const KdeColorSet tooltip = readKdeColorSet(settings, QString::fromLatin1(kdeColorsTooltip), defaultTooltipColors);
    const qreal contrast = 0.1 * readKdeInt(settings, QString::fromLatin1(kdeKde), QStringLiteral("contrast"), 7);

    QPalette palette;
    const QPalette::ColorGroup states[] = { QPalette::Active, QPalette::Inactive, QPalette::Disabled };
    const KdeColorSet activeTooltip = colorsForState(settings, tooltip, QPalette::Active);

    for (const QPalette::ColorGroup state : states) {
        const KdeColorSet stateView = colorsForState(settings, view, state);
        const KdeColorSet stateWindow = colorsForState(settings, window, state);
        const KdeColorSet stateButton = colorsForState(settings, button, state);
        const KdeColorSet stateSelection = selectionColorsForState(settings, window, selection, state);

        palette.setBrush(state, QPalette::WindowText, stateWindow.normalText);
        palette.setBrush(state, QPalette::Window, stateWindow.normalBackground);
        palette.setBrush(state, QPalette::Base, stateView.normalBackground);
        palette.setBrush(state, QPalette::Text, stateView.normalText);
        palette.setBrush(state, QPalette::Button, stateButton.normalBackground);
        palette.setBrush(state, QPalette::ButtonText, stateButton.normalText);
        palette.setBrush(state, QPalette::Highlight, stateSelection.normalBackground);
        palette.setBrush(state, QPalette::HighlightedText, stateSelection.normalText);
        palette.setBrush(state, QPalette::ToolTipBase, activeTooltip.normalBackground);
        palette.setBrush(state, QPalette::ToolTipText, activeTooltip.normalText);
        palette.setBrush(state, QPalette::PlaceholderText, stateView.inactiveText);
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
        palette.setBrush(state, QPalette::Accent, stateSelection.normalBackground);
#endif
        palette.setBrush(state, QPalette::AlternateBase, stateView.alternateBackground);
        palette.setBrush(state, QPalette::Link, stateView.linkText);
        palette.setBrush(state, QPalette::LinkVisited, stateView.visitedText);
        palette.setColor(state, QPalette::Light, kdeShade(stateWindow.normalBackground, LightShade, contrast));
        palette.setColor(state, QPalette::Midlight, kdeShade(stateWindow.normalBackground, MidlightShade, contrast));
        palette.setColor(state, QPalette::Mid, kdeShade(stateWindow.normalBackground, MidShade, contrast));
        palette.setColor(state, QPalette::Dark, kdeShade(stateWindow.normalBackground, DarkShade, contrast));
        palette.setColor(state, QPalette::Shadow, kdeShade(stateWindow.normalBackground, ShadowShade, contrast));
    }

    return palette;
}

} // namespace KdePortal
