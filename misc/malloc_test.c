#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "malloc.h"

char mem[1024];

void info(chunk_t* it) {
    chunk_t* ch = it;
    for (; ch; ch = ch->next) {
        printf("chunk:%d, size:%d, %s\n",
            (char*)ch - (char*)it,
            ch->size,
            ch->free ? "FREE" : "RESERVED");
    }
    printf("=========\n\n");
}

void* all (chunk_t* it, size_t size) {
    void* p = do_malloc(it, size);
    printf("  malloc: size:%d ptr:0x%x\n", size, p ? ((char*)p - (char*)it) : 0);
    info(it);
    return p;
}

void fr (chunk_t* it, void* p) {
    do_free(it, p);
    printf("  free: ptr:0x%X\n", p ? ((char*)p - (char*)it) : 0);
    info(it);
}

int main (int argc, char** argv) {
    void *p1, *p2, *p3, *p4, *p5, *p6;
    int i;

    memset(mem, 0, sizeof(mem));
    chunklist_init((chunk_t*)mem, sizeof(mem));

    info((chunk_t*)mem);

    for (i=0; i != 5; i++) {
        p1 = all((chunk_t*)mem, 128);
        p2 = all((chunk_t*)mem, 128);
        p3 = all((chunk_t*)mem, 128);
        p4 = all((chunk_t*)mem, 128);
        fr((chunk_t*)mem, p1);
        p1 = all((chunk_t*)mem, 128);
        fr((chunk_t*)mem, p1);
        fr((chunk_t*)mem, p3);
        fr((chunk_t*)mem, p2);
        fr((chunk_t*)mem, p4);
    }

}
