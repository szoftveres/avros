#include "malloc.h"

/*
 * Simple greedy malloc/free implementation
 */

void* do_malloc (chunk_t* it, size_t len) {
    chunk_t *ch;

    if (!len) {
        return (NULL);
    }
    for (; it; it = it->next) {
        if (!it->free) { /* occupied */
            continue;
        }
        if (len + sizeof(chunk_t) > it->size) {
            continue; /* free but too small  */
        }
        if (len + sizeof(chunk_t) + sizeof(chunk_t) >= it->size) {
            /* free and just perfect in size, reserve it! */
            it->free = 0;
            return ((char*)it + sizeof(chunk_t));
        }
        /* free but big, split it! */
        ch = (chunk_t*)((char*)it + len + sizeof(chunk_t));
        ch->next = it->next;
        it->next = ch;

        ch->free = it->free;

        ch->size = it->size - (len + sizeof(chunk_t));
        it->size -= ch->size;

        it->free = 0;
        return ((char*)it + sizeof(chunk_t));
    }
    return (NULL);
}


void do_free (chunk_t *it, void *p) {
    if (!p) {
        return;
    }
    /* mark chunk as free */
    ((chunk_t*)((char*)p - sizeof(chunk_t)))->free = 1;

    /* merge free chunks */
    for (; it; it = it->next) {
        while (it->free && it->next && it->next->free) {
            /* merge with next free */
            it->size += it->next->size;
            it->next = it->next->next;
        }
    }
    return;
}



void chunklist_init (chunk_t *it, size_t heap_size) {
    it->free = 1;
    it->size = heap_size;
    it->next = NULL;
}

