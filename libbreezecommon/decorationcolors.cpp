/*
 * SPDX-FileCopyrightText: 2022-2024 Paul A McAuley <kde@paulmcauley.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include "decorationcolors.h"
#include "colortools.h"
#include <KColorUtils>
#include <KStatefulBrush>

namespace Breeze
{

QPalette DecorationColors::s_cachedKdeGlobalPalette;
std::unique_ptr<DecorationPaletteGroup> DecorationColors::s_cachedDecorationPaletteGroupActive;
std::unique_ptr<DecorationPaletteGroup> DecorationColors::s_cachedDecorationPaletteGroupInactive;
std::map<DecorationButtonType, DecorationButtonPalette> DecorationColors::s_cachedButtonPalettes;
QByteArray DecorationColors::s_settingsUpdateUuid = "";
bool DecorationColors::s_cachedColorsGenerated = false;

DecorationColors::DecorationColors(const bool useCachedPalette, const bool forAppStyle)
    : m_forAppStyle(forAppStyle)
{
    if (m_forAppStyle) {
        m_useCachedPalette = false; // different apps can't access the same memory TODO:implement an appStyle cache using shared memory
    } else {
        m_useCachedPalette = useCachedPalette;
    }

    m_basePalette = m_useCachedPalette ? &s_cachedKdeGlobalPalette : &m_nonCachedClientPalette;
    m_decorationPaletteGroupActive = m_useCachedPalette ? &s_cachedDecorationPaletteGroupActive : &m_nonCachedDecorationPaletteGroupActive;
    m_decorationPaletteGroupInactive = m_useCachedPalette ? &s_cachedDecorationPaletteGroupInactive : &m_nonCachedDecorationPaletteGroupInactive;
    m_settingsUpdateUuid = m_useCachedPalette ? &s_settingsUpdateUuid : nullptr;
    m_colorsGenerated = m_useCachedPalette ? &s_cachedColorsGenerated : &m_nonCachedColorsGenerated;
    m_buttonPalettes = m_useCachedPalette ? &s_cachedButtonPalettes : &m_nonCachedButtonPalettes;

    if (!*m_decorationPaletteGroupActive) {
        *m_decorationPaletteGroupActive = std::make_unique<DecorationPaletteGroup>();
    }
    if (!*m_decorationPaletteGroupInactive) {
        *m_decorationPaletteGroupInactive = std::make_unique<DecorationPaletteGroup>();
    }

    if (!m_buttonPalettes->size() && !m_forAppStyle) { // appStyle should generate buttons separately
        const QList<DecorationButtonType> &coloredButtonTypes = m_forAppStyle ? coloredAppStyleDecorationButtonTypes : coloredWindowDecorationButtonTypes;

        // initialise m_buttonPalettes map so that only generate() needs called later -- ensures the values in the map are at the same memory location
        for (int i = 0; i < coloredButtonTypes.count(); i++) {
            DecorationButtonPalette buttonPalette(coloredButtonTypes[i]);
            m_buttonPalettes->insert({coloredButtonTypes[i], buttonPalette});
        }
    }
}

DecorationButtonPalette *DecorationColors::buttonPalette(DecorationButtonType type) const
{
    auto it = m_buttonPalettes->find(type);
    if (it != m_buttonPalettes->end()) {
        return &(it->second);
    } else {
        return nullptr;
    }
}

void DecorationColors::generateDecorationColors(const QPalette &palette,
                                                const QSharedPointer<InternalSettings> decorationSettings,
                                                QColor titleBarTextActive,
                                                QColor titleBarBaseActive,
                                                QColor titleBarTextInactive,
                                                QColor titleBarBaseInactive,
                                                QByteArray settingsUpdateUuid,
                                                const bool generateOneGroupOnly,
                                                const bool oneGroupActiveState)
{
    *m_basePalette = palette;
    if (m_useCachedPalette && !settingsUpdateUuid.isEmpty()) { // m_settingsUpdateUuid must only be accessed/modified when m_useCachedPalette is true
        *static_cast<QByteArray *>(m_settingsUpdateUuid) = settingsUpdateUuid;
    }

    if (!(generateOneGroupOnly && !oneGroupActiveState)) { // active
        generateDecorationPaletteGroup(palette, decorationSettings, true, titleBarTextActive, titleBarBaseActive, titleBarTextInactive, titleBarBaseInactive);
    }

    if (!(generateOneGroupOnly && oneGroupActiveState)) { // inactive
        generateDecorationPaletteGroup(palette, decorationSettings, false, titleBarTextActive, titleBarBaseActive, titleBarTextInactive, titleBarBaseInactive);
    }

    *m_colorsGenerated = true;
}

void DecorationColors::generateDecorationAndButtonColors(const QPalette &palette,
                                                         const QSharedPointer<InternalSettings> decorationSettings,
                                                         QColor titleBarTextActive,
                                                         QColor titleBarBaseActive,
                                                         QColor titleBarTextInactive,
                                                         QColor titleBarBaseInactive,
                                                         QByteArray settingsUpdateUuid,
                                                         const bool generateOneGroupOnly,
                                                         const bool oneGroupActiveState)
{
    generateDecorationColors(palette,
                             decorationSettings,
                             titleBarTextActive,
                             titleBarBaseActive,
                             titleBarTextInactive,
                             titleBarBaseInactive,
                             settingsUpdateUuid);

    for (auto i = m_buttonPalettes->begin(); i != m_buttonPalettes->end(); i++) {
        i->second.generate(decorationSettings, this->active(), this->inactive(), generateOneGroupOnly, oneGroupActiveState);
    }
}

void DecorationColors::generateDecorationPaletteGroup(const QPalette &palette,
                                                      const QSharedPointer<InternalSettings> decorationSettings,
                                                      const bool active,
                                                      QColor &titleBarTextActive,
                                                      QColor &titleBarBaseActive,
                                                      QColor &titleBarTextInactive,
                                                      QColor &titleBarBaseInactive)
{
    std::unique_ptr<DecorationPaletteGroup> *decorationPaletteGroup = active ? m_decorationPaletteGroupActive : m_decorationPaletteGroupInactive;

    (*decorationPaletteGroup)->titleBarBase = active ? titleBarBaseActive : titleBarBaseInactive;
    bool setTitleBarBaseOpacity = false;
    if (!decorationSettings->opaqueTitleBar()) {
        if ((*decorationPaletteGroup)->titleBarBase.alpha() == 255) {
            setTitleBarBaseOpacity = true;
        } else {
            bool override = active ? decorationSettings->overrideActiveTitleBarOpacity() : decorationSettings->overrideInactiveTitleBarOpacity();
            if (override) {
                setTitleBarBaseOpacity = true;
            }
        }
    }
    if (setTitleBarBaseOpacity) {
        (*decorationPaletteGroup)
            ->titleBarBase.setAlphaF(qreal(active ? decorationSettings->activeTitleBarOpacity() : decorationSettings->inactiveTitleBarOpacity()) / 100);
    }

    (*decorationPaletteGroup)->titleBarText = active ? titleBarTextActive : titleBarTextInactive;

    KStatefulBrush buttonFocusStatefulBrush;
    KStatefulBrush buttonHoverStatefulBrush;

    buttonFocusStatefulBrush = KStatefulBrush(KColorScheme::Button, KColorScheme::NegativeText);
    (*decorationPaletteGroup)->negative = buttonFocusStatefulBrush.brush(QPalette::ColorGroup::Active).color();
    // this was too pale
    // buttonHoverStatefulBrush = KStatefulBrush( KColorScheme::Button, KColorScheme::NegativeBackground );
    // colors.negativeBackground = buttonHoverStatefulBrush.brush( QPalette::ColorGroup::Active ).color();
    (*decorationPaletteGroup)->negativeLessSaturated = ColorTools::getDifferentiatedLessSaturatedColor((*decorationPaletteGroup)->negative);
    (*decorationPaletteGroup)->negativeSaturated = ColorTools::getDifferentiatedSaturatedColor((*decorationPaletteGroup)->negative);

    buttonFocusStatefulBrush = KStatefulBrush(KColorScheme::Button, KColorScheme::NeutralText);
    (*decorationPaletteGroup)->neutral = buttonFocusStatefulBrush.brush(QPalette::ColorGroup::Active).color();
    // this was too pale
    // buttonHoverStatefulBrush = KStatefulBrush( KColorScheme::Button, KColorScheme::NeutralBackground );
    // colors.neutralLessSaturated = buttonHoverStatefulBrush.brush( QPalette::ColorGroup::Active ).color();
    (*decorationPaletteGroup)->neutralLessSaturated = ColorTools::getDifferentiatedLessSaturatedColor((*decorationPaletteGroup)->neutral);
    (*decorationPaletteGroup)->neutralSaturated = ColorTools::getDifferentiatedSaturatedColor((*decorationPaletteGroup)->neutral);

    buttonFocusStatefulBrush = KStatefulBrush(KColorScheme::Button, KColorScheme::PositiveText);
    (*decorationPaletteGroup)->positive = buttonFocusStatefulBrush.brush(QPalette::ColorGroup::Active).color();
    // this was too pale
    // buttonHoverStatefulBrush = KStatefulBrush( KColorScheme::Button, KColorScheme::PositiveBackground );
    // colors.positiveLessSaturated = buttonHoverStatefulBrush.brush( QPalette::ColorGroup::Active ).color();
    (*decorationPaletteGroup)->positiveLessSaturated = ColorTools::getDifferentiatedLessSaturatedColor((*decorationPaletteGroup)->positive);
    (*decorationPaletteGroup)->positiveSaturated = ColorTools::getDifferentiatedSaturatedColor((*decorationPaletteGroup)->positive);

    buttonFocusStatefulBrush = KStatefulBrush(KColorScheme::Button, KColorScheme::FocusColor);
    (*decorationPaletteGroup)->buttonFocus = buttonFocusStatefulBrush.brush(QPalette::ColorGroup::Active).color();
    buttonHoverStatefulBrush = KStatefulBrush(KColorScheme::Button, KColorScheme::HoverColor);
    (*decorationPaletteGroup)->buttonHover = buttonHoverStatefulBrush.brush(QPalette::ColorGroup::Active).color();

    // this is required as the KDE accent colours feature sets these the same
    if ((*decorationPaletteGroup)->buttonFocus == (*decorationPaletteGroup)->buttonHover)
        (*decorationPaletteGroup)->buttonHover = ColorTools::getDifferentiatedLessSaturatedColor((*decorationPaletteGroup)->buttonFocus);

    (*decorationPaletteGroup)->fullySaturatedNegative = ColorTools::getDifferentiatedSaturatedColor((*decorationPaletteGroup)->negative, true);

    (*decorationPaletteGroup)->highlight = palette.color(QPalette::ColorGroup::Active, QPalette::ColorRole::Highlight);
    (*decorationPaletteGroup)->highlightLessSaturated = ColorTools::getLessSaturatedColorForWindowHighlight((*decorationPaletteGroup)->highlight, true);

    // set shadow
    (*decorationPaletteGroup)->shadow = decorationSettings->shadowColor();
    qreal shadowStrengthScale = active ? 1.0 : 0.5;
    (*decorationPaletteGroup)->shadow.setAlphaF(decorationSettings->shadowStrength() / 255.0 * shadowStrengthScale);

    // set windowOutline
    switch (decorationSettings->thinWindowOutlineStyle(active)) {
    case InternalSettings::EnumThinWindowOutlineStyle::WindowOutlineContrast:
        (*decorationPaletteGroup)->windowOutline =
            ColorTools::alphaMix((*decorationPaletteGroup)->titleBarText, decorationSettings->windowOutlineContrastOpacity(active) / 100.0f);
        break;
    case InternalSettings::EnumThinWindowOutlineStyle::WindowOutlineAccentColor:
        (*decorationPaletteGroup)->windowOutline = accentedWindowOutlineColor((*decorationPaletteGroup).get(), decorationSettings, active);
        break;
    case InternalSettings::EnumThinWindowOutlineStyle::WindowOutlineAccentWithContrast:
        (*decorationPaletteGroup)->windowOutline = fontMixedAccentWindowOutlineColor((*decorationPaletteGroup).get(), decorationSettings, active);
        break;
    case InternalSettings::EnumThinWindowOutlineStyle::WindowOutlineCustomColor:
        (*decorationPaletteGroup)->windowOutline =
            accentedWindowOutlineColor((*decorationPaletteGroup).get(), decorationSettings, active, decorationSettings->thinWindowOutlineCustomColor(active));
        break;
    case InternalSettings::EnumThinWindowOutlineStyle::WindowOutlineCustomWithContrast:
        (*decorationPaletteGroup)->windowOutline = fontMixedAccentWindowOutlineColor((*decorationPaletteGroup).get(),
                                                                                     decorationSettings,
                                                                                     active,
                                                                                     decorationSettings->thinWindowOutlineCustomColor(active));
        break;
    case InternalSettings::EnumThinWindowOutlineStyle::WindowOutlineShadowColor:
        (*decorationPaletteGroup)->windowOutline =
            ColorTools::alphaMix((*decorationPaletteGroup)->shadow, decorationSettings->windowOutlineShadowColorOpacity() / 100.0f);
        break;
    }
}

QColor DecorationColors::accentedWindowOutlineColor(DecorationPaletteGroup *decorationPaletteGroup,
                                                    const QSharedPointer<InternalSettings> decorationSettings,
                                                    bool active,
                                                    QColor customColor) const
{
    if (customColor.isValid()) {
        return ColorTools::alphaMix(customColor, decorationSettings->windowOutlineCustomColorOpacity(active) / 100.0f);

    } else {
        return ColorTools::alphaMix(decorationPaletteGroup->buttonFocus, decorationSettings->windowOutlineAccentColorOpacity(active) / 100.0f);
    }
}

QColor DecorationColors::fontMixedAccentWindowOutlineColor(DecorationPaletteGroup *decorationPaletteGroup,
                                                           const QSharedPointer<InternalSettings> decorationSettings,
                                                           bool active,
                                                           QColor customColor) const
{
    if (customColor.isValid()) {
        return ColorTools::alphaMix(KColorUtils::mix(decorationPaletteGroup->titleBarText, customColor, 0.75) // titlebar font mixed with custom
                                    ,
                                    decorationSettings->windowOutlineCustomWithContrastOpacity(active) / 100.0f);
    } else { // not a custom color
        return ColorTools::alphaMix(KColorUtils::mix(decorationPaletteGroup->titleBarText,
                                                     decorationPaletteGroup->buttonFocus,
                                                     0.75) // foreground active font mixed with accent
                                    ,
                                    decorationSettings->windowOutlineAccentWithContrastOpacity(active) / 100.0f);
    }
}

void DecorationColors::readSystemTitleBarColors(KSharedConfig::Ptr kdeGlobalConfig,
                                                QColor &systemBaseActive,
                                                QColor &systemBaseInactive,
                                                QColor &systemTextActive,
                                                QColor &systemTextInactive,
                                                QString colorSchemePath)
{
    KSharedConfig::Ptr nonGlobalConfig;
    KSharedConfig::Ptr *config = &kdeGlobalConfig;

    bool isGlobalColorScheme = (colorSchemePath.isEmpty() || colorSchemePath == QStringLiteral("kdeglobals"));
    if (!isGlobalColorScheme) {
        nonGlobalConfig = KSharedConfig::openConfig(colorSchemePath, KConfig::SimpleConfig);
        config = &nonGlobalConfig;
    }

    bool colorSchemeHasHeaderColor = KColorScheme::isColorSetSupported(*config, KColorScheme::Header);

    // get the alpha values from the system colour scheme
    if (colorSchemeHasHeaderColor) {
        KColorScheme activeHeader = KColorScheme(QPalette::Active, KColorScheme::Header, *config);
        systemBaseActive = activeHeader.background().color();
        systemTextActive = activeHeader.foreground().color();
        KColorScheme inactiveHeader = KColorScheme(QPalette::Inactive, KColorScheme::Header, *config);
        systemBaseInactive = inactiveHeader.background().color();
        systemTextInactive = inactiveHeader.foreground().color();
    } else {
        KConfigGroup wmConfig(*config, QStringLiteral("WM"));
        if (wmConfig.exists()) {
            systemBaseActive = wmConfig.readEntry("activeBackground", QColorConstants::Black);
            systemBaseInactive = wmConfig.readEntry("inactiveBackground", QColorConstants::Black);
            systemTextActive = wmConfig.readEntry("activeForeground", QColorConstants::Black);
            systemTextInactive = wmConfig.readEntry("inactiveForeground", QColorConstants::Black);
        }
    }
}
}
