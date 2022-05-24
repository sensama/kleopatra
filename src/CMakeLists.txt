# SPDX-FileCopyrightText: none
# SPDX-License-Identifier: BSD-3-Clause
add_subdirectory(icons)
add_subdirectory(mimetypes)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

if (NOT DISABLE_KWATCHGNUPG)
    add_subdirectory(kwatchgnupg)
endif()
add_subdirectory(libkleopatraclient)
add_subdirectory(conf)
add_subdirectory(kconf_update)

if(WIN32)
  set(_kleopatra_extra_uiserver_SRCS uiserver/uiserver_win.cpp)
  set(_kleopatra_extra_SRCS
      utils/gnupg-registry.c
      selftest/registrycheck.cpp
      utils/windowsprocessdevice.cpp
      utils/userinfo_win.cpp
  )
else()
  set(_kleopatra_extra_uiserver_SRCS uiserver/uiserver_unix.cpp)
  set(_kleopatra_extra_SRCS)
endif()

set(_kleopatra_uiserver_SRCS
    uiserver/sessiondata.cpp
    uiserver/uiserver.cpp
    ${_kleopatra_extra_uiserver_SRCS}
    uiserver/assuanserverconnection.cpp
    uiserver/echocommand.cpp
    uiserver/decryptverifycommandemailbase.cpp
    uiserver/decryptverifycommandfilesbase.cpp
    uiserver/signcommand.cpp
    uiserver/signencryptfilescommand.cpp
    uiserver/prepencryptcommand.cpp
    uiserver/prepsigncommand.cpp
    uiserver/encryptcommand.cpp
    uiserver/selectcertificatecommand.cpp
    uiserver/importfilescommand.cpp
    uiserver/createchecksumscommand.cpp
    uiserver/verifychecksumscommand.cpp

    selftest/uiservercheck.cpp
  )

if(ASSUAN2_FOUND)
  include_directories(${ASSUAN2_INCLUDES})
  set(_kleopatra_uiserver_extra_libs ${ASSUAN2_LIBRARIES})
else()
  include_directories(${ASSUAN_INCLUDES})
  if(WIN32)
    set(_kleopatra_uiserver_extra_libs ${ASSUAN_VANILLA_LIBRARIES})
  else()
    set(_kleopatra_uiserver_extra_libs ${ASSUAN_PTHREAD_LIBRARIES})
  endif()
endif()

if(HAVE_GPG_ERR_SOURCE_KLEO)
  add_definitions(-DGPG_ERR_SOURCE_DEFAULT=GPG_ERR_SOURCE_KLEO)
  add_definitions(-DGPGMEPP_ERR_SOURCE_DEFAULT=GPG_ERR_SOURCE_KLEO)
else()
  add_definitions(-DGPG_ERR_SOURCE_DEFAULT=GPG_ERR_SOURCE_USER_1)
  add_definitions(-DGPGMEPP_ERR_SOURCE_DEFAULT=GPG_ERR_SOURCE_USER_1)
endif()

if(KF5IdentityManagement_FOUND AND KF5MailTransport_FOUND AND KF5MailTransportAkonadi_FOUND)
  set(_kleopatra_mail_libs
      KF5::IdentityManagement # Export OpenPGP keys using WKS
      KF5::MailTransport
      KF5::MailTransportAkonadi
  )
  add_definitions(-DMAILAKONADI_ENABLED)
endif()

ki18n_wrap_ui(_kleopatra_uiserver_SRCS crypto/gui/signingcertificateselectionwidget.ui)

