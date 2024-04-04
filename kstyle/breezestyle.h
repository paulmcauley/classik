/*
 * SPDX-FileCopyrightText: 2014 Hugo Pereira Da Costa <hugo.pereira@free.fr>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef breezestyle_h
#define breezestyle_h

#include "breeze.h"
#include "breezehelper.h"
#include "breezemetrics.h"
#include "config-breeze.h"

#if BREEZE_HAVE_KSTYLE
#include <KStyle>
#endif

#include <QAbstractItemView>
#include <QAbstractScrollArea>

#include <QCommandLinkButton>
#include <QCommonStyle>
#include <QDockWidget>
#include <QFocusFrame>
#include <QHash>
#include <QIcon>
#include <QMdiSubWindow>
#include <QStyleOption>
#include <QWidget>

#include <functional>

namespace BreezePrivate
{
class TabBarData;
}

namespace Breeze
{

class Animations;
class FrameShadowFactory;
class Helper;
class MdiWindowShadowFactory;
class Mnemonics;
class ShadowHelper;
class SplitterFactory;
class WidgetExplorer;
class WindowManager;
class BlurHelper;
class ToolsAreaManager;

//* convenience typedef for base class
#if !BREEZE_HAVE_KSTYLE
using ParentStyleClass = QCommonStyle;
#else
using ParentStyleClass = KStyle;
#endif

//* base class for breeze style
/** it is responsible to draw all the primitives to be displayed on screen, on request from Qt paint engine */
class Style : public ParentStyleClass
{
    Q_OBJECT

    /* this tells kde applications that custom style elements are supported, using the kstyle mechanism */
    Q_CLASSINFO("X-KDE-CustomElements", "true")

public:
    //* constructor
    explicit Style();

    //* destructor
    ~Style() override;

    //* needed to avoid warnings at compilation time
    using ParentStyleClass::polish;
    using ParentStyleClass::unpolish;

    //* widget polishing
    void polish(QWidget *) override;

    //* widget unpolishing
    void unpolish(QWidget *) override;

    //* application polishing
    void polish(QApplication *) override;

    //* polish scrollarea
    void polishScrollArea(QAbstractScrollArea *);

    //* pixel metrics
    int pixelMetric(PixelMetric, const QStyleOption * = nullptr, const QWidget * = nullptr) const override;

    //* style hints
    int styleHint(StyleHint, const QStyleOption * = nullptr, const QWidget * = nullptr, QStyleHintReturn * = nullptr) const override;

    //* returns rect corresponding to one widget's subelement
    QRect subElementRect(SubElement, const QStyleOption *, const QWidget *) const override;

    //* returns rect corresponding to one widget's subcontrol
    QRect subControlRect(ComplexControl, const QStyleOptionComplex *, SubControl, const QWidget *) const override;

    //* returns size matching contents
    QSize sizeFromContents(ContentsType, const QStyleOption *, const QSize &, const QWidget *) const override;

    //* returns which subcontrol given QPoint corresponds to
    SubControl hitTestComplexControl(ComplexControl, const QStyleOptionComplex *, const QPoint &, const QWidget *) const override;

    //* primitives
    void drawPrimitive(PrimitiveElement, const QStyleOption *, QPainter *, const QWidget *) const override;

    //* controls
    void drawControl(ControlElement, const QStyleOption *, QPainter *, const QWidget *) const override;

    //* complex controls
    void drawComplexControl(ComplexControl, const QStyleOptionComplex *, QPainter *, const QWidget *) const override;

    //* generic text rendering
    void drawItemText(QPainter *, const QRect &, int alignment, const QPalette &, bool enabled, const QString &, QPalette::ColorRole = QPalette::NoRole)
        const override;

    bool event(QEvent *) override;

    //*@name event filters
    //@{

    bool eventFilter(QObject *, QEvent *) override;
    bool eventFilterScrollArea(QWidget *, QEvent *);
    bool eventFilterComboBoxContainer(QWidget *, QEvent *);
    bool eventFilterDockWidget(QDockWidget *, QEvent *);
    bool eventFilterMdiSubWindow(QMdiSubWindow *, QEvent *);
    bool eventFilterCommandLinkButton(QCommandLinkButton *, QEvent *);
    bool eventFilterScrollBar(QScrollBar *scrollBar, QEvent *event);
    int scrollBarPixelPosToRangeValue(QScrollBar *scrollBar, int pos) const;

    //* install event filter to object, in a unique way
    void addEventFilter(QObject *object)
    {
        object->removeEventFilter(this);
        object->installEventFilter(this);
    }

