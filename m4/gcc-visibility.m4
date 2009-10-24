dnl GCC_VISIBILITY_CC
dnl checks -fvisibility=internal with the C compiler, if it exists then
dnl defines ENABLE_VISIBILITY_CC in both configure script and Makefiles
AC_DEFUN([GCC_VISIBILITY_CC],[
  AC_LANG_ASSERT(C)
  if test "X$CC" != "X"; then
    AC_CACHE_CHECK([whether ${CC} accepts -fvisibility=internal],
      visibility_cv_cc,
      [visibility_old_cflags="$CFLAGS"
       CFLAGS="$CFLAGS -fvisibility=internal"
       AC_TRY_COMPILE(,, visibility_cv_cc=yes, visibility_cv_cc=no)
       CFLAGS="$visibility_old_cflags"
      ])
    if test $visibility_cv_cc = yes; then
      AC_DEFINE([ENABLE_VISIBILITY_CC], 1, [Define if symbol visibility C support is enabled.])
    fi
	AM_CONDITIONAL([ENABLE_VISIBILITY_CC], test "x$visibility_cv_cc" = "xyes")
  fi
])