set(_kleopatra_SRCS
  utils/accessibility.cpp
  utils/gui-helper.cpp
  utils/filedialog.cpp
  utils/kdpipeiodevice.cpp
  utils/headerview.cpp
  utils/scrollarea.cpp
  utils/dragqueen.cpp
  utils/multivalidator.cpp
  utils/systemtrayicon.cpp
  utils/accessibility.h
  utils/gui-helper.h
  utils/filedialog.h
  utils/kdpipeiodevice.h
  utils/headerview.h
  utils/scrollarea.h
  utils/dragqueen.h
  utils/multivalidator.h
  utils/systemtrayicon.h

  utils/path-helper.cpp
  utils/input.cpp
  utils/output.cpp
  utils/validation.cpp
  utils/wsastarter.cpp
  utils/iodevicelogger.cpp
  utils/log.cpp
  utils/action_data.cpp
  utils/types.cpp
  utils/archivedefinition.cpp
  utils/auditlog.cpp
  utils/clipboardmenu.cpp
  utils/kuniqueservice.cpp
  utils/tags.cpp
  utils/writecertassuantransaction.cpp
  utils/keyparameters.cpp
  utils/userinfo.cpp
  utils/keys.cpp
  utils/path-helper.h
  utils/input.h
  utils/output.h
  utils/validation.h
  utils/wsastarter.h
  utils/iodevicelogger.h
  utils/log.h
  utils/action_data.h
  utils/types.h
  utils/archivedefinition.h
  utils/auditlog.h
  utils/clipboardmenu.h
  utils/kuniqueservice.h
  utils/tags.h
  utils/writecertassuantransaction.h
  utils/keyparameters.h
  utils/userinfo.h
  utils/keys.h

  selftest/selftest.cpp
  selftest/enginecheck.cpp
  selftest/gpgconfcheck.cpp
  selftest/gpgagentcheck.cpp
  selftest/libkleopatrarccheck.cpp
  selftest/compliancecheck.cpp
  selftest/selftest.h
  selftest/enginecheck.h
  selftest/gpgconfcheck.h
  selftest/gpgagentcheck.h
  selftest/libkleopatrarccheck.h
  selftest/compliancecheck.h

  ${_kleopatra_extra_SRCS}

  view/errorlabel.cpp
  view/formtextinput.cpp
  view/htmllabel.cpp
  view/keylistcontroller.cpp
  view/keytreeview.cpp
  view/searchbar.cpp
  view/smartcardwidget.cpp
  view/openpgpkeycardwidget.cpp
  view/padwidget.cpp
  view/pgpcardwidget.cpp
  view/pivcardwidget.cpp
  view/p15cardwidget.cpp
  view/netkeywidget.cpp
  view/nullpinwidget.cpp
  view/tabwidget.cpp
  view/keycacheoverlay.cpp
  view/urllabel.cpp
  view/waitwidget.cpp
  view/welcomewidget.cpp
  view/errorlabel.h
  view/formtextinput.h
  view/htmllabel.h
  view/keylistcontroller.h
  view/keytreeview.h
  view/searchbar.h
  view/smartcardwidget.h
  view/openpgpkeycardwidget.h
  view/padwidget.h
  view/pgpcardwidget.h
  view/pivcardwidget.h
  view/p15cardwidget.h
  view/netkeywidget.h
  view/nullpinwidget.h
  view/tabwidget.h
  view/keycacheoverlay.h
  view/urllabel.h
  view/waitwidget.h
  view/welcomewidget.h

  dialogs/certificateselectiondialog.cpp
  dialogs/certifywidget.cpp
  dialogs/expirydialog.cpp
  dialogs/lookupcertificatesdialog.cpp
  dialogs/ownertrustdialog.cpp
  dialogs/selftestdialog.cpp
  dialogs/certifycertificatedialog.cpp
  dialogs/revokecertificationwidget.cpp
  dialogs/revokecertificationdialog.cpp
  dialogs/adduseriddialog.cpp
  dialogs/deletecertificatesdialog.cpp
  dialogs/setinitialpindialog.cpp
  dialogs/certificatedetailsdialog.cpp
  dialogs/certificatedetailswidget.cpp
  dialogs/trustchainwidget.cpp
  dialogs/weboftrustwidget.cpp
  dialogs/weboftrustdialog.cpp
  dialogs/exportdialog.cpp
  dialogs/subkeyswidget.cpp
  dialogs/gencardkeydialog.cpp
  dialogs/updatenotification.cpp
  dialogs/pivcardapplicationadministrationkeyinputdialog.cpp
  dialogs/certificatedetailsinputwidget.cpp
  dialogs/createcsrforcardkeydialog.cpp
  dialogs/groupdetailsdialog.cpp
  dialogs/editgroupdialog.cpp
  dialogs/revokekeydialog.cpp
  dialogs/nameandemailwidget.cpp
  dialogs/certificateselectiondialog.h
  dialogs/certifywidget.h
  dialogs/expirydialog.h
  dialogs/lookupcertificatesdialog.h
  dialogs/ownertrustdialog.h
  dialogs/selftestdialog.h
  dialogs/certifycertificatedialog.h
  dialogs/revokecertificationwidget.h
  dialogs/revokecertificationdialog.h
  dialogs/adduseriddialog.h
  dialogs/deletecertificatesdialog.h
  dialogs/setinitialpindialog.h
  dialogs/certificatedetailsdialog.h
  dialogs/certificatedetailswidget.h
  dialogs/trustchainwidget.h
  dialogs/weboftrustwidget.h
  dialogs/weboftrustdialog.h
  dialogs/exportdialog.h
  dialogs/subkeyswidget.h
  dialogs/gencardkeydialog.h
  dialogs/updatenotification.h
  dialogs/pivcardapplicationadministrationkeyinputdialog.h
  dialogs/certificatedetailsinputwidget.h
  dialogs/createcsrforcardkeydialog.h
  dialogs/groupdetailsdialog.h
  dialogs/editgroupdialog.h
  dialogs/revokekeydialog.h
  dialogs/nameandemailwidget.h

  crypto/controller.cpp
  crypto/certificateresolver.cpp
  crypto/sender.cpp
  crypto/recipient.cpp
  crypto/task.cpp
  crypto/taskcollection.cpp
  crypto/decryptverifytask.cpp
  crypto/decryptverifyemailcontroller.cpp
  crypto/decryptverifyfilescontroller.cpp
  crypto/autodecryptverifyfilescontroller.cpp
  crypto/encryptemailtask.cpp
  crypto/encryptemailcontroller.cpp
  crypto/newsignencryptemailcontroller.cpp
  crypto/signencrypttask.cpp
  crypto/signencryptfilescontroller.cpp
  crypto/signemailtask.cpp
  crypto/signemailcontroller.cpp
  crypto/createchecksumscontroller.cpp
  crypto/verifychecksumscontroller.cpp
  crypto/checksumsutils_p.cpp
  crypto/controller.h
  crypto/certificateresolver.h
  crypto/sender.h
  crypto/recipient.h
  crypto/task.h
  crypto/taskcollection.h
  crypto/decryptverifytask.h
  crypto/decryptverifyemailcontroller.h
  crypto/decryptverifyfilescontroller.h
  crypto/autodecryptverifyfilescontroller.h
  crypto/encryptemailtask.h
  crypto/encryptemailcontroller.h
  crypto/newsignencryptemailcontroller.h
  crypto/signencrypttask.h
  crypto/signencryptfilescontroller.h
  crypto/signemailtask.h
  crypto/signemailcontroller.h
  crypto/createchecksumscontroller.h
  crypto/verifychecksumscontroller.h
  crypto/checksumsutils_p.h

  crypto/gui/wizard.cpp
  crypto/gui/wizardpage.cpp
  crypto/gui/wizard.h
  crypto/gui/wizardpage.h

  crypto/gui/certificateselectionline.cpp
  crypto/gui/certificatelineedit.cpp
  crypto/gui/signingcertificateselectionwidget.cpp
  crypto/gui/signingcertificateselectiondialog.cpp
  crypto/gui/certificateselectionline.h
  crypto/gui/certificatelineedit.h
  crypto/gui/signingcertificateselectionwidget.h
  crypto/gui/signingcertificateselectiondialog.h

  crypto/gui/resultitemwidget.cpp
  crypto/gui/resultlistwidget.cpp
  crypto/gui/resultpage.cpp
  crypto/gui/resultitemwidget.h
  crypto/gui/resultlistwidget.h
  crypto/gui/resultpage.h

  crypto/gui/newresultpage.cpp
  crypto/gui/signencryptfileswizard.cpp
  crypto/gui/newresultpage.h
  crypto/gui/signencryptfileswizard.h

  crypto/gui/signencryptemailconflictdialog.cpp
  crypto/gui/signencryptemailconflictdialog.h

  crypto/gui/decryptverifyoperationwidget.cpp
  crypto/gui/decryptverifyfileswizard.cpp
  crypto/gui/decryptverifyfilesdialog.cpp
  crypto/gui/decryptverifyoperationwidget.h
  crypto/gui/decryptverifyfileswizard.h
  crypto/gui/decryptverifyfilesdialog.h

  crypto/gui/objectspage.cpp
  crypto/gui/resolverecipientspage.cpp
  crypto/gui/signerresolvepage.cpp
  crypto/gui/encryptemailwizard.cpp
  crypto/gui/signemailwizard.cpp
  crypto/gui/signencryptwidget.cpp
  crypto/gui/signencryptwizard.cpp
  crypto/gui/unknownrecipientwidget.cpp
  crypto/gui/objectspage.h
  crypto/gui/resolverecipientspage.h
  crypto/gui/signerresolvepage.h
  crypto/gui/encryptemailwizard.h
  crypto/gui/signemailwizard.h
  crypto/gui/signencryptwidget.h
  crypto/gui/signencryptwizard.h
  crypto/gui/unknownrecipientwidget.h

  crypto/gui/verifychecksumsdialog.cpp
  crypto/gui/verifychecksumsdialog.h

  commands/command.cpp
  commands/gnupgprocesscommand.cpp
  commands/detailscommand.cpp
  commands/exportcertificatecommand.cpp
  commands/exportgroupscommand.cpp
  commands/importcertificatescommand.cpp
  commands/importcertificatefromfilecommand.cpp
  commands/importcertificatefromclipboardcommand.cpp
  commands/importcertificatefromdatacommand.cpp
  commands/importcertificatefromkeyservercommand.cpp
  commands/lookupcertificatescommand.cpp
  commands/reloadkeyscommand.cpp
  commands/refreshcertificatecommand.cpp
  commands/refreshx509certscommand.cpp
  commands/refreshopenpgpcertscommand.cpp
  commands/deletecertificatescommand.cpp
  commands/decryptverifyfilescommand.cpp
  commands/signencryptfilescommand.cpp
  commands/signencryptfoldercommand.cpp
  commands/encryptclipboardcommand.cpp
  commands/signclipboardcommand.cpp
  commands/decryptverifyclipboardcommand.cpp
  commands/clearcrlcachecommand.cpp
  commands/dumpcrlcachecommand.cpp
  commands/dumpcertificatecommand.cpp
  commands/importcrlcommand.cpp
  commands/changeexpirycommand.cpp
  commands/changeownertrustcommand.cpp
  commands/changeroottrustcommand.cpp
  commands/changepassphrasecommand.cpp
  commands/certifycertificatecommand.cpp
  commands/revokecertificationcommand.cpp
  commands/selftestcommand.cpp
  commands/exportsecretkeycommand.cpp
  commands/exportsecretkeycommand_old.cpp
  commands/exportsecretsubkeycommand.cpp
  commands/exportopenpgpcertstoservercommand.cpp
  commands/exportopenpgpcerttoprovidercommand.cpp
  commands/adduseridcommand.cpp
  commands/newcertificatecommand.cpp
  commands/setinitialpincommand.cpp
  commands/learncardkeyscommand.cpp
  commands/checksumcreatefilescommand.cpp
  commands/checksumverifyfilescommand.cpp
  commands/exportpaperkeycommand.cpp
  commands/importpaperkeycommand.cpp
  commands/genrevokecommand.cpp
  commands/keytocardcommand.cpp
  commands/cardcommand.cpp
  commands/pivgeneratecardkeycommand.cpp
  commands/changepincommand.cpp
  commands/authenticatepivcardapplicationcommand.cpp
  commands/setpivcardapplicationadministrationkeycommand.cpp
  commands/certificatetopivcardcommand.cpp
  commands/importcertificatefrompivcardcommand.cpp
  commands/createopenpgpkeyfromcardkeyscommand.cpp
  commands/createcsrforcardkeycommand.cpp
  commands/revokekeycommand.cpp
  commands/revokeuseridcommand.cpp
  commands/command.h
  commands/gnupgprocesscommand.h
  commands/detailscommand.h
  commands/exportcertificatecommand.h
  commands/exportgroupscommand.h
  commands/importcertificatescommand.h
  commands/importcertificatefromfilecommand.h
  commands/importcertificatefromclipboardcommand.h
  commands/importcertificatefromdatacommand.h
  commands/importcertificatefromkeyservercommand.h
  commands/lookupcertificatescommand.h
  commands/reloadkeyscommand.h
  commands/refreshcertificatecommand.h
  commands/refreshx509certscommand.h
  commands/refreshopenpgpcertscommand.h
  commands/deletecertificatescommand.h
  commands/decryptverifyfilescommand.h
  commands/signencryptfilescommand.h
  commands/signencryptfoldercommand.h
  commands/encryptclipboardcommand.h
  commands/signclipboardcommand.h
  commands/decryptverifyclipboardcommand.h
  commands/clearcrlcachecommand.h
  commands/dumpcrlcachecommand.h
  commands/dumpcertificatecommand.h
  commands/importcrlcommand.h
  commands/changeexpirycommand.h
  commands/changeownertrustcommand.h
  commands/changeroottrustcommand.h
  commands/changepassphrasecommand.h
  commands/certifycertificatecommand.h
  commands/revokecertificationcommand.h
  commands/selftestcommand.h
  commands/exportsecretkeycommand.h
  commands/exportsecretkeycommand_old.h
  commands/exportsecretsubkeycommand.h
  commands/exportopenpgpcertstoservercommand.h
  commands/exportopenpgpcerttoprovidercommand.h
  commands/adduseridcommand.h
  commands/newcertificatecommand.h
  commands/setinitialpincommand.h
  commands/learncardkeyscommand.h
  commands/checksumcreatefilescommand.h
  commands/checksumverifyfilescommand.h
  commands/exportpaperkeycommand.h
  commands/importpaperkeycommand.h
  commands/genrevokecommand.h
  commands/keytocardcommand.h
  commands/cardcommand.h
  commands/pivgeneratecardkeycommand.h
  commands/changepincommand.h
  commands/authenticatepivcardapplicationcommand.h
  commands/setpivcardapplicationadministrationkeycommand.h
  commands/certificatetopivcardcommand.h
  commands/importcertificatefrompivcardcommand.h
  commands/createopenpgpkeyfromcardkeyscommand.h
  commands/createcsrforcardkeycommand.h
  commands/revokekeycommand.h
  commands/revokeuseridcommand.h

  conf/configuredialog.cpp
  conf/groupsconfigdialog.cpp
  conf/groupsconfigpage.cpp
  conf/groupsconfigwidget.cpp
  conf/configuredialog.h
  conf/groupsconfigdialog.h
  conf/groupsconfigpage.h
  conf/groupsconfigwidget.h

  newcertificatewizard/advancedsettingsdialog.cpp
  newcertificatewizard/chooseprotocolpage.cpp
  newcertificatewizard/enterdetailspage.cpp
  newcertificatewizard/keyalgo.cpp
  newcertificatewizard/keycreationpage.cpp
  newcertificatewizard/listwidget.cpp
  newcertificatewizard/newcertificatewizard.cpp
  newcertificatewizard/resultpage.cpp
  newcertificatewizard/wizardpage.cpp
  newcertificatewizard/advancedsettingsdialog_p.h
  newcertificatewizard/chooseprotocolpage_p.h
  newcertificatewizard/enterdetailspage_p.h
  newcertificatewizard/keyalgo_p.h
  newcertificatewizard/keycreationpage_p.h
  newcertificatewizard/listwidget.h
  newcertificatewizard/newcertificatewizard.h
  newcertificatewizard/resultpage_p.h
  newcertificatewizard/wizardpage_p.h

  smartcard/readerstatus.cpp
  smartcard/card.cpp
  smartcard/openpgpcard.cpp
  smartcard/netkeycard.cpp
  smartcard/pivcard.cpp
  smartcard/p15card.cpp
  smartcard/keypairinfo.cpp
  smartcard/utils.cpp
  smartcard/deviceinfowatcher.cpp
  smartcard/readerstatus.h
  smartcard/card.h
  smartcard/openpgpcard.h
  smartcard/netkeycard.h
  smartcard/pivcard.h
  smartcard/p15card.h
  smartcard/keypairinfo.h
  smartcard/utils.h
  smartcard/deviceinfowatcher.h

  accessibility/accessiblerichtextlabel.cpp
  accessibility/accessiblewidgetfactory.cpp
  accessibility/accessiblerichtextlabel_p.h
  accessibility/accessiblewidgetfactory.h

  aboutdata.cpp
  systrayicon.cpp
  kleopatraapplication.cpp
  mainwindow.cpp
  aboutdata.h
  systrayicon.h
  kleopatraapplication.h
  mainwindow.h
  main.cpp
  kleopatra.qrc
)

