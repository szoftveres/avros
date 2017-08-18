#ifndef _MALLOC_H_
#define _MALLOC_H_


#include <stdlib.h> /* size_t definition */


typedef struct chunk_s {
    size_t          size;
    char            free;
    struct chunk_s  *next;
} chunk_t;


typedef struct mempage_s {
    char        *heap_start;
    char        *heap_end;
    char        *brkval;     /* init to NULL */
    freelist_t  *freelist; /* init to NULL */
} mempage_t;


void* do_malloc (mempage_t* page, size_t len);
void do_free (mempage_t* page, void *p);
void init_mempage (mempage_t* page, char* heap_start, size_t heap_size);


#endif


