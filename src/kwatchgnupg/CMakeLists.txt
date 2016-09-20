
find_package(KF5Notifications ${KF5_VERSION} CONFIG REQUIRED)
find_package(KF5TextWidgets ${KF5_VERSION} CONFIG REQUIRED)

set(kwatchgnupg_SRCS
  ../utils/kdlogtextwidget.cpp
  ../utils/gnupg-helper.cpp
  ../utils/hex.cpp
  ../utils/kuniqueservice.cpp
  ../kleopatra_debug.cpp
  kwatchgnupgmainwin.cpp
  kwatchgnupgconfig.cpp
  aboutdata.cpp
  tray.cpp
  main.cpp
)

ecm_qt_declare_logging_category(kwatchgnupg_SRCS HEADER kwatchgnupg_debug.h IDENTIFIER KWATCHGNUPG_LOG CATEGORY_NAME org.kde.pim.kwatchgnupg)
qt5_add_resources(kwatchgnupg_SRCS kwatchgnupg.qrc)

if(WIN32)
  set(kwatchgnupg_SRCS ${kwatchgnupg_SRCS} ../utils/gnupg-registry.c)
endif()

add_executable(kwatchgnupg ${kwatchgnupg_SRCS})


target_link_libraries(kwatchgnupg
  KF5::DBusAddons  
  KF5::XmlGui
  KF5::Notifications
  KF5::TextWidgets
  KF5::IconThemes
  KF5::Libkleo
)

install(TARGETS kwatchgnupg ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

install(FILES images/kwatchgnupg.png images/kwatchgnupg2.png DESTINATION ${KDE_INSTALL_DATADIR}/kwatchgnupg/pics)