    //@}

protected Q_SLOTS:

    //* update configuration
    void configurationChanged();

    //* global configuration changed
    void globalConfigurationChanged(int type, int arg);
    void loadGlobalAnimationSettings();

    //* standard icons
    QIcon standardIconImplementation(StandardPixmap, const QStyleOption *, const QWidget *) const;

    //* set flag to regenerate cache of decorationColors and update configuration
    void generateDecorationColorsOnDecorationColorSettingsUpdate(QByteArray uuid);

protected:
    //* standard icons
    QIcon standardIcon(StandardPixmap pixmap, const QStyleOption *option = nullptr, const QWidget *widget = nullptr) const override
    {
        return standardIconImplementation(pixmap, option, widget);
    }

private:
    //* load configuration
    void loadConfiguration();

    bool isTabletMode() const;

    //*@name subelementRect specialized functions
    //@{

    QRect pushButtonContentsRect(const QStyleOption *, const QWidget *) const;
    QRect checkBoxContentsRect(const QStyleOption *, const QWidget *) const;
    QRect lineEditContentsRect(const QStyleOption *, const QWidget *) const;
    QRect progressBarGrooveRect(const QStyleOption *, const QWidget *) const;
    QRect progressBarContentsRect(const QStyleOption *, const QWidget *) const;
    QRect progressBarLabelRect(const QStyleOption *, const QWidget *) const;
    QRect frameContentsRect(const QStyleOption *, const QWidget *) const;
    QRect headerArrowRect(const QStyleOption *, const QWidget *) const;
    QRect headerLabelRect(const QStyleOption *, const QWidget *) const;
    QRect tabBarTabLeftButtonRect(const QStyleOption *, const QWidget *) const;
    QRect tabBarTabRightButtonRect(const QStyleOption *, const QWidget *) const;
    QRect tabWidgetTabBarRect(const QStyleOption *, const QWidget *) const;
    QRect tabWidgetTabContentsRect(const QStyleOption *, const QWidget *) const;
    QRect tabWidgetTabPaneRect(const QStyleOption *, const QWidget *) const;
    QRect tabWidgetCornerRect(SubElement, const QStyleOption *, const QWidget *widget) const;
    QRect toolBoxTabContentsRect(const QStyleOption *, const QWidget *) const;
    QRect genericLayoutItemRect(const QStyleOption *, const QWidget *) const;

    //@}

    //*@name subcontrol Rect specialized functions
    //@{

    QRect groupBoxSubControlRect(const QStyleOptionComplex *, SubControl, const QWidget *) const;
    QRect toolButtonSubControlRect(const QStyleOptionComplex *, SubControl, const QWidget *) const;
    QRect comboBoxSubControlRect(const QStyleOptionComplex *, SubControl, const QWidget *) const;
    QRect spinBoxSubControlRect(const QStyleOptionComplex *, SubControl, const QWidget *) const;
    QRect scrollBarInternalSubControlRect(const QStyleOptionComplex *, SubControl) const;

    enum class OverrideAutoHideArrows { NoOverride, ShowArrow, HideArrow };
    QRect scrollBarSubControlRect(const QStyleOptionComplex *,
                                  SubControl,
                                  const QWidget *,
                                  OverrideAutoHideArrows overrideHideSubLine = OverrideAutoHideArrows::NoOverride,
                                  OverrideAutoHideArrows overrideHideAddLine = OverrideAutoHideArrows::NoOverride) const;
    bool scrollBarAutoHideArrowsException(const QWidget *widget) const;
    bool shouldAutoHideArrows(const QWidget *widget) const;
    QRect dialSubControlRect(const QStyleOptionComplex *, SubControl, const QWidget *) const;
    QRect sliderSubControlRect(const QStyleOptionComplex *, SubControl, const QWidget *) const;

    //@}

    //*@name sizeFromContents
    //@{
    QSize defaultSizeFromContents(const QStyleOption *, const QSize &size, const QWidget *) const
    {
        return size;
    }

