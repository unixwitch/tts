AC_PREREQ([2.69])
AC_INIT([RT/TTS], [D.84.0], [felicity@loreley.flyingparchment.org.uk])
AC_CONFIG_SRCDIR([tts.c])
AC_CONFIG_HEADERS([config.h])

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
