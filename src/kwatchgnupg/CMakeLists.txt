# SPDX-FileCopyrightText: none
# SPDX-License-Identifier: BSD-3-Clause
set(kwatchgnupg_version 1.0)

find_package(KF${KF_MAJOR_VERSION}Notifications ${KF_MIN_VERSION} CONFIG REQUIRED)
find_package(KF${KF_MAJOR_VERSION}TextWidgets ${KF_MIN_VERSION} CONFIG REQUIRED)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version-kwatchgnupg.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/version-kwatchgnupg.h)

set(kwatchgnupg_SRCS
  ../utils/kuniqueservice.cpp
  ../kleopatra_debug.cpp
  kwatchgnupgmainwin.cpp
  kwatchgnupgconfig.cpp
  aboutdata.cpp
  tray.cpp
  ../utils/kuniqueservice.h
  ../kleopatra_debug.h
  kwatchgnupgmainwin.h
  kwatchgnupgconfig.h
  aboutdata.h
  tray.h
  main.cpp
  kwatchgnupg.qrc
)

ecm_qt_declare_logging_category(kwatchgnupg_SRCS HEADER kwatchgnupg_debug.h IDENTIFIER KWATCHGNUPG_LOG CATEGORY_NAME org.kde.pim.kwatchgnupg
        DESCRIPTION "kwatchgnupg (kwatchgnupg)"
        OLD_CATEGORY_NAMES log_kwatchgnupg
        EXPORT KLEOPATRA
    )

if(WIN32)
  set(kwatchgnupg_SRCS ${kwatchgnupg_SRCS} ../utils/gnupg-registry.c)
endif()

add_executable(kwatchgnupg ${kwatchgnupg_SRCS})
if (COMPILE_WITH_UNITY_CMAKE_SUPPORT)
    set_target_properties(kwatchgnupg PROPERTIES UNITY_BUILD ON)
endif()


target_link_libraries(kwatchgnupg
  KF${KF_MAJOR_VERSION}::DBusAddons
  KF${KF_MAJOR_VERSION}::XmlGui
  KF${KF_MAJOR_VERSION}::Notifications
  KF${KF_MAJOR_VERSION}::TextWidgets
  KF${KF_MAJOR_VERSION}::IconThemes
  KF5::Libkleo
  KF${KF_MAJOR_VERSION}::Crash
)

install(TARGETS kwatchgnupg ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

install(FILES images/kwatchgnupg.png images/kwatchgnupg2.png DESTINATION ${KDE_INSTALL_DATADIR}/kwatchgnupg/pics)
