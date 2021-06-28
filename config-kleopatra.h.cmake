/* Define to 1 if you have a recent enough libassuan */
#cmakedefine HAVE_USABLE_ASSUAN 1

/* Define to 1 if you have libassuan v2 */
#cmakedefine HAVE_ASSUAN2 1

#ifndef HAVE_ASSUAN2
/* Define to 1 if your libassuan has the assuan_fd_t type  */
#cmakedefine HAVE_ASSUAN_FD_T 1

/* Define to 1 if your libassuan has the assuan_inquire_ext function */
#cmakedefine HAVE_ASSUAN_INQUIRE_EXT 1

/* Define to 1 if your assuan_inquire_ext puts the buffer arguments into the callback signature */
#cmakedefine HAVE_NEW_STYLE_ASSUAN_INQUIRE_EXT 1

/* Define to 1 if your libassuan has the assuan_sock_get_nonce function */
#cmakedefine HAVE_ASSUAN_SOCK_GET_NONCE 1

#endif
/* Define to 1 if you build libkleopatraclient */
#cmakedefine HAVE_KLEOPATRACLIENT_LIBRARY 1

/* DBus available */
#cmakedefine01 HAVE_QDBUS

/* Defined if GpgME++ supports trust signatures */
#cmakedefine GPGMEPP_SUPPORTS_TRUST_SIGNATURES 1

/* Defined if QGpgME supports trust signatures */
#cmakedefine QGPGME_SUPPORTS_TRUST_SIGNATURES 1

/* Defined if QGpgME supports setting an expiration date for signatures */
#cmakedefine QGPGME_SUPPORTS_SIGNATURE_EXPIRATION 1

/* Defined if QGpgME supports changing the expiration date of the primary key and the subkeys simultaneously */
#cmakedefine QGPGME_SUPPORTS_CHANGING_EXPIRATION_OF_COMPLETE_KEY 1
