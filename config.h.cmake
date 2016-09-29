#ifndef __GST_KURENTO_CORE_CONFIG_H__
#define __GST_KURENTO_CORE_CONFIG_H__

/* Version */
#cmakedefine VERSION "@VERSION@"

/* Package name*/
#cmakedefine PACKAGE "@PACKAGE@"

/* The gettext domain name */
#cmakedefine GETTEXT_PACKAGE "@GETTEXT_PACKAGE@"

/* Tests will generate files for manual check if this macro is defined */
#cmakedefine MANUAL_CHECK

/* Library installation directory */
#cmakedefine KURENTO_MODULES_DIR "@CMAKE_INSTALL_PREFIX@/@CMAKE_INSTALL_LIBDIR@/@KURENTO_MODULES_DIR_INSTALL_PREFIX@"

#cmakedefine REGEX_REPLACE_EXISTS @REGEX_REPLACE_EXISTS@

#endif /* __GST_KURENTO_CORE_CONFIG_H__ */
