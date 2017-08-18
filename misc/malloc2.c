
#include "malloc2.h"

void* do_malloc (mempage_t *page, size_t len) {
    chunk_t *ch;
    chunk_t *it;

    for (it = page->heap_start; it; it = it->next) {
        if (!it->free) { /* occupied */
            continue;
        }
        if (len + sizeof(chunk_t) > it->size) {
            continue; /* free but too small  */
        }
        if (len + sizeof(chunk_t) + sizeof(chunk_t) >= it->size) {
            /* free and just perfect size, reserve it! */
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


void do_free (mempage_t* page, void *p) {
}



void init_mempage (mempage_t* page, char* heap_start, size_t heap_size) {
    
}

