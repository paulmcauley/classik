cmake_minimum_required(VERSION 3.16)
project(klassy)
set(PROJECT_VERSION "5.0.breeze5.27.11")
set(PROJECT_VERSION_MAJOR 5)

add_definitions( -DKLASSY_VERSION="${PROJECT_VERSION}" -DKLASSY_GIT_MASTER=0)

set(KF5_MIN_VERSION "5.102.0")
set(KDE_COMPILERSETTINGS_LEVEL "5.82")
set(QT_MIN_VERSION "5.15.2")

include(GenerateExportHeader)
include(WriteBasicConfigVersionFile)
include(FeatureSummary)

find_package(ECM ${KF5_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH}  ${CMAKE_SOURCE_DIR}/cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CMakePackageConfigHelpers)
include(ECMInstallIcons)
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(KDEClangFormat)
include(KDEGitCommitHooks)

include(GtkUpdateIconCache)

option(WITH_DECORATIONS "Build Klassy window decorations for KWin" ON)

if(WIN32 OR APPLE)
    set(WITH_DECORATIONS OFF)
endif()

find_package(KF5KCMUtils ${KF5_MIN_VERSION} REQUIRED)


find_package(KDecoration2 REQUIRED)
add_subdirectory(kdecoration)
add_subdirectory(libbreezecommon)
add_subdirectory(kstyle)
add_subdirectory(icons)
add_subdirectory(colors)
add_subdirectory(layout-templates)
add_subdirectory(look-and-feel)

if(EXISTS ${CMAKE_SOURCE_DIR}/po AND IS_DIRECTORY ${CMAKE_SOURCE_DIR}/po)
    find_package(KF5I18n ${KF5_MIN_VERSION} CONFIG REQUIRED)
    ki18n_install(po)
endif()

include(ECMSetupVersion)
ecm_setup_version(${PROJECT_VERSION} VARIABLE_PREFIX BREEZE
                  PACKAGE_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/KlassyConfigVersion.cmake"
                  )

# create a Config.cmake and a ConfigVersion.cmake file and install them
set(CMAKECONFIG_INSTALL_DIR "${KDE_INSTALL_CMAKEPACKAGEDIR}/Klassy")

configure_package_config_file("${CMAKE_CURRENT_SOURCE_DIR}/KlassyConfig.cmake.in"
                                  "${CMAKE_CURRENT_BINARY_DIR}/KlassyConfig.cmake"
                                  PATH_VARS KDE_INSTALL_FULL_DATADIR
                                  INSTALL_DESTINATION  ${CMAKECONFIG_INSTALL_DIR}
                                  )

# add clang-format target for all our real source files
file(GLOB_RECURSE ALL_CLANG_FORMAT_SOURCE_FILES *.cpp *.h)
kde_clang_format(${ALL_CLANG_FORMAT_SOURCE_FILES})
kde_configure_git_pre_commit_hook(CHECKS CLANG_FORMAT)

install(FILES  "${CMAKE_CURRENT_BINARY_DIR}/KlassyConfig.cmake"
               "${CMAKE_CURRENT_BINARY_DIR}/KlassyConfigVersion.cmake"
        DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
        COMPONENT Devel
        )

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
