# assuan configure checks
include(CheckFunctionExists)

if ( ASSUAN2_FOUND )
else ( ASSUAN2_FOUND )
  # TODO Clean this up with assuan 2 as hard dependency.
  message(FATAL_ERROR "At least version 2 of libassuan is required for Kleopatra.")
endif ( ASSUAN2_FOUND )

set( USABLE_ASSUAN_FOUND false )


  set( CMAKE_REQUIRED_INCLUDES ${ASSUAN2_INCLUDES} )

    set( CMAKE_REQUIRED_LIBRARIES ${ASSUAN2_LIBRARIES} )
    set( USABLE_ASSUAN_FOUND true )

  # TODO: this workaround will be removed as soon as we find better solution
  if(MINGW)
    set(CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES} ${KDEWIN32_INCLUDE_DIR}/mingw)
  elseif(MSVC)
    set(CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES} ${KDEWIN32_INCLUDE_DIR}/msvc)
  endif(MINGW)

if ( USABLE_ASSUAN_FOUND )
  # check if assuan.h can be compiled standalone (it couldn't, on
  # Windows, until recently, because of a HAVE_W32_SYSTEM #ifdef in
  # there)
  check_cxx_source_compiles( "
       #include <assuan.h>
       int main() {
           return 1;
       }
       "
       USABLE_ASSUAN_FOUND )
endif( USABLE_ASSUAN_FOUND )

if ( USABLE_ASSUAN_FOUND )

  # check whether assuan and gpgme may be linked to simultaneously
  check_function_exists( "assuan_get_pointer" USABLE_ASSUAN_FOUND )

endif( USABLE_ASSUAN_FOUND )

if ( USABLE_ASSUAN_FOUND )

  # check if gpg-error already has GPG_ERR_SOURCE_KLEO
  check_cxx_source_compiles("
        #include <gpg-error.h>
        static gpg_err_source_t src = GPG_ERR_SOURCE_KLEO;
        int main() { return 0; }
        "
    HAVE_GPG_ERR_SOURCE_KLEO )

endif ( USABLE_ASSUAN_FOUND )

if ( USABLE_ASSUAN_FOUND )
  message( STATUS "Usable assuan found for Kleopatra" )
else ( USABLE_ASSUAN_FOUND )
  message( STATUS "NO usable assuan found for Kleopatra" )
endif ( USABLE_ASSUAN_FOUND )

OPTION( BUILD_libkleopatraclient "Build directory kleopatra/libkleopatraclient" ${USABLE_ASSUAN_FOUND} )

if ( NOT USABLE_ASSUAN_FOUND )
  set( BUILD_libkleopatraclient false )
endif ( NOT USABLE_ASSUAN_FOUND )

if (USABLE_ASSUAN_FOUND)
  set (HAVE_USABLE_ASSUAN 1)
  set (HAVE_KLEOPATRACLIENT_LIBRARY 1)
else()
  set (HAVE_USABLE_ASSUAN 0)
  set (HAVE_KLEOPATRACLIENT_LIBRARY 0)
endif()

set(CMAKE_REQUIRED_INCLUDES)
set(CMAKE_REQUIRED_LIBRARIES)
