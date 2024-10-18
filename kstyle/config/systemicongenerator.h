/*
 * SPDX-FileCopyrightText: 2024 Paul A McAuley <kde@paulmcauley.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "breeze.h"
#include "decorationcolors.h"

namespace Breeze
{

class SystemIconGenerator
{
public:
    SystemIconGenerator(InternalSettingsPtr internalSettings);

    void generate();

private:
    void addSystemScales();
    void generateIconThemeDir(const QString themeDirPath, const QString themeName, const QString inherits, const DecorationColors &decorationColors);

    InternalSettingsPtr m_internalSettings;

    QList<qreal> m_scales = {1, 1.25, 1.5, 1.75, 2, 2.25, 2.5, 2.75, 3};
    const QMap<InternalSettings::EnumIconSize::type, int> m_iconSizes{
        {InternalSettings::EnumIconSize::IconSmallMedium, 16},
        {InternalSettings::EnumIconSize::IconMedium, 18},
        {InternalSettings::EnumIconSize::IconLargeMedium, 20},
        {InternalSettings::EnumIconSize::IconLarge, 22},
        {InternalSettings::EnumIconSize::IconVeryLarge, 24},
        {InternalSettings::EnumIconSize::IconGiant, 32},
        {InternalSettings::EnumIconSize::IconHumongous, 48},
    };

    struct iconType {
        DecorationButtonType type;
        bool checked;
        QString name;
    };

    const QList<iconType> m_iconTypes{
        {DecorationButtonType::ApplicationMenu, false, QStringLiteral("application-menu-symbolic")},
        {DecorationButtonType::ApplicationMenu, false, QStringLiteral("application-menu")},
        {DecorationButtonType::OnAllDesktops, false, QStringLiteral("window-pin-symbolic")},
        //{DecorationButtonType::OnAllDesktops, false, QStringLiteral("window-pin")},
        {DecorationButtonType::OnAllDesktops, true, QStringLiteral("window-unpin-symbolic")},
        //{DecorationButtonType::OnAllDesktops, true, QStringLiteral("window-unpin")},
        {DecorationButtonType::Minimize, false, QStringLiteral("window-minimize-symbolic")},
        {DecorationButtonType::Minimize, false, QStringLiteral("window-minimize")},
        {DecorationButtonType::Maximize, false, QStringLiteral("window-maximize-symbolic")},
        {DecorationButtonType::Maximize, false, QStringLiteral("window-maximize")},
        {DecorationButtonType::Maximize, true, QStringLiteral("window-restore-symbolic")},
        {DecorationButtonType::Maximize, true, QStringLiteral("window-restore")},
        {DecorationButtonType::Close, false, QStringLiteral("window-close-symbolic")},
        {DecorationButtonType::Close, false, QStringLiteral("window-close")},
        {DecorationButtonType::Close, false, QStringLiteral("dialog-close")},
        {DecorationButtonType::Close, false, QStringLiteral("geany-close-all")},
        {DecorationButtonType::Close, false, QStringLiteral("gtk-close")},
        {DecorationButtonType::Close, false, QStringLiteral("gtk-no")},
        {DecorationButtonType::Close, false, QStringLiteral("kontes-close")},
        {DecorationButtonType::Close, false, QStringLiteral("tab-close-other")},
        {DecorationButtonType::Close, false, QStringLiteral("tab-close")},
        //{DecorationButtonType::ContextHelp, false, QStringLiteral("help-contextual-symbolic")},
        //{DecorationButtonType::ContextHelp, false, QStringLiteral("help-contextual")},
        {DecorationButtonType::Shade, false, QStringLiteral("window-shade-symbolic")},
        {DecorationButtonType::Shade, false, QStringLiteral("window-shade")},
        {DecorationButtonType::Shade, true, QStringLiteral("window-unshade-symbolic")},
        {DecorationButtonType::Shade, true, QStringLiteral("window-unshade")},
        {DecorationButtonType::KeepBelow, false, QStringLiteral("window-keep-below-symbolic")},
        {DecorationButtonType::KeepBelow, false, QStringLiteral("window-keep-below")},
        {DecorationButtonType::KeepAbove, false, QStringLiteral("window-keep-above-symbolic")},
        {DecorationButtonType::KeepAbove, false, QStringLiteral("window-keep-above")},
    };
};

}
