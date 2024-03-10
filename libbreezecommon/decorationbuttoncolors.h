#ifndef BREEZE_DECORATIONBUTTONCOLORS_H
#define BREEZE_DECORATIONBUTTONCOLORS_H

/*
 * SPDX-FileCopyrightText: 2023-2024 Paul A McAuley <kde@paulmcauley.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "breeze.h"
#include "breezecommon_export.h"
#include "decorationcolors.h"
#include <KColorScheme>
#include <KDecoration2/DecorationButton>
#include <KDecoration2/DecorationSettings>
#include <QColor>
#include <memory>

namespace Breeze
{

struct DecorationPaletteGroup;
class DecorationColors;

const QList<KDecoration2::DecorationButtonType> coloredWindowDecorationButtonTypes{
    KDecoration2::DecorationButtonType::Menu,
    KDecoration2::DecorationButtonType::ApplicationMenu,
    KDecoration2::DecorationButtonType::OnAllDesktops,
    KDecoration2::DecorationButtonType::Minimize,
    KDecoration2::DecorationButtonType::Maximize,
    KDecoration2::DecorationButtonType::Close,
    KDecoration2::DecorationButtonType::ContextHelp,
    KDecoration2::DecorationButtonType::Shade,
    KDecoration2::DecorationButtonType::KeepBelow,
    KDecoration2::DecorationButtonType::KeepAbove,
    KDecoration2::DecorationButtonType::Custom,
};

const QList<KDecoration2::DecorationButtonType> coloredAppStyleDecorationButtonTypes{
    KDecoration2::DecorationButtonType::Minimize,
    KDecoration2::DecorationButtonType::Maximize,
    KDecoration2::DecorationButtonType::Close,
};

enum struct BREEZECOMMON_EXPORT OverridableButtonColorState {
    IconNormal,
    IconHover,
    IconPress,
    BackgroundNormal,
    BackgroundHover,
    BackgroundPress,
    OutlineNormal,
    OutlineHover,
    OutlinePress,
    COUNT,
};

enum struct BREEZECOMMON_EXPORT ButtonComponent { Icon, Background, Outline, COUNT };

const QStringList overridableButtonColorStatesJsonStrings{
    QStringLiteral("IconNormal"),
    QStringLiteral("IconHover"),
    QStringLiteral("IconPress"),
    QStringLiteral("BackgroundNormal"),
    QStringLiteral("BackgroundHover"),
    QStringLiteral("BackgroundPress"),
    QStringLiteral("OutlineNormal"),
    QStringLiteral("OutlineHover"),
    QStringLiteral("OutlinePress"),
};

const QStringList overrideColorItems{
    QStringLiteral("Custom"),
    QStringLiteral("TitleBarTextAuto"),
    QStringLiteral("TitleBarTextActive"),
    QStringLiteral("TitleBarTextInactive"),
    QStringLiteral("TitleBarBackgroundAuto"),
    QStringLiteral("TitleBarBackgroundActive"),
    QStringLiteral("TitleBarBackgroundInactive"),
    QStringLiteral("AccentButtonFocus"),
    QStringLiteral("AccentButtonHover"),
    QStringLiteral("AccentHighlight"),
    QStringLiteral("AccentHighlightLessSaturated"),
    QStringLiteral("NegativeText"),
    QStringLiteral("NegativeLessSaturated"),
    QStringLiteral("NegativeSaturated"),
    QStringLiteral("NegativeFullySaturated"),
    QStringLiteral("NeutralText"),
    QStringLiteral("NeutralLessSaturated"),
    QStringLiteral("NeutralSaturated"),
    QStringLiteral("PositiveText"),
    QStringLiteral("PositiveLessSaturated"),
    QStringLiteral("PositiveSaturated"),
    QStringLiteral("White"),
    QStringLiteral("WindowOutlineAuto"),
    QStringLiteral("WindowOutlineActive"),
    QStringLiteral("WindowOutlineInactive"),
    QStringLiteral("WindowShadowAuto"),
    QStringLiteral("WindowShadowActive"),
    QStringLiteral("WindowShadowInactive"),
};

struct BREEZECOMMON_EXPORT DecorationButtonPaletteGroup {
    QColor foregroundPress;
    QColor foregroundHover;
    QColor foregroundNormal;

    //* These cutOut parameters signify that the corresponding colour has been set to the titlebar colour, and that it could be painted with black and
    // CompositionMode_DestinationOut to give a cut-out effect.
    bool cutOutForegroundPress = false;
    bool cutOutForegroundHover = false;
    bool cutOutForegroundNormal = false;

    QColor backgroundPress;
    QColor backgroundHover;
    QColor backgroundNormal;

    QColor outlinePress;
    QColor outlineHover;
    QColor outlineNormal;
};

/**
 *  @brief Class to generate the colour palette used in a decoration button
 */
class BREEZECOMMON_EXPORT DecorationButtonPalette
{
public:
    DecorationButtonPalette(KDecoration2::DecorationButtonType buttonType);

    void generate(InternalSettingsPtr decorationSettings,
                  const DecorationPaletteGroup *decorationColorsActive,
                  const DecorationPaletteGroup *decorationColorsInactive,
                  const bool generateOneGroupOnly = false,
                  const bool oneGroupActiveState = true);
    const DecorationButtonPaletteGroup *active() const
    {
        return _active.get();
    }
    const DecorationButtonPaletteGroup *inactive() const
    {
        return _inactive.get();
    }

    KDecoration2::DecorationButtonType buttonType()
    {
        return _buttonType;
    }

    static QColor overrideColorItemsIndexToColor(const DecorationPaletteGroup *decorationColorsActive,
                                                 const DecorationPaletteGroup *decorationColorsInactive,
                                                 const int overrideColorItemsIndex,
                                                 const bool active);

private:
    void decodeButtonOverrideColors(const bool active);
    void generateBistateColors(ButtonComponent component,
                               const bool active,
                               QColor baseColor,
                               QColor &bistate1,
                               QColor &bistate2,
                               QColor accentHoverBase = QColor());
    void generateTristateColors(ButtonComponent component,
                                const bool active,
                                QColor baseColor,
                                QColor &tristate1,
                                QColor &tristate2,
                                QColor &tristate3,
                                QColor accentHoverBase = QColor());
    void generateButtonBackgroundPalette(const bool active);
    void generateButtonForegroundPalette(const bool active);
    void adjustPoorForegroundContrast(QColor &baseForegroundColor,
                                      const QColor &baseBackgroundColor,
                                      bool &cutOutParameter,
                                      const bool active,
                                      const DecorationPaletteGroup *decorationColorGroup);
    void generateButtonOutlinePalette(const bool active);

    InternalSettingsPtr _decorationSettings;
    KDecoration2::DecorationButtonType _buttonType;
    const DecorationPaletteGroup *_decorationColorsActive;
    const DecorationPaletteGroup *_decorationColorsInactive;

    bool _buttonOverrideColorsPresentActive{false};
    bool _buttonOverrideColorsPresentInactive{false};

    QMap<OverridableButtonColorState, QColor> _buttonOverrideColorsActive;
    QMap<OverridableButtonColorState, QColor> _buttonOverrideColorsInactive;

    std::shared_ptr<DecorationButtonPaletteGroup> _active;
    std::shared_ptr<DecorationButtonPaletteGroup> _inactive;
};

}

#endif
