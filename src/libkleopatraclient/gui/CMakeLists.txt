
add_library(kleopatraclientgui certificaterequester.cpp)
generate_export_header(kleopatraclientgui BASE_NAME kleopatraclientgui)

target_link_libraries(kleopatraclientgui
  kleopatraclientcore
  KF5::I18n
)

set_target_properties(kleopatraclientgui PROPERTIES
  VERSION ${libkleopatraclient_version}
  SOVERSION ${libkleopatraclient_soversion}
)

install(TARGETS kleopatraclientgui ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
