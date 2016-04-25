include_directories(${kleopatra_SOURCE_DIR}/src)

if(BUILD_libkleopatraclient)

  set(_kcm_kleopatra_libkleopatraclient_extra_SRCS
    smimevalidationconfigurationwidget.cpp
    smimevalidationconfigurationpage.cpp
    cryptooperationsconfigwidget.cpp
    cryptooperationsconfigpage.cpp
  )

  ki18n_wrap_ui(_kcm_kleopatra_libkleopatraclient_extra_SRCS
    smimevalidationconfigurationwidget.ui
  )

  kconfig_add_kcfg_files(_kcm_kleopatra_libkleopatraclient_extra_SRCS
    ${kleopatra_SOURCE_DIR}/src/kcfg/smimevalidationpreferences.kcfgc
  )

  set(_kcm_kleopatra_libkleopatraclient_extra_LIBS kleopatraclientgui)

  set(_kcm_kleopatra_libkleopatraclient_extra_install_FILES
    kleopatra_config_smimevalidation.desktop
    kleopatra_config_cryptooperations.desktop
  )

else()

  set(_kcm_kleopatra_libkleopatraclient_extra_SRCS)
  set(_kcm_kleopatra_libkleopatraclient_extra_LIBS)
  set(_kcm_kleopatra_libkleopatraclient_extra_install_FILES)

endif()

set(kcm_kleopatra_PART_SRCS
  dirservconfigpage.cpp
  appearanceconfigpage.cpp
  appearanceconfigwidget.cpp
  gnupgsystemconfigurationpage.cpp
  ${kleopatra_BINARY_DIR}/src/kleopatra_debug.cpp
  ${_kcm_kleopatra_libkleopatraclient_extra_SRCS}
)


ki18n_wrap_ui(kcm_kleopatra_PART_SRCS
  appearanceconfigwidget.ui
  smimevalidationconfigurationwidget.ui
)

kconfig_add_kcfg_files(kcm_kleopatra_PART_SRCS
  ${kleopatra_SOURCE_DIR}/src/kcfg/tooltippreferences.kcfgc
  ${kleopatra_SOURCE_DIR}/src/kcfg/emailoperationspreferences.kcfgc
  ${kleopatra_SOURCE_DIR}/src/kcfg/fileoperationspreferences.kcfgc
)

add_library(kcm_kleopatra MODULE ${kcm_kleopatra_PART_SRCS})

if(HAVE_KCMUTILS)
  set (_kcm_kleopatra_extra_libs KF5::KCMUtils)
endif()

target_link_libraries(kcm_kleopatra
  KF5::Libkleo
  KF5::IconThemes
  KF5::I18n
  KF5::WidgetsAddons
  KF5::ConfigWidgets
  ${_kcm_kleopatra_extra_libs}
  ${_kleopatra_dbusaddons_libs}
  ${_kcm_kleopatra_libkleopatraclient_extra_LIBS}
)

install(TARGETS kcm_kleopatra DESTINATION ${KDE_INSTALL_PLUGINDIR})

########### install files ###############

install(FILES
  kleopatra_config_dirserv.desktop
  kleopatra_config_appear.desktop
  kleopatra_config_gnupgsystem.desktop
  ${_kcm_kleopatra_libkleopatraclient_extra_install_FILES}
  DESTINATION ${KDE_INSTALL_KSERVICES5DIR}
)

