# SPDX-FileCopyrightText: none
# SPDX-License-Identifier: BSD-3-Clause
# always install the media types
install(FILES application-vnd-kde-kleopatra.xml DESTINATION ${KDE_INSTALL_MIMEDIR})

# require at least version 1.3 (same as for KF5)
find_package(SharedMimeInfo 1.3)
set_package_properties(SharedMimeInfo PROPERTIES
                       TYPE OPTIONAL
                       PURPOSE "Enables import of exported certificate groups with Kleopatra"
                      )

# update XDG mime-types if shared mime info is around
if(SharedMimeInfo_FOUND AND NOT CMAKE_CROSSCOMPILING)
    update_xdg_mimetypes(${KDE_INSTALL_MIMEDIR})
endif()
