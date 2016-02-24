#ifndef _MALLOC_H_
#define _MALLOC_H_


#include <stdlib.h>


typedef struct freelist_s {
    size_t              size;
    struct freelist_s   *next;
} freelist_t;


typedef struct mempage_s {
    char        *heap_start;
    char        *heap_end;
    char        *brkval;     /* init to NULL */
    freelist_t  *freelist_p; /* init to NULL */
} mempage_t;


void* do_malloc (mempage_t* page, size_t len);
void do_free (mempage_t* page, void *p);


#endif


