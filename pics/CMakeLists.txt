if (WIN32)
  set(EXTRAWINFILES gpg4win-compact.png)
endif()

install(FILES
  kleopatra_wizard.png
  kleopatra_wizard.svgz
  kleopatra_splashscreen.png
  kleopatra_splashscreen.svgz
  gpg4win-compact.png
  DESTINATION ${KDE_INSTALL_DATADIR}/kleopatra/pics
)
