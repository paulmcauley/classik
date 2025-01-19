/*
 * SPDX-FileCopyrightText: 2014 Hugo Pereira Da Costa <hugo.pereira@free.fr>
 * SPDX-FileCopyrightText: 2022 Paul A McAuley <kde@paulmcauley.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#define KLASSY_DECORATION_DEBUG_MODE 0
#define KLASSY_STYLE_DEBUG_MODE 0
#define KLASSY_QDEBUG_OUTPUT_PATH_RELATIVE_HOME "/Desktop/klassy_debug.txt"

#include "breezecommon_export.h"
#include "breezesettings.h"

#include <QFlags>
#include <QList>
#include <QPointer>
#include <QScopedPointer>
#include <QSharedPointer>
#include <QWeakPointer>

namespace Breeze
{

// COMMON-------------------------------------------------------------------

QString BREEZECOMMON_EXPORT klassyLongVersion();

//* standard pen widths
struct BREEZECOMMON_EXPORT PenWidth {
    /* Using 1 instead of slightly more than 1 causes symbols drawn with
     * pen strokes to look skewed. The exact amount added does not matter
     * as long as it isn't too visible. Even with QPen::setCosmetic(true),
     * 1px pen widths still look slightly worse.
     */
    // The standard pen stroke width for symbols.
    static constexpr qreal Symbol = 1.001;

    // The standard pen stroke width for frames.
    static constexpr qreal Frame = 1.001;

    // The standard pen stroke width for shadows.
    static constexpr qreal Shadow = 1.001;

    // A value for pen width arguments to make it clear that there is no pen stroke
    static constexpr int NoPen = 0;
};

// copied from KDecoration3/src/decorationdefines.h -- needs to be kept in sync
// originally Klassy linked directly to KDecoration but this did not work on the combined Qt5/Qt6 build on Plasma 6 with Qt5 applications
enum class DecorationButtonType {
    /**
     * The Menu button requests showing the window menu on left or right click.
     **/
    Menu,
    /**
     * The ApplicationMenu button requests showing the application's menu on left or right click.
     */
    ApplicationMenu,
    /**
     * The OnAllDesktops button requests toggling the DecoratedWindow's on all desktops state.
     * The DecoratedButton is only visible if multiple virtual desktops are available.
     **/
    OnAllDesktops,
    /**
     * The Minimize button requests minimizing the DecoratedWindow. The DecorationButton is only
     * enabled if the DecoratedWindow is minimizeable.
     **/
    Minimize,
    /**
     * The Maximize button requests maximizing the DecoratedWindow. The DecorationButton is checkable
     * and if the DecoratedWindow is maximized the DecorationButton is checked. The DecorationButton
     * supports multiple mouse buttons to change horizontal, vertical and overall maximized state.
     *
     * The DecorationButton is only enabled if the DecoratedWindow is maximizeable.
     **/
    Maximize,
    /**
     * The Close button requests closing the DecoratedWindow. The DecorationButton is only enabled
     * if the DecoratedWindow is closeable.
     **/
    Close,
    /**
     * The ContextHelp button requests entering the context help mode. The DecorationButton is only
     * visible if the DecoratedWindow provides context help.
     **/
    ContextHelp,
    /**
     * The Shade button requests toggling the DecoratedWindow's shaded state. The DecoratedButton
     * is only enabled if the DecoratedWindow is shadeable.
     **/
    Shade,
    /**
     * The KeepBelow button requests toggling the DecoratedWindow's keep below state.
     **/
    KeepBelow,
    /**
     * The KeepAbove button requests toggling the DecoratedWindow's keep above state.
     **/
    KeepAbove,
    /**
     * The Custom type allows a Decoration to provide custom DecorationButtons.
     **/
    Custom,
    /**
     * The Spacer button provides some space between buttons.
     */
    Spacer,
    COUNT
};

