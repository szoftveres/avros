#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include "../../../kernel/include/kernel.h"
#include "../../../servers/include/vfs.h"
#include "../../../servers/include/pm.h"
#include "../include/mstdlib.h"

int
mgetc (void) {
    return (readc(STDIN));
}

/*
 *
 */

void
mfputc (int fd, int data) {
    if (writec(fd, data) == EOF) {
        mexit(0); /* Behaves like SIGPIPE */
    } 
    return;
}

void
mfputu (int fd, unsigned int num) {
    if(num/10){
        mfputu(fd, num/10);
    }
    return (mfputc(fd, (num%10) + '0'));
}

static void
mfputx_internal (int fd, unsigned int num) {
    if (num / 0x10) {
        mfputx_internal(fd, num / 0x10);
    }
    mfputc(fd, (num % 0x10) + (((num % 0x10) > 9) ? ('a' - 10) : ('0')));
    return;
}

void
mfputx (int fd, unsigned int num) {
   mfputc(fd, '0');
   mfputc(fd, 'x');
   mfputx_internal(fd, num);
   return;
}

 
void mfprintf (int fd, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    for (;*fmt; fmt++) {
        if (*fmt != '%') {
            mfputc(fd, *fmt);
            continue;
        }
        fmt++;
        switch (*fmt) {
          case 'c':
            mfputc(fd, (char)va_arg(ap, int));
            break;
          case 's': {
                char* s = va_arg(ap, char*);
                while (*s) {
                    mfputc(fd, *s++);
                }
            }
            break;
          case 'x':
          case 'X':
            mfputx(fd, va_arg(ap, int));
            break;
          case 'd':
          case 'i':
            mfputu(fd, va_arg(ap, int));
            break;
        }
    }
    va_end(ap);
}

/*
=============
*/


getopt_p
initgetopt (void) {
    getopt_p opt_p;
    opt_p = pmmalloc(sizeof(getopt_t));
    ASSERT(opt_p);
    memset(opt_p, 0, sizeof(getopt_t));
    opt_p->optind = 1;
    opt_p->sp = 1;
    return (opt_p);
}


int
getopt (char* argv[], char* opts, getopt_p opt_p) {
	int c;
	char *cp;

    if (!opt_p) {
        return (EOF);
    }
	if (opt_p->sp == 1) {
		if ((opt_p->optind >= argc(argv)) ||
		   (argv[opt_p->optind][0] != '-') ||
           (argv[opt_p->optind][1] == '\0')) {
			return (EOF);
        } else if (!strcmp(argv[opt_p->optind], "--")) {
			opt_p->optind++;
			return (EOF);
		}
    }
	c = argv[opt_p->optind][opt_p->sp];
	if ((c == ':') || ((cp=strchr(opts, c)) == NULL)) {
        mfprintf(STDERR, "%s: -%c?\n", argv[0], c);
		if (argv[opt_p->optind][++opt_p->sp] == '\0') {
			opt_p->optind++;
			opt_p->sp = 1;
		}
		return ('?');
	}
	if (*++cp == ':') {
		if (argv[opt_p->optind][opt_p->sp+1] != '\0') {
			opt_p->optarg = &argv[opt_p->optind++][opt_p->sp+1];
        } else if (++opt_p->optind >= argc(argv)) {
		    mfprintf(STDERR, "%s: -%c needs arg\n", argv[0], c);
			opt_p->sp = 1;
			return ('?');
		} else {
			opt_p->optarg = argv[opt_p->optind++];
        }
		opt_p->sp = 1;
	} else {
		if (argv[opt_p->optind][++opt_p->sp] == '\0') {
			opt_p->sp = 1;
			opt_p->optind++;
		}
		opt_p->optarg = NULL;
	}
	return (c);
}



void unknown(char** argv, const char* s) {
    mfprintf(STDERR, "%s: %s?\n", argv[0], s);
}

void noargs(char** argv) {
    mfprintf(STDERR, "%s: args?\n", argv[0]);
}

void massert (int val, char* file, int line) {
    if (!val) {
        mfprintf(STDERR, "[%s:%d] assert fail\n", file, line);
        mexit(-1);
    }
}
