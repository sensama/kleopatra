project(kleopatra)

cmake_minimum_required(VERSION 2.8.12)

option(FORCE_DISABLE_KCMUTILS "Force building Kleopatra without KCMUtils. Doing this will disable configuration KCM Plugins. [default=OFF]" OFF)
option(DISABLE_KWATCHGNUPG "Don't build the kwatchgnupg tool [default=OFF]" OFF)

# Standalone build. Find / include everything neccessary.
set(KF5_VERSION "5.17.0")
set(GPGMEPP_VERSION "5.1.42")
set(KMIME_VERSION "5.1.40")
set(LIBKLEO_VERSION "5.1.41")
set(QT_REQUIRED_VERSION "5.4.0")

find_package(ECM ${KF5_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(ECMOptionalAddSubdirectory)
include(ECMInstallIcons)
include(ECMSetupVersion)
include(ECMAddTests)
include(ECMMarkNonGuiExecutable)
include(GenerateExportHeader)
include(ECMGenerateHeaders)
include(CMakePackageConfigHelpers)
include(FeatureSummary)
include(CheckFunctionExists)
include(ECMGeneratePriFile)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMAddAppIcon)
include(ECMQtDeclareLoggingCategory)

# Find KF5 packages
if (NOT FORCE_DISABLE_KCMUTILS)
  find_package(KF5KCMUtils ${KF5_VERSION} CONFIG REQUIRED)
endif()
find_package(KF5WidgetsAddons ${KF5_VERSION} CONFIG REQUIRED)
find_package(KF5ConfigWidgets ${KF5_VERSION} CONFIG REQUIRED)
find_package(KF5CoreAddons ${KF5_VERSION} CONFIG REQUIRED)
find_package(KF5Codecs ${KF5_VERSION} CONFIG REQUIRED)
find_package(KF5Config ${KF5_VERSION} CONFIG REQUIRED)
find_package(KF5I18n ${KF5_VERSION} CONFIG REQUIRED)
find_package(KF5IconThemes ${KF5_VERSION} CONFIG REQUIRED)
find_package(KF5XmlGui ${KF5_VERSION} CONFIG REQUIRED)
find_package(KF5WindowSystem ${KF5_VERSION} CONFIG REQUIRED)
find_package(KF5DocTools ${KF5_VERSION} CONFIG)

set_package_properties(KF5DocTools PROPERTIES DESCRIPTION "Documentation tools" TYPE OPTIONAL PURPOSE "Required to generate Kleopatra documentation.")

# Optional packages
if (WIN32)
  # Only a replacement available for Windows so this
  # is required on other platforms.
  find_package(KF5DBusAddons ${KF5_VERSION} CONFIG)
  set_package_properties(KF5DBusAddons PROPERTIES DESCRIPTION "Support library to work with DBus"
                         PURPOSE "DBus session integration"
                         URL "http://inqlude.org/libraries/kdbusaddons.html"
                         TYPE OPTIONAL)
else()
  find_package(KF5DBusAddons ${KF5_VERSION} CONFIG REQUIRED)
  set(_kleopatra_dbusaddons_libs KF5::DBusAddons)
endif()

set(HAVE_QDBUS ${Qt5DBus_FOUND})

# Kdepimlibs packages
find_package(KF5Libkleo ${LIBKLEO_VERSION} CONFIG REQUIRED)
find_package(KF5Mime ${KMIME_VERSION} CONFIG REQUIRED)
find_package(KF5Gpgmepp ${GPGMEPP_VERSION} CONFIG REQUIRED)

find_package(Qt5 ${QT_REQUIRED_VERSION} CONFIG REQUIRED Widgets Test Network PrintSupport)


set(HAVE_KCMUTILS ${KF5KCMUtils_FOUND})

find_package(Boost 1.34.0 REQUIRED)

find_path(Boost_TOPOLOGICAL_SORT_DIR NAMES boost/graph/topological_sort.hpp PATHS ${Boost_INCLUDE_DIRS})
if(NOT Boost_TOPOLOGICAL_SORT_DIR)
    message(FATAL_ERROR "The Boost Topological_sort header was NOT found. Should be part of Boost graph module.")
endif()

set(kleopatra_version 2.3.0)
set(kleopatra_release FALSE)

if(NOT kleopatra_release)
    if(GIT_FOUND)
        execute_process(COMMAND ${GIT_EXECUTABLE} log -1 --oneline --format=%h ${CMAKE_CURRENT_SOURCE_DIR}
            WORKING_DIRECTORY ${kdepim_SOURCE_DIR}/kleopatra
            OUTPUT_VARIABLE Kleopatra_WC_REVISION)
        string(REGEX REPLACE "\n" "" Kleopatra_WC_REVISION "${Kleopatra_WC_REVISION}")

        execute_process(COMMAND ${GIT_EXECUTABLE} log -1 --oneline --format=%ci ${CMAKE_CURRENT_SOURCE_DIR}
            WORKING_DIRECTORY ${kdepim_SOURCE_DIR}/kleopatra
            OUTPUT_VARIABLE Kleopatra_WC_LAST_CHANGED_DATE)
        string(REGEX REPLACE " [-0-9:+ ]*\n" "" Kleopatra_WC_LAST_CHANGED_DATE "${Kleopatra_WC_LAST_CHANGED_DATE}")

        set(kleopatra_version "${kleopatra_version}-git${Kleopatra_WC_REVISION} (${Kleopatra_WC_LAST_CHANGED_DATE})")
    endif()
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version-kleopatra.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/version-kleopatra.h)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules ${CMAKE_MODULE_PATH})
include (ConfigureChecks.cmake)

find_package(Gpgme REQUIRED)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config-kleopatra.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kleopatra.h)
include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${Boost_INCLUDE_DIR}
    ${GPGME_INCLUDES}
    )

if(USABLE_ASSUAN_FOUND)
    if(ASSUAN2_FOUND)
        include_directories(${ASSUAN2_INCLUDES})
    else()
        include_directories(${ASSUAN_INCLUDES})
    endif()
endif()

add_definitions(-D_ASSUAN_ONLY_GPG_ERRORS)
add_definitions(-DQT_NO_URL_CAST_FROM_STRING)

if(CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-missing-braces -Wno-parentheses -Wno-ignored-qualifiers")
endif()

kde_enable_exceptions()

add_subdirectory(pics)
add_subdirectory(src)

if(BUILD_TESTING)
    add_subdirectory(tests)
    add_subdirectory(autotests)
endif()
install( FILES kleopatra.categories DESTINATION ${KDE_INSTALL_CONFDIR} )

if(KF5DocTools_FOUND)
    add_subdirectory(doc)
endif()
