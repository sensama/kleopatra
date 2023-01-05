/* DBus available */
#cmakedefine01 HAVE_QDBUS

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

/* Defined if QGpgME supports revoking own OpenPGP keys */
#cmakedefine QGPGME_SUPPORTS_KEY_REVOCATION 1

/* Defined if QGpgME supports refreshing keys */
#cmakedefine QGPGME_SUPPORTS_KEY_REFRESH 1

/* Defined if QGpgME supports setting the file name of encrypted data */
#cmakedefine QGPGME_SUPPORTS_SET_FILENAME 1

/* Defined if QGpgME supports setting the primary user id of a key */
#cmakedefine QGPGME_SUPPORTS_SET_PRIMARY_UID 1

/* Defined if GpgME++ supports setting the curve when generating ECC card keys */
#cmakedefine GPGMEPP_SUPPORTS_SET_CURVE 1

/* Defined if QGpgME supports deferred start of ImportJob */
#cmakedefine01 QGPGME_SUPPORTS_DEFERRED_IMPORT_JOB
