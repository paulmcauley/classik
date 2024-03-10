/*
 * SPDX-FileCopyrightText: 2023-2024 Paul A McAuley <kde@paulmcauley.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include "decorationbuttoncolors.h"
#include "colortools.h"
#include <KColorUtils>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace Breeze
{

using KDecoration2::DecorationButtonType;

DecorationButtonPalette::DecorationButtonPalette(KDecoration2::DecorationButtonType buttonType)
    : _buttonType(buttonType)
    , _active(std::make_shared<DecorationButtonPaletteGroup>())
    , _inactive(std::make_shared<DecorationButtonPaletteGroup>())

{
}

void DecorationButtonPalette::generate(InternalSettingsPtr decorationSettings,
                                       const DecorationPaletteGroup *decorationColorsActive,
                                       const DecorationPaletteGroup *decorationColorsInactive,
                                       const bool generateOneGroupOnly,
                                       const bool oneGroupActiveState)
{
    _decorationSettings = decorationSettings;
    _decorationColorsActive = decorationColorsActive;
    _decorationColorsInactive = decorationColorsInactive;

    if (!(generateOneGroupOnly && !oneGroupActiveState)) { // active
        decodeButtonOverrideColors(true);
        generateButtonBackgroundPalette(true);
        generateButtonForegroundPalette(true);
        generateButtonOutlinePalette(true);
    }

    if (!(generateOneGroupOnly && oneGroupActiveState)) { // inactive
        decodeButtonOverrideColors(false);
        generateButtonBackgroundPalette(false);
        generateButtonForegroundPalette(false);
        generateButtonOutlinePalette(false);
    }
}

void DecorationButtonPalette::decodeButtonOverrideColors(const bool active)
{
    QMap<OverridableButtonColorState, QColor> &buttonOverrideColors = active ? _buttonOverrideColorsActive : _buttonOverrideColorsInactive;
    bool &buttonOverrideColorsPresent = active ? _buttonOverrideColorsPresentActive : _buttonOverrideColorsPresentInactive;

    buttonOverrideColors.clear();
    buttonOverrideColorsPresent = false;

    bool overrideButtonTypeValid = false;
    for (int i = 0; i < InternalSettings::EnumButtonOverrideColorsActiveButtonType::COUNT; i++) {
        if (static_cast<int>(_buttonType) == i) {
            overrideButtonTypeValid = true;
            break;
        }
    }
    if (!overrideButtonTypeValid) {
        return;
    }

    QByteArray overrideColorsSetting = active ? _decorationSettings->buttonOverrideColorsActive(static_cast<int>(_buttonType)).toUtf8()
                                              : _decorationSettings->buttonOverrideColorsInactive(static_cast<int>(_buttonType)).toUtf8();
    if (overrideColorsSetting.isEmpty()) {
        return;
    }

    QJsonDocument document = QJsonDocument::fromJson(overrideColorsSetting);

    QJsonObject buttonStatesObject = document.object();
    bool overrideColorLoaded = false;

    for (auto i = buttonStatesObject.begin(); i < buttonStatesObject.end(); i++) {
        QJsonArray colorArray = i->toArray();
        QColor color;
        int colorOpacity;
        int overridableButtonColorStatesIndex;
        int overrideColorItemsIndex;
        switch (colorArray.count()) {
        case 0:
        default:
            break;
        case 1:
            overridableButtonColorStatesIndex = overridableButtonColorStatesJsonStrings.indexOf(i.key());
            if (overridableButtonColorStatesIndex < 0)
                break;

            overrideColorItemsIndex = overrideColorItems.indexOf(colorArray[0].toString());
            if (overrideColorItemsIndex == 0)
                break;

            color = overrideColorItemsIndexToColor(_decorationColorsActive, _decorationColorsInactive, overrideColorItemsIndex, active);
            if (!color.isValid())
                break;

            buttonOverrideColors.insert(static_cast<OverridableButtonColorState>(overridableButtonColorStatesIndex), color);
            overrideColorLoaded = true;
            break;
        case 2:
            overridableButtonColorStatesIndex = overridableButtonColorStatesJsonStrings.indexOf(i.key());
            if (overridableButtonColorStatesIndex < 0)
                break;

            overrideColorItemsIndex = overrideColorItems.indexOf(colorArray[0].toString());
            if (overrideColorItemsIndex == 0)
                break;

            color = overrideColorItemsIndexToColor(_decorationColorsActive, _decorationColorsInactive, overrideColorItemsIndex, active);
            if (!color.isValid())
                break;

            colorOpacity = colorArray[1].toInt(-1);
            if (colorOpacity >= 0 && colorOpacity <= 100) {
                color.setAlphaF(colorOpacity / 100.0f);
            } else {
                break;
            }

            buttonOverrideColors.insert(static_cast<OverridableButtonColorState>(overridableButtonColorStatesIndex), color);
            overrideColorLoaded = true;
            break;
        case 3:
            overridableButtonColorStatesIndex = overridableButtonColorStatesJsonStrings.indexOf(i.key());
            if (overridableButtonColorStatesIndex < 0)
                break;

            color.setRed(colorArray[0].toInt());
            color.setGreen(colorArray[1].toInt());
            color.setBlue(colorArray[2].toInt());
            if (!color.isValid())
                break;

            buttonOverrideColors.insert(static_cast<OverridableButtonColorState>(overridableButtonColorStatesIndex), color);
            overrideColorLoaded = true;
            break;
        case 4:
            overridableButtonColorStatesIndex = overridableButtonColorStatesJsonStrings.indexOf(i.key());
            if (overridableButtonColorStatesIndex < 0)
                break;

            color.setRed(colorArray[1].toInt());
            color.setGreen(colorArray[2].toInt());
            color.setBlue(colorArray[3].toInt());
            if (!color.isValid())
                break;

            colorOpacity = colorArray[0].toInt(-1);
            if (colorOpacity >= 0 && colorOpacity <= 100) {
                color.setAlphaF(colorOpacity / 100.0f);
            } else {
                break;
            }

            buttonOverrideColors.insert(static_cast<OverridableButtonColorState>(overridableButtonColorStatesIndex), color);
            overrideColorLoaded = true;
            break;
        }
    }

    buttonOverrideColorsPresent = overrideColorLoaded;
}

QColor DecorationButtonPalette::overrideColorItemsIndexToColor(const DecorationPaletteGroup *decorationColorsActive,
                                                               const DecorationPaletteGroup *decorationColorsInactive,
                                                               const int overrideColorItemsIndex,
                                                               const bool active)
{
    switch (overrideColorItemsIndex) {
    default:
        return QColor();
    case 0:
        return Qt::transparent;
    case 1:
        return active ? decorationColorsActive->titleBarText : decorationColorsInactive->titleBarText;
    case 2:
        return decorationColorsActive->titleBarText;
    case 3:
        return decorationColorsInactive->titleBarText;
    case 4:
        return active ? decorationColorsActive->titleBarBase : decorationColorsInactive->titleBarBase;
    case 5:
        return decorationColorsActive->titleBarBase;
    case 6:
        return decorationColorsInactive->titleBarBase;
    case 7:
        return decorationColorsActive->buttonFocus;
    case 8:
        return decorationColorsActive->buttonHover;
    case 9:
        return decorationColorsActive->highlight;
    case 10:
        return decorationColorsActive->highlightLessSaturated;
    case 11:
        return decorationColorsActive->negative;
    case 12:
        return decorationColorsActive->negativeLessSaturated;
    case 13:
        return decorationColorsActive->negativeSaturated;
    case 14:
        return decorationColorsActive->fullySaturatedNegative;
    case 15:
        return decorationColorsActive->neutral;
    case 16:
        return decorationColorsActive->neutralLessSaturated;
    case 17:
        return decorationColorsActive->neutralSaturated;
    case 18:
        return decorationColorsActive->positive;
    case 19:
        return decorationColorsActive->positiveLessSaturated;
    case 20:
        return decorationColorsActive->positiveSaturated;
    case 21:
        return Qt::white;
    case 22:
        return active ? decorationColorsActive->windowOutline : decorationColorsInactive->windowOutline;
    case 23:
        return decorationColorsActive->windowOutline;
    case 24:
        return decorationColorsInactive->windowOutline;
    case 25:
        return active ? decorationColorsActive->shadow : decorationColorsInactive->shadow;
    case 26:
        return decorationColorsActive->shadow;
    case 27:
        return decorationColorsInactive->shadow;
    }
}

void DecorationButtonPalette::generateBistateColors(ButtonComponent component,
                                                    const bool active,
                                                    QColor baseColor,
                                                    QColor &bistate1,
                                                    QColor &bistate2,
                                                    QColor accentHoverBase)
{
    const DecorationPaletteGroup *decorationColors = active ? _decorationColorsActive : _decorationColorsInactive;
    bool isClose = _buttonType == DecorationButtonType::Close;

    qreal baseOpacity;
    int varyColor;
    switch (component) {
    default:
    case ButtonComponent::Background:
        baseOpacity = _decorationSettings->buttonBackgroundOpacity(active) / 100.0f;
        varyColor = isClose ? _decorationSettings->varyColorCloseBackground(active) : _decorationSettings->varyColorBackground(active);
        break;
    case ButtonComponent::Icon:
        baseOpacity = _decorationSettings->buttonIconOpacity(active) / 100.0f;
        varyColor = isClose ? _decorationSettings->varyColorCloseIcon(active) : _decorationSettings->varyColorIcon(active);
        break;
    case ButtonComponent::Outline:
        baseOpacity = qMin(_decorationSettings->buttonBackgroundOpacity(active) / 100.0f * 1.5, 1.0);
        varyColor = isClose ? _decorationSettings->varyColorCloseOutline(active) : _decorationSettings->varyColorOutline(active);
        break;
    }

    baseColor.setAlphaF(baseOpacity);
    if (accentHoverBase.isValid()) {
        accentHoverBase.setAlphaF(baseOpacity);
    }

    switch (varyColor) {
    default:
    case static_cast<int>(InternalSettings::EnumVaryColorBackground::No):
        bistate1 = accentHoverBase.isValid() ? accentHoverBase : baseColor;
        bistate2 = baseColor;
        break;
    case static_cast<int>(InternalSettings::EnumVaryColorBackground::Opaque):
        bistate1 = accentHoverBase.isValid() ? accentHoverBase : ColorTools::alphaMix(baseColor, 0.8);
        bistate2 = ColorTools::alphaMix(baseColor, 1.2);
        break;
    case static_cast<int>(InternalSettings::EnumVaryColorBackground::MostOpaqueHover):
        bistate1 = accentHoverBase.isValid() ? accentHoverBase : ColorTools::alphaMix(baseColor, 1.2);
        bistate2 = ColorTools::alphaMix(baseColor, 0.8);
        break;
    case static_cast<int>(InternalSettings::EnumVaryColorBackground::Transparent):
        bistate1 = accentHoverBase.isValid() ? accentHoverBase : ColorTools::alphaMix(baseColor, 1.2);
        bistate2 = ColorTools::alphaMix(baseColor, 0.8);
        break;
    case static_cast<int>(InternalSettings::EnumVaryColorBackground::MostTransparentHover):
        bistate1 = accentHoverBase.isValid() ? accentHoverBase : ColorTools::alphaMix(baseColor, 0.8);
        bistate2 = ColorTools::alphaMix(baseColor, 1.2);
        break;
    case static_cast<int>(InternalSettings::EnumVaryColorBackground::Light):
        bistate1 = accentHoverBase.isValid() ? accentHoverBase : baseColor;
        bistate2 = accentHoverBase.isValid() ? accentHoverBase.lighter() : baseColor.lighter();
        break;
    case static_cast<int>(InternalSettings::EnumVaryColorBackground::LightestHover):
        bistate1 = accentHoverBase.isValid() ? accentHoverBase.lighter() : baseColor.lighter();
        bistate2 = accentHoverBase.isValid() ? accentHoverBase : baseColor;
        break;
    case static_cast<int>(InternalSettings::EnumVaryColorBackground::Dark):
        bistate1 = accentHoverBase.isValid() ? accentHoverBase : baseColor;
        bistate2 = accentHoverBase.isValid() ? accentHoverBase.darker() : baseColor.darker();
        break;
    case static_cast<int>(InternalSettings::EnumVaryColorBackground::DarkestHover):
        bistate1 = accentHoverBase.isValid() ? accentHoverBase.darker() : baseColor.darker();
        bistate2 = accentHoverBase.isValid() ? accentHoverBase : baseColor;
        break;
    case static_cast<int>(InternalSettings::EnumVaryColorBackground::MoreTitleBar):
        bistate1 = accentHoverBase.isValid() ? accentHoverBase : baseColor;
        bistate2 = KColorUtils::mix(decorationColors->titleBarBase, accentHoverBase.isValid() ? accentHoverBase : baseColor, 0.3);
        break;
    case static_cast<int>(InternalSettings::EnumVaryColorBackground::MostTitleBarHover):
        bistate1 = KColorUtils::mix(decorationColors->titleBarBase, accentHoverBase.isValid() ? accentHoverBase : baseColor, 0.3);
        bistate2 = accentHoverBase.isValid() ? accentHoverBase : baseColor;
        break;
    case static_cast<int>(InternalSettings::EnumVaryColorBackground::LessTitleBar):
        bistate1 = KColorUtils::mix(decorationColors->titleBarBase, accentHoverBase.isValid() ? accentHoverBase : baseColor, 0.3);
        bistate2 = accentHoverBase.isValid() ? accentHoverBase : baseColor;
        break;
    case static_cast<int>(InternalSettings::EnumVaryColorBackground::LeastTitleBarHover):
        bistate1 = accentHoverBase.isValid() ? accentHoverBase : baseColor;
        bistate2 = KColorUtils::mix(decorationColors->titleBarBase, accentHoverBase.isValid() ? accentHoverBase : baseColor, 0.3);
        break;
    }
}

void DecorationButtonPalette::generateTristateColors(ButtonComponent component,
                                                     const bool active,
                                                     QColor baseColor,
                                                     QColor &tristate1,
                                                     QColor &tristate2,
                                                     QColor &tristate3,
                                                     QColor accentHoverBase)
{
    const DecorationPaletteGroup *decorationColors = active ? _decorationColorsActive : _decorationColorsInactive;
    bool isClose = _buttonType == DecorationButtonType::Close;

    qreal baseOpacity;
    int varyColor;
    switch (component) {
    default:
    case ButtonComponent::Background:
        baseOpacity = _decorationSettings->buttonBackgroundOpacity(active) / 100.0f;
        varyColor = isClose ? _decorationSettings->varyColorCloseBackground(active) : _decorationSettings->varyColorBackground(active);
        break;
    case ButtonComponent::Icon:
        baseOpacity = _decorationSettings->buttonIconOpacity(active) / 100.0f;
        varyColor = isClose ? _decorationSettings->varyColorCloseIcon(active) : _decorationSettings->varyColorIcon(active);
        break;
    case ButtonComponent::Outline:
        baseOpacity = qMin(_decorationSettings->buttonBackgroundOpacity(active) / 100.0f * 1.5, 1.0);
        varyColor = isClose ? _decorationSettings->varyColorCloseOutline(active) : _decorationSettings->varyColorOutline(active);
        break;
    }

    baseColor.setAlphaF(baseOpacity);
    if (accentHoverBase.isValid()) {
        accentHoverBase.setAlphaF(baseOpacity);
    }

    switch (varyColor) {
    default:
    case static_cast<int>(InternalSettings::EnumVaryColorBackground::No):
        tristate1 = accentHoverBase.isValid() ? accentHoverBase : baseColor;
        tristate2 = accentHoverBase.isValid() ? accentHoverBase : baseColor;
        tristate3 = accentHoverBase.isValid() ? accentHoverBase : baseColor;
        break;
    case static_cast<int>(InternalSettings::EnumVaryColorBackground::Opaque):
        tristate1 = ColorTools::alphaMix(baseColor, 0.6);
        tristate2 = accentHoverBase.isValid() ? accentHoverBase : baseColor;
        tristate3 = ColorTools::alphaMix(baseColor, 1.8);
        break;
    case static_cast<int>(InternalSettings::EnumVaryColorBackground::MostOpaqueHover):
        tristate1 = baseColor;
        tristate2 = accentHoverBase.isValid() ? accentHoverBase : ColorTools::alphaMix(baseColor, 1.8);
        tristate3 = ColorTools::alphaMix(baseColor, 0.6);
        break;
    case static_cast<int>(InternalSettings::EnumVaryColorBackground::Transparent):
        tristate1 = ColorTools::alphaMix(baseColor, 1.8);
        tristate2 = accentHoverBase.isValid() ? accentHoverBase : baseColor;
        tristate3 = ColorTools::alphaMix(baseColor, 0.6);
        break;
    case static_cast<int>(InternalSettings::EnumVaryColorBackground::MostTransparentHover):
        tristate1 = baseColor;
        tristate2 = accentHoverBase.isValid() ? accentHoverBase : ColorTools::alphaMix(baseColor, 0.6);
        tristate3 = ColorTools::alphaMix(baseColor, 1.8);
        break;
    case static_cast<int>(InternalSettings::EnumVaryColorBackground::Light):
        tristate1 = accentHoverBase.isValid() ? accentHoverBase : baseColor;
        tristate2 = accentHoverBase.isValid() ? accentHoverBase.lighter(125) : baseColor.lighter(125);
        tristate3 = accentHoverBase.isValid() ? accentHoverBase.lighter() : baseColor.lighter();
        break;
    case static_cast<int>(InternalSettings::EnumVaryColorBackground::LightestHover):
        tristate1 = accentHoverBase.isValid() ? accentHoverBase : baseColor;
        tristate2 = accentHoverBase.isValid() ? accentHoverBase.lighter() : baseColor.lighter();
        tristate3 = accentHoverBase.isValid() ? accentHoverBase.lighter(125) : baseColor.lighter(125);
        break;
    case static_cast<int>(InternalSettings::EnumVaryColorBackground::Dark):
        tristate1 = accentHoverBase.isValid() ? accentHoverBase : baseColor;
        tristate2 = accentHoverBase.isValid() ? accentHoverBase.darker(100) : baseColor.darker(100);
        tristate3 = accentHoverBase.isValid() ? accentHoverBase.darker() : baseColor.darker();
        break;
    case static_cast<int>(InternalSettings::EnumVaryColorBackground::DarkestHover):
        tristate1 = accentHoverBase.isValid() ? accentHoverBase : baseColor;
        tristate2 = accentHoverBase.isValid() ? accentHoverBase.darker() : baseColor.darker();
        tristate3 = accentHoverBase.isValid() ? accentHoverBase.darker(100) : baseColor.darker(100);
        break;
    case static_cast<int>(InternalSettings::EnumVaryColorBackground::MoreTitleBar):
        tristate1 = accentHoverBase.isValid() ? accentHoverBase : baseColor;
        tristate2 = KColorUtils::mix(decorationColors->titleBarBase, accentHoverBase.isValid() ? accentHoverBase : baseColor, 0.6);
        tristate3 = KColorUtils::mix(decorationColors->titleBarBase, accentHoverBase.isValid() ? accentHoverBase : baseColor, 0.3);
        break;
    case static_cast<int>(InternalSettings::EnumVaryColorBackground::MostTitleBarHover):
        tristate1 = accentHoverBase.isValid() ? accentHoverBase : baseColor;
        tristate2 = KColorUtils::mix(decorationColors->titleBarBase, accentHoverBase.isValid() ? accentHoverBase : baseColor, 0.3);
        tristate3 = KColorUtils::mix(decorationColors->titleBarBase, accentHoverBase.isValid() ? accentHoverBase : baseColor, 0.6);
        break;
    case static_cast<int>(InternalSettings::EnumVaryColorBackground::LessTitleBar):
        tristate1 = KColorUtils::mix(decorationColors->titleBarBase, accentHoverBase.isValid() ? accentHoverBase : baseColor, 0.3);
        tristate2 = KColorUtils::mix(decorationColors->titleBarBase, accentHoverBase.isValid() ? accentHoverBase : baseColor, 0.6);
        tristate3 = accentHoverBase.isValid() ? accentHoverBase : baseColor;
        break;
    case static_cast<int>(InternalSettings::EnumVaryColorBackground::LeastTitleBarHover):
        tristate1 = KColorUtils::mix(decorationColors->titleBarBase, accentHoverBase.isValid() ? accentHoverBase : baseColor, 0.3);
        tristate2 = accentHoverBase.isValid() ? accentHoverBase : baseColor;
        tristate3 = KColorUtils::mix(decorationColors->titleBarBase, accentHoverBase.isValid() ? accentHoverBase : baseColor, 0.6);
        break;
    }
}

void DecorationButtonPalette::generateButtonBackgroundPalette(const bool active)
{
    DecorationButtonPaletteGroup *group = active ? this->_active.get() : this->_inactive.get();
    QColor &backgroundNormal = group->backgroundNormal;
    QColor &backgroundHover = group->backgroundHover;
    QColor &backgroundPress = group->backgroundPress;
    const DecorationPaletteGroup *decorationColors = active ? _decorationColorsActive : _decorationColorsInactive;
    const QColor &titleBarText = decorationColors->titleBarText;
    const QColor &titleBarBase = decorationColors->titleBarBase;

    const int buttonBackgroundColors = _decorationSettings->buttonBackgroundColors(active);
    const bool negativeCloseBackgroundHoverPress = _decorationSettings->negativeCloseBackgroundHoverPress(active);
    const bool adjustBackgroundColorOnPoorContrast = _decorationSettings->adjustBackgroundColorOnPoorContrast(active);

    backgroundNormal = QColor();
    backgroundHover = QColor();
    backgroundPress = QColor();

    const bool negativeCloseCategory( // whether the button background colour is in the general negative category as selected in the "Background & Outline
                                      // Colours" combobox
        buttonBackgroundColors == InternalSettings::EnumButtonBackgroundColors::TitleBarTextNegativeClose
        || buttonBackgroundColors == InternalSettings::EnumButtonBackgroundColors::AccentNegativeClose
        || buttonBackgroundColors == InternalSettings::EnumButtonBackgroundColors::AccentTrafficLights);

    bool defaultButton =
        true; // flag indicates the button has standard colours for the behaviour and selected colour (i.e. is not a close/max/min with special colours)
    const bool &drawBackgroundNormally = (_buttonType == DecorationButtonType::Close) ? _decorationSettings->showCloseBackgroundNormally(active)
                                                                                      : _decorationSettings->showBackgroundNormally(active);
    const bool &drawBackgroundOnHover = (_buttonType == DecorationButtonType::Close) ? _decorationSettings->showCloseBackgroundOnHover(active)
                                                                                     : _decorationSettings->showBackgroundOnHover(active);
    const bool &drawBackgroundOnPress = (_buttonType == DecorationButtonType::Close) ? _decorationSettings->showCloseBackgroundOnPress(active)
                                                                                     : _decorationSettings->showBackgroundOnPress(active);

    QColor defaultButtonColor;
    QColor accentHoverBase;
    switch (buttonBackgroundColors) {
    default:
    case InternalSettings::EnumButtonBackgroundColors::Accent:
    case InternalSettings::EnumButtonBackgroundColors::AccentNegativeClose:
    case InternalSettings::EnumButtonBackgroundColors::AccentTrafficLights:
        defaultButtonColor = decorationColors->buttonFocus;
        if (_decorationSettings->useHoverAccent(active)) {
            accentHoverBase = decorationColors->buttonHover;
        }
        break;
    case InternalSettings::EnumButtonBackgroundColors::TitleBarText:
    case InternalSettings::EnumButtonBackgroundColors::TitleBarTextNegativeClose:
        defaultButtonColor = decorationColors->titleBarText;
        break;
    }

    bool setDefaultBackgroundNormallyOnly = false;

    // set normal, hover and press colours
    if (_buttonType == DecorationButtonType::Close && negativeCloseCategory) {
        defaultButton = false;
        if (drawBackgroundNormally) {
            QColor tristate1, tristate2, tristate3;
            if (negativeCloseBackgroundHoverPress)
                setDefaultBackgroundNormallyOnly = true;
            generateTristateColors(ButtonComponent::Background, active, decorationColors->fullySaturatedNegative, tristate1, tristate2, tristate3);
            backgroundNormal = tristate1;
            if (drawBackgroundOnHover) {
                backgroundHover = tristate2;
            }
            if (drawBackgroundOnPress) {
                backgroundPress = tristate3;
            }
        } else {
            QColor bistate1, bistate2;
            generateBistateColors(ButtonComponent::Background, active, decorationColors->fullySaturatedNegative, bistate1, bistate2);
            if (drawBackgroundOnHover) {
                backgroundHover = bistate1;
            }
            if (drawBackgroundOnPress) {
                backgroundPress = bistate2;
            }
        }
    } else if (_buttonType == DecorationButtonType::Minimize && buttonBackgroundColors == InternalSettings::EnumButtonBackgroundColors::AccentTrafficLights) {
        defaultButton = false;
        if (drawBackgroundNormally) {
            QColor tristate1, tristate2, tristate3;
            if (negativeCloseBackgroundHoverPress)
                setDefaultBackgroundNormallyOnly = true;
            generateTristateColors(ButtonComponent::Background, active, decorationColors->neutral, tristate1, tristate2, tristate3);
            backgroundNormal = tristate1;
            if (drawBackgroundOnHover)
                backgroundHover = tristate2;
            if (drawBackgroundOnPress)
                backgroundPress = tristate3;
        } else {
            QColor bistate1, bistate2;
            generateBistateColors(ButtonComponent::Background, active, decorationColors->neutral, bistate1, bistate2);
            if (drawBackgroundOnHover)
                backgroundHover = bistate1;
            if (drawBackgroundOnPress)
                backgroundPress = bistate2;
        }
    } else if (_buttonType == DecorationButtonType::Maximize && buttonBackgroundColors == InternalSettings::EnumButtonBackgroundColors::AccentTrafficLights) {
        defaultButton = false;
        if (drawBackgroundNormally) {
            QColor tristate1, tristate2, tristate3;
            if (negativeCloseBackgroundHoverPress)
                setDefaultBackgroundNormallyOnly = true;
            generateTristateColors(ButtonComponent::Background, active, decorationColors->positiveSaturated, tristate1, tristate2, tristate3);
            backgroundNormal = tristate1;
            if (drawBackgroundOnHover)
                backgroundHover = tristate2;
            if (drawBackgroundOnPress)
                backgroundPress = tristate3;
        } else {
            QColor bistate1, bistate2;
            generateBistateColors(ButtonComponent::Background, active, decorationColors->positiveSaturated, bistate1, bistate2);
            if (drawBackgroundOnHover)
                backgroundHover = bistate1;
            if (drawBackgroundOnPress)
                backgroundPress = bistate2;
        }
    }

    if (defaultButton || setDefaultBackgroundNormallyOnly) {
        if (drawBackgroundNormally) {
            QColor tristate1, tristate2, tristate3;
            generateTristateColors(ButtonComponent::Background, active, defaultButtonColor, tristate1, tristate2, tristate3, accentHoverBase);
            backgroundNormal = tristate1;
            if (drawBackgroundOnHover && !setDefaultBackgroundNormallyOnly)
                backgroundHover = tristate2;
            if (drawBackgroundOnPress && !setDefaultBackgroundNormallyOnly)
                backgroundPress = tristate3;
        } else {
            QColor bistate1, bistate2;
            generateBistateColors(ButtonComponent::Background, active, defaultButtonColor, bistate1, bistate2, accentHoverBase);
            if (drawBackgroundOnHover && !setDefaultBackgroundNormallyOnly)
                backgroundHover = bistate1;
            if (drawBackgroundOnPress && !setDefaultBackgroundNormallyOnly)
                backgroundPress = bistate2;
        }
    }

    const bool buttonOverrideColorsPresent = active ? _buttonOverrideColorsPresentActive : _buttonOverrideColorsPresentInactive;
    if (buttonOverrideColorsPresent) {
        auto &buttonOverrideColors = active ? _buttonOverrideColorsActive : _buttonOverrideColorsInactive;

        if (buttonOverrideColors.value(OverridableButtonColorState::BackgroundNormal).isValid() && drawBackgroundNormally) {
            backgroundNormal = buttonOverrideColors.value(OverridableButtonColorState::BackgroundNormal);
        }
        if (buttonOverrideColors.value(OverridableButtonColorState::BackgroundHover).isValid() && drawBackgroundOnHover) {
            backgroundHover = buttonOverrideColors.value(OverridableButtonColorState::BackgroundHover);
        }
        if (buttonOverrideColors.value(OverridableButtonColorState::BackgroundPress).isValid() && drawBackgroundOnPress) {
            backgroundPress = buttonOverrideColors.value(OverridableButtonColorState::BackgroundPress);
        }
    }

    // low contrast correction between background and titlebar
    if (adjustBackgroundColorOnPoorContrast) {
        if (backgroundNormal.isValid() && backgroundNormal.alpha() != 0
            && KColorUtils::contrastRatio(backgroundNormal, titleBarBase) < _decorationSettings->poorBackgroundContrastThreshold(active)) {
            backgroundNormal = KColorUtils::mix(backgroundNormal, titleBarText, 0.3);
        }

        if (backgroundHover.isValid() && backgroundHover.alpha() != 0
            && KColorUtils::contrastRatio(backgroundHover, titleBarBase) < _decorationSettings->poorBackgroundContrastThreshold(active)) {
            backgroundHover = KColorUtils::mix(backgroundHover, titleBarText, 0.3);
        }
        if (backgroundPress.isValid() && backgroundPress.alpha() != 0
            && KColorUtils::contrastRatio(backgroundPress, titleBarBase) < _decorationSettings->poorBackgroundContrastThreshold(active)) {
            backgroundPress = KColorUtils::mix(backgroundPress, titleBarText, 0.3);
        }
    }
}

void DecorationButtonPalette::generateButtonForegroundPalette(const bool active)
{
    DecorationButtonPaletteGroup *group = active ? this->_active.get() : this->_inactive.get();
    QColor &foregroundNormal = group->foregroundNormal;
    QColor &foregroundHover = group->foregroundHover;
    QColor &foregroundPress = group->foregroundPress;
    const DecorationPaletteGroup *decorationColors = active ? _decorationColorsActive : _decorationColorsInactive;

    foregroundNormal = QColor();
    foregroundHover = QColor();
    foregroundPress = QColor();
    group->cutOutForegroundNormal = false;
    group->cutOutForegroundHover = false;
    group->cutOutForegroundPress = false;

    const int buttonIconColors = _decorationSettings->buttonIconColors(active);
    const int closeButtonIconColor = _decorationSettings->closeButtonIconColor(active);

    bool defaultButton =
        true; // flag indicates the button has standard colours for the behaviour and selected colour (i.e. is not a close/max/min with special colours)

    const bool &drawIconNormally =
        (_buttonType == DecorationButtonType::Close) ? _decorationSettings->showCloseIconNormally(active) : _decorationSettings->showIconNormally(active);
    const bool &drawIconOnHover =
        (_buttonType == DecorationButtonType::Close) ? _decorationSettings->showCloseIconOnHover(active) : _decorationSettings->showIconOnHover(active);
    const bool &drawIconOnPress =
        (_buttonType == DecorationButtonType::Close) ? _decorationSettings->showCloseIconOnPress(active) : _decorationSettings->showIconOnPress(active);

    const bool negativeCloseBackground =
        (_decorationSettings->buttonBackgroundColors(active) == InternalSettings::EnumButtonBackgroundColors::AccentNegativeClose
         || _decorationSettings->buttonBackgroundColors(active) == InternalSettings::EnumButtonBackgroundColors::AccentTrafficLights
         || _decorationSettings->buttonBackgroundColors(active) == InternalSettings::EnumButtonBackgroundColors::TitleBarTextNegativeClose);

    const bool negativeWhenHoverPress = closeButtonIconColor == InternalSettings::EnumCloseButtonIconColor::NegativeWhenHoverPress
        && (buttonIconColors == InternalSettings::EnumButtonIconColors::TitleBarTextNegativeClose
            || buttonIconColors == InternalSettings::EnumButtonIconColors::AccentNegativeClose
            || buttonIconColors == InternalSettings::EnumButtonIconColors::AccentTrafficLights);

    if (closeButtonIconColor != InternalSettings::EnumCloseButtonIconColor::AsSelected
        && ((_buttonType == DecorationButtonType::Close && negativeCloseBackground)
            || (negativeWhenHoverPress
                && (_buttonType == DecorationButtonType::Close
                    || (buttonIconColors == InternalSettings::EnumButtonIconColors::AccentTrafficLights
                        && (_buttonType == DecorationButtonType::Maximize || _buttonType == DecorationButtonType::Minimize)))))) {
        if (closeButtonIconColor == InternalSettings::EnumCloseButtonIconColor::White) {
            if (drawIconNormally) {
                QColor tristate1, tristate2, tristate3;
                generateTristateColors(ButtonComponent::Icon, active, Qt::GlobalColor::white, tristate1, tristate2, tristate3);
                foregroundNormal = tristate1;
                if (drawIconOnHover)
                    foregroundHover = tristate2;
                if (drawIconOnPress)
                    foregroundPress = tristate3;
            } else {
                QColor bistate1, bistate2;
                generateBistateColors(ButtonComponent::Icon, active, Qt::GlobalColor::white, bistate1, bistate2);
                if (drawIconOnHover)
                    foregroundHover = Qt::GlobalColor::white;
                if (drawIconOnPress)
                    foregroundPress = Qt::GlobalColor::white;
            }

        } else {
            if (closeButtonIconColor == InternalSettings::EnumCloseButtonIconColor::WhiteWhenHoverPress) {
                QColor bistate1, bistate2;
                generateBistateColors(ButtonComponent::Icon, active, Qt::GlobalColor::white, bistate1, bistate2);
                if (drawIconOnHover)
                    foregroundHover = bistate1;
                if (drawIconOnPress)
                    foregroundPress = bistate2;
            } else if (negativeWhenHoverPress) {
                if (_buttonType == DecorationButtonType::Close) {
                    QColor bistate1, bistate2;
                    generateBistateColors(ButtonComponent::Icon, active, decorationColors->negativeSaturated, bistate1, bistate2);
                    if (drawIconOnHover)
                        foregroundHover = bistate1;
                    if (drawIconOnPress)
                        foregroundPress = bistate2;
                } else if (_buttonType == DecorationButtonType::Maximize) {
                    QColor bistate1, bistate2;
                    generateBistateColors(ButtonComponent::Icon, active, decorationColors->positiveSaturated, bistate1, bistate2);
                    if (drawIconOnHover)
                        foregroundHover = bistate1;
                    if (drawIconOnPress)
                        foregroundPress = bistate2;
                } else if (_buttonType == DecorationButtonType::Minimize) {
                    QColor bistate1, bistate2;
                    generateBistateColors(ButtonComponent::Icon, active, decorationColors->neutral, bistate1, bistate2);
                    if (drawIconOnHover)
                        foregroundHover = bistate1;
                    if (drawIconOnPress)
                        foregroundPress = bistate2;
                }
            }
        }
    }

    if (!negativeWhenHoverPress) {
        if (_buttonType == DecorationButtonType::Close
            && (buttonIconColors == InternalSettings::EnumButtonIconColors::TitleBarTextNegativeClose
                || buttonIconColors == InternalSettings::EnumButtonIconColors::AccentNegativeClose
                || buttonIconColors == InternalSettings::EnumButtonIconColors::AccentTrafficLights)) {
            defaultButton = false;
            if (drawIconNormally) {
                QColor tristate1, tristate2, tristate3;
                generateTristateColors(ButtonComponent::Icon, active, decorationColors->negativeSaturated, tristate1, tristate2, tristate3);
                if (!foregroundNormal.isValid())
                    foregroundNormal = tristate1;
                if (drawIconOnHover && !foregroundHover.isValid())
                    foregroundHover = tristate2;
                if (drawIconOnPress && !foregroundPress.isValid())
                    foregroundPress = tristate3;
            } else {
                QColor bistate1, bistate2;
                generateBistateColors(ButtonComponent::Icon, active, decorationColors->negativeSaturated, bistate1, bistate2);
                if (drawIconOnHover && !foregroundHover.isValid())
                    foregroundHover = bistate1;
                if (drawIconOnPress && !foregroundPress.isValid())
                    foregroundPress = bistate2;
            }
        } else if (_buttonType == DecorationButtonType::Maximize && (buttonIconColors == InternalSettings::EnumButtonIconColors::AccentTrafficLights)) {
            defaultButton = false;
            if (drawIconNormally) {
                QColor tristate1, tristate2, tristate3;
                generateTristateColors(ButtonComponent::Icon, active, decorationColors->positiveSaturated, tristate1, tristate2, tristate3);
                if (!foregroundNormal.isValid())
                    foregroundNormal = decorationColors->positiveSaturated;
                if (drawIconOnHover && !foregroundHover.isValid())
                    foregroundHover = decorationColors->positiveSaturated;
                if (drawIconOnPress && !foregroundPress.isValid())
                    foregroundPress = decorationColors->positiveSaturated;
            } else {
                QColor bistate1, bistate2;
                generateBistateColors(ButtonComponent::Icon, active, decorationColors->positiveSaturated, bistate1, bistate2);
                if (drawIconOnHover && !foregroundHover.isValid())
                    foregroundHover = bistate1;
                if (drawIconOnPress && !foregroundPress.isValid())
                    foregroundPress = bistate2;
            }
        } else if (_buttonType == DecorationButtonType::Minimize && (buttonIconColors == InternalSettings::EnumButtonIconColors::AccentTrafficLights)) {
            defaultButton = false;
            if (drawIconNormally) {
                QColor tristate1, tristate2, tristate3;
                generateTristateColors(ButtonComponent::Icon, active, decorationColors->neutral, tristate1, tristate2, tristate3);
                if (!foregroundNormal.isValid())
                    foregroundNormal = tristate1;
                if (drawIconOnHover && !foregroundHover.isValid())
                    foregroundHover = tristate2;
                if (drawIconOnPress && !foregroundPress.isValid())
                    foregroundPress = tristate3;
            } else {
                QColor bistate1, bistate2;
                generateBistateColors(ButtonComponent::Icon, active, decorationColors->neutral, bistate1, bistate2);
                if (drawIconOnHover && !foregroundHover.isValid())
                    foregroundHover = bistate1;
                if (drawIconOnPress && !foregroundPress.isValid())
                    foregroundPress = bistate2;
            }
        }
    }

    if (defaultButton) {
        QColor defaultButtonColor;
        switch (buttonIconColors) {
        default:
        case InternalSettings::EnumButtonIconColors::Accent:
        case InternalSettings::EnumButtonIconColors::AccentNegativeClose:
        case InternalSettings::EnumButtonIconColors::AccentTrafficLights:
            defaultButtonColor = decorationColors->buttonFocus;
            break;
        case InternalSettings::EnumButtonIconColors::TitleBarText:
        case InternalSettings::EnumButtonIconColors::TitleBarTextNegativeClose:
            defaultButtonColor = decorationColors->titleBarText;
            break;
        }

        if (drawIconNormally) {
            QColor tristate1, tristate2, tristate3;
            generateTristateColors(ButtonComponent::Icon, active, defaultButtonColor, tristate1, tristate2, tristate3);
            if (!foregroundNormal.isValid())
                foregroundNormal = tristate1;
            if (drawIconOnHover && !foregroundHover.isValid())
                foregroundHover = tristate2;
            if (drawIconOnPress && !foregroundPress.isValid())
                foregroundPress = tristate3;
        } else {
            QColor bistate1, bistate2;
            generateBistateColors(ButtonComponent::Icon, active, defaultButtonColor, bistate1, bistate2);
            if (drawIconOnHover && !foregroundHover.isValid())
                foregroundHover = bistate1;
            if (drawIconOnPress && !foregroundPress.isValid())
                foregroundPress = bistate2;
        }
    }

    const bool buttonOverrideColorsPresent = active ? _buttonOverrideColorsPresentActive : _buttonOverrideColorsPresentInactive;
    if (buttonOverrideColorsPresent) {
        auto &buttonOverrideColors = active ? _buttonOverrideColorsActive : _buttonOverrideColorsInactive;
        if (buttonOverrideColors.value(OverridableButtonColorState::IconNormal).isValid() && drawIconNormally) {
            foregroundNormal = buttonOverrideColors.value(OverridableButtonColorState::IconNormal);
        }
        if (buttonOverrideColors.value(OverridableButtonColorState::IconHover).isValid() && drawIconOnHover) {
            foregroundHover = buttonOverrideColors.value(OverridableButtonColorState::IconHover);
        }
        if (buttonOverrideColors.value(OverridableButtonColorState::IconPress).isValid() && drawIconOnPress) {
            foregroundPress = buttonOverrideColors.value(OverridableButtonColorState::IconPress);
        }
    }

    if (_decorationSettings->onPoorIconContrast(active) != InternalSettings::EnumOnPoorIconContrast::Nothing) {
        adjustPoorForegroundContrast(foregroundNormal, group->backgroundNormal, group->cutOutForegroundNormal, active, decorationColors);
        adjustPoorForegroundContrast(foregroundHover, group->backgroundHover, group->cutOutForegroundHover, active, decorationColors);
        adjustPoorForegroundContrast(foregroundPress, group->backgroundPress, group->cutOutForegroundPress, active, decorationColors);
    }
}

void DecorationButtonPalette::adjustPoorForegroundContrast(QColor &baseForegroundColor,
                                                           const QColor &baseBackgroundColor,
                                                           bool &cutOutParameter,
                                                           const bool active,
                                                           const DecorationPaletteGroup *decorationColorGroup)
{
    if (baseForegroundColor.isValid() && baseBackgroundColor.isValid() && baseForegroundColor.alpha() != 0) {
        if (_decorationSettings->onPoorIconContrast(active) == InternalSettings::EnumOnPoorIconContrast::TitleBarBackground) {
            QColor titleBarBase;
            titleBarBase = decorationColorGroup->titleBarBase;
            titleBarBase.setAlpha(255);

            if (ColorTools::getHigherContrastForegroundColor(baseForegroundColor,
                                                             baseBackgroundColor,
                                                             _decorationSettings->poorIconContrastThreshold(active),
                                                             baseForegroundColor,
                                                             titleBarBase)) {
                cutOutParameter = true;
            }
        } else if (_decorationSettings->onPoorIconContrast(active) == InternalSettings::EnumOnPoorIconContrast::BlackWhite) {
            ColorTools::getHigherContrastForegroundColor(baseForegroundColor,
                                                         baseBackgroundColor,
                                                         _decorationSettings->poorIconContrastThreshold(active),
                                                         baseForegroundColor);
        }
    } else if (baseForegroundColor.isValid() && baseForegroundColor.alpha() != 0) {
        // use BlackWhite mode even if TitleBarBackground selected as a poor contrast here will be with the titlebar
        ColorTools::getHigherContrastForegroundColor(baseForegroundColor,
                                                     decorationColorGroup->titleBarBase,
                                                     _decorationSettings->poorIconContrastThreshold(active),
                                                     baseForegroundColor);
    }
}

void DecorationButtonPalette::generateButtonOutlinePalette(const bool active)
{
    DecorationButtonPaletteGroup *group = active ? this->_active.get() : this->_inactive.get();
    QColor &outlineNormal = group->outlineNormal;
    QColor &outlineHover = group->outlineHover;
    QColor &outlinePress = group->outlinePress;
    const DecorationPaletteGroup *decorationColors = active ? _decorationColorsActive : _decorationColorsInactive;
    const QColor &text = decorationColors->titleBarText;
    const QColor &base = decorationColors->titleBarBase;

    const int buttonBackgroundColors = _decorationSettings->buttonBackgroundColors(active);
    const bool negativeCloseBackgroundHoverPress = _decorationSettings->negativeCloseBackgroundHoverPress(active);
    const bool adjustBackgroundColorOnPoorContrast = _decorationSettings->adjustBackgroundColorOnPoorContrast(active);

    outlineNormal = QColor();
    outlineHover = QColor();
    outlinePress = QColor();

    const bool negativeClose(buttonBackgroundColors == InternalSettings::EnumButtonBackgroundColors::TitleBarTextNegativeClose
                             || buttonBackgroundColors == InternalSettings::EnumButtonBackgroundColors::AccentNegativeClose
                             || buttonBackgroundColors == InternalSettings::EnumButtonBackgroundColors::AccentTrafficLights);

    bool defaultButton =
        true; // flag indicates the button has standard colours for the behaviour and selected colour (i.e. is not a close/max/min with special colours)
    const bool &drawOutlineNormally =
        (_buttonType == DecorationButtonType::Close) ? _decorationSettings->showCloseOutlineNormally(active) : _decorationSettings->showOutlineNormally(active);
    const bool &drawOutlineOnHover =
        (_buttonType == DecorationButtonType::Close) ? _decorationSettings->showCloseOutlineOnHover(active) : _decorationSettings->showOutlineOnHover(active);
    const bool &drawOutlineOnPress =
        (_buttonType == DecorationButtonType::Close) ? _decorationSettings->showCloseOutlineOnPress(active) : _decorationSettings->showOutlineOnPress(active);

    QColor defaultOutlineColor;
    QColor accentHoverBase;
    switch (buttonBackgroundColors) {
    default:
    case InternalSettings::EnumButtonBackgroundColors::Accent:
    case InternalSettings::EnumButtonBackgroundColors::AccentNegativeClose:
    case InternalSettings::EnumButtonBackgroundColors::AccentTrafficLights:
        defaultOutlineColor = decorationColors->buttonFocus;
        if (_decorationSettings->useHoverAccent(active)) {
            accentHoverBase = decorationColors->buttonHover;
        }
        break;
    case InternalSettings::EnumButtonBackgroundColors::TitleBarText:
    case InternalSettings::EnumButtonBackgroundColors::TitleBarTextNegativeClose:
        defaultOutlineColor = decorationColors->titleBarText;
        break;
    }

    bool setDefaultOutlineNormallyOnly = false;

    // set normal, hover and press colours
    if (_buttonType == DecorationButtonType::Close && negativeClose) {
        defaultButton = false;
        if (drawOutlineNormally) {
            QColor tristate1, tristate2, tristate3;
            if (negativeCloseBackgroundHoverPress)
                setDefaultOutlineNormallyOnly = true;
            generateTristateColors(ButtonComponent::Outline, active, decorationColors->fullySaturatedNegative, tristate1, tristate2, tristate3);
            outlineNormal = tristate1;
            if (drawOutlineOnHover)
                outlineHover = tristate2;
            if (drawOutlineOnPress)
                outlinePress = tristate3;
        } else {
            QColor bistate1, bistate2;
            generateBistateColors(ButtonComponent::Outline, active, decorationColors->fullySaturatedNegative, bistate1, bistate2);
            if (drawOutlineOnHover)
                outlineHover = bistate1;
            if (drawOutlineOnPress)
                outlinePress = bistate2;
        }
    } else if (_buttonType == DecorationButtonType::Minimize && buttonBackgroundColors == InternalSettings::EnumButtonBackgroundColors::AccentTrafficLights) {
        defaultButton = false;
        if (drawOutlineNormally) {
            QColor tristate1, tristate2, tristate3;
            if (negativeCloseBackgroundHoverPress)
                setDefaultOutlineNormallyOnly = true;
            generateTristateColors(ButtonComponent::Outline, active, decorationColors->neutral, tristate1, tristate2, tristate3);
            outlineNormal = tristate1;
            if (drawOutlineOnHover)
                outlineHover = tristate2;
            if (drawOutlineOnPress)
                outlinePress = tristate3;
        } else {
            QColor bistate1, bistate2;
            generateBistateColors(ButtonComponent::Outline, active, decorationColors->neutral, bistate1, bistate2);
            if (drawOutlineOnHover)
                outlineHover = bistate1;
            if (drawOutlineOnPress)
                outlinePress = bistate2;
        }
    } else if (_buttonType == DecorationButtonType::Maximize && buttonBackgroundColors == InternalSettings::EnumButtonBackgroundColors::AccentTrafficLights) {
        defaultButton = false;
        if (drawOutlineNormally) {
            QColor tristate1, tristate2, tristate3;
            if (negativeCloseBackgroundHoverPress)
                setDefaultOutlineNormallyOnly = true;
            generateTristateColors(ButtonComponent::Outline, active, decorationColors->positiveSaturated, tristate1, tristate2, tristate3);
            outlineNormal = tristate1;
            if (drawOutlineOnHover)
                outlineHover = tristate2;
            if (drawOutlineOnPress)
                outlinePress = tristate3;
        } else {
            QColor bistate1, bistate2;
            generateBistateColors(ButtonComponent::Outline, active, decorationColors->positiveSaturated, bistate1, bistate2);
            if (drawOutlineOnHover)
                outlineHover = bistate1;
            if (drawOutlineOnPress)
                outlinePress = bistate2;
        }
    }

    if (defaultButton || setDefaultOutlineNormallyOnly) {
        if (drawOutlineNormally) {
            QColor tristate1, tristate2, tristate3;
            generateTristateColors(ButtonComponent::Outline, active, defaultOutlineColor, tristate1, tristate2, tristate3, accentHoverBase);
            outlineNormal = tristate1;
            if (drawOutlineOnHover && !setDefaultOutlineNormallyOnly)
                outlineHover = tristate2;
            if (drawOutlineOnPress && !setDefaultOutlineNormallyOnly)
                outlinePress = tristate3;
        } else {
            QColor bistate1, bistate2;
            generateBistateColors(ButtonComponent::Outline, active, defaultOutlineColor, bistate1, bistate2, accentHoverBase);
            if (drawOutlineOnHover && !setDefaultOutlineNormallyOnly)
                outlineHover = bistate1;
            if (drawOutlineOnPress && !setDefaultOutlineNormallyOnly)
                outlinePress = bistate2;
        }
    }

    const bool buttonOverrideColorsPresent = active ? _buttonOverrideColorsPresentActive : _buttonOverrideColorsPresentInactive;
    if (buttonOverrideColorsPresent) {
        auto &buttonOverrideColors = active ? _buttonOverrideColorsActive : _buttonOverrideColorsInactive;
        if (buttonOverrideColors.value(OverridableButtonColorState::OutlineNormal).isValid() && drawOutlineNormally) {
            outlineNormal = buttonOverrideColors.value(OverridableButtonColorState::OutlineNormal);
        }
        if (buttonOverrideColors.value(OverridableButtonColorState::OutlineHover).isValid() && drawOutlineOnHover) {
            outlineHover = buttonOverrideColors.value(OverridableButtonColorState::OutlineHover);
        }
        if (buttonOverrideColors.value(OverridableButtonColorState::OutlinePress).isValid() && drawOutlineOnPress) {
            outlinePress = buttonOverrideColors.value(OverridableButtonColorState::OutlinePress);
        }
    }

    // low contrast correction between outline and titlebar
    if (adjustBackgroundColorOnPoorContrast) {
        if (outlineNormal.isValid() && outlineNormal.alpha() != 0
            && KColorUtils::contrastRatio(outlineNormal, base) < _decorationSettings->poorBackgroundContrastThreshold(active)) {
            outlineNormal = KColorUtils::mix(outlineNormal, text, 0.4);
        }

        if (outlineHover.isValid() && outlineHover.alpha() != 0
            && KColorUtils::contrastRatio(outlineHover, base) < _decorationSettings->poorBackgroundContrastThreshold(active)) {
            outlineHover = KColorUtils::mix(outlineHover, text, 0.4);
        }
        if (outlinePress.isValid() && outlinePress.alpha() != 0
            && KColorUtils::contrastRatio(outlinePress, base) < _decorationSettings->poorBackgroundContrastThreshold(active)) {
            outlinePress = KColorUtils::mix(outlinePress, text, 0.4);
        }
    }
}

}
