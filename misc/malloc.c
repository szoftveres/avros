/* This code comes from avr-glibc::malloc.c, Copyright stuff at the bottom */

#include "malloc.h"

void* do_malloc (mempage_t *page, size_t len) {
    freelist_t *fp1, *fp2, *sfp1, *sfp2;
    char *cp;
    size_t s, avail;

    /*
     * Our minimum chunk size is the size of a pointer (plus the
     * size of the "size" field, but we don't need to account for
     * this), otherwise we could not possibly fit a freelist entry
     * into the chunk later.
     */
    if (len < sizeof(freelist_t) - sizeof(size_t)) {
        len = sizeof(freelist_t) - sizeof(size_t);
    }

    /*
     * First, walk the free list and try finding a chunk that
     * would match exactly.  If we found one, we are done.  While
     * walking, note down the smallest chunk we found that would
     * still fit the request -- we need it for step 2.
     *
     */
    for (s = 0, fp1 = page->freelist, fp2 = NULL; fp1; fp2 = fp1, fp1 = fp1->next) {
        if (fp1->size < len) {
            continue;
        }
        if (fp1->size == len) {
            /*
             * Found it.  Disconnect the chunk from the
             * freelist, and return it.
             */
            if (fp2) {
                fp2->next = fp1->next;
            } else {
                page->freelist = fp1->next;
            }
            return ((void*) (&(fp1->next)));
        } else {
            if ((s == 0) || (fp1->size < s)) {
                /* this is the smallest chunk found so far */
                s = fp1->size;
                sfp1 = fp1;
                sfp2 = fp2;
            }
        }
    }
    /*
     * Step 2: If we found a chunk on the freelist that would fit
     * (but was too large), look it up again and use it, since it
     * is our closest match now.  Since the freelist entry needs
     * to be split into two entries then, watch out that the
     * difference between the requested size and the size of the
     * chunk found is large enough for another freelist entry; if
     * not, just enlarge the request size to what we have found,
     * and use the entire chunk.
     */
    if (s) {
        if ((s - len) < sizeof(freelist_t)) {
            /* Disconnect it from freelist and return it. */
            if (sfp2) {
                sfp2->next = sfp1->next;
            } else {
                page->freelist = sfp1->next;
            }
            return ((void*) (&(sfp1->next)));
        }
        /*
         * Split them up.  Note that we leave the first part
         * as the new (smaller) freelist entry, and return the
         * upper portion to the caller.  This saves us the
         * work to fix up the freelist chain; we just need to
         * fixup the size of the current entry, and note down
         * the size of the new chunk before returning it to
         * the caller.
         */
        cp = (char *)sfp1;
        s -= len;
        cp += s;
        sfp2 = (freelist_t *)cp;
        sfp2->size = len;
        sfp1->size = s - sizeof(size_t);
        return ((void*) (&(sfp2->next)));
    }
    /*
     * Step 3: If the request could not be satisfied from a
     * freelist entry, just prepare a new chunk.  This means we
     * need to obtain more memory first.  The largest address just
     * not allocated so far is remembered in the page->brkval variable.
     * Under Unix, the "break value" was the end of the data
     * segment as dynamically requested from the operating system.
     * Since we don't have an operating system, just make sure
     * that we don't collide with the stack.
     */
    if (!page->brkval) {
        page->brkval = page->heap_start;
    }
    cp = page->heap_end;
    if (cp <= page->brkval) {
      /*
       * Memory exhausted.
       */
      return (NULL);
    }
    avail = cp - page->brkval;
    /*
     * Both tests below are needed to catch the case len >= 0xfffe.
     */
    if (avail >= len && avail >= len + sizeof(size_t)) {
        fp1 = (freelist_t *)page->brkval;
        page->brkval += len + sizeof(size_t);
        fp1->size = len;
        return ((void*) (&(fp1->next)));
    }
    /*
     * Step 4: There's no help, just fail. :-/
     */
    return (NULL);
}


void do_free (mempage_t* page, void *p) {
    freelist_t *fp1, *fp2, *fpnew;
    char *cp1, *cp2, *cpnew;

    /* ISO C says free(NULL) must be a no-op */
    if (!p) {
        return;
    }

    cpnew = p;
    cpnew -= sizeof(size_t);
    fpnew = (freelist_t *)cpnew;
    fpnew->next = 0;

    /*
     * Trivial case first: if there's no freelist yet, our entry
     * will be the only one on it.  If this is the last entry, we
     * can reduce page->brkval instead.
     */
    if (!page->freelist) {
        if ((char *)p + fpnew->size == page->brkval) {
            page->brkval = cpnew;
        } else {
            page->freelist = fpnew;
        }
        return;
    }

    /*
     * Now, find the position where our new entry belongs onto the
     * freelist.  Try to aggregate the chunk with adjacent chunks
     * if possible.
     */
    for (fp1 = page->freelist, fp2 = NULL; fp1; fp2 = fp1, fp1 = fp1->next) {
        if (fp1 < fpnew) {
            continue;
        }
        cp1 = (char *)fp1;
        fpnew->next = fp1;
        if ((char *)&(fpnew->next) + fpnew->size == cp1) {
            /* upper chunk adjacent, assimilate it */
            fpnew->size += fp1->size + sizeof(size_t);
            fpnew->next = fp1->next;
        }
        if (!fp2) {
            /* new head of freelist */
            page->freelist = fpnew;
            return;
        }
        break;
    }
    /*
     * Note that we get here either if we hit the "break" above,
     * or if we fell off the end of the loop.  The latter means
     * we've got a new topmost chunk.  Either way, try aggregating
     * with the lower chunk if possible.
     */
    fp2->next = fpnew;
    cp2 = (char *)&(fp2->next);
    if (cp2 + fp2->size == cpnew) {
        /* lower junk adjacent, merge */
        fp2->size += fpnew->size + sizeof(size_t);
        fp2->next = fpnew->next;
    }
    /*
     * If there's a new topmost chunk, lower page->brkval instead.
     */

    /* advance to entry just before end of list */
    for (fp1 = page->freelist, fp2 = NULL; fp1->next; fp2 = fp1, fp1 = fp1->next);

    cp2 = (char *)&(fp1->next);
    if (cp2 + fp1->size == page->brkval) {
        if (fp2 == NULL) {
            /* Freelist is empty now. */
            page->freelist = NULL;
        } else {
            fp2->next = NULL;
        }
        page->brkval = cp2 - sizeof(size_t);
    }
}

/*
===========================================================================

   Copyright (c) 2002, 2004, 2010 Joerg Wunsch
   Copyright (c) 2010  Gerben van den Broeke
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:

   * Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.

   * Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in
     the documentation and/or other materials provided with the
     distribution.

   * Neither the name of the copyright holders nor the names of
     contributors may be used to endorse or promote products derived
     from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.
 
===========================================================================
*/

