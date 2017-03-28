if (WIN32)
  set(EXTRAWINFILES gpg4win-compact.png gpg4win.png)
endif()

install(FILES
  kleopatra_wizard.png
  kleopatra_wizard.svgz
  kleopatra_splashscreen.png
  kleopatra_splashscreen.svgz
  ${EXTRAWINFILES}
  DESTINATION ${KDE_INSTALL_DATADIR}/kleopatra/pics
)
