# SPDX-FileCopyrightText: none
# SPDX-License-Identifier: BSD-3-Clause
cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

set(RELEASE_SERVICE_VERSION_MAJOR "22")
set(RELEASE_SERVICE_VERSION_MINOR "03")
set(RELEASE_SERVICE_VERSION_MICRO "70")

# The RELEASE_SERVICE_VERSION is used by Gpg4win to add the Gpg4win version
if (NOT RELEASE_SERVICE_VERSION)
    set(RELEASE_SERVICE_VERSION "${RELEASE_SERVICE_VERSION_MAJOR}.${RELEASE_SERVICE_VERSION_MINOR}.${RELEASE_SERVICE_VERSION_MICRO}")
endif()
if(RELEASE_SERVICE_VERSION_MICRO LESS 10)
    set(KDE_APPLICATIONS_COMPACT_VERSION "${RELEASE_SERVICE_VERSION_MAJOR}${RELEASE_SERVICE_VERSION_MINOR}0${RELEASE_SERVICE_VERSION_MICRO}")
else()
    set(KDE_APPLICATIONS_COMPACT_VERSION "${RELEASE_SERVICE_VERSION_MAJOR}${RELEASE_SERVICE_VERSION_MINOR}${RELEASE_SERVICE_VERSION_MICRO}")
endif()

set(KLEOPATRA_VERSION_MAJOR "3")
set(KLEOPATRA_VERSION_MINOR "1")
set(KLEOPATRA_VERSION_MICRO "21")

set(kleopatra_version "${KLEOPATRA_VERSION_MAJOR}.${KLEOPATRA_VERSION_MINOR}.${KLEOPATRA_VERSION_MICRO}.${KDE_APPLICATIONS_COMPACT_VERSION}")
# The following is for Windows
set(kleopatra_version_win "${KLEOPATRA_VERSION_MAJOR}.${KLEOPATRA_VERSION_MINOR}.${KLEOPATRA_VERSION_MICRO}")
set(kleopatra_fileversion_win "${KLEOPATRA_VERSION_MAJOR},${KLEOPATRA_VERSION_MINOR},${KLEOPATRA_VERSION_MICRO},0")

project(kleopatra VERSION ${kleopatra_version})

option(DISABLE_KWATCHGNUPG "Don't build the kwatchgnupg tool [default=OFF]" OFF)

# Standalone build. Find / include everything necessary.
set(KF5_MIN_VERSION "5.90.0")
set(KMIME_VERSION "5.19.40")
set(LIBKLEO_VERSION "5.19.50")
set(QT_REQUIRED_VERSION "5.15.2")
set(GPGME_REQUIRED_VERSION "1.15.0")
set(BOOST_REQUIRED_VERSION "1.58")

if (WIN32)
  set(KF5_WANT_VERSION "5.70.0")
  set(KMIME_WANT_VERSION "5.12.0")
else ()
  set(KF5_WANT_VERSION ${KF5_MIN_VERSION})
  set(KMIME_WANT_VERSION ${KMIME_VERSION})
endif ()

find_package(ECM ${KF5_WANT_VERSION} CONFIG REQUIRED)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules ${CMAKE_MODULE_PATH})

include(ECMInstallIcons)
include(ECMSetupVersion)
include(ECMAddTests)
include(GenerateExportHeader)
include(ECMGenerateHeaders)
include(FeatureSummary)
include(CheckFunctionExists)
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMAddAppIcon)
include(ECMQtDeclareLoggingCategory)

