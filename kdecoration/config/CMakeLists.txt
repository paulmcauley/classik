### config classes
set(breezedecoration_config_SRCS
    breezeconfigwidget.cpp
    breezedetectwidget.cpp
    breezeexceptiondialog.cpp
    breezeexceptionlistwidget.cpp
    breezeexceptionmodel.cpp
    breezeitemmodel.cpp
    buttonsizing.cpp
    windowoutlinestyle.cpp
    loadpreset.cpp
    addpreset.cpp
    buttoncolors.cpp
    buttonbehaviour.cpp
    systemicongeneration.cpp
    titlebarspacing.cpp
    titlebaropacity.cpp
    shadowstyle.cpp
)

ki18n_wrap_ui(breezedecoration_config_SRCS
   ui/breezeconfigurationui.ui
   ui/breezeexceptiondialog.ui
   ui/breezeexceptionlistwidget.ui
   ui/buttonsizing.ui
   ui/windowoutlinestyle.ui
   ui/loadpreset.ui
   ui/addpreset.ui
   ui/buttoncolors.ui
   ui/buttonbehaviour.ui
   ui/systemicongeneration.ui
   ui/titlebarspacing.ui
   ui/titlebaropacity.ui
   ui/shadowstyle.ui
)

qt6_add_resources(breezedecoration_config_SRCS ui/klassydecoration_config.qrc)

kcoreaddons_add_plugin(kcm_klassydecoration SOURCES kcm_breezedecoration.cpp ${breezedecoration_config_SRCS} INSTALL_NAMESPACE "${KDECORATION_KCM_PLUGIN_DIR}")
#This is removed as kdecoration InternalSettings is now generated from libbreezecommon
#kconfig_add_kcfg_files(klassydecoration_STATIC breezesettings.kcfgc)
target_include_directories(kcm_klassydecoration PRIVATE ${CMAKE_SOURCE_DIR}/kdecoration ${CMAKE_BINARY_DIR}/kdecoration/)
target_link_libraries(kcm_klassydecoration
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::DBus
        Qt6::Widgets
        KDecoration2::KDecoration
    PRIVATE
        KF6::ConfigCore
        KF6::CoreAddons
        KF6::GuiAddons
        KF6::I18n
        KF6::IconThemes
        KF6::WindowSystem
        KF6::KCMUtils
        klassycommon6
)

kcmutils_generate_desktop_file(kcm_klassydecoration)

#install bundled presets
install(DIRECTORY presets  DESTINATION  ${KDE_INSTALL_QTPLUGINDIR}/org.kde.kdecoration2.kcm/klassydecoration)
