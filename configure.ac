AC_PREREQ([2.69])
AC_INIT([RT/TTS], [D.84.0], [felicity@loreley.flyingparchment.org.uk])
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
oLIBS="$LIBS"
LIBS="$LIBS $CURSES_LIB"
AC_CHECK_FUNCS([use_default_colors])
LIBS="$oLIBS"

AC_CHECK_HEADERS([IOKit/pwr_mgt/IOPMLib.h])

AC_CHECK_LIB(m, round)

oLDFLAGS="$LDFLAGS"
LDFLAGS="$LDFLAGS -framework IOKit"
AC_CHECK_FUNCS([IORegisterForSystemPower], [], [LDFLAGS="$oLDFLAGS"])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
