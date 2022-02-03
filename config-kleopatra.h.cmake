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

/* Defined if QGpgME supports retrieving the default value of a config entry */
#cmakedefine QGPGME_CRYPTOCONFIGENTRY_HAS_DEFAULT_VALUE 1

/* Defined if QGpgME supports WKD lookup */
#cmakedefine QGPGME_SUPPORTS_WKDLOOKUP 1

/* Defined if QGpgME supports specifying an import filter when importing keys */
#cmakedefine QGPGME_SUPPORTS_IMPORT_WITH_FILTER 1

/* Defined if QGpgME supports setting key origin when importing keys */
#cmakedefine QGPGME_SUPPORTS_IMPORT_WITH_KEY_ORIGIN 1

/* Defined if QGpgME supports the export of secret keys */
#cmakedefine QGPGME_SUPPORTS_SECRET_KEY_EXPORT 1

/* Defined if QGpgME supports the export of secret subkeys */
#cmakedefine QGPGME_SUPPORTS_SECRET_SUBKEY_EXPORT 1

/* Defined if QGpgME supports receiving keys by their key ids */
#cmakedefine QGPGME_SUPPORTS_RECEIVING_KEYS_BY_KEY_ID 1