# Find KF5 packages
find_package(KF5WidgetsAddons ${KF5_WANT_VERSION} CONFIG REQUIRED)
find_package(KF5ConfigWidgets ${KF5_WANT_VERSION} CONFIG REQUIRED)
find_package(KF5CoreAddons ${KF5_WANT_VERSION} CONFIG REQUIRED)
find_package(KF5Codecs ${KF5_WANT_VERSION} CONFIG REQUIRED)
find_package(KF5Config ${KF5_WANT_VERSION} CONFIG REQUIRED)
find_package(KF5I18n ${KF5_WANT_VERSION} CONFIG REQUIRED)
find_package(KF5IconThemes ${KF5_WANT_VERSION} CONFIG REQUIRED)
find_package(KF5ItemModels ${KF5_WANT_VERSION} CONFIG REQUIRED)
find_package(KF5XmlGui ${KF5_WANT_VERSION} CONFIG REQUIRED)
find_package(KF5WindowSystem ${KF5_WANT_VERSION} CONFIG REQUIRED)
find_package(KF5DocTools ${KF5_WANT_VERSION} CONFIG)
find_package(KF5Crash ${KF5_WANT_VERSION} REQUIRED)

set_package_properties(KF5DocTools PROPERTIES
    DESCRIPTION "Documentation tools"
    PURPOSE "Required to generate Kleopatra documentation."
    TYPE OPTIONAL)

# Optional packages
if (WIN32)
  # Only a replacement available for Windows so this
  # is required on other platforms.
  find_package(KF5DBusAddons ${KF5_WANT_VERSION} CONFIG)
  set_package_properties(KF5DBusAddons PROPERTIES DESCRIPTION "Support library to work with DBus"
                         PURPOSE "DBus session integration"
                         URL "https://inqlude.org/libraries/kdbusaddons.html"
                         TYPE OPTIONAL)
else()
  find_package(KF5DBusAddons ${KF5_WANT_VERSION} CONFIG REQUIRED)
  set(_kleopatra_dbusaddons_libs KF5::DBusAddons)
endif()

set(HAVE_QDBUS ${Qt${QT_MAJOR_VERSION}DBus_FOUND})

find_package(Gpgmepp ${GPGME_REQUIRED_VERSION} CONFIG REQUIRED)
if (Gpgmepp_VERSION VERSION_GREATER_EQUAL "1.16.0")
    set(GPGMEPP_SUPPORTS_TRUST_SIGNATURES 1)
endif()
find_package(QGpgme ${GPGME_REQUIRED_VERSION} CONFIG REQUIRED)
if (QGpgme_VERSION VERSION_GREATER_EQUAL "1.16.0")
    set(QGPGME_SUPPORTS_TRUST_SIGNATURES 1)
    set(QGPGME_SUPPORTS_SIGNATURE_EXPIRATION 1)
endif()
if (QGpgme_VERSION VERSION_GREATER_EQUAL "1.16.1")
    set(QGPGME_SUPPORTS_CHANGING_EXPIRATION_OF_COMPLETE_KEY 1)
    set(QGPGME_CRYPTOCONFIGENTRY_HAS_DEFAULT_VALUE 1)
    set(QGPGME_SUPPORTS_WKDLOOKUP 1)
    set(QGPGME_SUPPORTS_IMPORT_WITH_FILTER 1)
    set(QGPGME_SUPPORTS_IMPORT_WITH_KEY_ORIGIN 1)
    set(QGPGME_SUPPORTS_SECRET_KEY_EXPORT 1)
    set(QGPGME_SUPPORTS_SECRET_SUBKEY_EXPORT 1)
    set(QGPGME_SUPPORTS_RECEIVING_KEYS_BY_KEY_ID 1)
endif()

# Kdepimlibs packages
find_package(KF5Libkleo ${LIBKLEO_VERSION} CONFIG REQUIRED)
find_package(KF5Mime ${KMIME_WANT_VERSION} CONFIG REQUIRED)

find_package(Qt${QT_MAJOR_VERSION} ${QT_REQUIRED_VERSION} CONFIG REQUIRED Widgets Test Network PrintSupport)

find_package(Assuan2 REQUIRED)


find_package(Boost ${BOOST_REQUIRED_VERSION} MODULE REQUIRED)

find_path(Boost_TOPOLOGICAL_SORT_DIR NAMES boost/graph/topological_sort.hpp PATHS ${Boost_INCLUDE_DIRS})
if(NOT Boost_TOPOLOGICAL_SORT_DIR)
    message(FATAL_ERROR "The Boost Topological_sort header was NOT found. Should be part of Boost graph module.")
