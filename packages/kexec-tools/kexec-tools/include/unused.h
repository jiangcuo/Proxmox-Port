#ifndef UNUSED_H
#define UNUSED_H

/* http://sourcefrog.net/weblog/software/languages/C/unused.html */

#ifdef UNUSED
#elif defined(__GNUC__)
# define UNUSED(x) UNUSED_ ## x __attribute__((unused))
#elif defined(__LCLINT__)
# define UNUSED(x) /*@unused@*/ x
#else
# define UNUSED(x) x
#endif

#endif
