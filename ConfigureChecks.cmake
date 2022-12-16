# assuan configure checks
include(CheckFunctionExists)

if ( ASSUAN2_FOUND )
else ( ASSUAN2_FOUND )
  # TODO Clean this up with assuan 2 as hard dependency.
  message(FATAL_ERROR "At least version 2 of libassuan is required for Kleopatra.")
endif ( ASSUAN2_FOUND )

  message( STATUS "Usable assuan found for Kleopatra" )
