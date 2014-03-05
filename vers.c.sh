#! /bin/sh

vers=`sed -n '/^AC_INIT/ { s/^AC_INIT(\[\(.*\)\], \[\(.*\)\], \[\(.*\)\])/\2/; p; }' $1`
cat >vers.c <<EOF
char const *tts_version = "$vers";
EOF
