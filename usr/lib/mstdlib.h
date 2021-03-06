#ifndef _MSTDLIB_H_
#define _MSTDLIB_H_

enum {
    STDIN = 0,
    STDOUT,
    STDERR,
    /* Must be the last one */
    STDMAX
};


void mfputc (int fd, int data);

int mgetc (void);

void mfputu (int fd, unsigned int num);

void mfputx (int fd, unsigned int num);


typedef struct getopt_s {
    char*   optarg;
    int     optind; /*=1*/
    int     sp;     /*=1*/
} getopt_t, *getopt_p;


getopt_p initgetopt (void);

int getopt (char* argv[], char* opts, getopt_p opt_p);

void mfprintf (int fd, const char* fmt, ...);

void unknown (char** argv, const char* s);
void noargs (char** argv);
void massert (int val, char* file, int line);

#define ASSERT(x) massert((int)x, __FILE__, __LINE__)

#endif
