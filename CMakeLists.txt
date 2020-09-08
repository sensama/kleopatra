set(kleopatra_version 3.1.11)
# The following is for Windows. Keep in line with kleopatra_version.
set(kleopatra_fileversion 3,1,11,0)
cmake_minimum_required(VERSION 3.5)
project(kleopatra VERSION ${kleopatra_version})

# The RELEASE_SERVICE_VERSION is used by Gpg4win to add the Gpg4win version
if (NOT RELEASE_SERVICE_VERSION)
    set (RELEASE_SERVICE_VERSION "20.08.2")
endif()

option(FORCE_DISABLE_KCMUTILS "Force building Kleopatra without KCMUtils. Doing this will disable configuration KCM Plugins. [default=OFF]" OFF)
option(DISABLE_KWATCHGNUPG "Don't build the kwatchgnupg tool [default=OFF]" OFF)

# Standalone build. Find / include everything necessary.
set(KF5_MIN_VERSION "5.71.0")
set(KMIME_VERSION "5.15.2")
set(LIBKLEO_VERSION "5.15.2")
set(QT_REQUIRED_VERSION "5.13.0")
set(GPGME_REQUIRED_VERSION "1.11.1")

find_package(ECM ${KF5_MIN_VERSION} CONFIG REQUIRED)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules ${CMAKE_MODULE_PATH})


include(ECMInstallIcons)
include(ECMSetupVersion)
include(ECMAddTests)

include(GenerateExportHeader)
include(ECMGenerateHeaders)

include(FeatureSummary)
include(CheckFunctionExists)
include(ECMGeneratePriFile)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDEFrameworkCompilerSettings NO_POLICY_SCOPE)
include(ECMAddAppIcon)
include(ECMQtDeclareLoggingCategory)

# Find KF5 packages
if (NOT FORCE_DISABLE_KCMUTILS)
  find_package(KF5KCMUtils ${KF5_MIN_VERSION} CONFIG REQUIRED)
endif()
find_package(KF5WidgetsAddons ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5ConfigWidgets ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5CoreAddons ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5Codecs ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5Config ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5I18n ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5IconThemes ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5ItemModels ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5XmlGui ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5WindowSystem ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5DocTools ${KF5_MIN_VERSION} CONFIG)
find_package(KF5Crash ${KF5_MIN_VERSION} REQUIRED)

set_package_properties(KF5DocTools PROPERTIES DESCRIPTION "Documentation tools" TYPE OPTIONAL PURPOSE "Required to generate Kleopatra documentation.")

# Optional packages
if (WIN32)
  # Only a replacement available for Windows so this
  # is required on other platforms.
  find_package(KF5DBusAddons ${KF5_MIN_VERSION} CONFIG)
  set_package_properties(KF5DBusAddons PROPERTIES DESCRIPTION "Support library to work with DBus"
                         PURPOSE "DBus session integration"
                         URL "https://inqlude.org/libraries/kdbusaddons.html"
                         TYPE OPTIONAL)
else()
  find_package(KF5DBusAddons ${KF5_MIN_VERSION} CONFIG REQUIRED)
  set(_kleopatra_dbusaddons_libs KF5::DBusAddons)
endif()

set(HAVE_QDBUS ${Qt5DBus_FOUND})

find_package(Gpgmepp ${GPGME_REQUIRED_VERSION} CONFIG REQUIRED)
find_package(QGpgme ${GPGME_REQUIRED_VERSION} CONFIG REQUIRED)

# Kdepimlibs packages
find_package(KF5Libkleo ${LIBKLEO_VERSION} CONFIG REQUIRED)
find_package(KF5Mime ${KMIME_VERSION} CONFIG REQUIRED)

find_package(Qt5 ${QT_REQUIRED_VERSION} CONFIG REQUIRED Widgets Test Network PrintSupport)

find_package(Assuan2 REQUIRED)

set(HAVE_KCMUTILS ${KF5KCMUtils_FOUND})
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


find_package(Boost 1.34.0 REQUIRED)

find_path(Boost_TOPOLOGICAL_SORT_DIR NAMES boost/graph/topological_sort.hpp PATHS ${Boost_INCLUDE_DIRS})
if(NOT Boost_TOPOLOGICAL_SORT_DIR)
    message(FATAL_ERROR "The Boost Topological_sort header was NOT found. Should be part of Boost graph module.")
endif()

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

include (ConfigureChecks.cmake)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config-kleopatra.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kleopatra.h)
include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${Boost_INCLUDE_DIR}
    ${ASSUAN2_INCLUDES}
    )

add_definitions(-D_ASSUAN_ONLY_GPG_ERRORS)
#add_definitions(-DQT_DISABLE_DEPRECATED_BEFORE=0x050f00)
add_definitions(-DKF_DISABLE_DEPRECATED_BEFORE_AND_AT=0x054800)
if(CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-missing-braces -Wno-parentheses -Wno-ignored-qualifiers")
endif()
add_definitions(-DQT_NO_EMIT)

kde_enable_exceptions()


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

if(KF5DocTools_FOUND)
    add_subdirectory(doc)
endif()
