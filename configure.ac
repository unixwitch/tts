AC_PREREQ([2.69])
AC_INIT([TTS], [D.84.1], [ft@le-fay.org])
AC_CONFIG_SRCDIR([tts.c])
AC_CONFIG_HEADERS([config.h])

AC_CANONICAL_HOST

case $host_os in
	darwin*)
		CPPFLAGS="$CPPFLAGS -D_DARWIN_USE_64_BIT_INODE=1"
		;;
	linux*)
		CPPFLAGS="$CPPFLAGS -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64"
		;;
	netbsd*)
		CPPFLAGS="$CPPFLAGS -D_NETBSD_SOURCE"
		;;
	solaris*)
		CPPFLAGS="-D__EXTENSIONS__ -D_FILE_OFFSET_BITS=64"
		;;
esac

AC_PROG_CC
AC_PROG_INSTALL

AX_WITH_CURSES
AS_IF([test "x$ax_cv_curses" != xyes], [
	AC_ERROR([XSI/SVR4 curses is required to compile TTS])
])
oLIBS="$LIBS"
LIBS="$LIBS $CURSES_LIB"
AC_CHECK_FUNCS([use_default_colors wcslcpy wcslcat])
LIBS="$oLIBS"

AC_CHECK_HEADERS([IOKit/pwr_mgt/IOPMLib.h])

AC_CHECK_LIB(m, round)

oLDFLAGS="$LDFLAGS"
LDFLAGS="$LDFLAGS -framework IOKit"
AC_CHECK_FUNCS([IORegisterForSystemPower], [], [LDFLAGS="$oLDFLAGS"])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
