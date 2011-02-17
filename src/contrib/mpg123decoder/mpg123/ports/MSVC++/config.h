#define strcasecmp _strcmpi
#define strncasecmp _strnicmp
#define strdup _strdup

#define HAVE_STRERROR 1
#define HAVE_SYS_TYPES_H 1

#define HAVE_STRDUP
#define HAVE_STDLIB_H
#define HAVE_STRING_H

/* We want some frame index, eh? */
#define FRAME_INDEX 1
#define INDEX_SIZE 1000

/* also gapless playback! */
#define GAPLESS 1
/* #ifdef GAPLESS
#undef GAPLESS
#endif */

/* #define DEBUG
#define EXTRA_DEBUG */

#define REAL_IS_FLOAT

#define inline __inline

/* we are on win32 */
#define HAVE_WINDOWS_H

/* use the unicode support within libmpg123 */
#ifdef UNICODE
	#define WANT_WIN32_UNICODE
#endif
