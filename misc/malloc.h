#ifndef _MALLOC_H_
#define _MALLOC_H_


#include <stdlib.h> /* size_t type */


typedef struct chunk_s {
    struct chunk_s  *next;
    size_t          size;
    char            free;
} chunk_t;


void* do_malloc (chunk_t* it, size_t len);
void do_free (chunk_t* it, void *p);
void chunklist_init (chunk_t* it, size_t heap_size);


#endif


