#ifndef LIBSIXEL_CONFIG_H
#define LIBSIXEL_CONFIG_H

#define PACKAGE_NAME "libsixel"
#define PACKAGE_VERSION "1.8.7"
#define PACKAGE_STRING "libsixel 1.8.7"
#define PACKAGE_TARNAME "libsixel"
#define PACKAGE_BUGREPORT "saitoha@me.com"
#define PACKAGE_URL ""
#define VERSION "1.8.7"

#define STDC_HEADERS 1

#define HAVE_ASSERT_H 1
#define HAVE_CTYPE_H 1
#define HAVE_ERRNO_H 1
#define HAVE_FCNTL_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_IO_H 1
#define HAVE_LIMITS_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_MATH_H 1
#define HAVE_MEMORY_H 1
#define HAVE_SETJMP_H 1
#define HAVE_SIGNAL_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDIO_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_TIME_H 1

#define HAVE_CALLOC 1
#define HAVE_MALLOC 1
#define HAVE_REALLOC 1
#define HAVE_MEMCPY 1
#define HAVE_MEMMOVE 1
#define HAVE_MEMSET 1
#define HAVE_STRCHR 1
#define HAVE_STRDUP 1
#define HAVE_STRERROR 1
#define HAVE_STRNCMP 1
#define HAVE_STRSTR 1
#define HAVE_STRTOL 1
#define HAVE_STRTOUL 1
#define HAVE_FLOOR 1
#define HAVE_POW 1
#define HAVE_SQRT 1
#define HAVE_CLOCK 1
#define HAVE_LDIV 1
#define HAVE_LOCALECONV 1
#define HAVE_SETMODE 1
#define HAVE__SETMODE 1
#define HAVE_ISATTY 1
#define HAVE_STAT 1
#define HAVE_SIGNAL 1
#define HAVE_SETJMP 1
#define HAVE_LONGJMP 1
#define HAVE_CLEARERR 1

#define HAVE_DECL_SIGHUP 0
#define HAVE_DECL_SIGINT 1
#define HAVE_DECL_SIGTERM 1

#define HAVE__BOOL 1
#define HAVE_FUNC_ATTRIBUTE_DEPRECATED 1

#define _CRT_NONSTDC_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#define _USE_MATH_DEFINES

#ifdef _MSC_VER
#include <io.h>
#include <sys/stat.h>

#pragma warning(disable: 4100)  /* unreferenced formal parameter */
#pragma warning(disable: 4132)  /* const object should be initialized */
#pragma warning(disable: 4244)  /* conversion from int to unsigned char */
#pragma warning(disable: 4267)  /* conversion from size_t to int */
#pragma warning(disable: 4706)  /* assignment within conditional expression */
#pragma warning(disable: 4996)  /* POSIX name deprecated */

#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif
#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#endif
#ifndef S_IRUSR
#define S_IRUSR _S_IREAD
#endif
#ifndef S_IWUSR
#define S_IWUSR _S_IWRITE
#endif
#ifndef S_ISDIR
#define S_ISDIR(m)  (((m) & _S_IFMT) == _S_IFDIR)
#endif
#endif

#endif
