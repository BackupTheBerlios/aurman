dnl GCC_GNU89_INLINE_CC
dnl checks -fgnu89-inline with the C compiler, if it exists then defines
dnl ENABLE_GNU89_INLINE_CC in both configure script and Makefiles
AC_DEFUN([GCC_GNU89_INLINE_CC],[
  AC_LANG_ASSERT(C)
  if test "X$CC" != "X"; then
    AC_CACHE_CHECK([for -fgnu89-inline],
    gnu89_inline_cv_cc,
    [ gnu89_inline_old_cflags="$CFLAGS"
      CFLAGS="$CFLAGS -fgnu89-inline"
      AC_TRY_COMPILE(,, gnu89_inline_cv_cc=yes, gnu89_inline_cv_cc=no)
      CFLAGS="$gnu89_inline_old_cflags"
    ])
    if test $gnu89_inline_cv_cc = yes; then
      AC_DEFINE([ENABLE_GNU89_INLINE_CC], 1, [Define if gnu89 inlining semantics should be used.])
    fi
	AM_CONDITIONAL([ENABLE_GNU89_INLINE_CC], test "x$gnu89_inline_cv_cc" = "xyes")
  fi
])