if(WIN32)
  configure_file (versioninfo.rc.in versioninfo.rc)
  set(_kleopatra_SRCS ${CMAKE_CURRENT_BINARY_DIR}/versioninfo.rc ${_kleopatra_SRCS})
endif()

set (_kleopatra_SRCS conf/kleopageconfigdialog.cpp ${_kleopatra_SRCS})

ecm_qt_declare_logging_category(_kleopatra_SRCS HEADER kleopatra_debug.h IDENTIFIER KLEOPATRA_LOG CATEGORY_NAME org.kde.pim.kleopatra
        DESCRIPTION "kleopatra (kleopatra)"
        OLD_CATEGORY_NAMES log_kleopatra
        EXPORT KLEOPATRA
    )


if(KLEO_MODEL_TEST)
  add_definitions(-DKLEO_MODEL_TEST)
  set(_kleopatra_SRCS ${_kleopatra_SRCS} models/modeltest.cpp)
endif()

ki18n_wrap_ui(_kleopatra_SRCS
  dialogs/ownertrustdialog.ui
  dialogs/selectchecklevelwidget.ui
  dialogs/selftestdialog.ui
  dialogs/setinitialpindialog.ui
  dialogs/trustchainwidget.ui
  dialogs/subkeyswidget.ui
  newcertificatewizard/listwidget.ui
)

