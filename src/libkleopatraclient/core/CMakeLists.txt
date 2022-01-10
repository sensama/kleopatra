# SPDX-FileCopyrightText: none
# SPDX-License-Identifier: BSD-3-Clause
add_definitions(
  -D_ASSUAN_ONLY_GPG_ERRORS
)

if(WIN32)
  set(_kleopatraclientcore_extra_SRCS ../../utils/gnupg-registry.c)
else()
  set(_kleopatraclientcore_extra_SRCS)
endif()

ecm_qt_declare_logging_category(_kleopatraclientcore_common_SRCS HEADER libkleopatraclientcore_debug.h IDENTIFIER LIBKLEOPATRACLIENTCORE_LOG CATEGORY_NAME org.kde.pim.libkleopatraclientcore
        DESCRIPTION "kleopatra (kleopatra)"
        OLD_CATEGORY_NAMES log_libkleopatraclientcore
        EXPORT KLEOPATRA
    )


add_library(kleopatraclientcore
  ${_kleopatraclientcore_extra_SRCS}
  initialization.cpp
  command.cpp
  selectcertificatecommand.cpp
  signencryptfilescommand.cpp
  decryptverifyfilescommand.cpp
  libkleopatraclientcore_debug.cpp
  ${_kleopatraclientcore_common_SRCS}
)
generate_export_header(kleopatraclientcore BASE_NAME kleopatraclientcore)

set_target_properties(kleopatraclientcore PROPERTIES
  VERSION ${libkleopatraclient_version}
  SOVERSION ${libkleopatraclient_soversion}
)
if (COMPILE_WITH_UNITY_CMAKE_SUPPORT)
    set_target_properties(kleopatraclientcore PROPERTIES UNITY_BUILD ON)
endif()


if(WIN32)
  if(ASSUAN2_FOUND)
    target_link_libraries(kleopatraclientcore
      
      ${ASSUAN2_LIBRARIES}
      ws2_32
    )
  else()
    target_link_libraries(kleopatraclientcore
      
      ${ASSUAN_VANILLA_LIBRARIES}
      ws2_32
    )
  endif()
else()
  if(ASSUAN2_FOUND)
    target_link_libraries(kleopatraclientcore
      
      ${ASSUAN2_LIBRARIES}
    )
  else()
    target_link_libraries(kleopatraclientcore
      
      ${ASSUAN_PTHREAD_LIBRARIES}
    )
  endif()
endif()

target_link_libraries(kleopatraclientcore Qt${QT_MAJOR_VERSION}::Widgets KF5::I18n Gpgmepp)

install(TARGETS kleopatraclientcore ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
