dnl config.m4 for extension nocheq

PHP_ARG_ENABLE([nocheq],
  [whether to enable nocheq support],
  [AS_HELP_STRING([--enable-nocheq],
    [Enable nocheq support])],
  [no])

if test "$PHP_NOCHEQ" != "no"; then
  PHP_NEW_EXTENSION(nocheq, nocheq.c, $ext_shared)
fi