kconfig_add_kcfg_files(_kleopatra_SRCS
  kcfg/tooltippreferences.kcfgc
  kcfg/emailoperationspreferences.kcfgc
  kcfg/fileoperationspreferences.kcfgc
  kcfg/smimevalidationpreferences.kcfgc
  kcfg/tagspreferences.kcfgc
  kcfg/settings.kcfgc
)

file(GLOB ICONS_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/icons/*-apps-kleopatra.png")
ecm_add_app_icon(_kleopatra_SRCS ICONS ${ICONS_SRCS})

add_executable(kleopatra_bin ${_kleopatra_SRCS} ${_kleopatra_uiserver_SRCS})

# For the ConfigureDialog & KCMs
target_link_libraries(kleopatra_bin kcm_kleopatra_static)

#if (COMPILE_WITH_UNITY_CMAKE_SUPPORT)
#    set_target_properties(kleopatra_bin PROPERTIES UNITY_BUILD ON)
#endif()
set_target_properties(kleopatra_bin PROPERTIES OUTPUT_NAME kleopatra)

if (WIN32)
  set(_kleopatra_platform_libs "secur32")
endif ()

target_link_libraries(kleopatra_bin
  Gpgmepp
  QGpgme
  ${_kleopatra_extra_libs}
  KF5::Libkleo
  KF5::Mime
  KF5::I18n
  KF5::XmlGui
  KF5::IconThemes
  KF5::WindowSystem
  KF5::CoreAddons
  KF5::ItemModels
  KF5::Crash
  ${_kleopatra_mail_libs}
  Qt${QT_MAJOR_VERSION}::Network
  Qt${QT_MAJOR_VERSION}::PrintSupport # Printing secret keys
  ${_kleopatra_uiserver_extra_libs}
  ${_kleopatra_dbusaddons_libs}
  kleopatraclientcore
  ${_kleopatra_platform_libs}
)

install(TARGETS kleopatra_bin ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

install(
  PROGRAMS data/org.kde.kleopatra.desktop data/kleopatra_import.desktop
  DESTINATION ${KDE_INSTALL_APPDIR}
)
install(FILES data/org.kde.kleopatra.appdata.xml DESTINATION ${KDE_INSTALL_METAINFODIR})
install(
  PROGRAMS data/kleopatra_signencryptfiles.desktop
        data/kleopatra_signencryptfolders.desktop
        data/kleopatra_decryptverifyfiles.desktop
        data/kleopatra_decryptverifyfolders.desktop
  DESTINATION ${KDE_INSTALL_DATADIR}/kio/servicemenus
)
