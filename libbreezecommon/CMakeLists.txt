################# dependencies #################
### Qt/KDE
find_package(Qt${QT_MAJOR_VERSION} ${QT_MIN_VERSION} REQUIRED CONFIG COMPONENTS Widgets)
find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS Config GuiAddons ConfigWidgets IconThemes I18n)
find_package(KDecoration2 REQUIRED)

################# breezestyle target #################
set(breezecommon_LIB_SRCS
    breeze.cpp
    breezeboxshadowrenderer.cpp
    colortools.cpp
    dbusupdatenotifier.cpp
    decorationbuttoncolors.cpp
    decorationcolors.cpp
    decorationexceptionlist.cpp
    geometrytools.cpp
    presetsmodel.cpp
    renderdecorationbuttonicon.cpp
    renderdecorationbuttonicon18by18.cpp
    styleklassy.cpp
    stylekite.cpp
    styleoxygen.cpp
    styletraditional.cpp
    styleredmond10.cpp
    styleredmond11.cpp
    systemicontheme.cpp
    dbusmessages.h
    setqdebug_logging.h
)

kconfig_add_kcfg_files(breezecommon_LIB_SRCS ../kdecoration/breezesettings.kcfgc)

add_library(klassycommon5 ${breezecommon_LIB_SRCS})

generate_export_header(klassycommon5
    BASE_NAME breezecommon
    EXPORT_FILE_NAME breezecommon_export.h)

target_link_libraries(klassycommon5
    PUBLIC
        Qt::Core
        Qt::Gui
        Qt::DBus

        KDecoration2::KDecoration
    PRIVATE
        KF5::GuiAddons
        KF5::ConfigCore
        KF5::ConfigGui
        KF5::ConfigWidgets
        KF5::IconThemes
        KF5::I18n)

set_target_properties(klassycommon5 PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR})

install(TARGETS klassycommon5 ${KDE_INSTALL_TARGETS_DEFAULT_ARGS} LIBRARY NAMELINK_SKIP)
