#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "malloc.h"


mempage_t pg;

char mem[1024];

void info(mempage_t* page) {
    printf("Mem start:%p, Mem end:%p, brkval:%p, freelist:%p\n", 
                (page->heap_start - page->heap_start),
                (page->heap_end - page->heap_start),
                page->brkval ? (page->brkval - page->heap_start) : NULL,
                page->freelist ? ((char*)page->freelist - page->heap_start) : NULL);

}

void* all(mempage_t* page, size_t size) {
    void* p = do_malloc(page, size); 
    printf("malloc: size:0x%x ptr: %p\n", size, p ? ((char*)p - page->heap_start) : NULL);
    info(page);
    return p;
}

void fr(mempage_t* page, void* p) {
    do_free(page, p); 
    printf("free: ptr: %p\n", p ? ((char*)p - page->heap_start) : NULL);
    info(page);
}
int main (int argc, char** argv) {
    void *p1, *p2, *p3, *p4, *p5, *p6;

    memset(mem, 0, sizeof(mem));
    pg.heap_start = &mem[0];
    pg.heap_end = &mem[1024];
    pg.brkval = NULL;
    pg.freelist = NULL;

    info(&pg);
    printf("\n\n");

    p1 = all(&pg, 256);
    p2 = all(&pg, 512);
    p3 = all(&pg, 128);
    p4 = all(&pg, 256);
    fr(&pg, p2);
    p2 = all(&pg, 128);
    fr(&pg, p2);
    fr(&pg, p1);
    fr(&pg, p3);
    


}