// list of keys used for window decoration exceptions
static QStringList windecoExceptionKeys = {
    "Enabled",
    "ExceptionProgramNamePattern",
    "ExceptionWindowPropertyPattern",
    "ExceptionWindowPropertyType",
    "HideTitleBar",
    "OpaqueTitleBar",
    "PreventApplyOpacityToHeader",
    "ExceptionBorder",
    "BorderSize",
    "ExceptionPreset",
};

// KDECORATION-------------------------------------------------------------

//* convenience typedefs
using InternalSettingsPtr = QSharedPointer<InternalSettings>;
using InternalSettingsList = QList<InternalSettingsPtr>;
using InternalSettingsListIterator = QListIterator<InternalSettingsPtr>;

//* metrics
namespace Metrics
{

//* corner radius (pixels)
// Paul A McAuley: No longer used - now InternalSettings::windowCornerRadius()
// Frame_FrameRadius = 3,

//* titlebar metrics, in units of small spacing

// Paul A McAuley: No longer used - now InternalSettings::titleBarTopMargin(), InternalSettings::titleBarBottomMargin()
// TitleBar_TopMargin = 2,
// TitleBar_BottomMargin = 2,

// Paul A McAuley: No longer used - now InternalSettings::titleBarLeftMargin(), InternalSettings::titleBarRightMargin()
// TitleBar_SideMargin = 2,

// Paul A McAuley: No longer used - now InternalSettings::buttonSpacingLeft() and InternalSettings::buttonSpacingRight()
// TitleBar_ButtonSpacing = 2,

// shadow dimensions (pixels)
static constexpr int Decoration_Shadow_Overlap = 3;

};

// KSTYLE------------------------------------------------------------------

//*@name convenience typedef
//@{

//* scoped pointer convenience typedef
template<typename T>
using WeakPointer = QPointer<T>;

//* scoped pointer convenience typedef
template<typename T>
using ScopedPointer = QScopedPointer<T, QScopedPointerPodDeleter>;

//@}

//* animation mode
enum BREEZECOMMON_EXPORT AnimationMode {
    AnimationNone = 0,
    AnimationHover = 0x1,
    AnimationFocus = 0x2,
    AnimationEnable = 0x4,
    AnimationPressed = 0x8,
};

Q_DECLARE_FLAGS(AnimationModes, AnimationMode)

//* corners
enum BREEZECOMMON_EXPORT Corner {
    CornerTopLeft = 0x1,
    CornerTopRight = 0x2,
    CornerBottomLeft = 0x4,
    CornerBottomRight = 0x8,
    CornersTop = CornerTopLeft | CornerTopRight,
    CornersBottom = CornerBottomLeft | CornerBottomRight,
    CornersLeft = CornerTopLeft | CornerBottomLeft,
    CornersRight = CornerTopRight | CornerBottomRight,
    AllCorners = CornerTopLeft | CornerTopRight | CornerBottomLeft | CornerBottomRight,
};

Q_DECLARE_FLAGS(Corners, Corner)

//* sides
enum BREEZECOMMON_EXPORT Side {
    SideLeft = 0x1,
    SideTop = 0x2,
    SideRight = 0x4,
    SideBottom = 0x8,
    AllSides = SideLeft | SideTop | SideRight | SideBottom,
};

Q_DECLARE_FLAGS(Sides, Side)

//* checkbox state
enum BREEZECOMMON_EXPORT CheckBoxState {
    CheckOff,
    CheckPartial,
    CheckOn,
    CheckAnimated,
};

//* radio button state
enum BREEZECOMMON_EXPORT RadioButtonState {
    RadioOff,
    RadioOn,
    RadioAnimated,
};

//* arrow orientation
enum BREEZECOMMON_EXPORT ArrowOrientation {
    ArrowNone,
    ArrowUp,
    ArrowDown,
    ArrowLeft,
    ArrowRight,
};
}

Q_DECLARE_OPERATORS_FOR_FLAGS(Breeze::AnimationModes)
Q_DECLARE_OPERATORS_FOR_FLAGS(Breeze::Corners)
Q_DECLARE_OPERATORS_FOR_FLAGS(Breeze::Sides)