endif()

set(kleopatra_release FALSE)

if(NOT kleopatra_release)
    find_package(Git)
    if(GIT_FOUND)
        execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse
                        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                        RESULT_VARIABLE rc
                        ERROR_QUIET)
        if(rc EQUAL 0)
            execute_process(COMMAND ${GIT_EXECUTABLE} log -1 --oneline --format=%h ${CMAKE_CURRENT_SOURCE_DIR}
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                OUTPUT_VARIABLE Kleopatra_WC_REVISION)
            string(REGEX REPLACE "\n" "" Kleopatra_WC_REVISION "${Kleopatra_WC_REVISION}")

            execute_process(COMMAND ${GIT_EXECUTABLE} log -1 --oneline --format=%cI ${CMAKE_CURRENT_SOURCE_DIR}
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                OUTPUT_VARIABLE Kleopatra_WC_LAST_CHANGED_DATE)
            string(REGEX REPLACE "^([0-9]+)-([0-9]+)-([0-9]+)T([0-9]+):([0-9]+):([0-9]+).*$" "\\1\\2\\3T\\4\\5\\6"
                   Kleopatra_WC_LAST_CHANGED_DATE "${Kleopatra_WC_LAST_CHANGED_DATE}")

            set(kleopatra_version "${kleopatra_version}+git${Kleopatra_WC_LAST_CHANGED_DATE}~${Kleopatra_WC_REVISION}")
        endif()
    endif()
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version-kleopatra.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/version-kleopatra.h)

include (ConfigureChecks.cmake)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config-kleopatra.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kleopatra.h)
include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${Boost_INCLUDE_DIRS}
    ${ASSUAN2_INCLUDES}
    )

add_definitions(-D_ASSUAN_ONLY_GPG_ERRORS)
add_definitions(-DQT_DISABLE_DEPRECATED_BEFORE=0x050e00)
add_definitions(-DKF_DISABLE_DEPRECATED_BEFORE_AND_AT=0x055B00)
if(CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-missing-braces -Wno-parentheses -Wno-ignored-qualifiers")
endif()
add_definitions(-DQT_NO_EMIT)
remove_definitions(-DQT_NO_FOREACH)

# Disable the use of QStringBuilder for operator+ to prevent crashes when
# returning the result of concatenating string temporaries in lambdas. We do
# this for example in some std::transform expressions.
# This is a known issue: https://bugreports.qt.io/browse/QTBUG-47066
# Alternatively, one would always have to remember to force the lambdas to
# return a QString instead of QStringBuilder, but that's just too easy to
# forget and, unfortunately, the compiler doesn't issue a warning if one forgets
# this. So, it's just too dangerous.
# One can still use QStringBuilder explicitly with the operator% if necessary.
remove_definitions(-DQT_USE_FAST_OPERATOR_PLUS)
remove_definitions(-DQT_USE_QSTRINGBUILDER)

kde_enable_exceptions()
option(USE_UNITY_CMAKE_SUPPORT "Use UNITY cmake support (speedup compile time)" OFF)

set(COMPILE_WITH_UNITY_CMAKE_SUPPORT OFF)
if (USE_UNITY_CMAKE_SUPPORT)
    set(COMPILE_WITH_UNITY_CMAKE_SUPPORT ON)
endif()


add_subdirectory(pics)
add_subdirectory(src)

if(BUILD_TESTING)
    add_subdirectory(tests)
    add_subdirectory(autotests)
endif()

ecm_qt_install_logging_categories(
        EXPORT KLEOPATRA
        FILE kleopatra.categories
        DESTINATION ${KDE_INSTALL_LOGGINGCATEGORIESDIR}
        )

ki18n_install(po)
if(KF5DocTools_FOUND)
    kdoctools_install(po)
    add_subdirectory(doc)
endif()
feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)

