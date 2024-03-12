/*
 * SPDX-FileCopyrightText: 2022-2023 Paul A McAuley <kde@paulmcauley.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "systemicontheme.h"
#include "colortools.h"
#include <KIconLoader>
#include <QIcon>

namespace Breeze
{

using KDecoration2::DecorationButtonType;

void SystemIconTheme::paintIconFromSystemTheme(QString iconName)
{
    QColor color = m_painter->pen().color();

    KIconLoader *iconLoader = KIconLoader::global();
    QPalette originalPalette;
    if (!m_internalSettings->forceColorizeSystemIcons()) {
        originalPalette = iconLoader->customPalette();
        m_palette.setColor(QPalette::WindowText, color);
        iconLoader->setCustomPalette(m_palette);
    }

    int m_iconWidthScaled = qRound(m_iconWidth * m_painter->device()->devicePixelRatioF());
    QPixmap iconPixmap = iconLoader->loadIcon(iconName, KIconLoader::Group::NoGroup, m_iconWidthScaled);
    iconPixmap.setDevicePixelRatio(m_painter->device()->devicePixelRatioF());
    QSize pixmapSize(m_iconWidth, m_iconWidth);
    QRect rect(QPoint(0, 0), pixmapSize);

    if (m_internalSettings->forceColorizeSystemIcons()) {
        // convert the alpha of the icon into tinted colour on transparent
        QImage iconImage(iconPixmap.toImage());
        ColorTools::convertAlphaToColor(iconImage, color);

        m_painter->drawImage(rect, iconImage);
    } else {
        m_painter->drawPixmap(rect, iconPixmap);

        if (originalPalette == QPalette()) {
            iconLoader->resetPalette();
        } else {
            iconLoader->setCustomPalette(originalPalette);
        }
    }
}

void SystemIconTheme::renderIcon()
{
    paintIconFromSystemTheme(m_systemIconFromTheme);
}

void SystemIconTheme::systemIconNames(KDecoration2::DecorationButtonType type, QString &systemIconName, QString &systemIconCheckedName)
{
    switch (type) {
    case DecorationButtonType::Close:
        systemIconName = isSystemIconNameAvailable(QStringLiteral("window-close-symbolic"), QStringLiteral("window-close"));
        systemIconCheckedName = systemIconName;
        break;

    case DecorationButtonType::Maximize:
        systemIconCheckedName = isSystemIconNameAvailable(QStringLiteral("window-restore-symbolic"), QStringLiteral("window-restore"));
        systemIconName = isSystemIconNameAvailable(QStringLiteral("window-maximize-symbolic"), QStringLiteral("window-maximize"));
        break;

    case DecorationButtonType::Minimize:
        systemIconName = isSystemIconNameAvailable(QStringLiteral("window-minimize-symbolic"), QStringLiteral("window-minimize"));
        systemIconCheckedName = systemIconName;
        break;

    case DecorationButtonType::OnAllDesktops:
        systemIconCheckedName = isSystemIconNameAvailable(QStringLiteral("window-unpin-symbolic"), QStringLiteral("window-unpin"));
        systemIconName = isSystemIconNameAvailable(QStringLiteral("window-pin-symbolic"), QStringLiteral("window-pin"));
        break;

    case DecorationButtonType::Shade:
        systemIconCheckedName = isSystemIconNameAvailable(QStringLiteral("window-unshade-symbolic"), QStringLiteral("window-unshade"));
        systemIconName = isSystemIconNameAvailable(QStringLiteral("window-shade-symbolic"), QStringLiteral("window-shade"));
        break;

    case DecorationButtonType::KeepBelow:
        systemIconName = isSystemIconNameAvailable(QStringLiteral("window-keep-below-symbolic"), QStringLiteral("window-keep-below"));
        systemIconCheckedName = systemIconName;
        break;

    case DecorationButtonType::KeepAbove:
        systemIconName = isSystemIconNameAvailable(QStringLiteral("window-keep-above-symbolic"), QStringLiteral("window-keep-above"));
        systemIconCheckedName = systemIconName;
        break;

    case DecorationButtonType::ApplicationMenu:
        systemIconName = isSystemIconNameAvailable(QStringLiteral("application-menu-symbolic"), QStringLiteral("application-menu"));
        systemIconCheckedName = systemIconName;
        break;

    case DecorationButtonType::ContextHelp:
        systemIconName = isSystemIconNameAvailable(QStringLiteral("help-contextual-symbolic"), QStringLiteral("help-contextual"));
        systemIconCheckedName = systemIconName;
        break;

    default:
        break;
    }
}

QString SystemIconTheme::isSystemIconNameAvailable(const QString &preferredIconName, const QString &backupIconName)
{
    KIconLoader *iconLoader = KIconLoader::global();
    if (iconLoader->hasIcon(preferredIconName))
        return preferredIconName;
    else if (iconLoader->hasIcon(backupIconName))
        return backupIconName;
    else
        return QString();
}

}