    QSize checkBoxSizeFromContents(const QStyleOption *, const QSize &, const QWidget *) const;
    QSize lineEditSizeFromContents(const QStyleOption *, const QSize &, const QWidget *) const;
    QSize comboBoxSizeFromContents(const QStyleOption *, const QSize &, const QWidget *) const;
    QSize spinBoxSizeFromContents(const QStyleOption *, const QSize &, const QWidget *) const;
    QSize sliderSizeFromContents(const QStyleOption *, const QSize &, const QWidget *) const;
    QSize pushButtonSizeFromContents(const QStyleOption *, const QSize &, const QWidget *) const;
    QSize toolButtonSizeFromContents(const QStyleOption *, const QSize &, const QWidget *) const;
    QSize menuBarItemSizeFromContents(const QStyleOption *, const QSize &, const QWidget *) const;
    QSize menuItemSizeFromContents(const QStyleOption *, const QSize &, const QWidget *) const;
    QSize progressBarSizeFromContents(const QStyleOption *, const QSize &, const QWidget *) const;
    QSize tabWidgetSizeFromContents(const QStyleOption *, const QSize &, const QWidget *) const;
    QSize tabBarTabSizeFromContents(const QStyleOption *, const QSize &, const QWidget *) const;
    QSize headerSectionSizeFromContents(const QStyleOption *, const QSize &, const QWidget *) const;
    QSize itemViewItemSizeFromContents(const QStyleOption *, const QSize &, const QWidget *) const;

    //@}

    //*@name primitives specialized functions
    //@{

    bool emptyPrimitive(const QStyleOption *, QPainter *, const QWidget *) const
    {
        return true;
    }

