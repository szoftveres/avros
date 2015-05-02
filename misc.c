#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include "kernel.h"
#include "misc.h"
#include "dm.h"
#include "pm.h"


void
mfputc (int fd, int data) {
    if (writec(fd, data) == EOF) {
        mexit(0); /* Behaves like SIGPIPE */
    } 
    return;
}

/*
 *
 */


int
mgetc (void) {
    return (readc(0));
}

/*
 *
 */

void
mfputu (int fd, unsigned int num) {
    if(num/10){
        mfputu(fd, num/10);
    }
    return (mfputc(fd, (num%10) + '0'));
}

/*
 *
 */

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
   mfprintf(fd, "0x");
   mfputx_internal(fd, num);
   return;
}

 
void mfprintf (int fd, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    while (*fmt) {
        if (*fmt == '%') {
            int i;
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
                i = va_arg(ap, int);
                mfputx(fd, i);
                break;
              case 'd':
              case 'i':
                i = va_arg(ap, int);
                mfputu(fd, i);
                break;
            }
        } else {
            mfputc(fd, *fmt);
        }
        fmt++;
    }
    va_end(ap);
}





getopt_p
initgetopt (void) {
    getopt_p opt_p;
    opt_p = pmmalloc(sizeof(getopt_t));
    if (opt_p) {
        memset(opt_p, 0, sizeof(getopt_t));
        opt_p->optind = 1;
        opt_p->sp = 1;
    }
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
        mfprintf(2, "%s: opt %c?\n", argv[0], c);
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
		    mfprintf(2, "%s: opt needs arg: %c\n", argv[0], c);
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
    mfprintf(2, "%s: %s?\n", argv[0], s);
}

void noargs(char** argv) {
    mfprintf(2, "%s: args?\n", argv[0]);
}
