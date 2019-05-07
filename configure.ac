AC_INIT([occi11)], [1.0])
AM_INIT_AUTOMAKE([subdir-objects])

AC_PROG_CXX
AC_CONFIG_FILES([Makefile])

AC_ARG_WITH(
	[occi],
	[AS_HELP_STRING(
		[--with-occi],
		[Oracle SDK include directory (default is /usr/include/oracle/12.1/client64)])
	],
	[],
	[with_occi=/usr/include/oracle/12.1/client64]
)
AC_CHECK_FILE(["${with_occi}/occi.h"],,[AC_MSG_ERROR([occi.h was not found on your system])])
AC_SUBST(with_occi)

AC_ARG_WITH(
	[occilib],
	[AS_HELP_STRING(
		[--with-occilib],
		[Oracle SDK library directory (default is /usr/lib/oracle/12.1/client64/lib)])
	],
	[],
	[with_occilib=/usr/lib/oracle/12.1/client64/lib]
)
AC_CHECK_FILE(["${with_occilib}/libocci.so"],,[AC_MSG_ERROR([libocci.so was not found on your system])])
AC_SUBST(with_occilib)

AC_OUTPUT