    bool drawFramePrimitive(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawFrameLineEditPrimitive(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawFrameFocusRectPrimitive(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawFrameMenuPrimitive(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawFrameGroupBoxPrimitive(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawFrameTabWidgetPrimitive(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawFrameTabBarBasePrimitive(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawFrameWindowPrimitive(const QStyleOption *, QPainter *, const QWidget *) const;

    bool drawIndicatorArrowUpPrimitive(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
    {
        return drawIndicatorArrowPrimitive(ArrowUp, option, painter, widget);
    }

    bool drawIndicatorArrowDownPrimitive(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
    {
        return drawIndicatorArrowPrimitive(ArrowDown, option, painter, widget);
    }

    bool drawIndicatorArrowLeftPrimitive(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
    {
        return drawIndicatorArrowPrimitive(ArrowLeft, option, painter, widget);
    }

    bool drawIndicatorArrowRightPrimitive(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
    {
        return drawIndicatorArrowPrimitive(ArrowRight, option, painter, widget);
    }

    bool drawIndicatorArrowPrimitive(ArrowOrientation, const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawIndicatorHeaderArrowPrimitive(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawPanelButtonCommandPrimitive(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawPanelButtonToolPrimitive(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawTabBarPanelButtonToolPrimitive(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawPanelScrollAreaCornerPrimitive(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawPanelMenuPrimitive(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawPanelTipLabelPrimitive(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawPanelItemViewItemPrimitive(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawIndicatorCheckBoxPrimitive(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawIndicatorRadioButtonPrimitive(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawIndicatorButtonDropDownPrimitive(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawIndicatorTabClosePrimitive(const QStyleOption *, QPainter *painter, const QWidget *widget) const;
    bool drawIndicatorTabTearPrimitive(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawIndicatorToolBarHandlePrimitive(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawIndicatorToolBarSeparatorPrimitive(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawIndicatorBranchPrimitive(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawWidgetPrimitive(const QStyleOption *, QPainter *, const QWidget *) const;

    //@}

    //*@name controls specialized functions
    //@{

    bool emptyControl(const QStyleOption *, QPainter *, const QWidget *) const
    {
        return true;
    }

    bool drawPushButtonLabelControl(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawToolButtonLabelControl(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawCheckBoxLabelControl(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawComboBoxLabelControl(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawMenuBarItemControl(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawMenuItemControl(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawProgressBarControl(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawProgressBarContentsControl(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawProgressBarGrooveControl(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawProgressBarLabelControl(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawScrollBarSliderControl(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawScrollBarAddLineControl(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawScrollBarSubLineControl(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawShapedFrameControl(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawFocusFrame(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawRubberBandControl(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawHeaderSectionControl(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawHeaderEmptyAreaControl(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawTabBarTabLabelControl(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawTabBarTabShapeControl(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawToolBoxTabLabelControl(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawToolBoxTabShapeControl(const QStyleOption *, QPainter *, const QWidget *) const;
    bool drawDockWidgetTitleControl(const QStyleOption *, QPainter *, const QWidget *) const;

    //*@}

    //*@name complex ontrols specialized functions
    //@{
    bool drawGroupBoxComplexControl(const QStyleOptionComplex *, QPainter *, const QWidget *) const;
    bool drawToolButtonComplexControl(const QStyleOptionComplex *, QPainter *, const QWidget *) const;
    bool drawComboBoxComplexControl(const QStyleOptionComplex *, QPainter *, const QWidget *) const;
    bool drawSpinBoxComplexControl(const QStyleOptionComplex *, QPainter *, const QWidget *) const;
    bool drawSliderComplexControl(const QStyleOptionComplex *, QPainter *, const QWidget *) const;
    bool drawDialComplexControl(const QStyleOptionComplex *, QPainter *, const QWidget *) const;
    bool drawScrollBarComplexControl(const QStyleOptionComplex *, QPainter *, const QWidget *) const;
    bool drawTitleBarComplexControl(const QStyleOptionComplex *, QPainter *, const QWidget *) const;
    //@}

    //!*@name various utility functions
    //@{

    //* spinbox arrows
    void renderSpinBoxArrow(const SubControl &, const QStyleOptionSpinBox *, QPainter *, const QWidget *) const;

    //* return dial angle based on option and value
    qreal dialAngle(const QStyleOptionSlider *, int) const;

    //* returns relevant scrollbar parent
    /** needed to detect parent focus */
    const QWidget *scrollBarParent(const QWidget *) const;

    //* returns true if given scrollbar arrow is animated
    QColor scrollBarArrowColor(const QStyleOptionSlider *, const SubControl &, const QWidget *) const;

    //* scrollbar buttons
    enum ScrollBarButtonType {
        NoButton,
        SingleButton,
        DoubleButton,
    };

    //* returns height for scrollbar buttons depending of button types
    int scrollBarButtonHeight(const ScrollBarButtonType &type) const
    {
        switch (type) {
        case NoButton:
            return Metrics::ScrollBar_NoButtonHeight;
        case SingleButton:
            return Metrics::ScrollBar_SingleButtonHeight;
        case DoubleButton:
            return Metrics::ScrollBar_DoubleButtonHeight;
        default:
            return 0;
        }
    }

    //@}

    //* translucent background
    void setTranslucentBackground(QWidget *) const;

    void drawToolsAreaSeparator(QPainter *painter, Helper *_helper, ToolsAreaManager *_toolsAreaManager, QMainWindow *mw);
    void drawToolsAreaBackground(QPainter *painter, Helper *_helper, ToolsAreaManager *_toolsAreaManager, QMainWindow *mw, const QRect &rect);

    //* create toolbar extension icon
    QIcon toolBarExtensionIcon(StandardPixmap, const QStyleOption *, const QWidget *) const;

    //* create title bar button icon
    QIcon titleBarButtonIcon(StandardPixmap, const QStyleOption *, const QWidget *) const;

    //* return true if option corresponds to QtQuick control
    bool isQtQuickControl(const QStyleOption *, const QWidget *) const;

    //@}

    //* adjust rect based on provided margins
    QRect insideMargin(const QRect &r, int margin) const
    {
        return insideMargin(r, margin, margin);
    }

    //* adjust rect based on provided margins
    QRect insideMargin(const QRect &r, int marginWidth, int marginHeight) const
    {
        return r.adjusted(marginWidth, marginHeight, -marginWidth, -marginHeight);
    }

    //* expand size based on margins
    QSize expandSize(const QSize &size, int margin) const
    {
        return expandSize(size, margin, margin);
    }

    //* expand size based on margins
    QSize expandSize(const QSize &size, int marginWidth, int marginHeight) const
    {
        return size + 2 * QSize(marginWidth, marginHeight);
    }

    //* returns true for vertical tabs
    bool isVerticalTab(const QStyleOptionTab *option) const
    {
        return isVerticalTab(option->shape);
    }

    bool isVerticalTab(const QTabBar::Shape &shape) const
    {
        return shape == QTabBar::RoundedEast || shape == QTabBar::RoundedWest || shape == QTabBar::TriangularEast || shape == QTabBar::TriangularWest;
    }

    //* right to left alignment handling
    using ParentStyleClass::visualRect;
    QRect visualRect(const QStyleOption *opt, const QRect &subRect) const
    {
        return ParentStyleClass::visualRect(opt->direction, opt->rect, subRect);
    }

    //* centering
    QRect centerRect(const QRect &rect, const QSize &size) const
    {
        return centerRect(rect, size.width(), size.height());
    }

    QRect centerRect(const QRect &rect, int width, int height) const
    {
        return QRect(rect.left() + (rect.width() - width) / 2, rect.top() + (rect.height() - height) / 2, width, height);
    }

    /*
    Checks whether the point is before the bound rect for bound of given orientation.
    This is needed to implement custom number of buttons in scrollbars,
    as well as proper mouse-hover
    */
    inline bool preceeds(const QPoint &, const QRect &, const QStyleOption *) const;

    //* return which arrow button is hit by point for scrollbar double buttons
    inline QStyle::SubControl scrollBarHitTest(const QRect &, const QPoint &, const QStyleOption *) const;

    //! return true if one of the widget's parent inherits requested type
    inline bool hasParent(const QWidget *, const char *) const;

    //* return true if icons should be shown in menus
    bool showIconsInMenuItems() const;

    //* return true if icons should be shown on buttons
    bool showIconsOnPushButtons() const;

    //* return true if passed widget is a menu title (KMenu::addTitle)
    bool hasAlteredBackground(const QWidget *) const;

    /*
     * return true if the widget should be highlighted with neutral color
     * if widget is null, check for the styleObject in option
     * return false if the widget has the mouse hover or is focused.
     */
    bool hasHighlightNeutral(const QObject *widget, const QStyleOption *option, bool mouseOver = false, bool focus = false) const;

    //*@name scrollbar button types (for addLine and subLine )
    //@{
    ScrollBarButtonType _addLineButtons = SingleButton;
    ScrollBarButtonType _subLineButtons = SingleButton;
    //@}

    //* focus frame
    QPointer<QFocusFrame> _focusFrame = nullptr;

    //* helper
    Helper *_helper = nullptr;

    //* shadow helper
    ShadowHelper *_shadowHelper = nullptr;

    //* animations
    Animations *_animations = nullptr;

    //* keyboard accelerators
    Mnemonics *_mnemonics = nullptr;

    //* blur helper
    BlurHelper *_blurHelper = nullptr;

    //* window manager
    WindowManager *_windowManager = nullptr;

    //* frame shadows
    FrameShadowFactory *_frameShadowFactory = nullptr;

    //* mdi window shadows
    MdiWindowShadowFactory *_mdiWindowShadowFactory = nullptr;

    //* splitter Factory, to extend splitters hit area
    SplitterFactory *_splitterFactory = nullptr;

    //* signal manager for the tools area
    ToolsAreaManager *_toolsAreaManager = nullptr;

    //* widget explorer
    WidgetExplorer *_widgetExplorer = nullptr;

    //* tabbar data
    BreezePrivate::TabBarData *_tabBarData = nullptr;

    //* icon hash
    using IconCache = QHash<StandardPixmap, QIcon>;
    IconCache _iconCache;

    //* pointer to primitive specialized function
    using StylePrimitive = std::function<bool(const Style &, const QStyleOption *, QPainter *, const QWidget *)>;
    StylePrimitive _frameFocusPrimitive;

    //* pointer to control specialized function
    using StyleControl = std::function<bool(const Style &, const QStyleOption *, QPainter *, const QWidget *)>;

    //* pointer to control specialized function
    using StyleComplexControl = std::function<bool(const Style &, const QStyleOptionComplex *, QPainter *, const QWidget *)>;

    //*@name custom elements
    //@{

    //* use Argb Drag and Drop Window
    QStyle::StyleHint SH_ArgbDndWindow;

    //! styled painting for KCapacityBar
    QStyle::ControlElement CE_CapacityBar;

    //@}
};

//_________________________________________________________________________
bool Style::preceeds(const QPoint &point, const QRect &bound, const QStyleOption *option) const
{
    if (option->state & QStyle::State_Horizontal) {
        if (option->direction == Qt::LeftToRight) {
            return point.x() < bound.right();
        } else {
            return point.x() > bound.x();
        }

    } else {
        return point.y() < bound.y();
    }
}

//_________________________________________________________________________
QStyle::SubControl Style::scrollBarHitTest(const QRect &rect, const QPoint &point, const QStyleOption *option) const
{
    if (option->state & QStyle::State_Horizontal) {
        if (option->direction == Qt::LeftToRight) {
            return point.x() < rect.center().x() ? QStyle::SC_ScrollBarSubLine : QStyle::SC_ScrollBarAddLine;
        } else {
            return point.x() > rect.center().x() ? QStyle::SC_ScrollBarSubLine : QStyle::SC_ScrollBarAddLine;
        }

    } else {
        return point.y() < rect.center().y() ? QStyle::SC_ScrollBarSubLine : QStyle::SC_ScrollBarAddLine;
    }
}

//_________________________________________________________________________
bool Style::hasParent(const QWidget *widget, const char *className) const
{
    if (!widget) {
        return false;
    }

    while ((widget = widget->parentWidget())) {
        if (widget->inherits(className)) {
            return true;
        }
    }

    return false;
}
}

#endif
